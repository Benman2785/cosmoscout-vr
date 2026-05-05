////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#include "Plugin.hpp"
#include "logger.hpp"

#include "../../../src/cs-core/SolarSystem.hpp"
#include "../../../src/cs-core/TimeControl.hpp"
#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/Settings.hpp"
#include "../../../src/cs-core/InputManager.hpp"
#include "../../../src/cs-gui/GuiItem.hpp"
#include "../../../src/cs-gui/WorldSpaceGuiArea.hpp"
#include "../../../src/cs-scene/CelestialAnchor.hpp"

#include <VistaKernel/DisplayManager/VistaDisplayManager.h>
#include <VistaKernel/DisplayManager/VistaDisplaySystem.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>
#include <VistaKernel/GraphicsManager/VistaSceneGraph.h>
#include <VistaKernel/GraphicsManager/VistaTransformNode.h>
#include <VistaKernel/InteractionManager/VistaKeyboardSystemControl.h>
#include <VistaKernel/InteractionManager/VistaUserPlatform.h>
#include <VistaKernel/VistaSystem.h>
#include <VistaKernelOpenSGExt/VistaOpenSGMaterialTools.h>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase *create()
{
  return new csp::messe::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase *pluginBase)
{
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::messe
{

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void from_json(nlohmann::json const &j, Plugin::Settings &o)
  {
    cs::core::Settings::deserialize(j, "showHudInIdle", o.mShowHudInIdle);
    cs::core::Settings::deserialize(j, "autoStartAfterWarmup", o.mAutoStartAfterWarmup);
    cs::core::Settings::deserialize(j, "warmupSeconds", o.mWarmupSeconds);
    cs::core::Settings::deserialize(j, "addThresholdSeconds", o.mAddThresholdSeconds);
    cs::core::Settings::deserialize(j, "hudGifPath", o.mHudGifPath);
    cs::core::Settings::deserialize(j, "hudWidth", o.mHudWidth);
    cs::core::Settings::deserialize(j, "hudHeight", o.mHudHeight);
    cs::core::Settings::deserialize(j, "hudDistanceMeters", o.mHudDistanceMeters);
    cs::core::Settings::deserialize(j, "hudVerticalOffsetMeters", o.mHudVerticalOffsetMeters);
    cs::core::Settings::deserialize(j, "hudScale", o.mHudScale);
    cs::core::Settings::deserialize(j, "ignoreDepth", o.mIgnoreDepth);
  }

  void to_json(nlohmann::json &j, Plugin::Settings const &o)
  {
    cs::core::Settings::serialize(j, "showHudInIdle", o.mShowHudInIdle);
    cs::core::Settings::serialize(j, "autoStartAfterWarmup", o.mAutoStartAfterWarmup);
    cs::core::Settings::serialize(j, "warmupSeconds", o.mWarmupSeconds);
    cs::core::Settings::serialize(j, "addThresholdSeconds", o.mAddThresholdSeconds);
    cs::core::Settings::serialize(j, "hudGifPath", o.mHudGifPath);
    cs::core::Settings::serialize(j, "hudWidth", o.mHudWidth);
    cs::core::Settings::serialize(j, "hudHeight", o.mHudHeight);
    cs::core::Settings::serialize(j, "hudDistanceMeters", o.mHudDistanceMeters);
    cs::core::Settings::serialize(j, "hudVerticalOffsetMeters", o.mHudVerticalOffsetMeters);
    cs::core::Settings::serialize(j, "hudScale", o.mHudScale);
    cs::core::Settings::serialize(j, "ignoreDepth", o.mIgnoreDepth);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  namespace
  {
    std::string toFileUrl(std::string path)
    {
      std::replace(path.begin(), path.end(), '\\', '/');
      if (path.rfind("file://", 0) == 0)
      {
        return path;
      }
      return "file://" + path;
    }
  } // namespace

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::init()
  {
    logger().info("Loading plugin...");

    mOnLoadConnection = mAllSettings->onLoad().connect([this]()
                                                       { onLoad(); });
    mOnSaveConnection = mAllSettings->onSave().connect([this]()
                                                       { onSave(); });

    onLoad();
    createHud();
    bindModeHotkey();
    pushOverlayState(true);

    logger().info("Loading done.");
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::deInit()
  {
    logger().info("Unloading plugin...");

    onSave();
    unbindTimeHotkeys();
    unbindModeHotkey();
    destroyHud();

    mAllSettings->onLoad().disconnect(mOnLoadConnection);
    mAllSettings->onSave().disconnect(mOnSaveConnection);

    logger().info("Unloading done.");
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::update()
  {
    auto const now = Clock::now();

    if (mPhase == Phase::eWarmup && now >= mWarmupEndsAt)
    {
      if (mPluginSettings.mAutoStartAfterWarmup && mPreparedMinutes > 0)
      {
        startRun(mPreparedMinutes);
      }
      else
      {
        mPhase = Phase::eArmed;
        mGuiManager->showNotification(
            "Messe-Modus", "1 / 2 / 3 / 5",
            "timer");
        pushOverlayState(true);
      }
    }

    if (mPhase == Phase::eRunning && now >= mRunEndsAt)
    {
      finishRun();
    }

    pushOverlayState(false);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::onLoad()
  {
    auto pluginIt = mAllSettings->mPlugins.find("csp-messe");
    if (pluginIt != mAllSettings->mPlugins.end())
    {
      from_json(pluginIt->second, mPluginSettings);
    }

    pushOverlayState(true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::onSave()
  {
    mAllSettings->mPlugins["csp-messe"] = mPluginSettings;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::bindModeHotkey()
  {
    GetVistaSystem()->GetKeyboardSystemControl()->BindAction('m', [this]()
                                                             { enterMesseMode(); });
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::unbindModeHotkey()
  {
    GetVistaSystem()->GetKeyboardSystemControl()->UnbindAction('m');
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::bindTimeHotkeys()
  {
    if (mTimeHotkeysBound)
    {
      return;
    }

    auto *keyboard = GetVistaSystem()->GetKeyboardSystemControl();

    keyboard->BindAction('1', [this]()
                         { handleTimeHotkey(1); });
    keyboard->BindAction('2', [this]()
                         { handleTimeHotkey(2); });
    keyboard->BindAction('3', [this]()
                         { handleTimeHotkey(3); });
    keyboard->BindAction('5', [this]()
                         { handleTimeHotkey(5); });

    mTimeHotkeysBound = true;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::unbindTimeHotkeys()
  {
    if (!mTimeHotkeysBound)
    {
      return;
    }

    auto *keyboard = GetVistaSystem()->GetKeyboardSystemControl();

    keyboard->UnbindAction('1');
    keyboard->UnbindAction('2');
    keyboard->UnbindAction('3');
    keyboard->UnbindAction('5');

    mTimeHotkeysBound = false;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::enterMesseMode()
  {
    bindTimeHotkeys();
    resetSession(Phase::eAwaitWarmupSelection);

    pushOverlayState(true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::resetSession(Phase targetPhase)
  {
    mPhase = targetPhase;
    mPreparedMinutes = 0;
    mWarmupEndsAt = Clock::time_point{};
    mRunEndsAt = Clock::time_point{};

    if (targetPhase == Phase::eIdle || targetPhase == Phase::eFinished)
    {
      unbindTimeHotkeys();
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::handleTimeHotkey(int minutes)
  {
    switch (mPhase)
    {
    case Phase::eAwaitWarmupSelection:
      startWarmup(minutes);
      break;

    case Phase::eWarmup:
      mPreparedMinutes = minutes;

      pushOverlayState(true);
      break;

    case Phase::eArmed:
      startRun(minutes);
      break;

    case Phase::eRunning:
      addTime(minutes);
      break;

    case Phase::eIdle:
    case Phase::eFinished:
      break;
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::startWarmup(int minutes)
  {
    mPreparedMinutes = minutes;
    mPhase = Phase::eWarmup;
    mWarmupEndsAt = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                       std::chrono::duration<double>(mPluginSettings.mWarmupSeconds));

    pushOverlayState(true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::startRun(int minutes)
  {
    mPreparedMinutes = minutes;
    mPhase = Phase::eRunning;
    mRunEndsAt = Clock::now() + std::chrono::minutes(minutes);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::finishRun()
  {
    resetSession(Phase::eFinished);

    pushOverlayState(true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::addTime(int minutes)
  {
    auto const remaining = getRemainingRunSeconds(Clock::now());

    if (remaining >= mPluginSettings.mAddThresholdSeconds)
    {
      return;
    }

    mRunEndsAt += std::chrono::minutes(minutes);

    pushOverlayState(true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  double Plugin::getRemainingWarmupSeconds(Clock::time_point now) const
  {
    if (mPhase != Phase::eWarmup)
    {
      return 0.0;
    }

    return std::max(
        0.0, std::chrono::duration_cast<std::chrono::duration<double>>(mWarmupEndsAt - now).count());
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  double Plugin::getRemainingRunSeconds(Clock::time_point now) const
  {
    if (mPhase != Phase::eRunning)
    {
      return 0.0;
    }

    return std::max(
        0.0, std::chrono::duration_cast<std::chrono::duration<double>>(mRunEndsAt - now).count());
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  double Plugin::getApproxAltitude() const
  {
    auto const &observer = mSolarSystem->getObserver();
    auto object = mSolarSystem->getObjectByCenterName(observer.getCenterName());
    if (!object)
    {
      return 0.0;
    }

    glm::dvec3 const &radii = object->getRadii();
    double const meanRadius = (radii.x + radii.y + radii.z) / 3.0;
    double const distancetoCenter = glm::length(observer.getPosition());

    return std::max(0.0, distancetoCenter - meanRadius);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  std::string Plugin::formatDistance(double meters) const
  {
    if (meters >= 1000.0)
    {
      return fmt::format("{:.2f} km", meters / 1000.0);
    }
    return fmt::format("{:.0f} m", meters);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  std::string Plugin::formatSpeed(double metersPerSecond) const
  {
    if (metersPerSecond >= 1000.0)
    {
      return fmt::format("{:.2f} km/s", metersPerSecond / 1000.0);
    }
    return fmt::format("{:.0f} m/s", metersPerSecond);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  std::string Plugin::formatTime(double seconds) const
  {
    auto const totalSeconds = std::max(0, static_cast<int>(std::ceil(seconds)));
    auto const minutes = totalSeconds / 60;
    auto const restSeconds = totalSeconds % 60;

    return fmt::format("{:02d}:{:02d}", minutes, restSeconds);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  std::string Plugin::phaseLabel() const
  {
    switch (mPhase)
    {
    case Phase::eIdle:
      return "Bereit";
    case Phase::eAwaitWarmupSelection:
      return "Messe-Modus";
    case Phase::eWarmup:
      return "Eingewöhnung";
    case Phase::eArmed:
      return "Startbereit";
    case Phase::eRunning:
      return "Flugphase";
    case Phase::eFinished:
      return "Beendet";
    }

    return "Unbekannt";
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  std::string Plugin::hintText(Clock::time_point now) const
  {
    switch (mPhase)
    {
    case Phase::eIdle:
      return "M = Messe-Modus";

    case Phase::eAwaitWarmupSelection:
      return "1 / 2 / 3 / 5";

    case Phase::eWarmup:
      if (mPluginSettings.mAutoStartAfterWarmup)
      {
        return fmt::format("Vorauswahl: {} min –", mPreparedMinutes);
      }
      return fmt::format(
          "Vorauswahl: {} min –", mPreparedMinutes);

    case Phase::eArmed:
      return fmt::format(
          "1 / 2 / 3 / 5 startet den Flug. Vorauswahl aus Warm-up: {} min.", mPreparedMinutes);

    case Phase::eRunning:
      if (getRemainingRunSeconds(now) < mPluginSettings.mAddThresholdSeconds)
      {
        return " ";
      }
      return fmt::format("Zeit addiert erst unter {:.0f}s Restzeit.",
                         mPluginSettings.mAddThresholdSeconds);

    case Phase::eFinished:
      return "Flug beendet";
    }

    return {};
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::createHud()
  {
    destroyHud();

    auto *pSG = GetVistaSystem()->GetGraphicsManager()->GetSceneGraph();
    auto *displayManager = GetVistaSystem()->GetDisplayManager();
    auto *displaySystem = displayManager->GetDisplaySystem();
    auto *userPlatform = GetVistaSystem()->GetPlatformFor(displaySystem);
    auto *userNode = userPlatform->GetPlatformUserNode();

    mHudAnchor.reset(pSG->NewTransformNode(userNode));

    mHudArea = std::make_unique<cs::gui::WorldSpaceGuiArea>(
        static_cast<int>(mPluginSettings.mHudWidth),
        static_cast<int>(mPluginSettings.mHudHeight));

    mHudArea->setIgnoreDepth(mPluginSettings.mIgnoreDepth);
    mHudArea->setEnableBackfaceCulling(false);

    mHudAnchor->SetTranslation(
        0.0F,
        static_cast<float>(mPluginSettings.mHudVerticalOffsetMeters),
        -static_cast<float>(mPluginSettings.mHudDistanceMeters));

    mHudTransform.reset(pSG->NewTransformNode(mHudAnchor.get()));

    mHudTransform->Scale(0.001F * static_cast<float>(mHudArea->getWidth()) *
                             static_cast<float>(mPluginSettings.mHudScale),
                         0.001F * static_cast<float>(mHudArea->getHeight()) *
                             static_cast<float>(mPluginSettings.mHudScale),
                         1.0F);

    mHudNode.reset(pSG->NewOpenGLNode(mHudTransform.get(), mHudArea.get()));

    // mHudNode.get(), static_cast<int>(cs::utils::DrawOrder::eGui);

    VistaOpenSGMaterialTools::SetSortKeyOnSubtree(
        mHudTransform.get(), static_cast<int>(cs::utils::DrawOrder::eGui));

    mHudItem = std::make_unique<cs::gui::GuiItem>(
        "file://../share/resources/gui/csp-messe-hud.html", true);

    mHudArea->addItem(mHudItem.get());
    mHudItem->waitForFinishedLoading();

    pushOverlayState(true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::destroyHud()
  {
    if (mHudAnchor)
    {
      auto *displayManager = GetVistaSystem()->GetDisplayManager();
      auto *displaySystem = displayManager->GetDisplaySystem();
      auto *userPlatform = GetVistaSystem()->GetPlatformFor(displaySystem);
      auto *userNode = userPlatform->GetPlatformUserNode();

      userNode->DisconnectChild(mHudAnchor.get());
    }

    mHudNode.reset();
    mHudTransform.reset();
    mHudAnchor.reset();
    mHudItem.reset();
    mHudArea.reset();
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::updateHudTransform()
  {
    if (!mHudAnchor)
    {
      return;
    }

    auto const &observer = mSolarSystem->getObserver();

    cs::scene::CelestialAnchor hudAnchor(observer.getCenterName(), observer.getFrameName());

    glm::dvec3 forward = observer.getRotation() * glm::dvec3(0.0, 0.0, -1.0);
    glm::dvec3 up = observer.getRotation() * glm::dvec3(0.0, 1.0, 0.0);
    glm::dvec3 hudPosition = observer.getPosition() + forward * mPluginSettings.mHudDistanceMeters + up * mPluginSettings.mHudVerticalOffsetMeters;

    hudAnchor.setPosition(hudPosition);
    hudAnchor.setRotation(observer.getRotation());
    hudAnchor.setScale(observer.getScale());

    auto transform =
        observer.getRelativeTransform(mTimeControl->pSimulationTime.get(), hudAnchor);

    // auto hudTransform = transform;

    mHudAnchor->SetTransform(glm::value_ptr(transform), true);
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

  void Plugin::pushOverlayState(bool force)
  {
    if (!mHudItem)
    {
      return;
    }

    auto const now = Clock::now();

    if (!force)
    {
      auto const dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastOverlayPush);
      if (dt.count() < 25)
      {
        return;
      }
    }

    nlohmann::json state;
    state["visible"] = mPluginSettings.mShowHudInIdle || mPhase != Phase::eIdle;
    state["phase"] = phaseLabel();
    state["hint"] = hintText(now);
    state["altitude"] = formatDistance(getApproxAltitude());
    state["speed"] = formatSpeed(mSolarSystem->pCurrentObserverSpeed.get());
    state["gifPath"] = toFileUrl(mPluginSettings.mHudGifPath);
    //  state["gifPath"]    = "file://../share/resources/gui/img/csp-messe.gif";
    state["hudWidth"] = mPluginSettings.mHudWidth;
    state["hudHeight"] = mPluginSettings.mHudHeight;
    state["prepared"] = mPreparedMinutes;
    state["remaining"] = "00:00";
    state["timeLabel"] = "";
    state["phaseClass"] = "idle";

    switch (mPhase)
    {
    case Phase::eWarmup:
      state["remaining"] = formatTime(getRemainingWarmupSeconds(now));
      state["timeLabel"] = "Eingewöhnung";
      state["phaseClass"] = "warmup";
      break;

    case Phase::eRunning:
      state["remaining"] = formatTime(getRemainingRunSeconds(now));
      state["timeLabel"] = "Flugzeit";
      state["phaseClass"] = "running";
      break;

    case Phase::eArmed:
      state["remaining"] = fmt::format("{} min", mPreparedMinutes);
      state["timeLabel"] = "Vorauswahl";
      state["phaseClass"] = "armed";
      break;

    case Phase::eAwaitWarmupSelection:
      state["remaining"] = "--:--";
      state["timeLabel"] = "Warten auf Auswahl";
      state["phaseClass"] = "armed";
      break;

    case Phase::eFinished:
      state["remaining"] = "00:00";
      state["timeLabel"] = "Abgelaufen";
      state["phaseClass"] = "finished";
      break;

    case Phase::eIdle:
    default:
      state["remaining"] = "--:--";
      state["timeLabel"] = "Bereit";
      state["phaseClass"] = "idle";
      break;
    }

    auto const serialized = state.dump();

    if (force || serialized != mLastOverlayJSON)
    {
      mHudItem->callJavascript("CosmoScout.messe.setState", serialized);
      mLastOverlayJSON = serialized;
    }

    mLastOverlayPush = now;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::messeSSS