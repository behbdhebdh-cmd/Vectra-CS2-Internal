#pragma once

#include "src/config/settings.hpp"
#include "src/engine/snapshot.hpp"
#include <optional>

namespace vectra {
enum class ProjectionState { Visible, BehindCamera, Invalid };
struct ProjectionResult { ProjectionState state{ProjectionState::Invalid}; Vec2 screen{}; };
struct Target { const PlayerSnapshot* player{}; Bone bone{Bone::Head}; Vec3 position{}; float fov{}; Angles desired{}; };
class Targeting {
public:
    static std::optional<Target> Acquire(const FrameSnapshot& frame, const Settings& settings);
    static Angles CalculateAngles(const Vec3& from, const Vec3& to);
    static float AngularDistance(const Angles& from, const Angles& to);
    static Angles Normalize(Angles angles);
    static ProjectionResult Project(const Vec3& point, const std::array<float, 16>& matrix, const Vec2& displaySize);
};
}
