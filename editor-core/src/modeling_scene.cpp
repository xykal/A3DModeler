#include "a3d/modeling_scene.hpp"

namespace a3d {

EditableMesh ModelingScene::makePrimitive(PrimitiveType primitive) {
    switch (primitive) {
        case PrimitiveType::Plane: return EditableMesh::plane();
        case PrimitiveType::UVSphere: return EditableMesh::uvSphere();
        case PrimitiveType::Cylinder: return EditableMesh::cylinder();
        case PrimitiveType::Cone: return EditableMesh::cone();
        case PrimitiveType::Cube:
        default: return EditableMesh::cube();
    }
}

ObjectId ModelingScene::addPrimitive(PrimitiveType primitive, std::string_view name) {
    const ObjectId id=scene_.createObject(primitive,name);
    meshes_.emplace(id,makePrimitive(primitive));
    activeObject_=id;
    return id;
}

ObjectId ModelingScene::addMesh(EditableMesh meshValue, std::string_view name) {
    const ObjectId id=scene_.createObject(PrimitiveType::Cube,name.empty()?"Imported Mesh":name);
    meshes_.emplace(id,std::move(meshValue));
    activeObject_=id;
    return id;
}

std::optional<ObjectId> ModelingScene::duplicateObject(ObjectId sourceId) {
    const auto found=meshes_.find(sourceId);
    if (found==meshes_.end()) return std::nullopt;
    const auto duplicate=scene_.duplicateObject(sourceId);
    if (!duplicate) return std::nullopt;
    meshes_.emplace(*duplicate,found->second);
    activeObject_=*duplicate;
    return duplicate;
}

std::optional<ObjectId> ModelingScene::separateSelectedFaces(ObjectId sourceId) {
    auto* sourceMesh=mesh(sourceId);
    const auto* sourceObject=object(sourceId);
    if (!sourceMesh || !sourceObject) return std::nullopt;
    const PrimitiveType primitive=sourceObject->primitive;
    const Transform transform=sourceObject->transform;
    const std::string name=sourceObject->name+" Part";
    auto extracted=sourceMesh->extractSelectedFaces();
    if (!extracted) return std::nullopt;
    const ObjectId id=addPrimitive(primitive,name);
    *mesh(id)=std::move(*extracted);
    object(id)->transform=transform;
    activeObject_=id;
    return id;
}

std::size_t ModelingScene::joinVisibleObjects(ObjectId destinationId) {
    auto* destinationMesh=mesh(destinationId);
    const auto* destinationObject=object(destinationId);
    if (!destinationMesh || !destinationObject || destinationObject->locked) return 0;
    const Transform destinationTransform=destinationObject->transform;
    std::vector<ObjectId> joined;
    for (const auto id:objectIds()) {
        if (id==destinationId) continue;
        const auto* sourceObject=object(id); const auto* sourceMesh=mesh(id);
        if (!sourceObject || !sourceMesh || !sourceObject->visible || sourceObject->locked) continue;
        if (destinationMesh->appendTransformed(*sourceMesh,sourceObject->transform,destinationTransform)) joined.push_back(id);
    }
    for (const auto id:joined) { meshes_.erase(id); (void)scene_.removeObject(id); }
    activeObject_=destinationId;
    return joined.size();
}

bool ModelingScene::removeObject(ObjectId id) {
    if (!scene_.find(id)) return false;
    meshes_.erase(id);
    const bool removed=scene_.removeObject(id);
    if (activeObject_==id) {
        const auto ids=scene_.objectIds();
        activeObject_=ids.empty()?std::nullopt:std::optional<ObjectId>{ids.back()};
    }
    return removed;
}

bool ModelingScene::selectObject(std::optional<ObjectId> id) noexcept {
    if (id && !scene_.find(*id)) return false;
    activeObject_=id;
    return true;
}

bool ModelingScene::applyTransform(ObjectId id, const Transform& transform) {
    const auto* current=scene_.find(id);
    if (!current || current->locked || current->transform.nearlyEquals(transform)) return false;
    transformHistory_.execute(std::make_unique<TransformCommand>(id,current->transform,transform),scene_);
    return true;
}

bool ModelingScene::undoTransform() { return transformHistory_.undo(scene_); }
bool ModelingScene::redoTransform() { return transformHistory_.redo(scene_); }

EditableMesh* ModelingScene::mesh(ObjectId id) noexcept {
    const auto found=meshes_.find(id);
    return found==meshes_.end()?nullptr:&found->second;
}

const EditableMesh* ModelingScene::mesh(ObjectId id) const noexcept {
    const auto found=meshes_.find(id);
    return found==meshes_.end()?nullptr:&found->second;
}

std::vector<ObjectSummary> ModelingScene::summaries() const {
    std::vector<ObjectSummary> result;
    for (const ObjectId id:scene_.objectIds()) {
        const auto* item=scene_.find(id);
        const auto* itemMesh=mesh(id);
        if (!item || !itemMesh) continue;
        result.push_back({id,item->name,item->primitive,item->visible,item->locked,activeObject_==id,itemMesh->stats()});
    }
    return result;
}

} // namespace a3d
