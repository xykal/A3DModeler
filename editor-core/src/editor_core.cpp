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

ObjectId Scene::createObject() {
    const ObjectId id = nextId_++;
    objects_.emplace(id, SceneObject{id, {}});
    return id;
}

bool Scene::removeObject(ObjectId id) { return objects_.erase(id) != 0; }

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
    if (!object || object->transform.nearlyEquals(transform)) return false;
    history_.execute(std::make_unique<TransformCommand>(id, object->transform, transform), scene_);
    return true;
}

} // namespace a3d
