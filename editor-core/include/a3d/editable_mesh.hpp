#pragma once

#include "a3d/editor_core.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace a3d {

using VertexId = std::uint32_t;
using FaceId = std::uint32_t;

enum class SelectionMode : std::uint8_t {
    Vertex,
    Edge,
    Face
};

struct MeshVertex final {
    Vec3 position{};
    bool selected{false};
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
    [[nodiscard]] bool flipFace(FaceId face);
    [[nodiscard]] std::optional<VertexId> mergeSelectedVertices();
    [[nodiscard]] std::optional<FaceId> fillSelectedVertices();
    [[nodiscard]] std::vector<FaceId> duplicateSelectedFaces(Vec3 offset);
    [[nodiscard]] std::vector<FaceId> triangulateSelectedFaces();
    [[nodiscard]] std::optional<FaceId> extrudeFace(FaceId face, float distance);
    [[nodiscard]] std::optional<FaceId> insetFace(FaceId face, float amount);
    [[nodiscard]] std::optional<FaceId> bevelFace(FaceId face, float insetAmount, float depth);
    [[nodiscard]] bool setVertexSelected(VertexId vertex, bool selected);
    [[nodiscard]] bool setEdgeSelected(VertexId first, VertexId second, bool selected);
    [[nodiscard]] bool setFaceSelected(FaceId face, bool selected);
    [[nodiscard]] bool isEdgeSelected(VertexId first, VertexId second) const noexcept;
    [[nodiscard]] std::optional<Vec3> selectionCenter(SelectionMode mode) const noexcept;
    [[nodiscard]] bool translateSelected(SelectionMode mode, Vec3 delta) noexcept;
    [[nodiscard]] bool rotateSelected(SelectionMode mode, int axis, float radians) noexcept;
    [[nodiscard]] bool scaleSelected(SelectionMode mode, Vec3 factors) noexcept;
    void clearSelection() noexcept;
    void selectAll(SelectionMode mode);
    void invertSelection(SelectionMode mode);
    void recalculateNormals() noexcept;

    [[nodiscard]] std::size_t selectedVertexCount() const noexcept;
    [[nodiscard]] std::size_t selectedEdgeCount() const noexcept { return selectedEdges_.size(); }
    [[nodiscard]] std::size_t selectedFaceCount() const noexcept;
    [[nodiscard]] const std::vector<MeshVertex>& vertices() const noexcept { return vertices_; }
    [[nodiscard]] const std::vector<MeshFace>& faces() const noexcept { return faces_; }
    [[nodiscard]] const std::set<std::pair<VertexId, VertexId>>& selectedEdges() const noexcept { return selectedEdges_; }
    [[nodiscard]] MeshStats stats() const;
    [[nodiscard]] bool validate() const noexcept;

private:
    [[nodiscard]] std::set<std::pair<VertexId, VertexId>> topologyEdges() const;
    [[nodiscard]] std::set<VertexId> selectedVertexIds(SelectionMode mode) const;
    [[nodiscard]] static std::pair<VertexId, VertexId> orderedEdge(VertexId first, VertexId second) noexcept;

    std::vector<MeshVertex> vertices_{};
    std::vector<MeshFace> faces_{};
    std::set<std::pair<VertexId, VertexId>> selectedEdges_{};
};

class MeshHistory final {
public:
    explicit MeshHistory(std::size_t memoryBudgetBytes = 32U * 1024U * 1024U);

    [[nodiscard]] bool commit(EditableMesh before, EditableMesh after);
    [[nodiscard]] bool undo(EditableMesh& mesh);
    [[nodiscard]] bool redo(EditableMesh& mesh);
    void clear() noexcept;

    [[nodiscard]] bool canUndo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return cursor_ < entries_.size(); }
    [[nodiscard]] std::size_t memoryUsed() const noexcept { return memoryUsed_; }

private:
    struct Entry final {
        EditableMesh before;
        EditableMesh after;
        std::size_t bytes{};
    };

    [[nodiscard]] static std::size_t estimateBytes(const EditableMesh& mesh) noexcept;
    void trimToBudget();

    std::vector<Entry> entries_{};
    std::size_t cursor_{0};
    std::size_t memoryUsed_{0};
    std::size_t memoryBudgetBytes_;
};

} // namespace a3d
