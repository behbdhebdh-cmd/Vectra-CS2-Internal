#include "src/features/triggerbot.hpp"
#include <algorithm>
namespace vectra { TriggerStatus Triggerbot::Update(const FrameSnapshot& frame, Settings& s, InputController& input) {
    TriggerStatus status{};
    if (!s.triggerEnabled.load()) return status;
    if (!s.masterEnabled.load()) { status.state = TriggerState::MasterDisabled; return status; }
    if (!s.privateMatchAuthorized.load()) { status.state = TriggerState::SessionUnauthorized; return status; }
    if (!input.GameFocused()) { status.state = TriggerState::WindowUnfocused; return status; }
    if (!input.SnapshotFresh() || !frame.valid) { status.state = TriggerState::SnapshotStale; return status; }
    if (frame.crosshairEntityIndex <= 0) { status.state = TriggerState::NoCrosshairTarget; return status; }
    const auto player = std::find_if(frame.players.begin(), frame.players.end(), [&](const PlayerSnapshot& value) { return value.pawnEntityIndex == static_cast<uint32_t>(frame.crosshairEntityIndex); });
    if (player == frame.players.end()) { status.state = TriggerState::NoCrosshairTarget; return status; }
    status.targetIndex = player->index;
    if (s.teamCheckEnabled.load() && player->team == frame.localTeam) { status.state = TriggerState::FriendlyTarget; return status; }
    if (!player->alive || player->dormant) { status.state = TriggerState::IneligibleTarget; return status; }
    const ClickResult result = input.ClickPrimary();
    status.clickEmitted = result == ClickResult::Sent;
    status.inputError = input.LastInputError();
    status.state = result == ClickResult::Failed ? TriggerState::InputFailed : TriggerState::EligibleEnemy;
    return status;
} }
