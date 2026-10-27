#pragma once

#include "a3d/editable_mesh.hpp"
#include "a3d/editor_core.hpp"

#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace a3d {

struct ObjectSummary final {
    ObjectId id{};
    std::string name{};
    PrimitiveType primitive{PrimitiveType::Cube};
    bool visible{true};
    bool locked{false};
    bool active{false};
    MeshStats meshStats{};
};

class ModelingScene final {
public:
    [[nodiscard]] ObjectId addPrimitive(PrimitiveType primitive, std::string_view name = {});
    [[nodiscard]] std::optional<ObjectId> duplicateObject(ObjectId sourceId);
    [[nodiscard]] std::optional<ObjectId> separateSelectedFaces(ObjectId sourceId);
    [[nodiscard]] bool removeObject(ObjectId id);
    [[nodiscard]] bool selectObject(std::optional<ObjectId> id) noexcept;
    [[nodiscard]] bool applyTransform(ObjectId id, const Transform& transform);
    [[nodiscard]] bool undoTransform();
    [[nodiscard]] bool redoTransform();

    [[nodiscard]] EditableMesh* mesh(ObjectId id) noexcept;
    [[nodiscard]] const EditableMesh* mesh(ObjectId id) const noexcept;
    [[nodiscard]] SceneObject* object(ObjectId id) noexcept { return scene_.find(id); }
    [[nodiscard]] const SceneObject* object(ObjectId id) const noexcept { return scene_.find(id); }
    [[nodiscard]] std::vector<ObjectId> objectIds() const { return scene_.objectIds(); }
    [[nodiscard]] std::vector<ObjectSummary> summaries() const;
    [[nodiscard]] std::optional<ObjectId> activeObject() const noexcept { return activeObject_; }
    [[nodiscard]] Scene& scene() noexcept { return scene_; }
    [[nodiscard]] const Scene& scene() const noexcept { return scene_; }

private:
    [[nodiscard]] static EditableMesh makePrimitive(PrimitiveType primitive);

    Scene scene_{};
    CommandStack transformHistory_{};
    std::unordered_map<ObjectId, EditableMesh> meshes_{};
    std::optional<ObjectId> activeObject_{};
};

} // namespace a3d
