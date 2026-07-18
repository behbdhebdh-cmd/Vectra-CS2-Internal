#include "src/engine/game_reader.hpp"
#include <Windows.h>
#include "offsets/offsets.hpp"
#include "offsets/client_dll.hpp"
#include "offsets/animationsystem_dll.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t kExpectedBuild = 14170;
constexpr size_t kEntityStride = 0x70;
constexpr size_t kEntityBlockOffset = 0x10;
constexpr size_t kEntityBlockSize = 0x200;
constexpr uint32_t kEntityIndexMask = 0x7FFF;

// CModelState stores the world-space bone cache at this offset for build 14170.
// Each entry is a matrix3x4a_t; its first three floats are the joint position.
constexpr size_t kBoneCacheOffsetInModelState = 0x80;
constexpr size_t kBoneCacheStride = 0x20;
constexpr std::array<int, 4> kBones{6, 5, 3, 0};

// Standard CS2 player skeleton (T and CT) for build 14170.  These are model
// joints, not screen-space approximations, so they must never be adjusted after
// projection.
constexpr std::array<int, static_cast<size_t>(vectra::SkeletonJoint::LeftUpperLeg)> kUpperSkeletonBones{
    6, 5, 4, 2, 0,
    8, 9, 10,
    13, 14, 15
};

constexpr size_t kMaximumModelBones = 256;
constexpr size_t kUtlVectorDataOffset = 0x0;
constexpr size_t kUtlVectorSizeOffset = 0x10;

struct BoneCacheEntry {
    vectra::Vec3 position{};
    float scale{};
    std::array<float, 4> rotation{};
};
static_assert(sizeof(BoneCacheEntry) == kBoneCacheStride);

bool IsAccessibleProtect(DWORD protect) {
    const DWORD base = protect & 0xFF;
    return base == PAGE_READONLY || base == PAGE_READWRITE || base == PAGE_WRITECOPY || base == PAGE_EXECUTE_READ || base == PAGE_EXECUTE_READWRITE || base == PAGE_EXECUTE_WRITECOPY;
}
bool ValidCollisionBounds(const vectra::Vec3& mins, const vectra::Vec3& maxs) {
    if (!vectra::Finite(mins) || !vectra::Finite(maxs)) return false;
    const vectra::Vec3 extent{maxs.x - mins.x, maxs.y - mins.y, maxs.z - mins.z};
    return std::abs(mins.x) <= 256.f && std::abs(mins.y) <= 256.f && std::abs(mins.z) <= 256.f && std::abs(maxs.x) <= 256.f && std::abs(maxs.y) <= 256.f && std::abs(maxs.z) <= 256.f && extent.x >= 1.f && extent.x <= 128.f && extent.y >= 1.f && extent.y <= 128.f && extent.z >= 16.f && extent.z <= 128.f;
}
bool ValidViewOffset(const vectra::Vec3& offset) {
    return vectra::Finite(offset) && std::abs(offset.x) <= 16.f && std::abs(offset.y) <= 16.f && offset.z >= 0.f && offset.z <= 128.f;
}
bool BoneInsideBounds(const vectra::Vec3& bone, const vectra::Vec3& origin, const vectra::Vec3& mins, const vectra::Vec3& maxs) {
    constexpr float margin = 16.f;
    return bone.x >= origin.x + mins.x - margin && bone.x <= origin.x + maxs.x + margin &&
           bone.y >= origin.y + mins.y - margin && bone.y <= origin.y + maxs.y + margin &&
           bone.z >= origin.z + mins.z - margin && bone.z <= origin.z + maxs.z + margin;
}
float JointDistance(const vectra::Vec3& a, const vectra::Vec3& b) { return vectra::Length(a - b); }
bool PlausibleLegSegment(const vectra::Vec3& a, const vectra::Vec3& b, float maximum) {
    const float distance = JointDistance(a, b);
    return std::isfinite(distance) && distance >= 1.f && distance <= maximum;
}
}

namespace vectra {
template <typename T>
bool GameReader::Read(uintptr_t address, T& value) const {
    if (!IsReadable(address, sizeof(T))) return false;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
}

bool GameReader::ReadBytes(uintptr_t address, void* destination, size_t size) const {
    if (!destination || !IsReadable(address, size)) return false;
    std::memcpy(destination, reinterpret_cast<const void*>(address), size);
    return true;
}

bool GameReader::IsReadable(uintptr_t address, size_t size) const {
    if (!address || !size || address + size < address) return false;
    for (const auto& region : regionCache_) {
        if (address >= region.begin && address + size <= region.end) return region.readable;
    }
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi))) return false;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    const uintptr_t end = begin + mbi.RegionSize;
    const bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && IsAccessibleProtect(mbi.Protect);
    regionCache_.push_back({begin, end, readable});
    return readable && address >= begin && address + size <= end;
}

bool GameReader::ReadPointer(uintptr_t address, uintptr_t& value) const {
    if (!Read(address, value) || !value || value < 0x10000 || (value & (alignof(void*) - 1)) != 0) return false;
    return IsReadable(value, sizeof(uintptr_t));
}

bool GameReader::ReadString(uintptr_t address, std::string& value, size_t maxLength) const {
    uintptr_t text{};
    if (!ReadPointer(address, text)) return false; // CUtlString starts with a char pointer.
    value.clear(); value.reserve(maxLength);
    for (size_t i = 0; i < maxLength; ++i) {
        char c{}; if (!Read(text + i, c)) return false; if (c == '\0') return !value.empty();
        value.push_back(static_cast<unsigned char>(c) >= 32 ? c : '?');
    }
    return true;
}

bool GameReader::Initialize() {
    const HMODULE client = GetModuleHandleW(L"client.dll");
    const HMODULE engine = GetModuleHandleW(L"engine2.dll");
    if (!client || !engine) {
        ready_ = false;
        status_ = L"Waiting for client.dll and engine2.dll";
        return false;
    }
    clientBase_ = reinterpret_cast<uintptr_t>(client);
    engineBase_ = reinterpret_cast<uintptr_t>(engine);
    uint32_t build{};
    if (!Read(engineBase_ + cs2_dumper::offsets::engine2_dll::dwBuildNumber, build)) { ready_ = false; status_ = L"Unable to read engine build number"; return false; }
    buildNumber_ = build;
    if (build != kExpectedBuild) { ready_ = false; status_ = L"Dump build mismatch; expected 14170"; return false; }
    if (!ReadPointer(clientBase_ + cs2_dumper::offsets::client_dll::dwEntityList, entitySystem_)) { ready_ = false; status_ = L"Entity system pointer is invalid"; return false; }
    ready_ = true;
    status_ = L"Offsets validated (build 14170)";
    return true;
}

std::shared_ptr<const FrameSnapshot> GameReader::Capture() {
    auto frame = std::make_shared<FrameSnapshot>();
    frame->capturedAt = std::chrono::steady_clock::now();
    frame->gameBuild = buildNumber_;
    regionCache_.clear();
    regionCache_.reserve(32);
    frame->players.reserve(64);
    if (!ready_) return FinalizeCapture(std::move(frame));
    if (!Read(clientBase_ + cs2_dumper::offsets::client_dll::dwViewMatrix, frame->viewProjection)) { ready_ = false; status_ = L"View matrix became unreadable"; return FinalizeCapture(std::move(frame)); }
    if (!Read(clientBase_ + cs2_dumper::offsets::client_dll::dwViewAngles, frame->viewAngles)) { ready_ = false; status_ = L"View angles became unreadable"; return FinalizeCapture(std::move(frame)); }
    uintptr_t localPawn{};
    if (!ReadPointer(clientBase_ + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn, localPawn)) return FinalizeCapture(std::move(frame));
    uint8_t localTeam{}; uintptr_t localNode{}; Vec3 localOrigin{}, viewOffset{}; int32_t crosshairIndex{-1};
    if (!Read(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum, localTeam) || !ReadPointer(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode, localNode) || !Read(localNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin, localOrigin) || !Read(localPawn + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset, viewOffset) || !Finite(localOrigin) || !ValidViewOffset(viewOffset)) return FinalizeCapture(std::move(frame));
    Read(localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iIDEntIndex, crosshairIndex);
    frame->localTeam = localTeam; frame->localViewOffset = viewOffset; frame->localEye = {localOrigin.x + viewOffset.x, localOrigin.y + viewOffset.y, localOrigin.z + viewOffset.z}; frame->crosshairEntityIndex = crosshairIndex; frame->inGame = true;
    uintptr_t cameraServices{};
    Angles viewPunch{};
    int32_t viewPunchTick{-1};
    int shotsFired{};
    if (!Read(localPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_iShotsFired, shotsFired)) {
        frame->recoilDataState = RecoilDataState::ShotsFiredUnreadable;
    } else if (!ReadPointer(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pCameraServices, cameraServices)) {
        frame->recoilDataState = RecoilDataState::CameraServicesUnavailable;
    } else if (!Read(cameraServices + cs2_dumper::schemas::client_dll::CPlayer_CameraServices::m_vecCsViewPunchAngle, viewPunch)) {
        frame->recoilDataState = RecoilDataState::ViewPunchUnreadable;
    } else if (!Read(cameraServices + cs2_dumper::schemas::client_dll::CPlayer_CameraServices::m_nCsViewPunchAngleTick, viewPunchTick) || viewPunchTick < 0) {
        frame->recoilDataState = RecoilDataState::ViewPunchTickInvalid;
    } else if (!Finite(viewPunch) || std::abs(viewPunch.pitch) > 90.f || std::abs(viewPunch.yaw) > 90.f) {
        frame->recoilDataState = RecoilDataState::ViewPunchInvalid;
    } else {
        frame->localShotsFired = std::clamp(shotsFired, 0, 1000);
        frame->localViewPunch = viewPunch;
        frame->viewPunchTick = viewPunchTick;
        frame->hasRecoilData = true;
        frame->recoilDataState = RecoilDataState::Ready;
    }
    float sensitivity{};
    if (Read(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_flMouseSensitivity, sensitivity) &&
        std::isfinite(sensitivity) && sensitivity >= .01f && sensitivity <= 20.f) {
        frame->mouseSensitivity = sensitivity;
        frame->sensitivitySource = SensitivitySource::Pawn;
    } else {
        uintptr_t sensitivityAddress{};
        if (ReadPointer(clientBase_ + cs2_dumper::offsets::client_dll::dwSensitivity, sensitivityAddress) &&
            Read(sensitivityAddress + cs2_dumper::offsets::client_dll::dwSensitivity_sensitivity, sensitivity) &&
            std::isfinite(sensitivity) && sensitivity >= .01f && sensitivity <= 20.f) {
            frame->mouseSensitivity = sensitivity;
            frame->sensitivitySource = SensitivitySource::GlobalFallback;
        }
    }
    if (frame->recoilDataState == RecoilDataState::Ready && frame->sensitivitySource == SensitivitySource::Unavailable)
        frame->recoilDataState = RecoilDataState::SensitivityUnavailable;
    uintptr_t weaponServices{};
    uint32_t activeWeaponHandle{0xFFFFFFFF};
    if (!ReadPointer(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices, weaponServices)) {
        if (frame->recoilDataState == RecoilDataState::Ready) frame->recoilDataState = RecoilDataState::WeaponServicesUnavailable;
    } else if (!Read(weaponServices + cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon, activeWeaponHandle)) {
        if (frame->recoilDataState == RecoilDataState::Ready) frame->recoilDataState = RecoilDataState::ActiveWeaponUnavailable;
    } else {
        frame->activeWeaponHandle = activeWeaponHandle;
        const uintptr_t activeWeapon = EntityByHandle(activeWeaponHandle);
        if (activeWeapon) {
            Read(activeWeapon + cs2_dumper::schemas::client_dll::C_CSWeaponBase::m_bInReload, frame->localWeaponReloading);
        } else if (frame->recoilDataState == RecoilDataState::Ready) {
            frame->recoilDataState = RecoilDataState::ActiveWeaponUnavailable;
        }
    }
    for (uint32_t index = 1; index <= 64; ++index) {
        const uintptr_t controller = EntityByIndex(index);
        if (controller) PopulatePlayer(controller, index, *frame);
    }
    frame->valid = true;
    return FinalizeCapture(std::move(frame));
}

std::shared_ptr<const FrameSnapshot> GameReader::FinalizeCapture(std::shared_ptr<FrameSnapshot> frame) {
    const auto completed = std::chrono::steady_clock::now();
    frame->publishedAt = completed;
    frame->captureDurationMs = std::chrono::duration<float, std::milli>(completed - frame->capturedAt).count();
    if (lastCaptureCompleted_.time_since_epoch().count() != 0) {
        const float seconds = std::chrono::duration<float>(completed - lastCaptureCompleted_).count();
        if (seconds > 0.0001f) {
            const float instantaneousHz = 1.0f / seconds;
            captureHz_ = captureHz_ > 0.f ? captureHz_ * 0.8f + instantaneousHz * 0.2f : instantaneousHz;
        }
    }
    lastCaptureCompleted_ = completed;
    frame->captureHz = captureHz_;
    return frame;
}

uintptr_t GameReader::EntityByIndex(uint32_t index) const {
    const uintptr_t blockAddress = entitySystem_ + kEntityBlockOffset + sizeof(uintptr_t) * (index / kEntityBlockSize);
    uintptr_t block{};
    if (!ReadPointer(blockAddress, block)) return 0;
    uintptr_t entity{};
    return ReadPointer(block + kEntityStride * (index % kEntityBlockSize), entity) ? entity : 0;
}

uintptr_t GameReader::EntityByHandle(uint32_t handle) const { return handle == 0xFFFFFFFF ? 0 : EntityByIndex(handle & kEntityIndexMask); }

bool GameReader::ResolveSkeletonLayout(uintptr_t modelState, SkeletonLayout& layout) {
    uintptr_t modelBinding{};
    if (!ReadPointer(modelState + cs2_dumper::schemas::client_dll::CModelState::m_hModel, modelBinding)) return false;
    if (const auto cached = skeletonLayoutCache_.find(modelBinding); cached != skeletonLayoutCache_.end()) {
        layout = cached->second;
        return true;
    }

    layout.indices.fill(-1);
    ReadString(modelState + cs2_dumper::schemas::client_dll::CModelState::m_ModelName, layout.modelName, 160);

    std::array<uintptr_t, 10> candidates{modelBinding};
    size_t candidateCount = 1;
    for (const size_t offset : {size_t{0x0}, size_t{0x8}, size_t{0x10}, size_t{0x18}, size_t{0x20}, size_t{0x28}, size_t{0x30}, size_t{0x38}, size_t{0x40}}) {
        uintptr_t candidate{};
        if (!ReadPointer(modelBinding + offset, candidate)) continue;
        bool duplicate = false;
        for (size_t i = 0; i < candidateCount; ++i) duplicate |= candidates[i] == candidate;
        if (!duplicate && candidateCount < candidates.size()) candidates[candidateCount++] = candidate;
    }

    for (size_t candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex) {
        const uintptr_t modelData = candidates[candidateIndex];
        const uintptr_t skeleton = modelData + cs2_dumper::schemas::animationsystem_dll::PermModelData_t::m_modelSkeleton;
        if (skeleton < modelData) continue;

        uintptr_t namesData{}, parentsData{};
        int namesCount{}, parentsCount{};
        const uintptr_t namesVector = skeleton + cs2_dumper::schemas::animationsystem_dll::ModelSkeletonData_t::m_boneName;
        const uintptr_t parentsVector = skeleton + cs2_dumper::schemas::animationsystem_dll::ModelSkeletonData_t::m_nParent;
        if (!ReadPointer(namesVector + kUtlVectorDataOffset, namesData) ||
            !Read(namesVector + kUtlVectorSizeOffset, namesCount) ||
            !ReadPointer(parentsVector + kUtlVectorDataOffset, parentsData) ||
            !Read(parentsVector + kUtlVectorSizeOffset, parentsCount) ||
            namesCount <= 0 || namesCount > static_cast<int>(kMaximumModelBones) || parentsCount != namesCount ||
            !IsReadable(namesData, static_cast<size_t>(namesCount) * sizeof(uintptr_t)) ||
            !IsReadable(parentsData, static_cast<size_t>(parentsCount) * sizeof(int16_t))) continue;

        SkeletonLayout resolved{};
        resolved.indices.fill(-1);
        resolved.boneCount = static_cast<size_t>(namesCount);
        resolved.modelName = layout.modelName;
        for (int i = 0; i < namesCount; ++i) {
            std::string boneName;
            if (!ReadString(namesData + static_cast<size_t>(i) * sizeof(uintptr_t), boneName, 64)) continue;
            std::transform(boneName.begin(), boneName.end(), boneName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            SkeletonJoint joint{SkeletonJoint::Count};
            if (boneName == "pelvis") joint = SkeletonJoint::Pelvis;
            else if (boneName == "leg_upper_l") joint = SkeletonJoint::LeftUpperLeg;
            else if (boneName == "leg_lower_l") joint = SkeletonJoint::LeftLowerLeg;
            else if (boneName == "ankle_l") joint = SkeletonJoint::LeftAnkle;
            else if (boneName == "leg_upper_r") joint = SkeletonJoint::RightUpperLeg;
            else if (boneName == "leg_lower_r") joint = SkeletonJoint::RightLowerLeg;
            else if (boneName == "ankle_r") joint = SkeletonJoint::RightAnkle;
            if (joint != SkeletonJoint::Count) resolved.indices[static_cast<size_t>(joint)] = static_cast<int16_t>(i);
        }

        const auto indexOf = [&](SkeletonJoint joint) { return resolved.indices[static_cast<size_t>(joint)]; };
        const int16_t pelvis = indexOf(SkeletonJoint::Pelvis);
        const int16_t leftUpper = indexOf(SkeletonJoint::LeftUpperLeg), leftLower = indexOf(SkeletonJoint::LeftLowerLeg), leftAnkle = indexOf(SkeletonJoint::LeftAnkle);
        const int16_t rightUpper = indexOf(SkeletonJoint::RightUpperLeg), rightLower = indexOf(SkeletonJoint::RightLowerLeg), rightAnkle = indexOf(SkeletonJoint::RightAnkle);
        if (pelvis < 0 || leftUpper < 0 || leftLower < 0 || leftAnkle < 0 || rightUpper < 0 || rightLower < 0 || rightAnkle < 0) continue;

        std::array<int16_t, kMaximumModelBones> parents{};
        if (!ReadBytes(parentsData, parents.data(), static_cast<size_t>(parentsCount) * sizeof(int16_t))) continue;
        const bool hierarchyValid = parents[leftUpper] == pelvis && parents[leftLower] == leftUpper && parents[leftAnkle] == leftLower &&
            parents[rightUpper] == pelvis && parents[rightLower] == rightUpper && parents[rightAnkle] == rightLower;
        if (!hierarchyValid) continue;

        skeletonLayoutCache_[modelBinding] = resolved;
        layout = std::move(resolved);
        return true;
    }
    return false;
}

bool GameReader::PopulatePlayer(uintptr_t controller, uint32_t index, FrameSnapshot& frame) {
    uint32_t pawnHandle{};
    if (!Read(controller + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn, pawnHandle)) return false;
    const uintptr_t pawn = EntityByHandle(pawnHandle); if (!pawn) return false;
    int health{}; uint8_t team{}, lifeState{}; uintptr_t node{};
    if (!Read(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth, health) || !Read(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum, team) || !Read(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_lifeState, lifeState) || !ReadPointer(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode, node)) return false;
    bool dormant{}; Vec3 origin{};
    if (!Read(node + cs2_dumper::schemas::client_dll::CGameSceneNode::m_bDormant, dormant) || !Read(node + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin, origin) || !Finite(origin)) return false;
    uintptr_t boneArray{}, collision{};
    const uintptr_t modelState = node + cs2_dumper::schemas::client_dll::CSkeletonInstance::m_modelState;
    if (modelState < node || !ReadPointer(modelState + kBoneCacheOffsetInModelState, boneArray)) return false;
    SkeletonLayout skeletonLayout{};
    const bool hasSkeletonLayout = ResolveSkeletonLayout(modelState, skeletonLayout);
    const size_t boneCount = hasSkeletonLayout ? skeletonLayout.boneCount : 16;
    if (!boneCount || boneCount > kMaximumModelBones || !IsReadable(boneArray, boneCount * sizeof(BoneCacheEntry))) return false;
    std::array<BoneCacheEntry, kMaximumModelBones> boneCache{};
    if (!ReadBytes(boneArray, boneCache.data(), boneCount * sizeof(BoneCacheEntry))) return false;
    PlayerSnapshot player{}; player.index = index; player.generation = pawnHandle; player.pawnEntityIndex = pawnHandle & kEntityIndexMask; player.team = team; player.health = std::clamp(health, 0, 100); player.alive = lifeState == 0 && health > 0; player.dormant = dormant; player.visible = !dormant; player.origin = origin;
    player.skeletonBoneIndices.fill(-1);
    player.modelName = skeletonLayout.modelName;
    if (player.modelName.empty()) ReadString(modelState + cs2_dumper::schemas::client_dll::CModelState::m_ModelName, player.modelName, 160);
    if (ReadPointer(pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pCollision, collision) && Read(collision + cs2_dumper::schemas::client_dll::CCollisionProperty::m_vecMins, player.collisionMins) && Read(collision + cs2_dumper::schemas::client_dll::CCollisionProperty::m_vecMaxs, player.collisionMaxs)) player.hasCollisionBounds = ValidCollisionBounds(player.collisionMins, player.collisionMaxs);
    auto& cachedName = nameCache_[index];
    const bool identityChanged = cachedName.controller != controller || cachedName.generation != pawnHandle;
    const bool refreshDue = frame.capturedAt - cachedName.refreshedAt >= std::chrono::seconds(2);
    if (identityChanged || refreshDue) {
        std::string latestName;
        if (ReadString(controller + cs2_dumper::schemas::client_dll::CCSPlayerController::m_sSanitizedPlayerName, latestName, 64)) cachedName.value = std::move(latestName);
        cachedName.controller = controller;
        cachedName.generation = pawnHandle;
        cachedName.refreshedAt = frame.capturedAt;
    }
    player.name = cachedName.value;
    for (size_t i = 0; i < kBones.size(); ++i) {
        const int boneIndex = kBones[i];
        const Vec3 bone = boneCache[static_cast<size_t>(boneIndex)].position;
        if (Finite(bone)) { player.bones[i] = bone; player.hasBone[i] = true; }
    }
    constexpr size_t kUpperJointCount = static_cast<size_t>(SkeletonJoint::LeftUpperLeg);
    for (size_t i = 0; i < kUpperJointCount; ++i) {
        const int boneIndex = kUpperSkeletonBones[i];
        const Vec3 joint = boneCache[static_cast<size_t>(boneIndex)].position;
        if (Finite(joint) && (!player.hasCollisionBounds || BoneInsideBounds(joint, origin, player.collisionMins, player.collisionMaxs))) {
            player.skeletonJoints[i] = joint;
            player.hasSkeletonJoint[i] = true;
            player.skeletonBoneIndices[i] = static_cast<int16_t>(boneIndex);
        }
    }
    if (hasSkeletonLayout) {
        const auto copyJoint = [&](SkeletonJoint joint) {
            const size_t slot = static_cast<size_t>(joint);
            const int16_t boneIndex = skeletonLayout.indices[slot];
            if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= boneCount) return;
            const Vec3 position = boneCache[static_cast<size_t>(boneIndex)].position;
            if (!Finite(position) || (player.hasCollisionBounds && !BoneInsideBounds(position, origin, player.collisionMins, player.collisionMaxs))) return;
            player.skeletonJoints[slot] = position;
            player.hasSkeletonJoint[slot] = true;
            player.skeletonBoneIndices[slot] = boneIndex;
        };
        for (const SkeletonJoint joint : {SkeletonJoint::Pelvis, SkeletonJoint::LeftUpperLeg, SkeletonJoint::LeftLowerLeg, SkeletonJoint::LeftAnkle,
                 SkeletonJoint::RightUpperLeg, SkeletonJoint::RightLowerLeg, SkeletonJoint::RightAnkle}) copyJoint(joint);
        const auto invalidateLeg = [&](SkeletonJoint upper, SkeletonJoint lower, SkeletonJoint ankle) {
            const size_t u = static_cast<size_t>(upper), l = static_cast<size_t>(lower), a = static_cast<size_t>(ankle), p = static_cast<size_t>(SkeletonJoint::Pelvis);
            if (!player.hasSkeletonJoint[p] || !player.hasSkeletonJoint[u] || !player.hasSkeletonJoint[l] || !player.hasSkeletonJoint[a] ||
                !PlausibleLegSegment(player.skeletonJoints[p], player.skeletonJoints[u], 30.f) ||
                !PlausibleLegSegment(player.skeletonJoints[u], player.skeletonJoints[l], 45.f) ||
                !PlausibleLegSegment(player.skeletonJoints[l], player.skeletonJoints[a], 45.f)) {
                player.hasSkeletonJoint[u] = player.hasSkeletonJoint[l] = player.hasSkeletonJoint[a] = false;
            }
        };
        invalidateLeg(SkeletonJoint::LeftUpperLeg, SkeletonJoint::LeftLowerLeg, SkeletonJoint::LeftAnkle);
        invalidateLeg(SkeletonJoint::RightUpperLeg, SkeletonJoint::RightLowerLeg, SkeletonJoint::RightAnkle);
    }
    constexpr std::array<float, 4> verticalFractions{.94f, .82f, .68f, .45f};
    for (size_t i = 0; i < kBones.size(); ++i) {
        const bool validBone = player.hasBone[i] && (!player.hasCollisionBounds || BoneInsideBounds(player.bones[i], origin, player.collisionMins, player.collisionMaxs));
        if (validBone) {
            player.targetPoints[i] = player.bones[i];
            player.hasTargetPoint[i] = true;
        } else if (player.hasCollisionBounds) {
            player.targetPoints[i] = {origin.x + (player.collisionMins.x + player.collisionMaxs.x) * .5f, origin.y + (player.collisionMins.y + player.collisionMaxs.y) * .5f, origin.z + player.collisionMins.z + (player.collisionMaxs.z - player.collisionMins.z) * verticalFractions[i]};
            player.hasTargetPoint[i] = true;
        }
    }
    frame.players.push_back(std::move(player));
    return true;
}
}
