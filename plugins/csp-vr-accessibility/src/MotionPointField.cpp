////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR)
// SPDX-License-Identifier: MIT

#include "MotionPointField.hpp"

#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-utils/convert.hpp"
#include "logger.hpp"

#include <VistaKernel/DisplayManager/VistaDisplayManager.h>
#include <VistaKernel/DisplayManager/VistaDisplaySystem.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/InteractionManager/VistaUserPlatform.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <random>

    namespace csp::vraccessibility {

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // Shaders
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  const char* MotionPointField::VERT_SHADER = R"(
#version 330

layout(location = 0) in vec3 iPosition;

uniform mat4 uMatModelView;
uniform mat4 uMatProjection;
uniform float uPointSize;

void main() {
    // Die Punkte sind bereits im Kameraraum (lokal relativ zum Kopf),
    // daher brauchen wir hier theoretisch keine Model-Matrix, wenn der Node
    // an der Kamera hängt. Wir nutzen die Standard-Matrizen zur Sicherheit.
    vec4 viewPos = uMatModelView * vec4(iPosition, 1.0);
    gl_Position  = uMatProjection * viewPos;
    gl_PointSize = uPointSize;
}
)";

  const char* MotionPointField::FRAG_SHADER = R"(
#version 330

uniform vec4 uColor;
out vec4 oColor;

void main() {
    // Macht die Punkte rund
    float d = length(gl_PointCoord - vec2(0.5));
    if (d > 0.5) {
        discard;
    }
    // Optional: Weicher Rand (Fade out)
    float alpha = 1.0 - smoothstep(0.4, 0.5, d);
    oColor = vec4(uColor.rgb, uColor.a * alpha);
}
)";

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  MotionPointField::MotionPointField(
      std::shared_ptr<cs::core::SolarSystem> solarSystem, Plugin::Settings::MotionPoints & settings)
      : mSolarSystem(std::move(solarSystem))
      , mSettings(settings) {

    // 1. Initialisierung der Punkte
    generatePoints();

    // 2. OpenGL Buffer Setup
    mVBO.Bind(GL_ARRAY_BUFFER);
    mVBO.BufferData(mPoints.size() * sizeof(glm::vec3), mPoints.data(), GL_DYNAMIC_DRAW);
    mVBO.Release();

    mVAO.EnableAttributeArray(0);
    mVAO.SpecifyAttributeArrayFloat(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0, &mVBO);

    // 3. Shader Setup
    mShader.InitVertexShaderFromString(VERT_SHADER);
    mShader.InitFragmentShaderFromString(FRAG_SHADER);
    mShader.Link();

    mUniforms.modelView  = mShader.GetUniformLocation("uMatModelView");
    mUniforms.projection = mShader.GetUniformLocation("uMatProjection");
    mUniforms.color      = mShader.GetUniformLocation("uColor");
    mUniforms.pointSize  = mShader.GetUniformLocation("uPointSize");

    // 4. Scene Graph Setup
    // WICHTIG: Wir hängen uns an die Platform (Kamera), damit die Punkte immer "bei uns" sind.
    VistaSceneGraph* sg       = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
    auto*            platform = GetVistaSystem()
                         ->GetPlatformFor(GetVistaSystem()->GetDisplayManager()->GetDisplaySystem())
                         ->GetPlatformNode();

    mRoot.reset(sg->NewTransformNode(platform));
    mGLNode.reset(sg->NewOpenGLNode(mRoot.get(), this));

    // Transparentes Rendering aktivieren (für Alpha Blending im Shader)
    VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
        mGLNode.get(), static_cast<int>(cs::utils::DrawOrder::eTransparentItems));
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  MotionPointField::~MotionPointField() {
    if (mRoot && mGLNode) {
      mRoot->DisconnectChild(mGLNode.get());
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void MotionPointField::generatePoints() {
    std::mt19937                          rng(1337);
    std::uniform_real_distribution<float> dist(-1.0F, 1.0F);

    mPoints.clear();
    int count = mSettings.mCount.get();
    mPoints.reserve(count);
    float radius = mSettings.mRadius.get();

    while (mPoints.size() < count) {
      glm::vec3 p(dist(rng), dist(rng), dist(rng));
      // Wir füllen die Kugel gleichmäßig
      if (glm::length(p) <= 1.0F && glm::length(p) > 0.01F) {
        mPoints.push_back(p * radius);
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void MotionPointField::updatePoints() {
    // -----------------------------------------------------------------------
    // MAGIE PASSIERT HIER: Counter-Movement Logic
    // -----------------------------------------------------------------------

    // Wir holen uns die Position und Rotation des Observers im Sonnensystem.
    // Das ist präziser als die Vista-Plattform, da es die "Flugbewegung" beinhaltet.
    auto       observer   = mSolarSystem->getObserver();
    glm::dvec3 currentPos = observer.getPosition();
    glm::dquat currentRot = observer.getRotation();

    // Wir brauchen 'static', um den Zustand vom letzten Frame zu kennen.
    // Beim ersten Frame initialisieren wir es einfach mit den aktuellen Werten.
    static glm::dvec3 lastPos  = currentPos;
    static glm::dquat lastRot  = currentRot;
    static bool       firstRun = true;

    if (firstRun) {
      lastPos  = currentPos;
      lastRot  = currentRot;
      firstRun = false;
      return;
    }

    // 1. Wie sehr hat sich der Observer bewegt? (Delta in Welt-Koordinaten)
    glm::dvec3 deltaPos = currentPos - lastPos;

    // 2. Wir müssen dieses Welt-Delta in das LOKALE System der Partikelwolke drehen.
    // Da die Wolke an der Kamera hängt, dreht sie sich mit der Kamera mit.
    // Um das auszugleichen, rotieren wir das Delta mit der INVERSEN Rotation des Observers.
    glm::vec3 localDelta = glm::vec3(glm::inverse(currentRot) * deltaPos);

    // 3. Wie sehr hat sich der Observer gedreht?
    // Wir brauchen die relative Rotation zwischen Frame A und B.
    // Rotation_Neu = DeltaRot * Rotation_Alt  =>  DeltaRot = Rotation_Neu * Inverse(Rotation_Alt)
    glm::quat deltaRot        = glm::quat(currentRot * glm::inverse(lastRot));
    glm::quat inverseDeltaRot = glm::inverse(deltaRot);

    // Thresholds um unnötige Updates zu vermeiden
    if (glm::length(localDelta) < 0.0001f && glm::angle(deltaRot) < 0.0001f) {
      return;
    }

    float radius   = mSettings.mRadius.get();
    float radiusSq = radius * radius;

    for (auto& p : mPoints) {
      // SCHRITT A: Translation ausgleichen (Gegenbewegung)
      // Wenn ich vorwärts fliege, müssen die Punkte rückwärts an mir vorbei (minus localDelta).
      p -= localDelta;

      // SCHRITT B: Rotation ausgleichen
      // Wenn ich den Kopf nach rechts drehe, dreht sich mein lokales System nach rechts.
      // Der Punkt, der vorher "vor mir" war, ist jetzt "links" im lokalen System.
      // Das entspricht einer Rotation des Punktes um die Inverse der Kopf-Rotation.
      p = inverseDeltaRot * p;

      // SCHRITT C: Wrap-Around (Unendlicher Nebel)
      // Wenn ein Punkt die Sphäre verlässt, spiegeln wir ihn auf die andere Seite.
      // Das ist besser als neu erzeugen, da es einen kontinuierlichen Fluss erhält.
      if (glm::dot(p, p) > radiusSq) {
        // Einfachste Methode: Punktspiegelung am Ursprung.
        // Ein Punkt der vorne rausfliegt, kommt hinten wieder rein.
        p = -p;

        // Optional: Ein bisschen Zufall hinzufügen, damit Muster nicht auffallen
        // p += glm::vec3(randF(), randF(), randF()) * 0.1f;
      }
    }

    // Update Buffer
    mVBO.Bind(GL_ARRAY_BUFFER);
    // BufferSubData ist schneller als BufferData für Updates
    mVBO.BufferSubData(0, mPoints.size() * sizeof(glm::vec3), mPoints.data());
    mVBO.Release();

    // Werte für nächsten Frame speichern
    lastPos = currentPos;
    lastRot = currentRot;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  bool MotionPointField::Do() {
    if (!mSettings.mEnabled.get()) {
      return true;
    }

    // Update Logik aufrufen
    updatePoints();

    // WICHTIG: Shader binden!
    mShader.Bind();

    // Matrizen holen. Wir nutzen den DisplayManager von Vista.
    // Da unser Node an der Plattform hängt, ist die ModelView Matrix hier
    // bereits die Transformation der Kamera. Die Punkte sind lokal (0,0,0 ist der Kopf).
    // Das passt perfekt.
    GLfloat glMat[16];

    // Projection Matrix
    glGetFloatv(GL_PROJECTION_MATRIX, glMat);
    glUniformMatrix4fv(mUniforms.projection, 1, GL_FALSE, glMat);

    // ModelView Matrix
    glGetFloatv(GL_MODELVIEW_MATRIX, glMat);
    glUniformMatrix4fv(mUniforms.modelView, 1, GL_FALSE, glMat);

    // Uniforms setzen
    glUniform4fv(
        mUniforms.color, 1, glm::value_ptr(Plugin::GetColorFromHexString(mSettings.mColor.get())));
    glUniform1f(mUniforms.pointSize, mSettings.mSize.get());

    // State setup
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Depth Test ausschalten, damit Punkte auch "in" Objekten zu sehen sind (optional)
    // Oder einschalten, damit sie hinter Planeten verschwinden.
    glEnable(GL_DEPTH_TEST);

    // Shader-Attribute für Vertex Position nutzen
    mVAO.Bind();

    // Draw Call für modernes OpenGL
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(mPoints.size()));

    mVAO.Release();
    mShader.Release(); // Shader wieder freigeben

    glPopAttrib();

    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  bool MotionPointField::GetBoundingBox(VistaBoundingBox & bb) {
    // Damit Culling funktioniert, geben wir die Box der Wolke an
    float r = mSettings.mRadius.get();
    bb.SetBounds(&glm::vec3(-r)[0], &glm::vec3(r)[0]);
    return true;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::vraccessibility