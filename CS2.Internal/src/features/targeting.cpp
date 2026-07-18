#include "src/features/targeting.hpp"
#include <algorithm>
#include <cmath>

namespace vectra {
Angles Targeting::Normalize(Angles a) { while (a.yaw > 180.f) a.yaw -= 360.f; while (a.yaw < -180.f) a.yaw += 360.f; a.pitch = std::clamp(a.pitch, -89.f, 89.f); return a; }
Angles Targeting::CalculateAngles(const Vec3& from, const Vec3& to) { const Vec3 d = to - from; const float hyp = std::hypot(d.x, d.y); return Normalize({-std::atan2(d.z, hyp) * 57.2957795f, std::atan2(d.y, d.x) * 57.2957795f}); }
float Targeting::AngularDistance(const Angles& from, const Angles& to) { const Angles d = Normalize({to.pitch - from.pitch, to.yaw - from.yaw}); return std::hypot(d.pitch, d.yaw); }
ProjectionResult Targeting::Project(const Vec3& p, const std::array<float, 16>& m, const Vec2& display) {
    if (!Finite(p) || !std::isfinite(display.x) || !std::isfinite(display.y) || display.x <= 0.f || display.y <= 0.f) return {};
    const float clipX = p.x * m[0] + p.y * m[1] + p.z * m[2] + m[3];
    const float clipY = p.x * m[4] + p.y * m[5] + p.z * m[6] + m[7];
    const float clipW = p.x * m[12] + p.y * m[13] + p.z * m[14] + m[15];
    if (!std::isfinite(clipX) || !std::isfinite(clipY) || !std::isfinite(clipW)) return {};
    if (clipW <= 0.001f) return {ProjectionState::BehindCamera, {}};
    const Vec2 screen{display.x * .5f + (clipX / clipW) * display.x * .5f, display.y * .5f - (clipY / clipW) * display.y * .5f};
    return std::isfinite(screen.x) && std::isfinite(screen.y) ? ProjectionResult{ProjectionState::Visible, screen} : ProjectionResult{};
}
std::optional<Target> Targeting::Acquire(const FrameSnapshot& frame, const Settings& settings) {
    if (!frame.valid || !frame.inGame) return std::nullopt;
    std::optional<Target> best; const float maxDistance = settings.maxDistance.load(); const float maxFov = settings.aimFovDegrees.load();
    const Bone preferred = static_cast<Bone>(std::clamp(settings.selectedBone.load(), 0, static_cast<int>(Bone::Count) - 1));
    for (const auto& p : frame.players) {
        if (!p.alive || p.dormant || (settings.teamCheckEnabled.load() && p.team == frame.localTeam) || !p.visible) continue;
        if (Length(p.origin - frame.localEye) > maxDistance) continue;
        Bone selected = preferred;
        if (!p.hasTargetPoint[static_cast<size_t>(selected)]) { selected = Bone::Head; if (!p.hasTargetPoint[static_cast<size_t>(selected)]) continue; }
        const Vec3 point = p.targetPoints[static_cast<size_t>(selected)]; if (!Finite(point)) continue;
        const Angles desired = CalculateAngles(frame.localEye, point); const float fov = AngularDistance(frame.viewAngles, desired);
        if (fov > maxFov || (best && fov >= best->fov)) continue;
        best = Target{&p, selected, point, fov, desired};
    }
    return best;
}
}
