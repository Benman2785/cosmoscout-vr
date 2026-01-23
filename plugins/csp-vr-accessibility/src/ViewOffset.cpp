////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ViewOffset.hpp"
#include "../../../src/cs-utils/logger.hpp"

#include <VistaKernel/VistaSystem.h>
#include <VistaKernel/GraphicsManager/VistaGraphicsManager.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/GraphicsManager/VistaGroupNode.h> // Wichtig für Suche
#include <VistaBase/VistaQuaternion.h>
#include <VistaBase/VistaVector3D.h>
#include <glm/glm.hpp>
#include <iostream> // Für std::cout falls logger versagt

namespace csp::vraccessibility {

ViewOffset::ViewOffset(Plugin::Settings::ViewOffset const& settings)
    : mSettings(settings) {

  auto* sg = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
  
  // DEBUG: Wir suchen den Node und geben Bescheid
  logger().info("ViewOffset: Searching for 'VirtualPlatform'...");

  mPlatformNode = dynamic_cast<VistaTransformNode*>(sg->GetNode("VirtualPlatform"));

  if (!mPlatformNode) {
      logger().warn("ViewOffset: 'VirtualPlatform' not found directly. Trying 'VirtualPlatform-Node'...");
      mPlatformNode = dynamic_cast<VistaTransformNode*>(sg->GetNode("VirtualPlatform-Node"));
  }

  // FALLBACK SUCHE: Falls wir den Namen nicht kennen, drucken wir alle Root-Kinder aus
  if (!mPlatformNode) {
      logger().error("ViewOffset: CRITICAL - VirtualPlatform Node NOT found!");
      
      VistaGroupNode* root = sg->GetRoot();
      logger().info("ViewOffset: Dumping SceneGraph Root Children to help you find the name:");
      for (unsigned int i = 0; i < root->GetNumChildren(); ++i) {
          IVistaNode* child = root->GetChild(i);
          logger().info("   Child {}: '{}' (Type: {})", i, child->GetName(), (int)child->GetType());
      }
  } else {
      logger().info("ViewOffset: SUCCESS - Found Node '{}'", mPlatformNode->GetName());
      configure(mSettings);
  }
}

void ViewOffset::configure(Plugin::Settings::ViewOffset const& settings) {
  mSettings = settings;

  if (mPlatformNode) {
      // DEBUG LOGGING
      logger().info("ViewOffset: Applying Settings. Enabled: {}, Pitch: {}", 
          mSettings.mEnabled.get(), mSettings.mPitch.get());

    if (mSettings.mEnabled.get()) {
      float pitchRad = glm::radians(static_cast<float>(mSettings.mPitch.get()));
      VistaAxisAndAngle axisAngle(VistaVector3D(1, 0, 0), pitchRad);
      VistaQuaternion   orientation(axisAngle);

      mPlatformNode->SetRotation(orientation);
      logger().info("ViewOffset: Rotation set to {} degrees.", mSettings.mPitch.get());
    } else {
      mPlatformNode->SetRotation(VistaQuaternion(0, 0, 0, 1));
      logger().info("ViewOffset: Rotation reset to 0.");
    }
  }
}

void ViewOffset::update() {
    // FORCE UPDATE: Falls das Tracking die Rotation überschreibt, 
    // setzen wir sie hier jeden Frame neu.
    if (mPlatformNode && mSettings.mEnabled.get()) {
        float pitchRad = glm::radians(static_cast<float>(mSettings.mPitch.get()));
        VistaAxisAndAngle axisAngle(VistaVector3D(1, 0, 0), pitchRad);
        VistaQuaternion   orientation(axisAngle);
        
        // Wir holen die aktuelle Rotation und vergleichen (Optional, spart Performance)
        // Aber für den Test "hämmern" wir den Wert rein.
        mPlatformNode->SetRotation(orientation);
    }
}

} // namespace csp::vraccessibility