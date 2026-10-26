#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace a3d {

using ObjectId = std::uint64_t;

struct Vec3 final {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};

    [[nodiscard]] bool nearlyEquals(const Vec3& other, float epsilon = 0.0001F) const noexcept;
};

struct Transform final {
    Vec3 position{};
    Vec3 rotationEuler{};
    Vec3 scale{1.0F, 1.0F, 1.0F};

    [[nodiscard]] bool nearlyEquals(const Transform& other, float epsilon = 0.0001F) const noexcept;
};

enum class PrimitiveType : std::uint8_t {
    Cube,
    Plane,
    UVSphere,
    Cylinder,
    Cone
};

struct SceneObject final {
    ObjectId id{};
    std::string name{"Object"};
    PrimitiveType primitive{PrimitiveType::Cube};
    Transform transform{};
    bool visible{true};
    bool locked{false};
};

class Scene final {
public:
    [[nodiscard]] ObjectId createObject(PrimitiveType primitive = PrimitiveType::Cube,
                                        std::string_view requestedName = {});
    [[nodiscard]] std::optional<ObjectId> duplicateObject(ObjectId sourceId);
    [[nodiscard]] bool removeObject(ObjectId id);
    [[nodiscard]] bool renameObject(ObjectId id, std::string_view name);
    [[nodiscard]] bool setVisible(ObjectId id, bool visible);
    [[nodiscard]] bool setLocked(ObjectId id, bool locked);
    [[nodiscard]] SceneObject* find(ObjectId id) noexcept;
    [[nodiscard]] const SceneObject* find(ObjectId id) const noexcept;
    [[nodiscard]] std::vector<ObjectId> objectIds() const;
    [[nodiscard]] std::size_t size() const noexcept { return objects_.size(); }

private:
    ObjectId nextId_{1};
    std::unordered_map<ObjectId, SceneObject> objects_{};
};

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Scene& scene) = 0;
    virtual void undo(Scene& scene) = 0;
    [[nodiscard]] virtual std::size_t memoryCost() const noexcept = 0;
};

class TransformCommand final : public Command {
public:
    TransformCommand(ObjectId objectId, Transform before, Transform after);
    void execute(Scene& scene) override;
    void undo(Scene& scene) override;
    [[nodiscard]] std::size_t memoryCost() const noexcept override { return sizeof(*this); }

private:
    ObjectId objectId_;
    Transform before_;
    Transform after_;
};

class CommandStack final {
public:
    explicit CommandStack(std::size_t memoryBudgetBytes = 8U * 1024U * 1024U);

    void execute(std::unique_ptr<Command> command, Scene& scene);
    [[nodiscard]] bool undo(Scene& scene);
    [[nodiscard]] bool redo(Scene& scene);
    void clear() noexcept;

    [[nodiscard]] bool canUndo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return cursor_ < commands_.size(); }
    [[nodiscard]] std::size_t memoryUsed() const noexcept { return memoryUsed_; }

private:
    void trimToBudget();

    std::vector<std::unique_ptr<Command>> commands_{};
    std::size_t cursor_{0};
    std::size_t memoryUsed_{0};
    std::size_t memoryBudgetBytes_;
};

class Editor final {
public:
    Editor();

    [[nodiscard]] Scene& scene() noexcept { return scene_; }
    [[nodiscard]] const Scene& scene() const noexcept { return scene_; }
    [[nodiscard]] CommandStack& history() noexcept { return history_; }

    void select(std::optional<ObjectId> id) noexcept;
    [[nodiscard]] std::optional<ObjectId> selection() const noexcept { return selectedObject_; }
    [[nodiscard]] bool applyTransform(ObjectId id, const Transform& transform);

private:
    Scene scene_{};
    CommandStack history_{};
    std::optional<ObjectId> selectedObject_{};
};

} // namespace a3d
