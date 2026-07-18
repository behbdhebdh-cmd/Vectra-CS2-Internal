#include "src/features/aim_assist.hpp"
#include "src/features/targeting.hpp"
#include <cmath>
namespace vectra { AimStatus AimAssist::Update(const FrameSnapshot& frame, Settings& s, InputController& input, const Vec2& displaySize) {
    AimStatus status{};
    const auto stop = [&](AimState state) { status.state = state; return status; };
    if (!s.aimEnabled.load()) return stop(AimState::Disabled);
    if (!s.masterEnabled.load()) return stop(AimState::MasterDisabled);
    if (!s.privateMatchAuthorized.load()) return stop(AimState::SessionUnauthorized);
    if (!input.GameFocused()) return stop(AimState::WindowUnfocused);
    if (!input.SnapshotFresh() || !frame.valid) return stop(AimState::SnapshotStale);
    if (!(GetAsyncKeyState(s.aimActivationVk.load()) & 0x8000)) return stop(AimState::ActivationReleased);
    const auto target = Targeting::Acquire(frame, s); if (!target) return stop(AimState::NoTarget);
    const ProjectionResult projected = Targeting::Project(target->position, frame.viewProjection, displaySize);
    if (projected.state != ProjectionState::Visible) return stop(AimState::NoTarget);

    static uint32_t previousIndex{};
    static uint32_t previousGeneration{};
    if (previousIndex != target->player->index || previousGeneration != target->player->generation) {
        previousIndex = target->player->index;
        previousGeneration = target->player->generation;
    }
    status.targetIndex = target->player->index;
    status.angularError = target->fov;
    status.targetScreen = projected.screen;
    status.screenError = {projected.screen.x - displaySize.x * .5f, projected.screen.y - displaySize.y * .5f};
    status.hasProjectedTarget = true;
    const float divisor = std::max(1.0f, s.smoothing.load());
    const float scale = std::max(.01f, s.aimScreenGain.load()) / divisor;
    if (std::hypot(status.screenError.x, status.screenError.y) <= 1.5f) {
        status.state = AimState::Tracking;
        return status;
    }
    status.movementX = status.screenError.x * scale;
    status.movementY = status.screenError.y * scale;
    input.QueueRelative(status.movementX, status.movementY);
    status.state = AimState::Tracking;
    return status;
} }
