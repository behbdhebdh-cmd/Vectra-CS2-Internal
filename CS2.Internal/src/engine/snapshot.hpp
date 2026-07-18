#pragma once

#include "src/common/types.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace vectra {
enum class Bone : uint8_t { Head, Neck, Chest, Pelvis, Count };
enum class SensitivitySource : uint8_t { Unavailable, Pawn, GlobalFallback };
enum class RecoilDataState : uint8_t {
    Unavailable,
    Ready,
    ShotsFiredUnreadable,
    CameraServicesUnavailable,
    ViewPunchUnreadable,
    ViewPunchTickInvalid,
    ViewPunchInvalid,
    SensitivityUnavailable,
    WeaponServicesUnavailable,
    ActiveWeaponUnavailable
};
enum class SkeletonJoint : uint8_t {
    Head, Neck, SpineUpper, SpineLower, Pelvis,
    LeftUpperArm, LeftLowerArm, LeftHand,
    RightUpperArm, RightLowerArm, RightHand,
    LeftUpperLeg, LeftLowerLeg, LeftAnkle,
    RightUpperLeg, RightLowerLeg, RightAnkle,
    Count
};
struct PlayerSnapshot {
    uint32_t index{};
    uint32_t generation{};
    uint32_t pawnEntityIndex{};
    int team{};
    int health{};
    bool alive{};
    bool dormant{};
    bool visible{};
    std::string name;
    std::string modelName;
    Vec3 origin{};
    Vec3 collisionMins{};
    Vec3 collisionMaxs{};
    bool hasCollisionBounds{};
    std::array<Vec3, static_cast<size_t>(Bone::Count)> bones{};
    std::array<bool, static_cast<size_t>(Bone::Count)> hasBone{};
    std::array<Vec3, static_cast<size_t>(SkeletonJoint::Count)> skeletonJoints{};
    std::array<bool, static_cast<size_t>(SkeletonJoint::Count)> hasSkeletonJoint{};
    std::array<int16_t, static_cast<size_t>(SkeletonJoint::Count)> skeletonBoneIndices{};
    std::array<Vec3, static_cast<size_t>(Bone::Count)> targetPoints{};
    std::array<bool, static_cast<size_t>(Bone::Count)> hasTargetPoint{};
};
struct FrameSnapshot {
    bool valid{};
    bool inGame{};
    int localTeam{};
    Vec3 localEye{};
    Vec3 localViewOffset{};
    int32_t crosshairEntityIndex{-1};
    Angles viewAngles{};
    Angles localViewPunch{};
    int32_t viewPunchTick{-1};
    int localShotsFired{};
    uint32_t activeWeaponHandle{0xFFFFFFFF};
    bool localWeaponReloading{};
    float mouseSensitivity{};
    SensitivitySource sensitivitySource{SensitivitySource::Unavailable};
    bool hasRecoilData{};
    RecoilDataState recoilDataState{RecoilDataState::Unavailable};
    std::array<float, 16> viewProjection{};
    std::chrono::steady_clock::time_point capturedAt{};
    std::chrono::steady_clock::time_point publishedAt{};
    float captureDurationMs{};
    float captureHz{};
    uint32_t gameBuild{};
    std::vector<PlayerSnapshot> players;
};
}
