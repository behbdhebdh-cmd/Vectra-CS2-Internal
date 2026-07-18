#pragma once

#include <atomic>

namespace vectra {
struct Settings {
    std::atomic_bool masterEnabled{false};
    std::atomic_bool privateMatchAuthorized{false};
    std::atomic_bool menuOpen{true};
    std::atomic_bool teamCheckEnabled{true};
    std::atomic_bool espEnabled{true};
    std::atomic_bool drawConicalHat{true};
    std::atomic_bool drawSkeleton{true};
    std::atomic_bool espCornerBoxes{true};
    std::atomic_bool drawNames{true};
    std::atomic_bool drawHealth{true};
    std::atomic_bool espVisibilityColors{true};
    std::atomic<float> espOpacity{0.85f};
    std::atomic_bool espGlowEnabled{true};
    std::atomic<float> espGlowStrength{0.60f};
    std::atomic_bool aimEnabled{false};
    std::atomic_bool drawAimFovCircle{true};
    std::atomic<float> aimFovCircleOpacity{0.70f};
    std::atomic_bool triggerEnabled{false};
    std::atomic_bool rcsEnabled{false};
    std::atomic<float> rcsStrength{100.0f};
    std::atomic_bool requireFocus{true};
    std::atomic<float> aimFovDegrees{3.0f};
    std::atomic<float> smoothing{8.0f};
    std::atomic<float> maxDistance{2500.0f};
    std::atomic<float> aimScreenGain{0.35f};
    std::atomic<int> selectedBone{0};
    std::atomic<int> aimActivationVk{0x06}; // XBUTTON2
};
}
