////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#ifndef CSP_MESSE_PLUGIN_HPP
#define CSP_MESSE_PLUGIN_HPP

#include "../../../src/cs-core/PluginBase.hpp"

#include <chrono>
#include <string>

namespace csp::messe {

/// Messe / booth mode with warm-up phase, timed free flight and a head-locked HUD overlay.
///
/// Implemented behavior for the ambiguous workflow in the request:
///   * M enters "Messe-Modus".
///   * First 1 / 2 / 3 / 5 starts the warm-up and stores a preferred run length.
///   * After warm-up, another 1 / 2 / 3 / 5 starts the actual timed run by default.
///   * Optional auto-start after warm-up can be enabled in the settings.
class Plugin : public cs::core::PluginBase {
 public:
  struct Settings {
    bool        mShowHudInIdle{true};
    bool        mAutoStartAfterWarmup{false};
    double      mWarmupSeconds{30.0};
    double      mAddThresholdSeconds{90.0};
    std::string mHudGifPath{"../share/resources/gui/img/csp-messe-placeholder.gif"};
    uint32_t    mHudWidth{220};
    uint32_t    mHudHeight{220};
  };

  void init() override;
  void deInit() override;
  void update() override;

 private:
  enum class Phase {
    eIdle,
    eAwaitWarmupSelection,
    eWarmup,
    eArmed,
    eRunning,
    eFinished
  };

  using Clock = std::chrono::steady_clock;

  void onLoad();
  void onSave();

  void bindModeHotkey();
  void unbindModeHotkey();

  void bindTimeHotkeys();
  void unbindTimeHotkeys();

  void enterMesseMode();
  void resetSession(Phase targetPhase = Phase::eIdle);
  void handleTimeHotkey(int minutes);

  void startWarmup(int minutes);
  void startRun(int minutes);
  void finishRun();
  void addTime(int minutes);

  double      getRemainingWarmupSeconds(Clock::time_point now) const;
  double      getRemainingRunSeconds(Clock::time_point now) const;
  std::string formatTime(double seconds) const;
  std::string phaseLabel() const;
  std::string hintText(Clock::time_point now) const;

  void pushOverlayState(bool force);

  Settings mPluginSettings;

  Phase mPhase{Phase::eIdle};

  int mPreparedMinutes{0};

  Clock::time_point mWarmupEndsAt{};
  Clock::time_point mRunEndsAt{};
  Clock::time_point mLastOverlayPush{};

  std::string mLastOverlayJSON;
  bool        mTimeHotkeysBound{false};

  int mOnLoadConnection{-1};
  int mOnSaveConnection{-1};
};

} // namespace csp::messe

#endif // CSP_MESSE_PLUGIN_HPP
