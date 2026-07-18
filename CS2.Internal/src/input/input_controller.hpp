#pragma once

#include "src/config/settings.hpp"
#include <Windows.h>
#include <chrono>

namespace vectra {
enum class MoveResult { Accumulating, Sent, Failed };
enum class ClickResult { Cooldown, Sent, Failed };
class InputController {
public:
    explicit InputController(Settings& settings) : settings_(settings) {}
    void SetGameWindow(HWND window) { gameWindow_ = window; }
    void SetSnapshotFresh(bool fresh) { snapshotFresh_.store(fresh); }
    bool Allowed() const;
    bool GameFocused() const;
    bool SnapshotFresh() const { return snapshotFresh_.load(); }
    void BeginFrame();
    void QueueRelative(float dx, float dy);
    MoveResult FlushRelative();
    MoveResult MoveRelative(float dx, float dy);
    void ResetResidual();
    void ResetRelative();
    DWORD LastInputError() const { return lastInputError_.load(); }
    ClickResult ClickPrimary();
private:
    Settings& settings_;
    HWND gameWindow_{};
    std::atomic_bool snapshotFresh_{};
    std::atomic<DWORD> lastInputError_{};
    float residualX_{};
    float residualY_{};
    float queuedX_{};
    float queuedY_{};
    std::chrono::steady_clock::time_point lastClick_{};
};
}
