#include "src/input/input_controller.hpp"
#include <cmath>

namespace vectra {
bool InputController::Allowed() const {
    if (!settings_.masterEnabled.load() || !settings_.privateMatchAuthorized.load() || !snapshotFresh_.load()) return false;
    return GameFocused();
}
bool InputController::GameFocused() const {
    return !settings_.requireFocus.load() || (gameWindow_ && GetForegroundWindow() == gameWindow_);
}
void InputController::BeginFrame() { queuedX_ = queuedY_ = 0.f; }
void InputController::QueueRelative(float dx, float dy) {
    if (std::isfinite(dx)) queuedX_ += dx;
    if (std::isfinite(dy)) queuedY_ += dy;
}
MoveResult InputController::FlushRelative() {
    const float dx = queuedX_, dy = queuedY_;
    queuedX_ = queuedY_ = 0.f;
    if (dx == 0.f && dy == 0.f) return MoveResult::Accumulating;
    return MoveRelative(dx, dy);
}
MoveResult InputController::MoveRelative(float dx, float dy) {
    if (!Allowed()) return MoveResult::Failed;
    residualX_ += dx;
    residualY_ += dy;
    const long moveX = static_cast<long>(std::trunc(residualX_));
    const long moveY = static_cast<long>(std::trunc(residualY_));
    if (moveX == 0 && moveY == 0) return MoveResult::Accumulating;

    residualX_ -= static_cast<float>(moveX);
    residualY_ -= static_cast<float>(moveY);
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dx = moveX; input.mi.dy = moveY; input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SetLastError(ERROR_SUCCESS);
    if (SendInput(1, &input, sizeof(input)) == 1) {
        lastInputError_.store(ERROR_SUCCESS);
        return MoveResult::Sent;
    }
    residualX_ += static_cast<float>(moveX);
    residualY_ += static_cast<float>(moveY);
    lastInputError_.store(GetLastError());
    return MoveResult::Failed;
}
void InputController::ResetResidual() { residualX_ = residualY_ = 0.f; }
void InputController::ResetRelative() { ResetResidual(); queuedX_ = queuedY_ = 0.f; }
ClickResult InputController::ClickPrimary() {
    if (!Allowed()) return ClickResult::Failed;
    const auto now = std::chrono::steady_clock::now();
    if (now - lastClick_ < std::chrono::milliseconds(75)) return ClickResult::Cooldown;
    INPUT inputs[2]{}; inputs[0].type = inputs[1].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SetLastError(ERROR_SUCCESS);
    if (SendInput(2, inputs, sizeof(INPUT)) == 2) {
        lastInputError_.store(ERROR_SUCCESS);
        lastClick_ = now;
        return ClickResult::Sent;
    }
    lastInputError_.store(GetLastError());
    return ClickResult::Failed;
}
}
