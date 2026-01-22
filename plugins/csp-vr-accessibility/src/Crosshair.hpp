////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#ifndef CSP_VR_ACCESSIBILITY_CROSSHAIR_HPP
#define CSP_VR_ACCESSIBILITY_CROSSHAIR_HPP

#include "Plugin.hpp"

#include "../../../src/cs-scene/CelestialObject.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLDraw.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaOGLExt/VistaBufferObject.h>
#include <VistaOGLExt/VistaGLSLShader.h>
#include <VistaOGLExt/VistaTexture.h>
#include <VistaOGLExt/VistaVertexArrayObject.h>

namespace cs::core {
class Settings;
class SolarSystem;
} // namespace cs::core

namespace csp::vraccessibility {

/// The crosshair renders as a texturised quad in front of the observer.
/// The offset between the observer and the crosshair is set by a transformation node in the scenegraph.
/// The settings can be changed at runtime and determine the color and size of the crosshair.
class Crosshair : public IVistaOpenGLDraw {
 public:
  Crosshair(
      std::shared_ptr<cs::core::SolarSystem> solarSystem, Plugin::Settings::Crosshair& crosshairSettings);

  Crosshair(Crosshair const& other) = delete;
  Crosshair(Crosshair&& other)      = default;

  Crosshair& operator=(Crosshair const& other) = delete;
  Crosshair& operator=(Crosshair&& other)      = delete;

  ~Crosshair();

  /// Configures the internal renderer according to the given values.
  void configure(Plugin::Settings::Crosshair& crosshairSettings);

  /// Updates the offset of the crosshair according to the current settings
  void update();

  bool Do() override;
  bool GetBoundingBox(VistaBoundingBox& bb) override;

 private:
  std::shared_ptr<cs::core::Settings>    mSettings;
  std::shared_ptr<cs::core::SolarSystem> mSolarSystem;

  std::unique_ptr<VistaTransformNode> mOffsetNode;
  std::unique_ptr<VistaOpenGLNode>    mGLNode;

  Plugin::Settings::Crosshair&       mCrosshairSettings;
  std::unique_ptr<VistaTexture> mTexture;
  VistaGLSLShader               mShader;
  VistaVertexArrayObject        mVAO;
  VistaBufferObject             mVBO;

  glm::vec3 lastObserverDir = glm::uvec3(0, 0, -1);

  struct {
    uint32_t modelViewMatrix  = 0;
    uint32_t projectionMatrix = 0;
    uint32_t texture          = 0;
    uint32_t extent           = 0;
    uint32_t size             = 0;
    uint32_t alpha            = 0;
    uint32_t color            = 0;
    uint32_t observerDir      = 0; // normalized direction of flight
    uint32_t planetUp         = 0; // normalized up vector from planet surface
    uint32_t viewDir          = 0;
  } mUniforms;

  static const char* VERT_SHADER;
  static const char* FRAG_SHADER;
}; // class Crosshair
} // namespace csp::vraccessibility

#endif // CSP_VR_ACCESSIBILITY_CROSSHAIR_HPP
