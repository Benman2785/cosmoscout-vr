////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#include "ObserverNavigationNode.hpp"

#include "../cs-core/Settings.hpp"
#include "../cs-core/SolarSystem.hpp"
#include "../cs-gui/GuiItem.hpp"
#include "../cs-scene/CelestialSurface.hpp"
#include "../cs-utils/convert.hpp"

#include <VistaAspects/VistaPropertyAwareable.h>
#include <glm/gtx/io.hpp>

////////////////////////////////////////////////////////////////////////////////////////////////////

ObserverNavigationNode::ObserverNavigationNode(cs::core::SolarSystem* pSolarSystem,
    cs::core::Settings* pSettings, VistaPropertyList const& oParams)
    : mSolarSystem(pSolarSystem)
    , mSettings(pSettings)
    , mTime(nullptr)
    , mTranslation(nullptr)
    , mRotation(nullptr)
    , mOffset(nullptr)
    , mRotationOffset(nullptr)
    , mPreventNavigationWhenHoveredGui(
          oParams.GetValueOrDefault<bool>("prevent_navigation_when_hovered_gui", true))
    , mFixedHorizon(oParams.GetValueOrDefault<bool>("fixed_horizon", true))
    , mCurveFlight(oParams.GetValueOrDefault<bool>("curve_flight", true))
    
    // Wir lesen nicht mehr aus oParams, sondern aus pSettings->mObserver
    , mLimitHeightMin(pSettings->mObserver.mLimitHeightMin)
    , mLimitHeightMax(pSettings->mObserver.mLimitHeightMax)

    , mMaxAngularSpeed(oParams.GetValueOrDefault<double>("max_angular_speed", glm::pi<double>()))
    , mMaxLinearSpeed(oParams.GetValueOrDefault<VistaVector3D>(
          "max_linear_speed", VistaVector3D(1.0, 1.0, 1.0)))
    , mAngularDirection(1.0, 0.0, 0.0, 0.0)
    , mAngularSpeed(0.0)
    , mAngularDeceleration(oParams.GetValueOrDefault<double>("angular_deceleration", 0.1))
    , mLinearDirection(0.0)
    , mLinearSpeed(0.0)
    , mLinearDeceleration(oParams.GetValueOrDefault<double>("linear_deceleration", 0.1))
    , mLastTime(-1.0) {

  mLinearSpeed.mDirection = cs::utils::AnimationDirection::eLinear;

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): deleted in IVdfnNode::~IVdfnNode()
  RegisterInPortPrototype("time", new TVdfnPortTypeCompare<TVdfnPort<double>>);

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): deleted in IVdfnNode::~IVdfnNode()
  RegisterInPortPrototype("translation", new TVdfnPortTypeCompare<TVdfnPort<VistaVector3D>>);

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): deleted in IVdfnNode::~IVdfnNode()
  RegisterInPortPrototype("rotation", new TVdfnPortTypeCompare<TVdfnPort<VistaQuaternion>>);

  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): deleted in IVdfnNode::~IVdfnNode()
  RegisterInPortPrototype("offset", new TVdfnPortTypeCompare<TVdfnPort<VistaVector3D>>);

  RegisterInPortPrototype("rotation_offset", new TVdfnPortTypeCompare<TVdfnPort<VistaQuaternion>>);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool ObserverNavigationNode::PrepareEvaluationRun() {
  mTime           = dynamic_cast<TVdfnPort<double>*>(GetInPort("time"));
  mTranslation    = dynamic_cast<TVdfnPort<VistaVector3D>*>(GetInPort("translation"));
  mRotation       = dynamic_cast<TVdfnPort<VistaQuaternion>*>(GetInPort("rotation"));
  mOffset         = dynamic_cast<TVdfnPort<VistaVector3D>*>(GetInPort("offset"));
  mRotationOffset = dynamic_cast<TVdfnPort<VistaQuaternion>*>(GetInPort("rotation_offset"));

  return GetIsValid();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool ObserverNavigationNode::GetIsValid() const {
  return ((mTranslation || mRotation || mRotationOffset || mOffset) && mTime);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool ObserverNavigationNode::DoEvalNode() {
  double dTtime = mTime->GetValue();

  // skip first update since we do not know the delta time
  if (mLastTime < 0.0) {
    mLastTime = dTtime;
    return true;
  }

  // ensures that we don't loose time on many very small updates
  double dDeltaTime = dTtime - mLastTime;
  if (dDeltaTime < Vista::Epsilon) {
    return true;
  }

  mLastTime = dTtime;

  if (mTranslation) {
    auto tmp              = mTranslation->GetValue();
    auto vLinearDirection = glm::dvec3(tmp[0], tmp[1], tmp[2]);

    // update velocities
    if (glm::length(vLinearDirection) > 0.0) {
      mLinearDirection = vLinearDirection;

      if (mLinearSpeed.mEndValue == 0.0) {
        mLinearSpeed.mStartValue = 1.0;
        mLinearSpeed.mEndValue   = 1.0;
      }
    } else {
      if (mLinearSpeed.mEndValue == 1.0) {
        mLinearSpeed.mStartValue = mLinearSpeed.get(dTtime);
        mLinearSpeed.mEndValue   = 0.0;
        mLinearSpeed.mStartTime  = dTtime;
        mLinearSpeed.mEndTime    = dTtime + mLinearDeceleration;
      }
    }
  }

  if (mRotation) {
    auto          tmp       = mRotation->GetValue().GetNormalized().GetAxisAndAngle();
    VistaVector3D axis      = tmp.m_v3Axis;
    double        angle     = tmp.m_fAngle;
    auto          qRotation = glm::angleAxis(angle, glm::dvec3(axis[0], axis[1], axis[2]));

    // update velocities
    if (angle > 0.0) {
      mAngularDirection = qRotation;

      if (mAngularSpeed.mEndValue == 0.0) {
        mAngularSpeed.mStartValue = 1.0;
        mAngularSpeed.mEndValue   = 1.0;
      }
    } else {
      if (mAngularSpeed.mEndValue == 1.0) {
        mAngularSpeed.mStartValue = mAngularSpeed.get(dTtime);
        mAngularSpeed.mEndValue   = 0.0;
        mAngularSpeed.mStartTime  = dTtime;
        mAngularSpeed.mEndTime    = dTtime + mAngularDeceleration;
      }
    }
  }

  glm::dvec3 vOffset(0.0);

  if (mOffset) {
    auto tmp = mOffset->GetValue();
    vOffset  = glm::dvec3(tmp[0], tmp[1], tmp[2]);
  }

  auto vTranslation = mLinearDirection;

  vTranslation.x *= mMaxLinearSpeed[0];
  vTranslation.y *= mMaxLinearSpeed[1];
  vTranslation.z *= mMaxLinearSpeed[2];
  vTranslation *= mLinearSpeed.get(dTtime);

  vTranslation *= dDeltaTime;
  vTranslation += vOffset;

  auto&      oObs = mSolarSystem->getObserver();
  glm::dvec3 newPosition;

  newPosition = oObs.getPosition() + oObs.getRotation() * vTranslation * oObs.getScale();
  oObs.setPosition(newPosition);

  // HEIGHT LIMITING
  if (mSolarSystem->pActiveObject.get() && (mLimitHeightMin || mLimitHeightMax)) {

    auto planet = mSolarSystem->pActiveObject.get();

    // Planet radii *must* be scaled
    glm::dvec3 radii = planet->getRadii() * planet->getScale();

    // Planet center in observer/world coordinates
    glm::dvec3 planetPos = planet->getPosition();

    // Observer position in world coords
    glm::dvec3 obsPos = oObs.getPosition();

    // Vector from planet center to observer
    glm::dvec3 vObserver = obsPos - planetPos;

    // Compute point on the surface directly beneath observer
    glm::dvec3 surfacePos = cs::utils::convert::scaleToGeodeticSurface(vObserver, radii);

    // convert surfacePos back to world coordinates
    surfacePos += planetPos;

    glm::dvec3 surfacePosEllipsoid = surfacePos;

    // add surface height data to surfacePos
    if (planet->getSurface()) {
      auto lngLatHeight = cs::utils::convert::cartesianToLngLatHeight(vObserver, radii);
      surfacePos +=
          glm::normalize(surfacePos) * (planet->getSurface()->getHeight(lngLatHeight.xy()) *
                                           mSettings->mGraphics.pHeightScale.get());
    }

    // local planetary surface radius
    double surfaceRadius          = glm::length(surfacePos - planetPos);
    double surfaceRadiusEllipsoid = glm::length(surfacePosEllipsoid - planetPos);

    // current radius
    double rObserver = glm::length(vObserver);

    // current height above surface
    double height          = rObserver - surfaceRadius;
    double heightEllipsoid = rObserver - surfaceRadiusEllipsoid;

    double minHeight = 2000.0 * planet->getScale();
    double maxHeight = 500000.0 * planet->getScale();

    bool   changed      = false;
    double targetRadius = rObserver;

    if (mLimitHeightMin && height < minHeight) {
      targetRadius = surfaceRadius + minHeight;
      // logger().info("Min Height={}", targetRadius);
      changed = true;
    }

    if (mLimitHeightMax && heightEllipsoid > maxHeight) {
      targetRadius = surfaceRadiusEllipsoid + maxHeight;
      // logger().info("Max Height={}", targetRadius);
      changed = true;
    }

    if (changed) {
      glm::dvec3 dir    = glm::normalize(vObserver);
      glm::dvec3 newPos = planetPos + dir * targetRadius;

      oObs.setPosition(newPos);
      newPosition = newPos;
    }
  }

  glm::dquat qRotationOffset(1, 0, 0, 0);

  if (mRotationOffset) {
    auto          tmp   = mRotationOffset->GetValue().GetNormalized().GetAxisAndAngle();
    VistaVector3D axis  = tmp.m_v3Axis;
    double        angle = tmp.m_fAngle;
    qRotationOffset     = glm::angleAxis(angle, glm::dvec3(axis[0], axis[1], axis[2]));
  }

  auto       qRotation     = mAngularDirection;
  glm::dvec3 vRotationAxis = glm::axis(qRotation);
  double     dRotationAngle =
      glm::angle(qRotation) * dDeltaTime * mMaxAngularSpeed * mAngularSpeed.get(dTtime);

  auto newRotation = glm::normalize(
      oObs.getRotation() * glm::angleAxis(dRotationAngle, vRotationAxis) * qRotationOffset);

  // --- STABILER CURVE-FLIGHT: sanfte Neigung an Planetenkrümmung ---
  if (mCurveFlight && mSolarSystem->pActiveObject.get()) {

    // Planetendaten
    auto       planet       = mSolarSystem->pActiveObject.get();
    glm::dvec3 planetCenter = planet->getPosition();

    // newPosition ist schon berechnet (Position wurde angewendet), benutze diese Position
    // um das lokale Up zu bestimmen (Vektor vom Planetenmittelpunkt zum Beobachter).
    glm::dvec3 newUp = glm::normalize(newPosition - planetCenter);

    // current forward aus der gerade berechneten Rotation
    glm::dvec3 forward = newRotation * glm::dvec3(0.0, 0.0, 1.0);

    // Wie sehr zeigt forward in radialer (newUp-)Richtung?
    // (positive Werte: forward zeigt weg vom Planeten; negative: in Richtung Planet)
    double radial = glm::dot(forward, newUp);

    // Wenn forward bereits tangential ist, nichts tun
    if (std::abs(radial) > 1e-6) {

      // Tangentialprojektionsvektor (forward in Tangentialebene):
      glm::dvec3 projected = forward - radial * newUp;

      // numerische Absicherung: falls projected zu klein, skip
      if (glm::length2(projected) > 1e-12) {
        projected = glm::normalize(projected);

        // kleinster Rotationsaxis/angle von forward -> projected
        glm::dvec3 axis = glm::cross(forward, projected);

        // L�nge von axis ist sin(angle). Clamp um numerische Probleme zu vermeiden.
        double sinAngle = glm::clamp(glm::length(axis), 0.0, 1.0);
        double angle    = std::asin(sinAngle);

        // falls angle numerisch sehr klein -> skip
        if (angle > 1e-9) {
          axis = glm::normalize(axis);

          // --- WICHTIG: weiche Korrektur (tweakbar)
          // 0.0 = keine Korrektur, 1.0 = vollst�ndige sofortige Korrektur (w�rde flackern)
          // Empfohlener Standard: 0.05 .. 0.2 (je nach gew�nschtem "naklapp"-Gef�hl)
          const double correctionFactor = 0.08; // <- anpassen wenn du aggressiver/leichter willst

          double appliedAngle = angle * correctionFactor;

          // Wenn appliedAngle sehr klein ist, skip (vermeidet tiny quaternion jitter)
          if (appliedAngle > 1e-7) {
            glm::dquat correction = glm::angleAxis(appliedAngle, axis);
            // wende Korrektur vor der bestehenden Rotation an (lokale Rotation)
            newRotation = glm::normalize(correction * newRotation);
          }
        }
      }
    }
  }

  // If mFixedHorizon is set, we rotate the observer so that the horizon of the active object is
  // always leveled. For now, this breaks if we are in outer space or looking straight up or down.
  // But it can be very useful in cases were we know that the user is always close to a planet.
  if (mFixedHorizon && mSolarSystem->pActiveObject.get()) {
    auto radii      = mSolarSystem->pActiveObject.get()->getRadii();
    auto surfacePos = cs::utils::convert::scaleToGeodeticSurface(newPosition, radii);

    // can lead to normal pointing to the wrong direction
    // auto distance   = newPosition - surfacePos;
    // glm::dvec3 normal = glm::normalize(distance);

    auto       planetCenter = mSolarSystem->pActiveObject.get()->getPosition();
    glm::dvec3 normal       = glm::normalize(newPosition - planetCenter);

    glm::dvec3 z = (newRotation * glm::dvec4(0, 0, 1, 0)).xyz();
    glm::dvec3 x = -glm::cross(z, normal);
    glm::dvec3 y = -glm::cross(x, z);

    x = glm::normalize(x);
    y = glm::normalize(y);
    z = glm::normalize(z);

    newRotation = glm::toQuat(glm::dmat3(x, y, z));
  }

  oObs.setRotation(newRotation);

  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

ObserverNavigationNodeCreate::ObserverNavigationNodeCreate(
    cs::core::SolarSystem* pSolarSystem, cs::core::Settings* pSettings)
    : mSolarSystem(pSolarSystem)
    , mSettings(pSettings) {
}

////////////////////////////////////////////////////////////////////////////////////////////////////

IVdfnNode* ObserverNavigationNodeCreate::CreateNode(const VistaPropertyList& oParams) const {
  return new ObserverNavigationNode(mSolarSystem, mSettings, oParams.GetSubListConstRef("param"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
