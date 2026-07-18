#pragma once
#include "src/config/settings.hpp"
#include "src/engine/snapshot.hpp"
#include "src/input/input_controller.hpp"
namespace vectra {
enum class AimState {
    Disabled,
    MasterDisabled,
    SessionUnauthorized,
    WindowUnfocused,
    SnapshotStale,
    ActivationReleased,
    NoTarget,
    Tracking,
    InputFailed
};
struct AimStatus {
    AimState state{AimState::Disabled};
    uint32_t targetIndex{};
    float angularError{};
    float movementX{};
    float movementY{};
    Vec2 targetScreen{};
    Vec2 screenError{};
    DWORD inputError{};
    bool inputEmitted{};
    bool hasProjectedTarget{};
};
class AimAssist {
public:
    static AimStatus Update(const FrameSnapshot& frame, Settings& settings, InputController& input, const Vec2& displaySize);
};
}
