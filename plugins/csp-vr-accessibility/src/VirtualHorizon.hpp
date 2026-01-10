////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#ifndef CSP_VR_ACCESSIBILITY_VIRTUAL_HORIZON_HPP
#define CSP_VR_ACCESSIBILITY_VIRTUAL_HORIZON_HPP

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

/// The virtualHorizon renders as a texturised quad in front of the observer.
/// The offset between the observer and the virtualHorizon is set by a transformation node in the scenegraph.
/// The settings can be changed at runtime and determine the color and size of the virtualHorizon.
class VirtualHorizon : public IVistaOpenGLDraw {
 public:
  VirtualHorizon(
      std::shared_ptr<cs::core::SolarSystem> solarSystem, Plugin::Settings::VirtualHorizon& virtualHorizonSettings);

  VirtualHorizon(VirtualHorizon const& other) = delete;
  VirtualHorizon(VirtualHorizon&& other)      = default;

  VirtualHorizon& operator=(VirtualHorizon const& other) = delete;
  VirtualHorizon& operator=(VirtualHorizon&& other)      = delete;

  ~VirtualHorizon();

  /// Configures the internal renderer according to the given values.
  void configure(Plugin::Settings::VirtualHorizon& virtualHorizonSettings);

  /// Updates the offset of the virtualHorizon according to the current settings
  void update();

  bool Do() override;
  bool GetBoundingBox(VistaBoundingBox& bb) override;

 private:
  std::shared_ptr<cs::core::Settings>    mSettings;
  std::shared_ptr<cs::core::SolarSystem> mSolarSystem;

  std::unique_ptr<VistaTransformNode> mOffsetNode;
  std::unique_ptr<VistaOpenGLNode>    mGLNode;

  Plugin::Settings::VirtualHorizon&       mVirtualHorizonSettings;
  std::unique_ptr<VistaTexture> mTexture;
  VistaGLSLShader               mShader;
  VistaVertexArrayObject        mVAO;
  VistaBufferObject             mVBO;

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
  } mUniforms;

  static const char* VERT_SHADER;
  static const char* FRAG_SHADER;
}; // class VirtualHorizon
} // namespace csp::vraccessibility

#endif // CSP_VR_ACCESSIBILITY_VIRTUAL_HORIZON_HPP
