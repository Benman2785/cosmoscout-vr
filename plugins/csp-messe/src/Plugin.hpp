////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR) <cosmoscout@dlr.de>
// SPDX-License-Identifier: MIT

#ifndef CSP_MESSE_PLUGIN_HPP
#define CSP_MESSE_PLUGIN_HPP

#include "../../../src/cs-core/PluginBase.hpp"
#include "../../../src/cs-scene/CelestialAnchor.hpp"

#include <glm/vec3.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

class VistaOpenGLNode;
class VistaTransformNode;

namespace cs::gui {
class GuiItem;
class WorldSpaceGuiArea;
} // namespace cs::gui

namespace csp::messe {

class Plugin : public cs::core::PluginBase {
 public:
  struct Settings {
    bool        mShowHudInIdle{true};
    bool        mAutoStartAfterWarmup{false};
    double      mWarmupSeconds{30.0};
    double      mAddThresholdSeconds{90.0};
    std::string mHudGifPath{"file://../share/resources/gui/img/csp-messe.webp"};
    uint32_t    mHudWidth{2400};
    uint32_t    mHudHeight{1920};
    double      mHudDistanceMeters{3.0};
    double      mHudVerticalOffsetMeters{-0.33};
    double      mHudScale{1.0};
    double      mHeightScale{1.0};
    bool        mIgnoreDepth{false};
  };

  void init() override;
  void deInit() override;
  void update() override;

 private:
  enum class Phase { eIdle, eAwaitWarmupSelection, eWarmup, eArmed, eRunning, eFinished };

  using Clock = std::chrono::steady_clock;

  void onLoad();
  void onSave();

  void bindModeHotkey();
  void unbindModeHotkey();

  void bindTimeHotkeys();
  void unbindTimeHotkeys();

  void bindTeleportModeHotkey();
  void unbindTeleportModeHotkey();

  void enterMesseMode();
  void resetSession(Phase targetPhase = Phase::eIdle);
  void handleTimeHotkey(int minutes);

  void enterTeleportMode();
  void bindTeleportBookmarkHotkeys();
  void unbindTeleportBookmarkHotkeys(bool restoreTimeHotkeys);
  void restoreTimeHotkeysIfNeeded();
  void handleTeleportBookmarkHotkey(int bookmarkID);

  void startWarmup(int minutes);
  void startRun(int minutes);
  void finishRun();
  void addTime(int minutes);

  double      getRemainingWarmupSeconds(Clock::time_point now) const;
  double      getRemainingRunSeconds(Clock::time_point now) const;
  double      getApproxAltitude() const;
  std::string formatTime(double seconds) const;
  std::string phaseLabel() const;
  std::string hintText(Clock::time_point now) const;
  std::string formatDistance(double meters) const;
  std::string formatSpeed(double metersPerSecond) const;

  void createHud();
  void destroyHud();
  void updateHudTransform();
  void pushOverlayState(bool force);

  Settings mPluginSettings;

  Phase mPhase{Phase::eIdle};
  int   mPreparedMinutes{0};

  Clock::time_point mWarmupEndsAt{};
  Clock::time_point mRunEndsAt{};
  Clock::time_point mLastOverlayPush{};

  glm::dvec3 mLastObserverPosition{0.0};
  glm::dvec3 mLastHudForward{0.0, 0.0, -1.0};

  Clock::time_point mLastObserverPositionTime{};

  bool mTeleportHotkeysBound = false;
  bool mHasLastObserverPosition{false};
  bool mHasLastHudForward{false};

  std::string mLastOverlayJSON;
  bool        mTimeHotkeysBound{false};

  std::unique_ptr<cs::gui::WorldSpaceGuiArea> mHudArea;
  std::unique_ptr<cs::gui::GuiItem>           mHudItem;
  std::unique_ptr<VistaTransformNode>         mHudAnchor;
  std::unique_ptr<VistaTransformNode>         mHudTransform;
  std::unique_ptr<VistaOpenGLNode>            mHudNode;

  int mOnLoadConnection{-1};
  int mOnSaveConnection{-1};
};

} // namespace csp::messe

#endif // CSP_MESSE_PLUGIN_HPP