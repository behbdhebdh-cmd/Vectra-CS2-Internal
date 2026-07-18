#pragma once

#include "src/config/settings.hpp"
#include "src/engine/snapshot.hpp"
#include "src/input/input_controller.hpp"

namespace vectra {
enum class RcsState {
    Disabled,
    MasterDisabled,
    SessionUnauthorized,
    WindowUnfocused,
    SnapshotStale,
    InvalidData,
    WaitingForSpray,
    Tracking,
    InputFailed
};

struct RcsStatus {
    RcsState state{RcsState::Disabled};
    float movementX{};
    float movementY{};
    Angles viewPunch{};
    Angles punchDelta{};
    int32_t punchTick{-1};
    int shotsFired{};
    uint32_t activeWeaponHandle{0xFFFFFFFF};
    float sensitivity{};
    SensitivitySource sensitivitySource{SensitivitySource::Unavailable};
    RecoilDataState recoilDataState{RecoilDataState::Unavailable};
    DWORD inputError{};
    bool inputEmitted{};
};

class RecoilControl {
public:
    static RcsStatus Update(const FrameSnapshot& frame, Settings& settings, InputController& input);
    static void Reset();
};
}
