////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#include "VirtualHorizon.hpp"

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

const char* VirtualHorizon::VERT_SHADER = R"(
#version 330

uniform mat4 uMatModelView;
uniform mat4 uMatProjection;
uniform float uExtent;
uniform float uSize;
uniform vec3 uObserverDir; 
uniform vec3 uPlanetUp;

layout(location = 0) in vec2 iQuadPos;
out vec2 vTexCoords;

void main() {

  vTexCoords = (iQuadPos + 1.0) * 0.5;

  // --- Compute PITCH angle only ---
  // Clamp dot to avoid NaNs
  float d = clamp(dot(normalize(uObserverDir), normalize(uPlanetUp)), -1.0, 1.0);

  // pitch: positive when climbing, negative when descending
  float pitch = asin(d);

  // --- Rotate quad ONLY around local X-axis ---
  float c = cos(pitch);
  float s = sin(pitch);

  mat3 rot = mat3(
    1.0, 0.0, 0.0,
    0.0,    c,   -s,
    0.0,    s,    c
  );

  // Base position (in camera space)
  vec3 basePos = vec3(iQuadPos.x * uSize * 10, iQuadPos.y * uSize * 10, -15.0 * uExtent);

  // Apply rotation
  vec3 rotatedPos = rot * basePos;

  // Transform to clip space
  vec3 pos = (uMatModelView * vec4(rotatedPos, 1.0)).xyz;
  gl_Position = uMatProjection * vec4(pos, 1.0);
}
)";

////////////////////////////////////////////////////////////////////////////////////////////////////

const char* VirtualHorizon::FRAG_SHADER = R"(
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

// Constructor
VirtualHorizon::VirtualHorizon(std::shared_ptr<cs::core::SolarSystem> solarSystem,
    Plugin::Settings::VirtualHorizon&                                 virtualHorizonSettings)
    : mSolarSystem(std::move(solarSystem))
    , mVirtualHorizonSettings(virtualHorizonSettings) {

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

  // Load Texture
  mTexture = cs::graphics::TextureLoader::loadFromFile(virtualHorizonSettings.mTexture.get());
  mTexture->SetWrapS(GL_REPEAT);
  mTexture->SetWrapR(GL_REPEAT);

  // Add to scenegraph
  VistaSceneGraph* pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();

  // add to GUI Node (gui-method)
  auto* platform = GetVistaSystem()
                       ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                       ->GetPlatformNode();
  mOffsetNode.reset(pSG->NewTransformNode(platform));
  mGLNode.reset(pSG->NewOpenGLNode(mOffsetNode.get(), this));

  VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
      mGLNode.get(), static_cast<int>(cs::utils::DrawOrder::eTransparentItems) - 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

// Destructor
VirtualHorizon::~VirtualHorizon() {
  // remove Nodes from GUI
  auto* platform = GetVistaSystem()
                       ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                       ->GetPlatformNode();
  platform->DisconnectChild(mOffsetNode.get());
  mOffsetNode->DisconnectChild(mGLNode.get());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void VirtualHorizon::configure(Plugin::Settings::VirtualHorizon& virtualHorizonSettings) {
  // check if texture settings changed
  if (mVirtualHorizonSettings.mTexture.get() != virtualHorizonSettings.mTexture.get()) {
    mTexture = cs::graphics::TextureLoader::loadFromFile(virtualHorizonSettings.mTexture.get());
    mTexture->SetWrapS(GL_REPEAT);
    mTexture->SetWrapR(GL_REPEAT);
  }
  mVirtualHorizonSettings = virtualHorizonSettings;
  // update Offset Node
  mOffsetNode->SetTranslation(0.0F, mVirtualHorizonSettings.mOffset.get(), 0.0F);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void VirtualHorizon::update() {
  mOffsetNode->SetTranslation(0.0F, mVirtualHorizonSettings.mOffset.get(), 0.0F);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool VirtualHorizon::Do() {
  auto const& mObserver           = mSolarSystem->getObserver();
  auto        dir                 = mObserver.getVelocityDirection();
  auto        speed               = mObserver.getVelocityMagnitude();
  glm::dvec3  nearestPlanetPos    = mObserver.getClosestPlanetToObserverPosition();
  glm::dvec3  nearestPlanetNormal = glm::normalize(nearestPlanetPos);
  // to be used by the shader:
  glm::vec3 observerDirF = glm::vec3(dir);
  glm::vec3 planetUpF    = glm::vec3(nearestPlanetNormal);

  if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z)) {
    logger().error("NaN detected in dir!");
  }

  if (!std::isfinite(speed)) {
    logger().error("NaN detected in speed!");
  }

  // do nothing if virtualHorizon is disabled
  if (!mVirtualHorizonSettings.mEnabled.get()) {
    return true;
  }

  cs::utils::FrameStats::ScopedTimer timer("VRAccessibility-VirtualHorizon");

  mShader.Bind();

  // Get modelview and projection matrices
  std::array<GLfloat, 16> glMatMV{};
  std::array<GLfloat, 16> glMatP{};
  glGetFloatv(GL_MODELVIEW_MATRIX, glMatMV.data());
  glGetFloatv(GL_PROJECTION_MATRIX, glMatP.data());

  // Quads Backface should also be drawn
  glDisable(GL_CULL_FACE);

  // Set uniforms
  glUniformMatrix4fv(mUniforms.modelViewMatrix, 1, GL_FALSE, glMatMV.data());
  glUniformMatrix4fv(mUniforms.projectionMatrix, 1, GL_FALSE, glMatP.data());
  mShader.SetUniform(mUniforms.texture, 0);
  mShader.SetUniform(mUniforms.extent, mVirtualHorizonSettings.mExtent.get());
  mShader.SetUniform(mUniforms.size, mVirtualHorizonSettings.mSize.get());
  mShader.SetUniform(mUniforms.alpha, mVirtualHorizonSettings.mAlpha.get());

  // Vectors used for calculating the rotation of the virtual horizon in the vertex shader
  glUniform3fv(mUniforms.observerDir, 1, glm::value_ptr(observerDirF));
  glUniform3fv(mUniforms.planetUp, 1, glm::value_ptr(planetUpF));
  // Color Vector
  glUniform4fv(mUniforms.color, 1,
      glm::value_ptr(Plugin::GetColorFromHexString(mVirtualHorizonSettings.mColor.get())));

  // Bind Texture
  mTexture->Bind(GL_TEXTURE0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Set filtering. GL_LINEAR is smooth, GL_NEAREST is pixelated.
  // MIN_FILTER is for when texture is small (far away)
  // MAG_FILTER is for when texture is large (close up)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Draw
  glPushAttrib(GL_ENABLE_BIT | GL_BLEND | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE); // turns black-mask texture into alpha (technically not correct)

  // Disable depth test -> always draw on top
  glDisable(GL_DEPTH_TEST);
  glDepthMask(false);

  mVAO.Bind();

  // Only one draw call
  glDrawArrays(GL_QUADS, 0, 4);

  mVAO.Release();

  // Clean Up
  mTexture->Unbind(GL_TEXTURE0);

  glPopAttrib();

  mShader.Release();

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool VirtualHorizon::GetBoundingBox(VistaBoundingBox& bb) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::vraccessibility
