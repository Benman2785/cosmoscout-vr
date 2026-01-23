////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#ifndef CSP_VR_ACCESSIBILITY_VIEW_OFFSET_HPP
#define CSP_VR_ACCESSIBILITY_VIEW_OFFSET_HPP

#include "../../../src/cs-core/Settings.hpp"
#include "Plugin.hpp"

#include <VistaKernel/GraphicsManager/VistaTransformNode.h>

namespace csp::vraccessibility {

/// This class allows rotating the VirtualPlatform to compensate for the physical 
/// inclination of devices like the Icaros Pro.
class ViewOffset {
 public:
  explicit ViewOffset(Plugin::Settings::ViewOffset const& settings);

  /// Updates the rotation of the VirtualPlatform based on the settings.
  void configure(Plugin::Settings::ViewOffset const& settings);

  /// Call this every frame? Actually not needed for static offset, 
  /// but good to have if we ever want to animate it.
  void update();

 private:
  Plugin::Settings::ViewOffset mSettings;
  VistaTransformNode* mPlatformNode = nullptr;
};

} // namespace csp::vraccessibility

#endif // CSP_VR_ACCESSIBILITY_VIEW_OFFSET_HPP