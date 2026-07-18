#include "src/features/recoil_control.hpp"
#include <algorithm>
#include <cmath>

namespace vectra {
namespace {
Angles previousPunch{};
int32_t previousPunchTick{-1};
uint32_t previousWeaponHandle{0xFFFFFFFF};
bool tracking{};
}

void RecoilControl::Reset() {
    previousPunch = {};
    previousPunchTick = -1;
    previousWeaponHandle = 0xFFFFFFFF;
    tracking = false;
}

RcsStatus RecoilControl::Update(const FrameSnapshot& frame, Settings& settings, InputController& input) {
    RcsStatus status{};
    status.viewPunch = frame.localViewPunch;
    status.punchTick = frame.viewPunchTick;
    status.shotsFired = frame.localShotsFired;
    status.activeWeaponHandle = frame.activeWeaponHandle;
    status.sensitivity = frame.mouseSensitivity;
    status.sensitivitySource = frame.sensitivitySource;
    status.recoilDataState = frame.recoilDataState;
    const auto stop = [&](RcsState state) { Reset(); input.ResetResidual(); status.state = state; return status; };
    if (!settings.rcsEnabled.load()) return stop(RcsState::Disabled);
    if (!settings.masterEnabled.load()) return stop(RcsState::MasterDisabled);
    if (!settings.privateMatchAuthorized.load()) return stop(RcsState::SessionUnauthorized);
    if (!input.GameFocused()) return stop(RcsState::WindowUnfocused);
    if (!input.SnapshotFresh() || !frame.valid) return stop(RcsState::SnapshotStale);
    if (!frame.hasRecoilData || frame.recoilDataState != RecoilDataState::Ready || frame.mouseSensitivity <= 0.f || frame.sensitivitySource == SensitivitySource::Unavailable ||
        frame.viewPunchTick < 0 || frame.activeWeaponHandle == 0 || frame.activeWeaponHandle == 0xFFFFFFFF || !Finite(frame.localViewPunch)) return stop(RcsState::InvalidData);
    if (frame.localWeaponReloading || !(GetAsyncKeyState(VK_LBUTTON) & 0x8000) || frame.localShotsFired < 1) return stop(RcsState::WaitingForSpray);

    Angles delta{};
    if (!tracking || previousWeaponHandle != frame.activeWeaponHandle) {
        input.ResetResidual();
        previousPunch = {};
        previousPunchTick = frame.viewPunchTick;
        previousWeaponHandle = frame.activeWeaponHandle;
        tracking = true;
        delta = frame.localViewPunch;
    } else if (frame.viewPunchTick == previousPunchTick) {
        status.state = RcsState::Tracking;
        return status;
    } else if (frame.viewPunchTick < previousPunchTick) {
        return stop(RcsState::InvalidData);
    } else {
        delta = {frame.localViewPunch.pitch - previousPunch.pitch, frame.localViewPunch.yaw - previousPunch.yaw};
    }
    previousPunch = frame.localViewPunch;
    previousPunchTick = frame.viewPunchTick;
    status.punchDelta = delta;
    if (!Finite(delta) || std::abs(delta.pitch) > 8.f || std::abs(delta.yaw) > 8.f) return stop(RcsState::InvalidData);

    constexpr float kMouseAngularScale = .022f;
    const float strength = std::clamp(settings.rcsStrength.load(), 0.f, 100.f) * .01f;
    const float countsPerDegree = 1.f / (frame.mouseSensitivity * kMouseAngularScale);
    status.movementX = -delta.yaw * countsPerDegree * strength;
    status.movementY = -delta.pitch * countsPerDegree * strength;
    if (!std::isfinite(status.movementX) || !std::isfinite(status.movementY) || std::abs(status.movementX) > 250.f || std::abs(status.movementY) > 250.f)
        return stop(RcsState::InvalidData);
    input.QueueRelative(status.movementX, status.movementY);
    status.state = RcsState::Tracking;
    return status;
}
}
