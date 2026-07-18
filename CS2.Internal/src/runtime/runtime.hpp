#pragma once

#include "src/config/settings.hpp"
#include "src/engine/game_reader.hpp"
#include "src/features/aim_assist.hpp"
#include "src/features/triggerbot.hpp"
#include "src/features/recoil_control.hpp"
#include "src/input/input_controller.hpp"
#include "src/render/present_hook.hpp"
#include <Windows.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

namespace vectra {
class Runtime {
public:
    static Runtime& Instance();
    void Run(HMODULE module);
    void RequestStop();
private:
    Runtime();
    bool IsExpectedProcess() const;
    void WorkerLoop();
    void DrawFrame();
    void DrawMenu();
    std::atomic_bool running_{};
    HMODULE module_{};
    Settings settings_;
    GameReader reader_;
    InputController input_;
    PresentHook present_;
    std::thread worker_;
    std::mutex snapshotMutex_;
    std::shared_ptr<const FrameSnapshot> snapshot_;
    std::mutex statusMutex_;
    std::wstring readerStatus_{L"Starting"};
    AimStatus aimStatus_{};
    TriggerStatus triggerStatus_{};
    RcsStatus rcsStatus_{};
    bool waitingForAimKey_{};
    bool f1WasDown_{};
    bool f2WasDown_{};
    bool debugAnnotations_{};
};
}
