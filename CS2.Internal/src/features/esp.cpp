#include "src/features/esp.hpp"
#include "src/features/targeting.hpp"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

namespace vectra {
namespace {
constexpr float kOutlineThickness = 3.5f;
constexpr float kFrameThickness = 1.5f;

ImU32 WithOpacity(int red, int green, int blue, float opacity) {
    return IM_COL32(red, green, blue, static_cast<int>(std::clamp(opacity, 0.f, 1.f) * 255.f));
}

ImU32 AccentColor(const PlayerSnapshot& player, const Settings& settings, float opacity) {
    if (!settings.espVisibilityColors.load()) return WithOpacity(235, 240, 242, opacity);
    return player.visible ? WithOpacity(92, 211, 166, opacity) : WithOpacity(221, 166, 83, opacity);
}

std::string TruncatedName(std::string_view name) {
    constexpr size_t kMaxCharacters = 18;
    if (name.size() <= kMaxCharacters) return std::string(name);
    return std::string(name.substr(0, kMaxCharacters - 3)) + "...";
}

void AddCornerFrame(ImDrawList* draw, const ImVec2& min, const ImVec2& max, ImU32 accent, ImU32 glowOuter, ImU32 glowInner, bool glowEnabled, float opacity) {
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    const float corner = std::clamp(std::min(width, height) * .26f, 7.f, 22.f);
    const ImU32 outline = WithOpacity(0, 0, 0, opacity * .72f);
    const std::array<std::pair<ImVec2, ImVec2>, 8> segments{{
        {{min.x, min.y}, {min.x + corner, min.y}}, {{min.x, min.y}, {min.x, min.y + corner}},
        {{max.x, min.y}, {max.x - corner, min.y}}, {{max.x, min.y}, {max.x, min.y + corner}},
        {{min.x, max.y}, {min.x + corner, max.y}}, {{min.x, max.y}, {min.x, max.y - corner}},
        {{max.x, max.y}, {max.x - corner, max.y}}, {{max.x, max.y}, {max.x, max.y - corner}}
    }};
    if (glowEnabled) {
        for (const auto& [start, end] : segments) draw->AddLine(start, end, glowOuter, 9.f);
        for (const auto& [start, end] : segments) draw->AddLine(start, end, glowInner, 5.f);
    }
    for (const auto& [start, end] : segments) draw->AddLine(start, end, outline, kOutlineThickness);
    for (const auto& [start, end] : segments) draw->AddLine(start, end, accent, kFrameThickness);
}

void AddConicalHat(ImDrawList* draw, const ImVec2& boxMin, float boxWidth, float boxHeight, float opacity) {
    const float hatWidth = std::clamp(boxWidth * .76f, 16.f, 58.f);
    const float hatHeight = hatWidth * .52f;
    const float clearance = std::clamp(boxHeight * .035f, 6.f, 12.f);
    const float centerX = boxMin.x + boxWidth * .5f;
    const float brimY = boxMin.y - clearance;
    const ImVec2 peak{centerX, brimY - hatHeight};
    const ImVec2 left{centerX - hatWidth * .5f, brimY};
    const ImVec2 right{centerX + hatWidth * .5f, brimY};
    const ImU32 outline = WithOpacity(62, 37, 17, opacity * .9f);
    const ImU32 straw = WithOpacity(218, 164, 68, opacity);
    const ImU32 highlight = WithOpacity(246, 207, 116, opacity * .78f);
    const auto line = [&](const ImVec2& from, const ImVec2& to, ImU32 color) {
        draw->AddLine(from, to, outline, 2.f);
        draw->AddLine(from, to, color, 1.f);
    };
    line(peak, left, straw);
    line(peak, right, straw);
    line({left.x - 2.f, brimY}, {right.x + 2.f, brimY}, WithOpacity(171, 111, 39, opacity));
    line(peak, {centerX - hatWidth * .20f, brimY}, straw);
    line(peak, {centerX + hatWidth * .20f, brimY}, straw);
    draw->AddLine({peak.x - hatWidth * .12f, peak.y + hatHeight * .48f}, {peak.x + hatWidth * .11f, peak.y + hatHeight * .70f}, highlight, 1.f);
}

void AddSkeleton(ImDrawList* draw, const PlayerSnapshot& player, const FrameSnapshot& frame, const Vec2& display, ImU32 accent, float opacity, bool debugAnnotations) {
    constexpr size_t kJointCount = static_cast<size_t>(SkeletonJoint::Count);
    constexpr std::array<std::pair<SkeletonJoint, SkeletonJoint>, 16> kConnections{{
        {SkeletonJoint::Head, SkeletonJoint::Neck}, {SkeletonJoint::Neck, SkeletonJoint::SpineUpper},
        {SkeletonJoint::SpineUpper, SkeletonJoint::SpineLower}, {SkeletonJoint::SpineLower, SkeletonJoint::Pelvis},
        {SkeletonJoint::Neck, SkeletonJoint::LeftUpperArm}, {SkeletonJoint::LeftUpperArm, SkeletonJoint::LeftLowerArm}, {SkeletonJoint::LeftLowerArm, SkeletonJoint::LeftHand},
        {SkeletonJoint::Neck, SkeletonJoint::RightUpperArm}, {SkeletonJoint::RightUpperArm, SkeletonJoint::RightLowerArm}, {SkeletonJoint::RightLowerArm, SkeletonJoint::RightHand},
        {SkeletonJoint::Pelvis, SkeletonJoint::LeftUpperLeg}, {SkeletonJoint::LeftUpperLeg, SkeletonJoint::LeftLowerLeg}, {SkeletonJoint::LeftLowerLeg, SkeletonJoint::LeftAnkle},
        {SkeletonJoint::Pelvis, SkeletonJoint::RightUpperLeg}, {SkeletonJoint::RightUpperLeg, SkeletonJoint::RightLowerLeg}, {SkeletonJoint::RightLowerLeg, SkeletonJoint::RightAnkle}
    }};
    std::array<ImVec2, kJointCount> points{};
    std::array<bool, kJointCount> visible{};
    for (size_t i = 0; i < kJointCount; ++i) {
        if (!player.hasSkeletonJoint[i]) continue;
        const ProjectionResult projected = Targeting::Project(player.skeletonJoints[i], frame.viewProjection, display);
        if (projected.state == ProjectionState::Visible) { points[i] = {projected.screen.x, projected.screen.y}; visible[i] = true; }
    }
    const ImU32 outline = WithOpacity(0, 0, 0, opacity * .72f);
    for (const auto& [from, to] : kConnections) {
        const size_t fromIndex = static_cast<size_t>(from);
        const size_t toIndex = static_cast<size_t>(to);
        if (!visible[fromIndex] || !visible[toIndex]) continue;
        draw->AddLine(points[fromIndex], points[toIndex], outline, 3.f);
        draw->AddLine(points[fromIndex], points[toIndex], accent, 1.25f);
    }
    if (debugAnnotations) {
        constexpr std::array<const char*, kJointCount> kJointNames{
            "head", "neck", "spine_up", "spine_low", "pelvis", "upper_arm_l", "lower_arm_l", "hand_l",
            "upper_arm_r", "lower_arm_r", "hand_r", "leg_upper_l", "leg_lower_l", "ankle_l",
            "leg_upper_r", "leg_lower_r", "ankle_r"};
        for (size_t i = 0; i < kJointCount; ++i) {
            if (!visible[i]) continue;
            const bool leg = i >= static_cast<size_t>(SkeletonJoint::LeftUpperLeg);
            const ImU32 marker = leg ? IM_COL32(255, 80, 80, 255) : IM_COL32(100, 210, 255, 230);
            draw->AddCircleFilled(points[i], leg ? 3.5f : 2.5f, marker, 12);
            char label[64]{};
            std::snprintf(label, sizeof(label), "%s [%d]", kJointNames[i], static_cast<int>(player.skeletonBoneIndices[i]));
            draw->AddText({points[i].x + 5.f, points[i].y - 7.f}, IM_COL32(255, 255, 255, 255), label);
        }
        const size_t pelvis = static_cast<size_t>(SkeletonJoint::Pelvis);
        if (visible[pelvis]) {
            if (!player.modelName.empty()) draw->AddText({points[pelvis].x + 5.f, points[pelvis].y + 10.f}, IM_COL32(255, 220, 80, 255), player.modelName.c_str());
            bool legsResolved = true;
            for (size_t i = static_cast<size_t>(SkeletonJoint::LeftUpperLeg); i < kJointCount; ++i) legsResolved &= player.hasSkeletonJoint[i];
            if (!legsResolved) draw->AddText({points[pelvis].x + 5.f, points[pelvis].y + 24.f}, IM_COL32(255, 80, 80, 255), "LEG SKELETON UNRESOLVED/INVALID");
        }
    }
}

void AddHealthBar(ImDrawList* draw, const PlayerSnapshot& player, const ImVec2& min, const ImVec2& max, float opacity) {
    const float height = max.y - min.y;
    const float ratio = std::clamp(static_cast<float>(player.health) / 100.f, 0.f, 1.f);
    const float left = min.x - 6.f;
    draw->AddRectFilled({left - 1.f, min.y - 1.f}, {left + 4.f, max.y + 1.f}, WithOpacity(0, 0, 0, opacity * .72f), 1.f);
    draw->AddRectFilled({left, min.y}, {left + 3.f, max.y}, WithOpacity(20, 28, 30, opacity), 1.f);
    const ImU32 healthColor = WithOpacity(static_cast<int>(235.f * (1.f - ratio)), static_cast<int>(220.f * ratio), 65, opacity);
    draw->AddRectFilled({left, max.y - height * ratio}, {left + 3.f, max.y}, healthColor, 1.f);
    if (player.health < 100) {
        char healthText[8]{};
        std::snprintf(healthText, sizeof(healthText), "%d", std::max(player.health, 0));
        const ImVec2 textSize = ImGui::CalcTextSize(healthText);
        draw->AddText({left - textSize.x - 3.f, max.y - height * ratio - textSize.y * .5f}, WithOpacity(238, 242, 242, opacity), healthText);
    }
}

void AddNameplate(ImDrawList* draw, const PlayerSnapshot& player, const ImVec2& min, float opacity) {
    const std::string name = TruncatedName(player.name);
    const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
    const ImVec2 textPosition{min.x - textSize.x * .5f + 0.f, min.y - textSize.y - 7.f};
    const ImVec2 plateMin{textPosition.x - 5.f, textPosition.y - 2.f};
    const ImVec2 plateMax{textPosition.x + textSize.x + 5.f, textPosition.y + textSize.y + 2.f};
    draw->AddRectFilled(plateMin, plateMax, WithOpacity(7, 12, 14, opacity * .78f), 2.f);
    draw->AddText(textPosition, WithOpacity(242, 246, 246, opacity), name.c_str());
}
} // namespace

void Esp::Draw(const FrameSnapshot& frame, const Settings& settings, bool debugAnnotations) {
    if ((!settings.espEnabled.load() && !debugAnnotations) || !frame.valid) return;
    if (std::chrono::steady_clock::now() - frame.capturedAt > std::chrono::milliseconds(150)) return;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 imguiDisplay = ImGui::GetIO().DisplaySize;
    const Vec2 display{imguiDisplay.x, imguiDisplay.y};
    for (const auto& p : frame.players) {
        if (!p.alive || p.dormant || (settings.teamCheckEnabled.load() && p.team == frame.localTeam) || !p.hasCollisionBounds) continue;
        float minX = std::numeric_limits<float>::max(), minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest(), maxY = std::numeric_limits<float>::lowest();
        bool validBox = true;
        for (int corner = 0; corner < 8; ++corner) {
            const Vec3 point{
                p.origin.x + ((corner & 1) ? p.collisionMaxs.x : p.collisionMins.x),
                p.origin.y + ((corner & 2) ? p.collisionMaxs.y : p.collisionMins.y),
                p.origin.z + ((corner & 4) ? p.collisionMaxs.z : p.collisionMins.z)};
            const ProjectionResult projected = Targeting::Project(point, frame.viewProjection, display);
            if (projected.state != ProjectionState::Visible) { validBox = false; break; }
            minX = std::min(minX, projected.screen.x); minY = std::min(minY, projected.screen.y);
            maxX = std::max(maxX, projected.screen.x); maxY = std::max(maxY, projected.screen.y);
        }
        const float width = maxX - minX, height = maxY - minY;
        if (!validBox || width < 2.f || height < 8.f || width > display.x * .95f || height > display.y * 1.5f) continue;
        const ImVec2 min{minX, minY}; const ImVec2 max{maxX, maxY};
        const float opacity = settings.espOpacity.load();
        const ImU32 accent = AccentColor(p, settings, opacity);
        const float glowStrength = std::clamp(settings.espGlowStrength.load(), 0.f, 1.f);
        const bool glowEnabled = settings.espGlowEnabled.load() && glowStrength > 0.f;
        const ImU32 glowOuter = AccentColor(p, settings, opacity * glowStrength * .18f);
        const ImU32 glowInner = AccentColor(p, settings, opacity * glowStrength * .36f);
        if (settings.espCornerBoxes.load()) AddCornerFrame(draw, min, max, accent, glowOuter, glowInner, glowEnabled, opacity);
        else {
            if (glowEnabled) {
                draw->AddRect(min, max, glowOuter, 0.f, 0, 9.f);
                draw->AddRect(min, max, glowInner, 0.f, 0, 5.f);
            }
            draw->AddRect(min, max, WithOpacity(0, 0, 0, opacity * .72f), 0.f, 0, kOutlineThickness);
            draw->AddRect(min, max, accent, 0.f, 0, kFrameThickness);
        }
        if (settings.drawSkeleton.load() || debugAnnotations) AddSkeleton(draw, p, frame, display, accent, opacity, debugAnnotations);
        if (settings.drawConicalHat.load()) AddConicalHat(draw, min, width, height, opacity);
        if (settings.drawHealth.load()) AddHealthBar(draw, p, min, max, opacity);
        if (settings.drawNames.load() && !p.name.empty()) AddNameplate(draw, p, {min.x + width * .5f, min.y}, opacity);
    }
    if (debugAnnotations) {
        const float ageMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - frame.publishedAt).count();
        char diagnostic[128]{};
        std::snprintf(diagnostic, sizeof(diagnostic), "VECTRA BONE DEBUG | build %u | snapshot %.1f ms | players %zu", frame.gameBuild, ageMs, frame.players.size());
        draw->AddRectFilled({10.f, 10.f}, {ImGui::CalcTextSize(diagnostic).x + 24.f, 34.f}, IM_COL32(0, 0, 0, 210), 3.f);
        draw->AddText({16.f, 15.f}, IM_COL32(255, 220, 80, 255), diagnostic);
    }
}
}
