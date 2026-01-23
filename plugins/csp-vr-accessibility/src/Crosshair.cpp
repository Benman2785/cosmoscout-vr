////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#include "Crosshair.hpp"

#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-graphics/TextureLoader.hpp"
#include "../../../src/cs-scene/CelestialObject.hpp"
#include "../../../src/cs-scene/CelestialObserver.hpp"
#include "../../../src/cs-utils/FrameStats.hpp"
#include "logger.hpp"

#include <VistaKernel/DisplayManager/VistaDisplayManager.h>
#include <VistaKernel/DisplayManager/VistaDisplaySystem.h>
#include <VistaKernel/GraphicsManager/VistaGraphicsManager.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/InteractionManager/VistaUserPlatform.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>
#include <glm/gtc/type_ptr.hpp>

#include <utility>

namespace csp::vraccessibility {

////////////////////////////////////////////////////////////////////////////////////////////////////

const char* Crosshair::VERT_SHADER = R"(
#version 330

uniform mat4 uMatModelView;
uniform mat4 uMatProjection;
uniform float uExtent;
uniform float uSize;

uniform vec3 uObserverDir; // Flugrichtung
uniform vec3 uPlanetUp;    // Oben
uniform float uRoll;       // Der berechnete Roll-Winkel

layout(location = 0) in vec2 iQuadPos;
out vec2 vTexCoords;

void main() {

  vTexCoords = (iQuadPos + 1.0) * 0.5;

  vec3 velocity = normalize(uObserverDir);
  vec3 planetUp = normalize(uPlanetUp);

  // --- 1. PITCH (Nase hoch/runter) ---
  float d = clamp(dot(velocity, planetUp), -1.0, 1.0);
  float pitch = -asin(d);

  // --- 2. ROLL (wird direkt übergeben) ---
  float roll = uRoll;

  // --- 3. ROTATIONSMATRIZEN ---

  // Rotation um X (Pitch)
  float cp = cos(pitch);
  float sp = sin(pitch);
  mat3 rotX = mat3(
    1.0, 0.0, 0.0,
    0.0,  cp, -sp,
    0.0,  sp,  cp
  );

  // Rotation um Z (Roll)
  float cr = cos(roll);
  float sr = sin(roll);
  mat3 rotZ = mat3(
     cr, -sr, 0.0,
     sr,  cr, 0.0,
    0.0, 0.0, 1.0
  );

  // Basis-Position
  vec3 basePos = vec3(iQuadPos.x * uSize * 10, iQuadPos.y * uSize * 10, -15.0 * uExtent);

  // Erst Pitch, dann Roll
  vec3 rotatedPos = rotZ * (rotX * basePos);

  vec3 pos = (uMatModelView * vec4(rotatedPos, 1.0)).xyz;
  gl_Position = uMatProjection * vec4(pos, 1.0);
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const char* Crosshair::FRAG_SHADER = R"(
#version 330

uniform sampler2D uTexture;
uniform float     uAlpha;
uniform float     uExtent;
uniform float     uSize;
uniform vec4      uCustomColor;

// inputs
in vec2 vTexCoords;

// outputs
layout(location = 0) out vec3 oColor;

void main() {
  oColor = texture(uTexture, vTexCoords).rgb;
  oColor *= uCustomColor.rgb;
  oColor *= uAlpha;
})";

////////////////////////////////////////////////////////////////////////////////////////////////////

Crosshair::Crosshair(std::shared_ptr<cs::core::SolarSystem> solarSystem,
                     Plugin::Settings::Crosshair&             crosshairSettings)
    : mSolarSystem(std::move(solarSystem))
    , mCrosshairSettings(crosshairSettings) {

  // Create initial Quad
  std::array<glm::vec2, 4> vertices{};
  vertices[0] = glm::vec2(-1.F, -1.F);
  vertices[1] = glm::vec2(1.F, -1.F);
  vertices[2] = glm::vec2(1.F, 1.F);
  vertices[3] = glm::vec2(-1.F, 1.F);

  mVBO.Bind(GL_ARRAY_BUFFER);
  mVBO.BufferData(vertices.size() * sizeof(glm::vec2), vertices.data(), GL_STATIC_DRAW);
  mVBO.Release();

  mVAO.EnableAttributeArray(0);
  mVAO.SpecifyAttributeArrayFloat(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), 0, &mVBO);

  // Create shader
  mShader.InitVertexShaderFromString(VERT_SHADER);
  mShader.InitFragmentShaderFromString(FRAG_SHADER);
  mShader.Link();

  // Get Uniform Locations
  mUniforms.modelViewMatrix  = mShader.GetUniformLocation("uMatModelView");
  mUniforms.projectionMatrix = mShader.GetUniformLocation("uMatProjection");
  mUniforms.texture          = mShader.GetUniformLocation("uTexture");
  mUniforms.extent           = mShader.GetUniformLocation("uExtent");
  mUniforms.size             = mShader.GetUniformLocation("uSize");
  mUniforms.alpha            = mShader.GetUniformLocation("uAlpha");
  mUniforms.color            = mShader.GetUniformLocation("uCustomColor");
  
  mUniforms.observerDir      = mShader.GetUniformLocation("uObserverDir");
  mUniforms.planetUp         = mShader.GetUniformLocation("uPlanetUp");
  mUniforms.roll             = mShader.GetUniformLocation("uRoll");

  // Load Texture
  mTexture = cs::graphics::TextureLoader::loadFromFile(crosshairSettings.mTexture.get());
  mTexture->SetWrapS(GL_REPEAT);
  mTexture->SetWrapR(GL_REPEAT);

  // Add to scenegraph
  VistaSceneGraph* pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  auto* platform = GetVistaSystem()
                       ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                       ->GetPlatformNode();
  mOffsetNode.reset(pSG->NewTransformNode(platform));
  mGLNode.reset(pSG->NewOpenGLNode(mOffsetNode.get(), this));

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mGLNode.get(), static_cast<int>(cs::utils::DrawOrder::eTransparentItems) - 1);
}

Crosshair::~Crosshair() {
  auto* platform = GetVistaSystem()
                       ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                       ->GetPlatformNode();
  platform->DisconnectChild(mOffsetNode.get());
  mOffsetNode->DisconnectChild(mGLNode.get());
}

void Crosshair::configure(Plugin::Settings::Crosshair& crosshairSettings) {
  if (mCrosshairSettings.mTexture.get() != crosshairSettings.mTexture.get()) {
    mTexture = cs::graphics::TextureLoader::loadFromFile(crosshairSettings.mTexture.get());
    mTexture->SetWrapS(GL_REPEAT);
    mTexture->SetWrapR(GL_REPEAT);
  }
  mCrosshairSettings = crosshairSettings;
  mOffsetNode->SetTranslation(0.0F, mCrosshairSettings.mOffset.get(), 0.0F);
}

void Crosshair::update() {
  mOffsetNode->SetTranslation(0.0F, mCrosshairSettings.mOffset.get(), 0.0F);
}

bool Crosshair::Do() {
  auto const& mObserver       = mSolarSystem->getObserver();
  auto        dir             = mObserver.getVelocityDirection();
  auto        speed           = mObserver.getVelocityMagnitude();
  glm::dvec3  nearestPlanetPos    = mObserver.getClosestPlanetToObserverPosition();
  glm::dvec3  nearestPlanetNormal = glm::normalize(nearestPlanetPos);

  // --- LOGIK: DREHGESCHWINDIGKEIT ---
  
  double currentTime = GetVistaSystem()->GetFrameClock();
  
  // Wir prüfen, ob dies ein neuer Frame ist (Zeit ist vorangeschritten).
  // In VR rendern wir 2x pro Frame (Links/Rechts).
  // Wenn currentTime == mLastTime, ist dies das zweite Auge -> keine Neuberechnung!
  if (currentTime > mLastTime) {
      
      double dt = currentTime - mLastTime;
      
      // Initialisierung beim allerersten Frame
      if (mLastTime == 0.0) {
          mLastTime = currentTime;
          mLastForward = mObserver.getRotation() * glm::dvec3(0.0, 0.0, -1.0);
          return true;
      }

      // Berechnung durchführen
      if (dt > 0.0001) {
          glm::dvec3 currentForward = mObserver.getRotation() * glm::dvec3(0.0, 0.0, -1.0);
          glm::dvec3 up = nearestPlanetNormal;
          
          // Projektion auf Tangentialebene
          glm::dvec3 f1 = glm::normalize(currentForward - glm::dot(currentForward, up) * up);
          glm::dvec3 f2 = glm::normalize(mLastForward - glm::dot(mLastForward, up) * up);

          glm::dvec3 cross = glm::cross(f2, f1);
          double sinAngle = glm::length(cross);
          double sign = (glm::dot(cross, up) > 0.0) ? 1.0 : -1.0;
          double yawAngle = std::asin(glm::clamp(sinAngle, -1.0, 1.0)) * sign;

          double yawRate = yawAngle / dt;

          // Mapping auf Roll
          const float ROLL_FACTOR = -0.3f; 
          float calculatedRoll = static_cast<float>(yawRate * ROLL_FACTOR);
          
          // Speichern in der Klassenvariable (für beide Augen)
          mCurrentRoll = glm::clamp(calculatedRoll, -1.0f, 1.0f);
          
          // Referenz für nächsten Frame aktualisieren
          mLastForward = currentForward;
      }
      
      // Zeitstempel aktualisieren (damit das zweite Auge weiß, dass es zu spät ist)
      mLastTime = currentTime;
  }
  // --- ENDE LOGIK ---

  if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z)) {
    logger().error("NaN detected in dir!");
  }

  if (!mCrosshairSettings.mEnabled.get()) {
    return true;
  }

  cs::utils::FrameStats::ScopedTimer timer("VRAccessibility-Crosshair");

  mShader.Bind();

  std::array<GLfloat, 16> glMatMV{};
  std::array<GLfloat, 16> glMatP{};
  glGetFloatv(GL_MODELVIEW_MATRIX, glMatMV.data());
  glGetFloatv(GL_PROJECTION_MATRIX, glMatP.data());

  glDisable(GL_CULL_FACE);

  glUniformMatrix4fv(mUniforms.modelViewMatrix, 1, GL_FALSE, glMatMV.data());
  glUniformMatrix4fv(mUniforms.projectionMatrix, 1, GL_FALSE, glMatP.data());
  mShader.SetUniform(mUniforms.texture, 0);
  mShader.SetUniform(mUniforms.extent, mCrosshairSettings.mExtent.get());
  mShader.SetUniform(mUniforms.size, mCrosshairSettings.mSize.get());
  mShader.SetUniform(mUniforms.alpha, mCrosshairSettings.mAlpha.get());

  glm::vec3 observerDirF = glm::vec3(dir);
  glm::vec3 planetUpF    = glm::vec3(nearestPlanetNormal);

  glUniform3fv(mUniforms.observerDir, 1, glm::value_ptr(observerDirF));
  glUniform3fv(mUniforms.planetUp, 1, glm::value_ptr(planetUpF));
  
  if (mUniforms.roll != -1) {
    // Hier nutzen wir jetzt die gespeicherte Klassenvariable
    glUniform1f(mUniforms.roll, mCurrentRoll);
  }

  glUniform4fv(mUniforms.color, 1,
      glm::value_ptr(Plugin::GetColorFromHexString(mCrosshairSettings.mColor.get())));

  mTexture->Bind(GL_TEXTURE0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glPushAttrib(GL_ENABLE_BIT | GL_BLEND | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE); 

  glDisable(GL_DEPTH_TEST);
  glDepthMask(false);

  mVAO.Bind();
  glDrawArrays(GL_QUADS, 0, 4);
  mVAO.Release();

  mTexture->Unbind(GL_TEXTURE0);

  glPopAttrib();
  mShader.Release();

  return true;
}

bool Crosshair::GetBoundingBox(VistaBoundingBox& bb) {
  return false;
}

} // namespace csp::vraccessibility