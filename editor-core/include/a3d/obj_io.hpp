#pragma once
#include "a3d/modeling_scene.hpp"
#include <optional>
#include <string>
#include <string_view>
namespace a3d {
[[nodiscard]] std::string exportObj(const ModelingScene& scene);
[[nodiscard]] std::optional<EditableMesh> importObj(std::string_view text, std::string* error = nullptr);
} // namespace a3d
