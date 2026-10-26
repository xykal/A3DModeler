#include "a3d/editor_core.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace a3d {
namespace {
bool close(float a, float b, float epsilon) noexcept { return std::fabs(a - b) <= epsilon; }
}

bool Vec3::nearlyEquals(const Vec3& other, float epsilon) const noexcept {
    return close(x, other.x, epsilon) && close(y, other.y, epsilon) && close(z, other.z, epsilon);
}

bool Transform::nearlyEquals(const Transform& other, float epsilon) const noexcept {
    return position.nearlyEquals(other.position, epsilon) &&
           rotationEuler.nearlyEquals(other.rotationEuler, epsilon) &&
           scale.nearlyEquals(other.scale, epsilon);
}

ObjectId Scene::createObject(PrimitiveType primitive, std::string_view requestedName) {
    const ObjectId id = nextId_++;
    std::string base = requestedName.empty() ? "Object" : std::string(requestedName);
    std::string unique = base;
    int suffix = 1;
    const auto nameExists = [this](const std::string& candidate) {
        return std::any_of(objects_.begin(), objects_.end(), [&](const auto& pair) {
            return pair.second.name == candidate;
        });
    };
    while (nameExists(unique)) unique = base + "." + std::to_string(suffix++);
    objects_.emplace(id, SceneObject{id, std::move(unique), primitive, {}, true, false});
    return id;
}

std::optional<ObjectId> Scene::duplicateObject(ObjectId sourceId) {
    const auto* source = find(sourceId);
    if (!source) return std::nullopt;
    const SceneObject snapshot = *source;
    const ObjectId copyId = createObject(snapshot.primitive, snapshot.name);
    auto* copy = find(copyId);
    copy->transform = snapshot.transform;
    copy->visible = snapshot.visible;
    copy->locked = false;
    return copyId;
}

bool Scene::removeObject(ObjectId id) { return objects_.erase(id) != 0; }

bool Scene::renameObject(ObjectId id, std::string_view name) {
    auto* object = find(id);
    if (!object || name.empty()) return false;
    const bool duplicate = std::any_of(objects_.begin(), objects_.end(), [&](const auto& pair) {
        return pair.first != id && pair.second.name == name;
    });
    if (duplicate) return false;
    object->name = name;
    return true;
}

bool Scene::setVisible(ObjectId id, bool visible) {
    auto* object = find(id);
    if (!object) return false;
    object->visible = visible;
    return true;
}

bool Scene::setLocked(ObjectId id, bool locked) {
    auto* object = find(id);
    if (!object) return false;
    object->locked = locked;
    return true;
}

std::vector<ObjectId> Scene::objectIds() const {
    std::vector<ObjectId> ids;
    ids.reserve(objects_.size());
    for (const auto& [id, object] : objects_) {
        (void)object;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

SceneObject* Scene::find(ObjectId id) noexcept {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : &found->second;
}

const SceneObject* Scene::find(ObjectId id) const noexcept {
    const auto found = objects_.find(id);
    return found == objects_.end() ? nullptr : &found->second;
}

TransformCommand::TransformCommand(ObjectId objectId, Transform before, Transform after)
    : objectId_(objectId), before_(before), after_(after) {}

void TransformCommand::execute(Scene& scene) {
    if (auto* object = scene.find(objectId_)) object->transform = after_;
}

void TransformCommand::undo(Scene& scene) {
    if (auto* object = scene.find(objectId_)) object->transform = before_;
}

CommandStack::CommandStack(std::size_t memoryBudgetBytes)
    : memoryBudgetBytes_(std::max<std::size_t>(memoryBudgetBytes, 1U)) {}

void CommandStack::execute(std::unique_ptr<Command> command, Scene& scene) {
    if (!command) return;
    while (commands_.size() > cursor_) {
        memoryUsed_ -= commands_.back()->memoryCost();
        commands_.pop_back();
    }
    command->execute(scene);
    memoryUsed_ += command->memoryCost();
    commands_.push_back(std::move(command));
    cursor_ = commands_.size();
    trimToBudget();
}

bool CommandStack::undo(Scene& scene) {
    if (!canUndo()) return false;
    commands_[--cursor_]->undo(scene);
    return true;
}

bool CommandStack::redo(Scene& scene) {
    if (!canRedo()) return false;
    commands_[cursor_++]->execute(scene);
    return true;
}

void CommandStack::clear() noexcept {
    commands_.clear(); cursor_ = 0; memoryUsed_ = 0;
}

void CommandStack::trimToBudget() {
    while (memoryUsed_ > memoryBudgetBytes_ && commands_.size() > 1) {
        memoryUsed_ -= commands_.front()->memoryCost();
        commands_.erase(commands_.begin());
        if (cursor_ > 0) --cursor_;
    }
}

Editor::Editor() : history_(8U * 1024U * 1024U) {}

void Editor::select(std::optional<ObjectId> id) noexcept {
    selectedObject_ = id && scene_.find(*id) ? id : std::nullopt;
}

bool Editor::applyTransform(ObjectId id, const Transform& transform) {
    const auto* object = scene_.find(id);
    if (!object || object->locked || object->transform.nearlyEquals(transform)) return false;
    history_.execute(std::make_unique<TransformCommand>(id, object->transform, transform), scene_);
    return true;
}

} // namespace a3d
