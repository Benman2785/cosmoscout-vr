////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ViewOffset.hpp"
#include "../../../src/cs-utils/logger.hpp"

#include <VistaBase/VistaQuaternion.h>
#include <VistaBase/VistaVector3D.h>
#include <VistaKernel/GraphicsManager/VistaGraphicsManager.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaKernel/VistaSystem.h>
#include <glm/glm.hpp>

namespace csp::vraccessibility {

ViewOffset::ViewOffset(Plugin::Settings::ViewOffset const& settings)
    : mSettings(settings) {

  auto* sg = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();

  // Wir greifen uns direkt den Vater aller Knoten.
  // Hierauf wendet CosmoScout die Planeten-Rotation an.
  mPlatformNode = dynamic_cast<VistaTransformNode*>(sg->GetNode("Virtualplatform-Node"));

  if (!mPlatformNode) {
    mPlatformNode = dynamic_cast<VistaTransformNode*>(sg->GetNode("VirtualPlatform-Node"));
  }

  if (!mPlatformNode) {
    logger().error("ViewOffset: CRITICAL - 'Virtualplatform-Node' not found!");
  } else {
    logger().info(
        "ViewOffset: Hooked into 'Virtualplatform-Node' for frame-based rotation correction.");
    configure(mSettings);
  }
}

void ViewOffset::configure(Plugin::Settings::ViewOffset const& settings) {
  mSettings = settings;
  // In dieser Variante passiert die Magie hauptsächlich im update() Loop,
  // da wir jeden Frame neu eingreifen müssen.
}

void ViewOffset::update() {
  if (mPlatformNode && mSettings.mEnabled.get()) {

    // 1. Wir holen uns die Rotation, die CosmoScout/SolarSystem für diesen Frame berechnet hat.
    // Diese Rotation sorgt dafür, dass wir gerade auf dem Planeten stehen.
    VistaQuaternion currentRotation;
    mPlatformNode->GetRotation(currentRotation);

    // 2. Wir bauen unsere lokale Offset-Rotation (Nicken nach unten/oben).
    // Wir nutzen GLM für die Umrechnung.
    float pitchRad = glm::radians(static_cast<float>(mSettings.mPitch.get()));

    // Rotation um die lokale X-Achse (1, 0, 0).
    VistaAxisAndAngle axisAngle(VistaVector3D(1, 0, 0), pitchRad);
    VistaQuaternion   offsetRotation(axisAngle);

    // 3. Wir kombinieren die Rotationen.
    // WICHTIG: Die Reihenfolge der Multiplikation bestimmt, ob wir
    // um die Welt-Achsen oder die lokalen Achsen drehen.
    // currentRotation * offsetRotation = Drehung um die LOKALE Achse der Plattform.
    // Das ist genau das, was wir wollen (den "Kopf" neigen, egal wo auf dem Planeten wir sind).
    VistaQuaternion finalRotation = currentRotation * offsetRotation;

    // 4. Wir schreiben das Ergebnis zurück.
    // Da CosmoScout im nächsten Frame wieder die "currentRotation" (ohne Offset)
    // aus der Physik-Engine lädt, addiert sich hier nichts auf. Es gibt kein Spinning.
    mPlatformNode->SetRotation(finalRotation);
  }
}

} // namespace csp::vraccessibility