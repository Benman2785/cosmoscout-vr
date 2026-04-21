////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#include "Plugin.hpp"
#include "logger.hpp"

#include "../../../src/cs-core/GuiManager.hpp"
#include "../../../src/cs-core/Settings.hpp"

#include <VistaKernel/VistaSystem.h>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN cs::core::PluginBase* create() {
  return new csp::messe::Plugin;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT_FN void destroy(cs::core::PluginBase* pluginBase) {
  delete pluginBase;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace csp::messe {

////////////////////////////////////////////////////////////////////////////////////////////////////

void from_json(nlohmann::json const& j, Plugin::Settings& o) {
  cs::core::Settings::deserialize(j, "showHudInIdle", o.mShowHudInIdle);
  cs::core::Settings::deserialize(j, "autoStartAfterWarmup", o.mAutoStartAfterWarmup);
  cs::core::Settings::deserialize(j, "warmupSeconds", o.mWarmupSeconds);
  cs::core::Settings::deserialize(j, "addThresholdSeconds", o.mAddThresholdSeconds);
  cs::core::Settings::deserialize(j, "hudGifPath", o.mHudGifPath);
  cs::core::Settings::deserialize(j, "hudWidth", o.mHudWidth);
  cs::core::Settings::deserialize(j, "hudHeight", o.mHudHeight);
}

void to_json(nlohmann::json& j, Plugin::Settings const& o) {
  cs::core::Settings::serialize(j, "showHudInIdle", o.mShowHudInIdle);
  cs::core::Settings::serialize(j, "autoStartAfterWarmup", o.mAutoStartAfterWarmup);
  cs::core::Settings::serialize(j, "warmupSeconds", o.mWarmupSeconds);
  cs::core::Settings::serialize(j, "addThresholdSeconds", o.mAddThresholdSeconds);
  cs::core::Settings::serialize(j, "hudGifPath", o.mHudGifPath);
  cs::core::Settings::serialize(j, "hudWidth", o.mHudWidth);
  cs::core::Settings::serialize(j, "hudHeight", o.mHudHeight);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
std::string normalizePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::init() {
  logger().info("Loading plugin...");

  mOnLoadConnection = mAllSettings->onLoad().connect([this]() { onLoad(); });
  mOnSaveConnection = mAllSettings->onSave().connect([this]() { onSave(); });

  onLoad();

  mGuiManager->addCSS("css/csp-messe.css");
  mGuiManager->addTemplate("csp-messe-template", "../share/resources/gui/csp-messe-template.html");
  mGuiManager->executeJavascriptFile("../share/resources/gui/js/csp-messe.js");
  mGuiManager->getGui()->executeJavascript(
      "if (window.CosmoScout && CosmoScout.messe) { CosmoScout.messe.init(); }");

  bindModeHotkey();
  pushOverlayState(true);

  logger().info("Loading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::deInit() {
  logger().info("Unloading plugin...");

  onSave();

  unbindTimeHotkeys();
  unbindModeHotkey();

  mAllSettings->onLoad().disconnect(mOnLoadConnection);
  mAllSettings->onSave().disconnect(mOnSaveConnection);

  if (mGuiManager && mGuiManager->getGui()) {
    mGuiManager->getGui()->executeJavascript(
        "if (window.CosmoScout && CosmoScout.messe) { CosmoScout.messe.deinit(); }");
  }

  mGuiManager->removeTemplate("csp-messe-template");
  mGuiManager->removeCSS("css/csp-messe.css");

  logger().info("Unloading done.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::update() {
  auto const now = Clock::now();

  if (mPhase == Phase::eWarmup && now >= mWarmupEndsAt) {
    if (mPluginSettings.mAutoStartAfterWarmup && mPreparedMinutes > 0) {
      startRun(mPreparedMinutes);
    } else {
      mPhase = Phase::eArmed;
      mGuiManager->showNotification(
          "Messe-Modus", "Eingewöhnungsphase beendet – 1 / 2 / 3 / 5 startet den Flug-Timer.",
          "timer");
      pushOverlayState(true);
    }
  }

  if (mPhase == Phase::eRunning && now >= mRunEndsAt) {
    finishRun();
  }

  pushOverlayState(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::onLoad() {
  if (mAllSettings->mPlugins.contains("csp-messe")) {
    from_json(mAllSettings->mPlugins.at("csp-messe"), mPluginSettings);
  }

  if (mGuiManager && mGuiManager->getGui()) {
    pushOverlayState(true);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::onSave() {
  mAllSettings->mPlugins["csp-messe"] = mPluginSettings;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::bindModeHotkey() {
  GetVistaSystem()->GetKeyboardSystemControl()->BindAction('m', 0, [this]() { enterMesseMode(); });
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::unbindModeHotkey() {
  GetVistaSystem()->GetKeyboardSystemControl()->UnbindAction('m');
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::bindTimeHotkeys() {
  if (mTimeHotkeysBound) {
    return;
  }

  auto* keyboard = GetVistaSystem()->GetKeyboardSystemControl();

  keyboard->BindAction('1', 0, [this]() { handleTimeHotkey(1); });
  keyboard->BindAction('2', 0, [this]() { handleTimeHotkey(2); });
  keyboard->BindAction('3', 0, [this]() { handleTimeHotkey(3); });
  keyboard->BindAction('5', 0, [this]() { handleTimeHotkey(5); });

  mTimeHotkeysBound = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::unbindTimeHotkeys() {
  if (!mTimeHotkeysBound) {
    return;
  }

  auto* keyboard = GetVistaSystem()->GetKeyboardSystemControl();

  keyboard->UnbindAction('1');
  keyboard->UnbindAction('2');
  keyboard->UnbindAction('3');
  keyboard->UnbindAction('5');

  mTimeHotkeysBound = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::enterMesseMode() {
  bindTimeHotkeys();
  resetSession(Phase::eAwaitWarmupSelection);

  mGuiManager->showNotification(
      "Messe-Modus", "Aktiviert – jetzt 1 / 2 / 3 / 5 wählen, um die Eingewöhnungsphase zu "
                     "starten.",
      "timer");
  pushOverlayState(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::resetSession(Phase targetPhase) {
  mPhase           = targetPhase;
  mPreparedMinutes = 0;
  mWarmupEndsAt    = Clock::time_point{};
  mRunEndsAt       = Clock::time_point{};

  if (targetPhase == Phase::eIdle || targetPhase == Phase::eFinished) {
    unbindTimeHotkeys();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::handleTimeHotkey(int minutes) {
  switch (mPhase) {
  case Phase::eAwaitWarmupSelection:
    startWarmup(minutes);
    break;

  case Phase::eWarmup:
    mPreparedMinutes = minutes;
    mGuiManager->showNotification(
        "Messe-Modus",
        fmt::format("Vorauswahl aktualisiert: {} min. Nach der Eingewöhnungsphase erneut 1 / 2 / "
                    "3 / 5 drücken.",
                    minutes),
        "schedule");
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
    // Intentionally ignored. The time hotkeys are only bound in Messe-Modus, but if some branch
    // keeps them active, we still want deterministic behavior.
    break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::startWarmup(int minutes) {
  mPreparedMinutes = minutes;
  mPhase           = Phase::eWarmup;
  mWarmupEndsAt    = Clock::now() + std::chrono::duration_cast<Clock::duration>(
                                       std::chrono::duration<double>(mPluginSettings.mWarmupSeconds));

  mGuiManager->showNotification(
      "Messe-Modus",
      fmt::format("{:.0f}s Eingewöhnungsphase gestartet. Vorauswahl: {} min.",
                  mPluginSettings.mWarmupSeconds, minutes),
      "timer");
  pushOverlayState(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::startRun(int minutes) {
  mPreparedMinutes = minutes;
  mPhase           = Phase::eRunning;
  mRunEndsAt       = Clock::now() + std::chrono::minutes(minutes);

  mGuiManager->showNotification(
      "Flug-Timer gestartet", fmt::format("{} min freies Fliegen.", minutes), "flight");
  pushOverlayState(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::finishRun() {
  resetSession(Phase::eFinished);

  mGuiManager->showNotification(
      "Flug-Zeit abgelaufen", "Die Runde ist beendet. Mit M startest du eine neue Messesession.",
      "timer_off");
  pushOverlayState(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::addTime(int minutes) {
  auto const remaining = getRemainingRunSeconds(Clock::now());

  if (remaining >= mPluginSettings.mAddThresholdSeconds) {
    mGuiManager->showNotification(
        "Zeit nicht addiert",
        fmt::format("Zeit darf erst addiert werden, wenn weniger als {:.0f}s verbleiben.",
                    mPluginSettings.mAddThresholdSeconds),
        "timer_off");
    return;
  }

  mRunEndsAt += std::chrono::minutes(minutes);

  mGuiManager->showNotification(
      "Zeit addiert", fmt::format("+{} min wurden zum Flug-Timer addiert.", minutes), "add");
  pushOverlayState(true);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double Plugin::getRemainingWarmupSeconds(Clock::time_point now) const {
  if (mPhase != Phase::eWarmup) {
    return 0.0;
  }

  return std::max(
      0.0, std::chrono::duration_cast<std::chrono::duration<double>>(mWarmupEndsAt - now).count());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

double Plugin::getRemainingRunSeconds(Clock::time_point now) const {
  if (mPhase != Phase::eRunning) {
    return 0.0;
  }

  return std::max(
      0.0, std::chrono::duration_cast<std::chrono::duration<double>>(mRunEndsAt - now).count());
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::string Plugin::formatTime(double seconds) const {
  auto const totalSeconds = std::max(0, static_cast<int>(std::ceil(seconds)));
  auto const minutes      = totalSeconds / 60;
  auto const restSeconds  = totalSeconds % 60;

  return fmt::format("{:02d}:{:02d}", minutes, restSeconds);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::string Plugin::phaseLabel() const {
  switch (mPhase) {
  case Phase::eIdle:
    return "Bereit";
  case Phase::eAwaitWarmupSelection:
    return "Messe-Modus";
  case Phase::eWarmup:
    return "Eingewöhnung";
  case Phase::eArmed:
    return "Startbereit";
  case Phase::eRunning:
    return "Flug läuft";
  case Phase::eFinished:
    return "Beendet";
  }

  return "Unbekannt";
}

////////////////////////////////////////////////////////////////////////////////////////////////////

std::string Plugin::hintText(Clock::time_point now) const {
  switch (mPhase) {
  case Phase::eIdle:
    return "M = Messe-Modus";

  case Phase::eAwaitWarmupSelection:
    return "1 / 2 / 3 / 5 startet die Eingewöhnungsphase.";

  case Phase::eWarmup:
    if (mPluginSettings.mAutoStartAfterWarmup) {
      return fmt::format("Vorauswahl: {} min – startet danach automatisch.", mPreparedMinutes);
    }
    return fmt::format(
        "Vorauswahl: {} min – danach erneut 1 / 2 / 3 / 5 zum Start drücken.", mPreparedMinutes);

  case Phase::eArmed:
    return fmt::format(
        "1 / 2 / 3 / 5 startet den Flug. Vorauswahl aus Warm-up: {} min.", mPreparedMinutes);

  case Phase::eRunning:
    if (getRemainingRunSeconds(now) < mPluginSettings.mAddThresholdSeconds) {
      return "1 / 2 / 3 / 5 addiert zusätzliche Zeit.";
    }
    return fmt::format("Zeit-Hotkeys addieren erst unter {:.0f}s Restzeit.",
                       mPluginSettings.mAddThresholdSeconds);

  case Phase::eFinished:
    return "M startet eine neue Messesession.";
  }

  return {};
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void Plugin::pushOverlayState(bool force) {
  if (!mGuiManager || !mGuiManager->getGui()) {
    return;
  }

  auto const now = Clock::now();

  if (!force) {
    auto const dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastOverlayPush);
    if (dt.count() < 100) {
      return;
    }
  }

  nlohmann::json state;
  state["visible"]    = mPluginSettings.mShowHudInIdle || mPhase != Phase::eIdle;
  state["phase"]      = phaseLabel();
  state["hint"]       = hintText(now);
  state["gifPath"]    = normalizePath(mPluginSettings.mHudGifPath);
  state["hudWidth"]   = mPluginSettings.mHudWidth;
  state["hudHeight"]  = mPluginSettings.mHudHeight;
  state["prepared"]   = mPreparedMinutes;
  state["remaining"]  = "00:00";
  state["timeLabel"]  = "";
  state["phaseClass"] = "idle";

  switch (mPhase) {
  case Phase::eWarmup:
    state["remaining"]  = formatTime(getRemainingWarmupSeconds(now));
    state["timeLabel"]  = "Eingewöhnung";
    state["phaseClass"] = "warmup";
    break;

  case Phase::eRunning:
    state["remaining"]  = formatTime(getRemainingRunSeconds(now));
    state["timeLabel"]  = "Flugzeit";
    state["phaseClass"] = "running";
    break;

  case Phase::eArmed:
    state["remaining"]  = fmt::format("{} min", mPreparedMinutes);
    state["timeLabel"]  = "Vorauswahl";
    state["phaseClass"] = "armed";
    break;

  case Phase::eAwaitWarmupSelection:
    state["remaining"]  = "--:--";
    state["timeLabel"]  = "Warten auf Auswahl";
    state["phaseClass"] = "armed";
    break;

  case Phase::eFinished:
    state["remaining"]  = "00:00";
    state["timeLabel"]  = "Abgelaufen";
    state["phaseClass"] = "finished";
    break;

  case Phase::eIdle:
  default:
    state["remaining"]  = "--:--";
    state["timeLabel"]  = "Bereit";
    state["phaseClass"] = "idle";
    break;
  }

  auto const serialized = state.dump();

  if (force || serialized != mLastOverlayJSON) {
    mGuiManager->getGui()->callJavascript("CosmoScout.messe.setState", serialized);
    mLastOverlayJSON = serialized;
  }

  mLastOverlayPush = now;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::messe
