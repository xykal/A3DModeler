#pragma once
#include "a3d/modeling_scene.hpp"
#include <istream>
#include <memory>
#include <ostream>
#include <string>

namespace a3d {
struct ProjectLoadResult final {
    std::unique_ptr<ModelingScene> scene{};
    std::string error{};
    [[nodiscard]] explicit operator bool() const noexcept { return scene != nullptr; }
};

class ProjectIo final {
public:
    [[nodiscard]] static bool save(const ModelingScene& scene, std::ostream& output);
    [[nodiscard]] static ProjectLoadResult load(std::istream& input);
    static constexpr std::uint32_t currentVersion = 1;
};
} // namespace a3d
