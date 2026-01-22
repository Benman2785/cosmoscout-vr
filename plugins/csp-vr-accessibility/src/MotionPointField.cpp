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
   oColor = vec4(1.0, 0.0, 1.0, 1.0); // Immer Magenta ausgeben, keine Discard-Logik
}
)";



//void main() {
//    // Macht die Punkte rund
//    float d = length(gl_PointCoord - vec2(0.5));
//    if (d > 0.5) {
//        discard;
//    }
//    // Optional: Weicher Rand (Fade out)
//    float alpha = 1.0 - smoothstep(0.4, 0.5, d);
//    oColor = vec4(uColor.rgb, uColor.a * alpha);
//}

////////////////////////////////////////////////////////////////////////////////////////////////////

MotionPointField::MotionPointField(
    std::shared_ptr<cs::core::SolarSystem> solarSystem, Plugin::Settings::MotionPoints& settings)
    : mSolarSystem(std::move(solarSystem))
    , mSettings(settings) {

    mRng = std::mt19937(1337);
    mDist = std::uniform_real_distribution<float>(-1.0f, 1.0f);

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
  // Check Link Status (Quick & Dirty Debugging)
  GLint success;
  glGetProgramiv(mShader.GetProgram(), GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(mShader.GetProgram(), 512, NULL, infoLog);
    logger().error("SHADER LINK ERROR: {}", infoLog);
  }

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
    if (mPoints.size() == 1) {
      logger().info("First Point at: {}, {}, {}", p.x, p.y, p.z);
    }
  }

  logger().info("Generated {} Points", mPoints.size());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void MotionPointField::updatePoints() {
    auto       observer   = mSolarSystem->getObserver();
    glm::dvec3 currentPos = observer.getPosition();
    glm::dquat currentRot = observer.getRotation();

    static glm::dvec3 lastPos  = currentPos;
    static glm::dquat lastRot  = currentRot;
    static bool       firstRun = true;

    if (firstRun) {
        lastPos  = currentPos;
        lastRot  = currentRot;
        firstRun = false;
        return;
    }

    // 1. Delta berechnen
    glm::dvec3 deltaPos = currentPos - lastPos;

    // 2. In lokales System rotieren
    glm::vec3 localDelta = glm::vec3(glm::inverse(currentRot) * deltaPos);
    
    // Rotations-Delta berechnen
    glm::quat deltaRot        = glm::quat(currentRot * glm::inverse(lastRot));
    glm::quat inverseDeltaRot = glm::inverse(deltaRot);

    // --- FIX: Teleport / Warp Detection ---
    float radius = mSettings.mRadius.get();
    
    // Wenn wir uns in einem Frame weiter bewegt haben als der Radius der Wolke,
    // macht das Verschieben keinen Sinn mehr (Punkte wären alle weg).
    // Das passiert beim "Fly-To" oder beim Initialisieren.
    // -> Wir tun so, als wären wir in eine neue Wolke geflogen.
    if (glm::length(localDelta) > radius) {
        // Resetten: Punkte neu verteilen um 0,0,0
        generatePoints(); 
        
        // WICHTIG: Referenzwerte updaten, damit im nächsten Frame das Delta klein ist
        lastPos = currentPos;
        lastRot = currentRot;
        return; 
    }

    float radiusSq = radius * radius;

    for (auto& p : mPoints) {
        // A: Bewegung
        p -= localDelta;

        // B: Rotation
        p = inverseDeltaRot * p;

        // C: Wrap-Around (Verbesserte Logik)
        // Prüfen ob Punkt außerhalb ist
        float distSq = glm::dot(p, p);
        if (distSq > radiusSq) {
            // Statt p = -p (was bei großen Distanzen versagt),
            // setzen wir den Punkt hart auf den Rand der Kugel auf der gegenüberliegenden Seite.
            
            // 1. Richtung zum Punkt bestimmen und normalisieren
            // (Schutz vor Division durch Null, falls p=(0,0,0) wäre, was hier unmöglich ist aber sicher ist sicher)
            glm::vec3 dir = glm::normalize(p); 
            
            // 2. Auf die gegenüberliegende Seite setzen (-dir) am Rand (* radius)
            // Wir ziehen ein kleines bisschen ab (0.95), damit er sicher "drinnen" ist
            // und nicht sofort wieder getriggert wird.
            p = -dir * radius * 0.95f; 
            
            // Optional: Zufällige Position auf der "Eintrittsfläche" wäre noch schöner,
            // aber das hier reicht für den "Tunnel-Effekt".
        }
    }

    // Buffer Update
    mVBO.Bind(GL_ARRAY_BUFFER);
    mVBO.BufferSubData(0, mPoints.size() * sizeof(glm::vec3), mPoints.data());
    mVBO.Release();

    lastPos = currentPos;
    lastRot = currentRot;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotionPointField::Do() {
  if (!mSettings.mEnabled.get()) {
    return true;
  }

  updatePoints();

  // Shader binden
  mShader.Bind();

  // Matrizen
  GLfloat glMat[16];
  glGetFloatv(GL_PROJECTION_MATRIX, glMat);
  glUniformMatrix4fv(mUniforms.projection, 1, GL_FALSE, glMat);
  glGetFloatv(GL_MODELVIEW_MATRIX, glMat);
  glUniformMatrix4fv(mUniforms.modelView, 1, GL_FALSE, glMat);

  // --- FIX 1: Farbe temporär hardcoden ---
  // Wir ignorieren kurz die Settings und machen sie Knallrot & voll deckend,
  // um sicherzugehen, dass es kein Problem mit dem Hex-String ist.
  glm::vec4 debugColor(1.0f, 0.0f, 0.0f, 1.0f);
  glUniform4fv(mUniforms.color, 1, glm::value_ptr(debugColor));

  // --- FIX 2: Größe erzwingen ---
  // Wir setzen eine riesige Größe (20 Pixel), damit wir sie nicht übersehen können.
  glUniform1f(mUniforms.pointSize, 20.0f);

  glPushAttrib(GL_ALL_ATTRIB_BITS); // Sichert ALLES (sicherer beim Debuggen)

  // --- FIX 3: Point Size aktivieren! ---
  // Ohne das hier wird gl_PointSize im Shader oft ignoriert.
  glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
  // Falls das obere nicht reicht (Treiber-abhängig):
  glEnable(GL_PROGRAM_POINT_SIZE);

  // --- FIX 4: Depth Test AUS ---
  // Wir wollen, dass die Punkte IMMER über alles andere gemalt werden (wie ein UI).
  // Wenn sie hinter Planeten verschwinden sollen, schalten wir das später wieder an.
  glDisable(GL_DEPTH_TEST);

  // Blending an, aber Depth Write aus (üblich für Partikel)
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  mVAO.Bind();
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(mPoints.size()));
  mVAO.Release();

  mShader.Release();
  glPopAttrib();

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool MotionPointField::GetBoundingBox(VistaBoundingBox& bb) {
  // Damit Culling funktioniert, geben wir die Box der Wolke an
  float r = mSettings.mRadius.get();
  bb.SetBounds(&glm::vec3(-r)[0], &glm::vec3(r)[0]);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::vraccessibility