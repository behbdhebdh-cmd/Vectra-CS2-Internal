#pragma once
#include "src/config/settings.hpp"
#include "src/engine/snapshot.hpp"
#include "src/input/input_controller.hpp"
namespace vectra {
enum class TriggerState { Disabled, MasterDisabled, SessionUnauthorized, WindowUnfocused, SnapshotStale, NoCrosshairTarget, FriendlyTarget, IneligibleTarget, EligibleEnemy, InputFailed };
struct TriggerStatus {
    TriggerState state{TriggerState::Disabled};
    uint32_t targetIndex{};
    bool clickEmitted{};
    DWORD inputError{};
};
class Triggerbot { public: static TriggerStatus Update(const FrameSnapshot& frame, Settings& settings, InputController& input); };
}
