////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#include "Crosshair.hpp"

#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-scene/CelestialObserver.hpp"
#include "../../../src/cs-scene/CelestialObject.hpp"
#include "../../../src/cs-graphics/TextureLoader.hpp"
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

uniform mat4  uMatModelView;
uniform mat4  uMatProjection;
uniform float uExtent;
uniform float uSize;

// inputs
layout(location = 0) in vec2 iQuadPos;

// outputs
out vec2  vTexCoords;

void main() {
  // Convert iQuadPos from [-1, 1] to [0, 1] for texture sampling
  vTexCoords  = (iQuadPos + 1.0) / 2.0;

  // Place quad on xy-axis, in front (negative z-axis) of the observer
  vec4 quadPos= vec4(iQuadPos.x * uSize, iQuadPos.y * uSize, -10.0 * uExtent, 1.0);
  vec3 pos    = (uMatModelView * quadPos).xyz;
  gl_Position = uMatProjection * vec4(pos, 1);
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
  //oColor *= 1 - clamp(length(vTexCoords), 0, 1);
})";

////////////////////////////////////////////////////////////////////////////////////////////////////

//Constructor
Crosshair::Crosshair(
    std::shared_ptr<cs::core::SolarSystem> solarSystem, Plugin::Settings::Crosshair& crosshairSettings)
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

  // Load Texture
  mTexture = cs::graphics::TextureLoader::loadFromFile(crosshairSettings.mTexture.get());
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

//Destructor
Crosshair::~Crosshair() {
  // remove Nodes from GUI
  auto* platform = GetVistaSystem()
                       ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                       ->GetPlatformNode();
  platform->DisconnectChild(mOffsetNode.get());
  mOffsetNode->DisconnectChild(mGLNode.get());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Crosshair::configure(Plugin::Settings::Crosshair& crosshairSettings) {
  // check if texture settings changed
  if (mCrosshairSettings.mTexture.get() != crosshairSettings.mTexture.get()) {
    mTexture = cs::graphics::TextureLoader::loadFromFile(crosshairSettings.mTexture.get());
    mTexture->SetWrapS(GL_REPEAT);
    mTexture->SetWrapR(GL_REPEAT);
  }
  mCrosshairSettings = crosshairSettings;
  // update Offset Node
  mOffsetNode->SetTranslation(0.0F, mCrosshairSettings.mOffset.get(), 0.0F);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Crosshair::update() {
  mOffsetNode->SetTranslation(0.0F, mCrosshairSettings.mOffset.get(), 0.0F);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool Crosshair::Do() {
  //TODO get Direction info into: mSolarSystem->getObserver();
  auto const& mObserver = mSolarSystem->getObserver();
  auto        dir       = mObserver.getVelocityDirection();
  auto        speed     = mObserver.getVelocityMagnitude();

  if (!std::isfinite(dir.x) || !std::isfinite(dir.y) || !std::isfinite(dir.z)) {
    logger().error("NaN detected in dir!");
  }

  if (!std::isfinite(speed)) {
    logger().error("NaN detected in speed!");
  }


  //logger().info("Observer moving towards: ({}, {}, {}), speed: {}", dir.x, dir.y, dir.z, speed);

  // do nothing if crosshair is disabled
  if (!mCrosshairSettings.mEnabled.get()) {
    return true;
  }

  cs::utils::FrameStats::ScopedTimer timer("VRAccessibility-Crosshair");

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
  mShader.SetUniform(mUniforms.extent, mCrosshairSettings.mExtent.get());
  mShader.SetUniform(mUniforms.size, mCrosshairSettings.mSize.get());
  mShader.SetUniform(mUniforms.alpha, mCrosshairSettings.mAlpha.get());
  glUniform4fv(mUniforms.color, 1,
      glm::value_ptr(Plugin::GetColorFromHexString(mCrosshairSettings.mColor.get())));

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

bool Crosshair::GetBoundingBox(VistaBoundingBox& bb) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::vraccessibility
