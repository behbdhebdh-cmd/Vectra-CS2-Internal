#include "src/runtime/runtime.hpp"
#include "src/features/aim_assist.hpp"
#include "src/features/esp.hpp"
#include "src/features/triggerbot.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cwctype>
#include <string>

namespace vectra {
Runtime::Runtime() : input_(settings_) {}
Runtime& Runtime::Instance() { static Runtime instance; return instance; }
bool Runtime::IsExpectedProcess() const {
    wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH); std::wstring name(path);
    std::transform(name.begin(), name.end(), name.begin(), towlower);
    return name.ends_with(L"\\cs2.exe");
}
void Runtime::Run(HMODULE module) {
    module_ = module;
    if (!IsExpectedProcess()) return;
    running_.store(true);
    if (!present_.Install([this] { DrawFrame(); })) { running_.store(false); return; }
    worker_ = std::thread(&Runtime::WorkerLoop, this);
    while (running_.load()) {
        if (GetAsyncKeyState(VK_END) & 1) RequestStop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (worker_.joinable()) worker_.join();
    settings_.masterEnabled.store(false);
    settings_.privateMatchAuthorized.store(false);
    present_.Shutdown();
}
void Runtime::RequestStop() { running_.store(false); }
void Runtime::WorkerLoop() {
    auto nextInitialize = std::chrono::steady_clock::now();
    auto nextCapture = nextInitialize;
    while (running_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextInitialize) {
            reader_.Initialize();
            { std::scoped_lock lock(statusMutex_); readerStatus_ = reader_.Status(); }
            nextInitialize = now + std::chrono::seconds(1);
        }
        const auto frame = reader_.Capture();
        { std::scoped_lock lock(snapshotMutex_); snapshot_ = frame; }
        const auto captureCompleted = std::chrono::steady_clock::now();
        { std::scoped_lock lock(statusMutex_); readerStatus_ = reader_.Status(); }
        nextCapture += std::chrono::milliseconds(8);
        if (nextCapture <= captureCompleted) nextCapture = captureCompleted + std::chrono::milliseconds(1);
        std::this_thread::sleep_until(nextCapture);
    }
}
namespace {
void Checkbox(const char* label, std::atomic_bool& value) { bool v = value.load(); if (ImGui::Checkbox(label, &v)) value.store(v); }
void SliderFloat(const char* label, std::atomic<float>& value, float min, float max, const char* format) { float v = value.load(); if (ImGui::SliderFloat(label, &v, min, max, format)) value.store(v); }
const char* AimStateText(AimState state) {
    switch (state) {
    case AimState::Disabled: return "disabled";
    case AimState::MasterDisabled: return "master enable is off";
    case AimState::SessionUnauthorized: return "private-session authorization is off";
    case AimState::WindowUnfocused: return "game window is not focused";
    case AimState::SnapshotStale: return "snapshot is stale";
    case AimState::ActivationReleased: return "waiting for activation key";
    case AimState::NoTarget: return "no eligible target in FOV";
    case AimState::Tracking: return "tracking";
    case AimState::InputFailed: return "SendInput failed";
    }
    return "unknown";
}
const char* TriggerStateText(TriggerState state) {
    switch (state) {
    case TriggerState::Disabled: return "disabled";
    case TriggerState::MasterDisabled: return "master enable is off";
    case TriggerState::SessionUnauthorized: return "private-session authorization is off";
    case TriggerState::WindowUnfocused: return "game window is not focused";
    case TriggerState::SnapshotStale: return "snapshot is stale";
    case TriggerState::NoCrosshairTarget: return "no crosshair target";
    case TriggerState::FriendlyTarget: return "friendly target";
    case TriggerState::IneligibleTarget: return "target is dead or dormant";
    case TriggerState::EligibleEnemy: return "enemy under crosshair";
    case TriggerState::InputFailed: return "SendInput failed";
    }
    return "unknown";
}
const char* RcsStateText(RcsState state) {
    switch (state) {
    case RcsState::Disabled: return "disabled";
    case RcsState::MasterDisabled: return "master enable is off";
    case RcsState::SessionUnauthorized: return "private-session authorization is off";
    case RcsState::WindowUnfocused: return "game window is not focused";
    case RcsState::SnapshotStale: return "recoil snapshot is unavailable";
    case RcsState::InvalidData: return "recoil data rejected";
    case RcsState::WaitingForSpray: return "waiting for first shot";
    case RcsState::Tracking: return "compensating";
    case RcsState::InputFailed: return "SendInput failed";
    }
    return "unknown";
}
const char* SensitivitySourceText(SensitivitySource source) {
    switch (source) {
    case SensitivitySource::Pawn: return "pawn";
    case SensitivitySource::GlobalFallback: return "global fallback";
    case SensitivitySource::Unavailable: return "unavailable";
    }
    return "unknown";
}
const char* RecoilDataStateText(RecoilDataState state) {
    switch (state) {
    case RecoilDataState::Unavailable: return "snapshot unavailable";
    case RecoilDataState::Ready: return "ready";
    case RecoilDataState::ShotsFiredUnreadable: return "shots fired unreadable";
    case RecoilDataState::CameraServicesUnavailable: return "camera services unavailable";
    case RecoilDataState::ViewPunchUnreadable: return "view punch unreadable";
    case RecoilDataState::ViewPunchTickInvalid: return "view punch tick invalid";
    case RecoilDataState::ViewPunchInvalid: return "view punch invalid";
    case RecoilDataState::SensitivityUnavailable: return "sensitivity unavailable";
    case RecoilDataState::WeaponServicesUnavailable: return "weapon services unavailable";
    case RecoilDataState::ActiveWeaponUnavailable: return "active weapon unavailable";
    }
    return "unknown";
}
void DrawRcsDebugOverlay(const RcsStatus& status) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    char line1[192]{}, line2[192]{}, line3[192]{}, line4[192]{};
    std::snprintf(line1, sizeof(line1), "RCS %s | shots %d | tick %d | weapon 0x%08X", RcsStateText(status.state), status.shotsFired, status.punchTick, status.activeWeaponHandle);
    std::snprintf(line2, sizeof(line2), "punch %.4f, %.4f | delta %.4f, %.4f", status.viewPunch.pitch, status.viewPunch.yaw, status.punchDelta.pitch, status.punchDelta.yaw);
    std::snprintf(line3, sizeof(line3), "sens %.4f (%s) | move %.2f, %.2f | input %s", status.sensitivity, SensitivitySourceText(status.sensitivitySource), status.movementX, status.movementY, status.inputEmitted ? "sent" : "none");
    std::snprintf(line4, sizeof(line4), "data %s", RecoilDataStateText(status.recoilDataState));
    const float width = std::max({ImGui::CalcTextSize(line1).x, ImGui::CalcTextSize(line2).x, ImGui::CalcTextSize(line3).x, ImGui::CalcTextSize(line4).x}) + 22.f;
    draw->AddRectFilled({10.f, 40.f}, {10.f + width, 124.f}, IM_COL32(0, 0, 0, 220), 3.f);
    draw->AddText({16.f, 44.f}, IM_COL32(255, 220, 80, 255), line1);
    draw->AddText({16.f, 64.f}, IM_COL32(230, 240, 245, 255), line2);
    draw->AddText({16.f, 84.f}, IM_COL32(230, 240, 245, 255), line3);
    draw->AddText({16.f, 104.f}, IM_COL32(230, 240, 245, 255), line4);
}
void DrawAimFovCircle(const Settings& settings, const Vec2& display) {
    if (!settings.aimEnabled.load() || !settings.drawAimFovCircle.load()) return;
    if (!std::isfinite(display.x) || !std::isfinite(display.y) || display.x <= 0.f || display.y <= 0.f) return;
    const float aimFov = settings.aimFovDegrees.load();
    if (!std::isfinite(aimFov) || aimFov <= 0.f) return;

    constexpr float kDegreesToRadians = 0.01745329252f;
    constexpr float kReferenceHorizontalFov = 90.f;
    const float radius = display.x * .5f * std::tan(aimFov * kDegreesToRadians) /
        std::tan(kReferenceHorizontalFov * kDegreesToRadians * .5f);
    const float maximumRadius = std::min(display.x, display.y) * .48f;
    if (!std::isfinite(radius) || maximumRadius < 8.f) return;

    const float opacity = std::clamp(settings.aimFovCircleOpacity.load(), 0.f, 1.f);
    const ImVec2 center{display.x * .5f, display.y * .5f};
    const float safeRadius = std::clamp(radius, 8.f, maximumRadius);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    draw->AddCircle(center, safeRadius, IM_COL32(0, 0, 0, static_cast<int>(opacity * 180.f)), 96, 3.5f);
    draw->AddCircle(center, safeRadius, IM_COL32(92, 211, 166, static_cast<int>(opacity * 255.f)), 96, 1.25f);
    draw->AddCircleFilled(center, 1.5f, IM_COL32(92, 211, 166, static_cast<int>(opacity * 255.f)), 12);
}
std::string KeyName(int vk) {
    switch (vk) {
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_LBUTTON: return "Left mouse";
    case VK_RBUTTON: return "Right mouse";
    case VK_MBUTTON: return "Middle mouse";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    default: break;
    }
    const UINT scanCode = MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC);
    char name[64]{};
    return GetKeyNameTextA(static_cast<LONG>(scanCode << 16), name, static_cast<int>(sizeof(name))) > 0 ? name : "Unknown";
}
}
void Runtime::DrawMenu() {
    if (!settings_.menuOpen.load()) return;
    ImGui::SetNextWindowSize({380.f, 0.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Vectra", nullptr, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::TextUnformatted("Input actions are disabled until both gates are enabled.");
    Checkbox("Master enable", settings_.masterEnabled);
    Checkbox("I authorize this private-match session", settings_.privateMatchAuthorized);
    Checkbox("Team check (ignore teammates)", settings_.teamCheckEnabled);
    ImGui::Separator();
    if (ImGui::CollapsingHeader("ESP", ImGuiTreeNodeFlags_DefaultOpen)) {
        Checkbox("Enable ESP", settings_.espEnabled);
        Checkbox("Conical hat ESP", settings_.drawConicalHat);
        Checkbox("Skeleton ESP", settings_.drawSkeleton);
        Checkbox("Corner-frame boxes", settings_.espCornerBoxes);
        SliderFloat("Overlay opacity", settings_.espOpacity, .25f, 1.f, "%.2f");
        Checkbox("Visibility colors", settings_.espVisibilityColors);
        Checkbox("Glow ESP", settings_.espGlowEnabled);
        SliderFloat("Glow strength", settings_.espGlowStrength, 0.f, 1.f, "%.2f");
        Checkbox("Player names", settings_.drawNames);
        Checkbox("Health bars", settings_.drawHealth);
    }
    if (ImGui::CollapsingHeader("Aim assist")) {
        Checkbox("Enable aim assist", settings_.aimEnabled);
        Checkbox("Show FOV circle", settings_.drawAimFovCircle);
        SliderFloat("FOV circle opacity", settings_.aimFovCircleOpacity, .1f, 1.f, "%.2f");
        int selectedPoint = std::clamp(settings_.selectedBone.load(), 0, static_cast<int>(Bone::Count) - 1);
        constexpr const char* targetPoints[] = {"Head", "Neck", "Chest", "Pelvis"};
        if (ImGui::Combo("Target point", &selectedPoint, targetPoints, IM_ARRAYSIZE(targetPoints))) settings_.selectedBone.store(selectedPoint);
        SliderFloat("FOV", settings_.aimFovDegrees, .1f, 20.f, "%.1f deg");
        SliderFloat("Smoothing", settings_.smoothing, 1.f, 30.f, "%.1f");
        SliderFloat("Screen gain", settings_.aimScreenGain, .01f, 2.f, "%.2f");
        SliderFloat("Max distance", settings_.maxDistance, 100.f, 10000.f, "%.0f");
        const std::string keyLabel = waitingForAimKey_ ? "Press an aim key...##aimkey" : "Aim key: " + KeyName(settings_.aimActivationVk.load()) + "##aimkey";
        if (ImGui::Button(keyLabel.c_str())) {
            for (int vk = 1; vk < 256; ++vk) GetAsyncKeyState(vk);
            waitingForAimKey_ = true;
        }
        if (waitingForAimKey_) {
            for (int vk = 1; vk < 256; ++vk) {
                if (!(GetAsyncKeyState(vk) & 1)) continue;
                if (vk == VK_ESCAPE) { waitingForAimKey_ = false; break; }
                if (vk == VK_F1 || vk == VK_END || vk == VK_LBUTTON) continue;
                settings_.aimActivationVk.store(vk);
                waitingForAimKey_ = false;
                break;
            }
        }
    }
    if (ImGui::CollapsingHeader("Recoil control")) {
        Checkbox("Enable RCS", settings_.rcsEnabled);
        SliderFloat("RCS strength", settings_.rcsStrength, 0.f, 100.f, "%.0f%%");
        ImGui::TextUnformatted("Active from shot 1 while holding left mouse.");
    }
    if (ImGui::CollapsingHeader("Triggerbot")) { Checkbox("Enable triggerbot", settings_.triggerEnabled); ImGui::TextUnformatted("Always active while enabled and authorized."); }
    std::wstring status;
    { std::scoped_lock lock(statusMutex_); status = readerStatus_; }
    std::shared_ptr<const FrameSnapshot> frame;
    { std::scoped_lock lock(snapshotMutex_); frame = snapshot_; }
    ImGui::Separator();
    ImGui::Text("Runtime: %ls", present_.Status().c_str());
    ImGui::Text("Reader: %ls", status.c_str());
    if (frame) {
        const float ageMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - frame->publishedAt).count();
        ImGui::Text("Capture: %.2f ms | %.1f Hz | age %.1f ms", frame->captureDurationMs, frame->captureHz, ageMs);
        ImGui::Text("Players: %zu | snapshot: %s", frame->players.size(), frame->valid && ageMs < 150.f ? "fresh" : "STALE");
    }
    ImGui::Text("Aim: %s", AimStateText(aimStatus_.state));
    if (aimStatus_.state == AimState::Tracking) ImGui::Text("Target %u | %.2f deg | screen %.1f, %.1f | move %.2f, %.2f%s", aimStatus_.targetIndex, aimStatus_.angularError, aimStatus_.screenError.x, aimStatus_.screenError.y, aimStatus_.movementX, aimStatus_.movementY, aimStatus_.inputEmitted ? "" : " (subpixel/aligned)");
    if (aimStatus_.state == AimState::InputFailed) ImGui::Text("Aim input error: %lu", aimStatus_.inputError);
    ImGui::Text("Trigger: %s", TriggerStateText(triggerStatus_.state));
    if (triggerStatus_.state == TriggerState::EligibleEnemy) ImGui::Text("Crosshair target %u%s", triggerStatus_.targetIndex, triggerStatus_.clickEmitted ? " | clicked" : " | cooldown");
    if (triggerStatus_.state == TriggerState::InputFailed) ImGui::Text("Trigger input error: %lu", triggerStatus_.inputError);
    ImGui::Text("RCS: %s", RcsStateText(rcsStatus_.state));
    ImGui::Text("Shots %d | tick %d | weapon 0x%08X", rcsStatus_.shotsFired, rcsStatus_.punchTick, rcsStatus_.activeWeaponHandle);
    ImGui::Text("Punch %.4f, %.4f | delta %.4f, %.4f", rcsStatus_.viewPunch.pitch, rcsStatus_.viewPunch.yaw, rcsStatus_.punchDelta.pitch, rcsStatus_.punchDelta.yaw);
    ImGui::Text("Sensitivity %.4f (%s)", rcsStatus_.sensitivity, SensitivitySourceText(rcsStatus_.sensitivitySource));
    ImGui::Text("RCS data: %s", RecoilDataStateText(rcsStatus_.recoilDataState));
    if (rcsStatus_.state == RcsState::Tracking) ImGui::Text("RCS move %.2f, %.2f%s", rcsStatus_.movementX, rcsStatus_.movementY, rcsStatus_.inputEmitted ? "" : " (aligned/initializing)");
    if (rcsStatus_.state == RcsState::InputFailed) ImGui::Text("RCS input error: %lu", rcsStatus_.inputError);
    ImGui::TextWrapped("Screenshot: %ls", present_.ScreenshotStatus().c_str());
    ImGui::TextUnformatted("F1: menu | F2: debug screenshot | END: unload runtime");
    ImGui::End();
}
void Runtime::DrawFrame() {
    const bool f1Down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    if (f1Down && !f1WasDown_) settings_.menuOpen.store(!settings_.menuOpen.load());
    f1WasDown_ = f1Down;
    const bool f2Down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    debugAnnotations_ = f2Down && !f2WasDown_;
    if (debugAnnotations_) present_.RequestScreenshot();
    f2WasDown_ = f2Down;
    std::shared_ptr<const FrameSnapshot> frame; { std::scoped_lock lock(snapshotMutex_); frame = snapshot_; }
    const ImVec2 imguiDisplay = ImGui::GetIO().DisplaySize;
    const Vec2 display{imguiDisplay.x, imguiDisplay.y};
    input_.SetGameWindow(present_.Window());
    const bool fresh = frame && frame->valid && std::chrono::steady_clock::now() - frame->capturedAt < std::chrono::milliseconds(150);
    input_.SetSnapshotFresh(fresh);
    input_.BeginFrame();
    if (frame) {
        aimStatus_ = AimAssist::Update(*frame, settings_, input_, display);
        rcsStatus_ = RecoilControl::Update(*frame, settings_, input_);
        triggerStatus_ = Triggerbot::Update(*frame, settings_, input_);
        DrawAimFovCircle(settings_, display);
        Esp::Draw(*frame, settings_, debugAnnotations_);
    } else {
        const FrameSnapshot empty{};
        aimStatus_ = AimAssist::Update(empty, settings_, input_, display);
        rcsStatus_ = RecoilControl::Update(empty, settings_, input_);
        triggerStatus_ = Triggerbot::Update(empty, settings_, input_);
        DrawAimFovCircle(settings_, display);
    }
    const MoveResult movementResult = input_.FlushRelative();
    const bool aimRequestedMovement = aimStatus_.movementX != 0.f || aimStatus_.movementY != 0.f;
    const bool rcsRequestedMovement = rcsStatus_.movementX != 0.f || rcsStatus_.movementY != 0.f;
    if (movementResult == MoveResult::Failed) {
        if (aimRequestedMovement) { aimStatus_.state = AimState::InputFailed; aimStatus_.inputError = input_.LastInputError(); }
        if (rcsRequestedMovement) { rcsStatus_.state = RcsState::InputFailed; rcsStatus_.inputError = input_.LastInputError(); }
    } else if (movementResult == MoveResult::Sent) {
        aimStatus_.inputEmitted = aimRequestedMovement;
        rcsStatus_.inputEmitted = rcsRequestedMovement;
    }
    if (debugAnnotations_) DrawRcsDebugOverlay(rcsStatus_);
    if (aimStatus_.state != AimState::Tracking && rcsStatus_.state != RcsState::Tracking) input_.ResetRelative();
    if (settings_.menuOpen.load() && aimStatus_.hasProjectedTarget) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        const ImVec2 point{aimStatus_.targetScreen.x, aimStatus_.targetScreen.y};
        draw->AddCircle(point, 5.f, IM_COL32(255, 80, 80, 255), 16, 1.5f);
        draw->AddLine({point.x - 7.f, point.y}, {point.x + 7.f, point.y}, IM_COL32(255, 80, 80, 255), 1.f);
        draw->AddLine({point.x, point.y - 7.f}, {point.x, point.y + 7.f}, IM_COL32(255, 80, 80, 255), 1.f);
    }
    DrawMenu();
}
}
