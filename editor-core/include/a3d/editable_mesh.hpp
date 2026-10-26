#pragma once

#include "a3d/editor_core.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace a3d {

using VertexId = std::uint32_t;
using FaceId = std::uint32_t;

struct MeshVertex final {
    Vec3 position{};
};

struct MeshFace final {
    std::vector<VertexId> vertices{};
    Vec3 normal{};
    bool selected{false};
    bool deleted{false};
};

struct MeshStats final {
    std::size_t vertices{};
    std::size_t edges{};
    std::size_t faces{};
    std::size_t triangles{};
};

class EditableMesh final {
public:
    [[nodiscard]] static EditableMesh cube(float size = 2.0F);
    [[nodiscard]] static EditableMesh plane(float size = 2.0F);

    [[nodiscard]] VertexId addVertex(Vec3 position);
    [[nodiscard]] std::optional<FaceId> addFace(std::vector<VertexId> vertices);
    [[nodiscard]] bool deleteFace(FaceId face);
    [[nodiscard]] std::optional<FaceId> extrudeFace(FaceId face, float distance);
    [[nodiscard]] bool setFaceSelected(FaceId face, bool selected);
    void clearSelection() noexcept;
    void recalculateNormals() noexcept;

    [[nodiscard]] const std::vector<MeshVertex>& vertices() const noexcept { return vertices_; }
    [[nodiscard]] const std::vector<MeshFace>& faces() const noexcept { return faces_; }
    [[nodiscard]] MeshStats stats() const;
    [[nodiscard]] bool validate() const noexcept;

private:
    std::vector<MeshVertex> vertices_{};
    std::vector<MeshFace> faces_{};
};

} // namespace a3d
