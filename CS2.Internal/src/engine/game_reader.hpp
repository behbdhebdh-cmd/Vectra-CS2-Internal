#pragma once

#include "src/engine/snapshot.hpp"
#include <memory>
#include <string>
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include <vector>

namespace vectra {
class GameReader {
public:
    bool Initialize();
    std::shared_ptr<const FrameSnapshot> Capture();
    bool Ready() const { return ready_; }
    const std::wstring& Status() const { return status_; }
private:
    bool IsReadable(uintptr_t address, size_t size) const;
    template <typename T> bool Read(uintptr_t address, T& value) const;
    bool ReadBytes(uintptr_t address, void* destination, size_t size) const;
    bool ReadPointer(uintptr_t address, uintptr_t& value) const;
    bool ReadString(uintptr_t address, std::string& value, size_t maxLength) const;
    std::shared_ptr<const FrameSnapshot> FinalizeCapture(std::shared_ptr<FrameSnapshot> frame);
    uintptr_t EntityByHandle(uint32_t handle) const;
    uintptr_t EntityByIndex(uint32_t index) const;
    bool PopulatePlayer(uintptr_t controller, uint32_t index, FrameSnapshot& frame);
    struct SkeletonLayout {
        std::array<int16_t, static_cast<size_t>(SkeletonJoint::Count)> indices{};
        size_t boneCount{};
        std::string modelName;
    };
    bool ResolveSkeletonLayout(uintptr_t modelState, SkeletonLayout& layout);
    struct MemoryRegion {
        uintptr_t begin{};
        uintptr_t end{};
        bool readable{};
    };
    struct CachedPlayerName {
        uintptr_t controller{};
        uint32_t generation{};
        std::string value;
        std::chrono::steady_clock::time_point refreshedAt{};
    };
    bool ready_{};
    uintptr_t clientBase_{};
    uintptr_t engineBase_{};
    uintptr_t entitySystem_{};
    uint32_t buildNumber_{};
    mutable std::vector<MemoryRegion> regionCache_;
    std::unordered_map<uint32_t, CachedPlayerName> nameCache_;
    std::unordered_map<uintptr_t, SkeletonLayout> skeletonLayoutCache_;
    std::chrono::steady_clock::time_point lastCaptureCompleted_{};
    float captureHz_{};
    std::wstring status_{L"Waiting for game modules"};
};
}
