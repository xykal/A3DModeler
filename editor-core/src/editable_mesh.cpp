#include "a3d/editable_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <unordered_map>
#include <utility>

namespace a3d {
namespace {
Vec3 subtract(Vec3 a, Vec3 b) noexcept { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
Vec3 normalized(Vec3 value) noexcept {
    const float length = std::sqrt(value.x*value.x + value.y*value.y + value.z*value.z);
    if (length <= 0.000001F) return {};
    return {value.x/length, value.y/length, value.z/length};
}
Vec3 rotateX(Vec3 p,float a) noexcept { const float c=std::cos(a),s=std::sin(a); return {p.x,p.y*c-p.z*s,p.y*s+p.z*c}; }
Vec3 rotateY(Vec3 p,float a) noexcept { const float c=std::cos(a),s=std::sin(a); return {p.x*c+p.z*s,p.y,-p.x*s+p.z*c}; }
Vec3 rotateZ(Vec3 p,float a) noexcept { const float c=std::cos(a),s=std::sin(a); return {p.x*c-p.y*s,p.x*s+p.y*c,p.z}; }
Vec3 toWorld(Vec3 p,const Transform& t) noexcept {
    p={p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z};
    p=rotateZ(rotateY(rotateX(p,t.rotationEuler.x),t.rotationEuler.y),t.rotationEuler.z);
    return {p.x+t.position.x,p.y+t.position.y,p.z+t.position.z};
}
Vec3 toLocal(Vec3 p,const Transform& t) noexcept {
    p={p.x-t.position.x,p.y-t.position.y,p.z-t.position.z};
    p=rotateX(rotateY(rotateZ(p,-t.rotationEuler.z),-t.rotationEuler.y),-t.rotationEuler.x);
    return {p.x/t.scale.x,p.y/t.scale.y,p.z/t.scale.z};
}
}

EditableMesh EditableMesh::cube(float size) {
    EditableMesh mesh;
    const float h = size * 0.5F;
    for (const Vec3 point : std::vector<Vec3>{{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
                                               {-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}})
        (void)mesh.addVertex(point);
    for (auto face : std::vector<std::vector<VertexId>>{{0,3,2,1},{4,5,6,7},{0,4,7,3},
                                                         {1,2,6,5},{3,7,6,2},{0,1,5,4}})
        (void)mesh.addFace(std::move(face));
    mesh.recalculateNormals();
    return mesh;
}

EditableMesh EditableMesh::plane(float size) {
    EditableMesh mesh;
    const float h = size * 0.5F;
    (void)mesh.addVertex({-h,0,-h}); (void)mesh.addVertex({h,0,-h});
    (void)mesh.addVertex({h,0,h}); (void)mesh.addVertex({-h,0,h});
    (void)mesh.addFace({0,1,2,3});
    mesh.recalculateNormals();
    return mesh;
}

EditableMesh EditableMesh::uvSphere(float radius, int segments, int rings) {
    EditableMesh mesh;
    segments=std::max(3,segments); rings=std::max(2,rings); radius=std::max(0.0001F,radius);
    constexpr float pi=3.14159265358979323846F;
    const VertexId top=mesh.addVertex({0,radius,0});
    for (int ring=1;ring<rings;++ring) {
        const float phi=pi*static_cast<float>(ring)/static_cast<float>(rings);
        const float y=radius*std::cos(phi), ringRadius=radius*std::sin(phi);
        for (int segment=0;segment<segments;++segment) {
            const float theta=2.0F*pi*static_cast<float>(segment)/static_cast<float>(segments);
            (void)mesh.addVertex({ringRadius*std::cos(theta),y,ringRadius*std::sin(theta)});
        }
    }
    const VertexId bottom=mesh.addVertex({0,-radius,0});
    for (int segment=0;segment<segments;++segment) {
        const VertexId a=1U+static_cast<VertexId>(segment);
        const VertexId b=1U+static_cast<VertexId>((segment+1)%segments);
        (void)mesh.addFace({top,b,a});
    }
    for (int ring=0;ring<rings-2;++ring) for (int segment=0;segment<segments;++segment) {
        const VertexId a=1U+static_cast<VertexId>(ring*segments+segment);
        const VertexId b=1U+static_cast<VertexId>(ring*segments+(segment+1)%segments);
        const VertexId c=1U+static_cast<VertexId>((ring+1)*segments+(segment+1)%segments);
        const VertexId d=1U+static_cast<VertexId>((ring+1)*segments+segment);
        (void)mesh.addFace({a,b,c,d});
    }
    const VertexId lastRing=1U+static_cast<VertexId>((rings-2)*segments);
    for (int segment=0;segment<segments;++segment) {
        const VertexId a=lastRing+static_cast<VertexId>(segment);
        const VertexId b=lastRing+static_cast<VertexId>((segment+1)%segments);
        (void)mesh.addFace({a,b,bottom});
    }
    mesh.recalculateNormals(); return mesh;
}

EditableMesh EditableMesh::cylinder(float radius, float height, int segments) {
    EditableMesh mesh;
    segments=std::max(3,segments); radius=std::max(0.0001F,radius); const float half=height*0.5F;
    constexpr float pi=3.14159265358979323846F;
    for (int layer=0;layer<2;++layer) for (int segment=0;segment<segments;++segment) {
        const float angle=2.0F*pi*static_cast<float>(segment)/static_cast<float>(segments);
        (void)mesh.addVertex({radius*std::cos(angle),layer==0?-half:half,radius*std::sin(angle)});
    }
    for (int segment=0;segment<segments;++segment) {
        const VertexId next=static_cast<VertexId>((segment+1)%segments);
        const VertexId a=static_cast<VertexId>(segment), b=next;
        const VertexId c=static_cast<VertexId>(segments)+next, d=static_cast<VertexId>(segments+segment);
        (void)mesh.addFace({a,b,c,d});
    }
    std::vector<VertexId> bottom,top;
    for (int segment=0;segment<segments;++segment) {
        bottom.push_back(static_cast<VertexId>(segments-1-segment));
        top.push_back(static_cast<VertexId>(segments+segment));
    }
    (void)mesh.addFace(std::move(bottom)); (void)mesh.addFace(std::move(top));
    mesh.recalculateNormals(); return mesh;
}

EditableMesh EditableMesh::cone(float radius, float height, int segments) {
    EditableMesh mesh;
    segments=std::max(3,segments); radius=std::max(0.0001F,radius); const float half=height*0.5F;
    constexpr float pi=3.14159265358979323846F;
    for (int segment=0;segment<segments;++segment) {
        const float angle=2.0F*pi*static_cast<float>(segment)/static_cast<float>(segments);
        (void)mesh.addVertex({radius*std::cos(angle),-half,radius*std::sin(angle)});
    }
    const VertexId apex=mesh.addVertex({0,half,0});
    for (int segment=0;segment<segments;++segment)
        (void)mesh.addFace({static_cast<VertexId>(segment),static_cast<VertexId>((segment+1)%segments),apex});
    std::vector<VertexId> base;
    for (int segment=0;segment<segments;++segment) base.push_back(static_cast<VertexId>(segments-1-segment));
    (void)mesh.addFace(std::move(base));
    mesh.recalculateNormals(); return mesh;
}

VertexId EditableMesh::addVertex(Vec3 position) {
    vertices_.push_back({position});
    return static_cast<VertexId>(vertices_.size() - 1U);
}

std::optional<FaceId> EditableMesh::addFace(std::vector<VertexId> vertices) {
    if (vertices.size() < 3U) return std::nullopt;
    if (std::any_of(vertices.begin(), vertices.end(), [&](VertexId id){ return id >= vertices_.size(); }))
        return std::nullopt;
    std::set<VertexId> unique(vertices.begin(), vertices.end());
    if (unique.size() < 3U) return std::nullopt;
    faces_.push_back({std::move(vertices), {}, false, false});
    recalculateNormals();
    return static_cast<FaceId>(faces_.size() - 1U);
}

bool EditableMesh::deleteFace(FaceId face) {
    if (face >= faces_.size() || faces_[face].deleted) return false;
    faces_[face].deleted = true;
    faces_[face].selected = false;
    selectedEdges_.clear();
    return true;
}

bool EditableMesh::flipFace(FaceId face) {
    if (face >= faces_.size() || faces_[face].deleted) return false;
    std::reverse(faces_[face].vertices.begin(), faces_[face].vertices.end());
    recalculateNormals();
    return true;
}

std::optional<VertexId> EditableMesh::mergeSelectedVertices() {
    const auto selected = selectedVertexIds(SelectionMode::Vertex);
    if (selected.size() < 2U) return std::nullopt;
    const VertexId target = *selected.begin();
    Vec3 center{};
    for (const VertexId id : selected) {
        center.x += vertices_[id].position.x;
        center.y += vertices_[id].position.y;
        center.z += vertices_[id].position.z;
    }
    const float inverse = 1.0F / static_cast<float>(selected.size());
    vertices_[target].position = {center.x*inverse, center.y*inverse, center.z*inverse};

    for (auto& face : faces_) {
        if (face.deleted) continue;
        for (auto& id : face.vertices) if (selected.contains(id)) id = target;
        std::vector<VertexId> unique;
        std::set<VertexId> seen;
        for (const VertexId id : face.vertices) if (seen.insert(id).second) unique.push_back(id);
        face.vertices = std::move(unique);
        if (face.vertices.size() < 3U) { face.deleted = true; face.selected = false; }
    }
    clearSelection();
    vertices_[target].selected = true;
    recalculateNormals();
    return target;
}

std::optional<FaceId> EditableMesh::fillSelectedVertices() {
    const auto selectedSet = selectedVertexIds(SelectionMode::Vertex);
    if (selectedSet.size() < 3U) return std::nullopt;
    std::vector<VertexId> selected(selectedSet.begin(), selectedSet.end());
    Vec3 center{};
    Vec3 minimum{std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    Vec3 maximum{-minimum.x,-minimum.y,-minimum.z};
    for (const VertexId id : selected) {
        const Vec3 p=vertices_[id].position;
        center.x+=p.x; center.y+=p.y; center.z+=p.z;
        minimum.x=std::min(minimum.x,p.x); minimum.y=std::min(minimum.y,p.y); minimum.z=std::min(minimum.z,p.z);
        maximum.x=std::max(maximum.x,p.x); maximum.y=std::max(maximum.y,p.y); maximum.z=std::max(maximum.z,p.z);
    }
    const float inverse=1.0F/static_cast<float>(selected.size());
    center={center.x*inverse,center.y*inverse,center.z*inverse};
    const Vec3 span{maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z};
    const int dropAxis = span.x<=span.y && span.x<=span.z ? 0 : (span.y<=span.z ? 1 : 2);
    std::sort(selected.begin(),selected.end(),[&](VertexId a,VertexId b) {
        const Vec3 pa=vertices_[a].position, pb=vertices_[b].position;
        const auto angle=[&](Vec3 p) {
            if (dropAxis==0) return std::atan2(p.z-center.z,p.y-center.y);
            if (dropAxis==1) return std::atan2(p.x-center.x,p.z-center.z);
            return std::atan2(p.y-center.y,p.x-center.x);
        };
        return angle(pa)<angle(pb);
    });
    clearSelection();
    const auto face=addFace(std::move(selected));
    if (face) faces_[*face].selected=true;
    return face;
}

std::vector<FaceId> EditableMesh::duplicateSelectedFaces(Vec3 offset) {
    std::vector<MeshFace> sources;
    for (const auto& face : faces_) if (!face.deleted && face.selected) sources.push_back(face);
    if (sources.empty()) return {};

    std::unordered_map<VertexId, VertexId> duplicates;
    for (const auto& face : sources) {
        for (const VertexId oldId : face.vertices) {
            if (duplicates.contains(oldId)) continue;
            const Vec3 p = vertices_[oldId].position;
            duplicates.emplace(oldId, addVertex({p.x+offset.x,p.y+offset.y,p.z+offset.z}));
        }
    }
    clearSelection();
    std::vector<FaceId> created;
    created.reserve(sources.size());
    for (const auto& source : sources) {
        std::vector<VertexId> vertices;
        vertices.reserve(source.vertices.size());
        for (const VertexId oldId : source.vertices) vertices.push_back(duplicates.at(oldId));
        if (const auto face = addFace(std::move(vertices))) {
            faces_[*face].selected = true;
            created.push_back(*face);
        }
    }
    recalculateNormals();
    return created;
}

std::vector<FaceId> EditableMesh::triangulateSelectedFaces() {
    std::vector<FaceId> sourceIds;
    for (std::size_t i=0;i<faces_.size();++i)
        if (!faces_[i].deleted && faces_[i].selected && faces_[i].vertices.size()>3U)
            sourceIds.push_back(static_cast<FaceId>(i));
    if (sourceIds.empty()) return {};

    std::vector<std::vector<VertexId>> triangles;
    for (const FaceId id:sourceIds) {
        const auto vertices=faces_[id].vertices;
        for (std::size_t i=1;i+1<vertices.size();++i)
            triangles.push_back({vertices[0],vertices[i],vertices[i+1]});
        faces_[id].deleted=true;
        faces_[id].selected=false;
    }
    std::vector<FaceId> created;
    created.reserve(triangles.size());
    for (auto& vertices:triangles) if (const auto face=addFace(std::move(vertices))) {
        faces_[*face].selected=true;
        created.push_back(*face);
    }
    recalculateNormals();
    return created;
}

std::vector<FaceId> EditableMesh::subdivideSelectedFaces() {
    std::vector<FaceId> sourceIds;
    for (std::size_t i=0;i<faces_.size();++i)
        if (!faces_[i].deleted && faces_[i].selected) sourceIds.push_back(static_cast<FaceId>(i));
    if (sourceIds.empty()) return {};
    std::vector<std::vector<VertexId>> newFaces;
    for (const FaceId id:sourceIds) {
        const auto vertices=faces_[id].vertices;
        Vec3 center{};
        for (const auto vertex:vertices) { const auto p=vertices_[vertex].position; center.x+=p.x; center.y+=p.y; center.z+=p.z; }
        const float inverse=1.0F/static_cast<float>(vertices.size());
        const VertexId centerId=addVertex({center.x*inverse,center.y*inverse,center.z*inverse});
        for (std::size_t i=0;i<vertices.size();++i)
            newFaces.push_back({vertices[i],vertices[(i+1U)%vertices.size()],centerId});
        faces_[id].deleted=true; faces_[id].selected=false;
    }
    std::vector<FaceId> created;
    for (auto& vertices:newFaces) if (const auto face=addFace(std::move(vertices))) {
        faces_[*face].selected=true; created.push_back(*face);
    }
    recalculateNormals();
    return created;
}

std::optional<FaceId> EditableMesh::dissolveSelectedFaces() {
    std::vector<FaceId> selectedFaces;
    std::map<std::pair<VertexId,VertexId>,int> edgeUses;
    for (std::size_t i=0;i<faces_.size();++i) {
        const auto& face=faces_[i];
        if (face.deleted || !face.selected) continue;
        selectedFaces.push_back(static_cast<FaceId>(i));
        for (std::size_t e=0;e<face.vertices.size();++e)
            ++edgeUses[orderedEdge(face.vertices[e],face.vertices[(e+1U)%face.vertices.size()])];
    }
    if (selectedFaces.size()<2U) return std::nullopt;
    std::map<VertexId,std::vector<VertexId>> adjacency;
    std::size_t boundaryEdges=0;
    for (const auto& [edge,uses]:edgeUses) if (uses==1) {
        adjacency[edge.first].push_back(edge.second);
        adjacency[edge.second].push_back(edge.first);
        ++boundaryEdges;
    }
    if (boundaryEdges<3U) return std::nullopt;
    for (const auto& [vertex,neighbors]:adjacency) { (void)vertex; if (neighbors.size()!=2U) return std::nullopt; }
    std::vector<VertexId> boundary;
    const VertexId start=adjacency.begin()->first;
    VertexId previous=std::numeric_limits<VertexId>::max(),current=start;
    do {
        boundary.push_back(current);
        const auto& neighbors=adjacency[current];
        const VertexId next=neighbors[0]==previous?neighbors[1]:neighbors[0];
        previous=current; current=next;
        if (boundary.size()>boundaryEdges) return std::nullopt;
    } while (current!=start);
    if (boundary.size()!=boundaryEdges) return std::nullopt;
    for (const auto id:selectedFaces) { faces_[id].deleted=true; faces_[id].selected=false; }
    const auto result=addFace(std::move(boundary));
    if (result) faces_[*result].selected=true;
    recalculateNormals();
    return result;
}

std::optional<FaceId> EditableMesh::dissolveSelectedEdge() {
    if (selectedEdges_.size()!=1U) return std::nullopt;
    const auto edge=*selectedEdges_.begin();
    std::vector<FaceId> adjacent;
    for (std::size_t i=0;i<faces_.size();++i) {
        const auto& face=faces_[i];
        if (face.deleted) continue;
        for (std::size_t e=0;e<face.vertices.size();++e) {
            if (orderedEdge(face.vertices[e],face.vertices[(e+1U)%face.vertices.size()])==edge) {
                adjacent.push_back(static_cast<FaceId>(i)); break;
            }
        }
    }
    if (adjacent.size()!=2U) return std::nullopt;
    for (auto& face:faces_) face.selected=false;
    faces_[adjacent[0]].selected=true; faces_[adjacent[1]].selected=true;
    selectedEdges_.clear();
    return dissolveSelectedFaces();
}

std::unique_ptr<EditableMesh> EditableMesh::extractSelectedFaces() {
    std::vector<FaceId> sources;
    for (std::size_t i=0;i<faces_.size();++i)
        if (!faces_[i].deleted && faces_[i].selected) sources.push_back(static_cast<FaceId>(i));
    if (sources.empty()) return nullptr;
    auto extracted=std::make_unique<EditableMesh>();
    std::unordered_map<VertexId,VertexId> remap;
    for (const FaceId faceId:sources) {
        for (const VertexId oldId:faces_[faceId].vertices) if (!remap.contains(oldId))
            remap.emplace(oldId,extracted->addVertex(vertices_[oldId].position));
    }
    for (const FaceId faceId:sources) {
        std::vector<VertexId> vertices;
        for (const VertexId oldId:faces_[faceId].vertices) vertices.push_back(remap.at(oldId));
        const auto face=extracted->addFace(std::move(vertices));
        if (face) (void)extracted->setFaceSelected(*face,true);
        faces_[faceId].deleted=true; faces_[faceId].selected=false;
    }
    selectedEdges_.clear();
    recalculateNormals(); extracted->recalculateNormals();
    return extracted;
}

bool EditableMesh::appendTransformed(const EditableMesh& source,const Transform& sourceTransform,
                                     const Transform& destinationTransform) {
    if (std::abs(destinationTransform.scale.x)<0.000001F ||
        std::abs(destinationTransform.scale.y)<0.000001F ||
        std::abs(destinationTransform.scale.z)<0.000001F) return false;
    std::vector<VertexId> remap;
    remap.reserve(source.vertices_.size());
    for (const auto& vertex:source.vertices_)
        remap.push_back(addVertex(toLocal(toWorld(vertex.position,sourceTransform),destinationTransform)));
    for (const auto& sourceFace:source.faces_) if (!sourceFace.deleted) {
        std::vector<VertexId> face;
        face.reserve(sourceFace.vertices.size());
        for (const auto vertex:sourceFace.vertices) face.push_back(remap[vertex]);
        const auto added=addFace(std::move(face));
        if (!added) return false;
    }
    recalculateNormals();
    return true;
}

std::optional<FaceId> EditableMesh::extrudeFace(FaceId faceId, float distance) {
    if (faceId >= faces_.size() || faces_[faceId].deleted) return std::nullopt;
    selectedEdges_.clear();
    const MeshFace source = faces_[faceId];
    if (source.vertices.size() < 3U) return std::nullopt;

    std::vector<VertexId> top;
    top.reserve(source.vertices.size());
    for (const VertexId oldId : source.vertices) {
        const Vec3 p = vertices_[oldId].position;
        top.push_back(addVertex({p.x + source.normal.x * distance,
                                 p.y + source.normal.y * distance,
                                 p.z + source.normal.z * distance}));
    }
    faces_[faceId].deleted = true;
    faces_[faceId].selected = false;
    const auto topFace = addFace(top);
    for (std::size_t i = 0; i < source.vertices.size(); ++i) {
        const std::size_t next = (i + 1U) % source.vertices.size();
        (void)addFace({source.vertices[i], source.vertices[next], top[next], top[i]});
    }
    if (topFace) faces_[*topFace].selected = true;
    recalculateNormals();
    return topFace;
}

std::optional<FaceId> EditableMesh::insetFace(FaceId faceId, float amount) {
    if (faceId >= faces_.size() || faces_[faceId].deleted || amount <= 0.0F || amount >= 1.0F)
        return std::nullopt;
    selectedEdges_.clear();
    const MeshFace source = faces_[faceId];
    if (source.vertices.size() < 3U) return std::nullopt;

    Vec3 centroid{};
    for (const VertexId id : source.vertices) {
        const Vec3 p = vertices_[id].position;
        centroid.x += p.x; centroid.y += p.y; centroid.z += p.z;
    }
    const float inverseCount = 1.0F / static_cast<float>(source.vertices.size());
    centroid.x *= inverseCount; centroid.y *= inverseCount; centroid.z *= inverseCount;

    std::vector<VertexId> inner;
    inner.reserve(source.vertices.size());
    for (const VertexId oldId : source.vertices) {
        const Vec3 p = vertices_[oldId].position;
        inner.push_back(addVertex({p.x + (centroid.x - p.x) * amount,
                                   p.y + (centroid.y - p.y) * amount,
                                   p.z + (centroid.z - p.z) * amount}));
    }

    faces_[faceId].deleted = true;
    faces_[faceId].selected = false;
    const auto innerFace = addFace(inner);
    for (std::size_t i = 0; i < source.vertices.size(); ++i) {
        const std::size_t next = (i + 1U) % source.vertices.size();
        (void)addFace({source.vertices[i], source.vertices[next], inner[next], inner[i]});
    }
    if (innerFace) faces_[*innerFace].selected = true;
    recalculateNormals();
    return innerFace;
}

std::pair<VertexId, VertexId> EditableMesh::orderedEdge(VertexId first, VertexId second) noexcept {
    return first < second ? std::pair{first, second} : std::pair{second, first};
}

std::set<std::pair<VertexId, VertexId>> EditableMesh::topologyEdges() const {
    std::set<std::pair<VertexId, VertexId>> edges;
    for (const auto& face : faces_) {
        if (face.deleted) continue;
        for (std::size_t i = 0; i < face.vertices.size(); ++i)
            edges.insert(orderedEdge(face.vertices[i], face.vertices[(i + 1U) % face.vertices.size()]));
    }
    return edges;
}

std::optional<FaceId> EditableMesh::bevelFace(FaceId face, float insetAmount, float depth) {
    if (face >= faces_.size() || faces_[face].deleted || insetAmount <= 0.0F || insetAmount >= 1.0F)
        return std::nullopt;
    const Vec3 normal = faces_[face].normal;
    clearSelection();
    faces_[face].selected = true;
    const auto inner = insetFace(face, insetAmount);
    if (!inner) return std::nullopt;
    if (std::fabs(depth) > 0.000001F &&
        !translateSelected(SelectionMode::Face, {normal.x*depth,normal.y*depth,normal.z*depth}))
        return std::nullopt;
    return inner;
}

bool EditableMesh::setVertexSelected(VertexId vertex, bool selected) {
    if (vertex >= vertices_.size()) return false;
    vertices_[vertex].selected = selected;
    return true;
}

bool EditableMesh::setEdgeSelected(VertexId first, VertexId second, bool selected) {
    if (first == second || first >= vertices_.size() || second >= vertices_.size()) return false;
    const auto edge = orderedEdge(first, second);
    if (!topologyEdges().contains(edge)) return false;
    if (selected) selectedEdges_.insert(edge); else selectedEdges_.erase(edge);
    return true;
}

bool EditableMesh::setFaceSelected(FaceId face, bool selected) {
    if (face >= faces_.size() || faces_[face].deleted) return false;
    faces_[face].selected = selected;
    return true;
}

bool EditableMesh::isEdgeSelected(VertexId first, VertexId second) const noexcept {
    return selectedEdges_.contains(orderedEdge(first, second));
}

std::set<VertexId> EditableMesh::selectedVertexIds(SelectionMode mode) const {
    std::set<VertexId> selected;
    if (mode == SelectionMode::Vertex) {
        for (std::size_t i = 0; i < vertices_.size(); ++i)
            if (vertices_[i].selected) selected.insert(static_cast<VertexId>(i));
    } else if (mode == SelectionMode::Edge) {
        for (const auto& [first, second] : selectedEdges_) { selected.insert(first); selected.insert(second); }
    } else {
        for (const auto& face : faces_)
            if (!face.deleted && face.selected) selected.insert(face.vertices.begin(), face.vertices.end());
    }
    return selected;
}

std::optional<Vec3> EditableMesh::selectionCenter(SelectionMode mode) const noexcept {
    const auto selected = selectedVertexIds(mode);
    if (selected.empty()) return std::nullopt;
    Vec3 center{};
    for (const VertexId id : selected) {
        center.x += vertices_[id].position.x;
        center.y += vertices_[id].position.y;
        center.z += vertices_[id].position.z;
    }
    const float inverse = 1.0F / static_cast<float>(selected.size());
    center.x *= inverse; center.y *= inverse; center.z *= inverse;
    return center;
}

bool EditableMesh::translateSelected(SelectionMode mode, Vec3 delta) noexcept {
    const auto selected = selectedVertexIds(mode);
    if (selected.empty()) return false;
    for (const VertexId id : selected) {
        vertices_[id].position.x += delta.x;
        vertices_[id].position.y += delta.y;
        vertices_[id].position.z += delta.z;
    }
    recalculateNormals();
    return true;
}

bool EditableMesh::rotateSelected(SelectionMode mode, int axis, float radians) noexcept {
    if (axis < 0 || axis > 2) return false;
    const auto selected = selectedVertexIds(mode);
    const auto center = selectionCenter(mode);
    if (selected.empty() || !center) return false;
    const float cosine = std::cos(radians), sine = std::sin(radians);
    for (const VertexId id : selected) {
        Vec3 p{vertices_[id].position.x-center->x, vertices_[id].position.y-center->y,
               vertices_[id].position.z-center->z};
        Vec3 rotated = p;
        if (axis == 0) rotated = {p.x, p.y*cosine-p.z*sine, p.y*sine+p.z*cosine};
        else if (axis == 1) rotated = {p.x*cosine+p.z*sine, p.y, -p.x*sine+p.z*cosine};
        else rotated = {p.x*cosine-p.y*sine, p.x*sine+p.y*cosine, p.z};
        vertices_[id].position = {center->x+rotated.x, center->y+rotated.y, center->z+rotated.z};
    }
    recalculateNormals();
    return true;
}

bool EditableMesh::scaleSelected(SelectionMode mode, Vec3 factors) noexcept {
    if (std::fabs(factors.x) < 0.0001F || std::fabs(factors.y) < 0.0001F ||
        std::fabs(factors.z) < 0.0001F) return false;
    const auto selected = selectedVertexIds(mode);
    const auto center = selectionCenter(mode);
    if (selected.empty() || !center) return false;
    for (const VertexId id : selected) {
        auto& p = vertices_[id].position;
        p = {center->x+(p.x-center->x)*factors.x,
             center->y+(p.y-center->y)*factors.y,
             center->z+(p.z-center->z)*factors.z};
    }
    recalculateNormals();
    return true;
}

void EditableMesh::clearSelection() noexcept {
    for (auto& vertex : vertices_) vertex.selected = false;
    selectedEdges_.clear();
    for (auto& face : faces_) face.selected = false;
}

void EditableMesh::selectAll(SelectionMode mode) {
    if (mode == SelectionMode::Vertex) {
        for (auto& vertex : vertices_) vertex.selected = true;
    } else if (mode == SelectionMode::Edge) {
        selectedEdges_ = topologyEdges();
    } else {
        for (auto& face : faces_) if (!face.deleted) face.selected = true;
    }
}

void EditableMesh::invertSelection(SelectionMode mode) {
    if (mode == SelectionMode::Vertex) {
        for (auto& vertex : vertices_) vertex.selected = !vertex.selected;
    } else if (mode == SelectionMode::Edge) {
        const auto edges = topologyEdges();
        std::set<std::pair<VertexId, VertexId>> inverted;
        for (const auto& edge : edges) if (!selectedEdges_.contains(edge)) inverted.insert(edge);
        selectedEdges_ = std::move(inverted);
    } else {
        for (auto& face : faces_) if (!face.deleted) face.selected = !face.selected;
    }
}

std::size_t EditableMesh::selectedVertexCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(vertices_.begin(), vertices_.end(),
        [](const MeshVertex& vertex) { return vertex.selected; }));
}

std::size_t EditableMesh::selectedFaceCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(faces_.begin(), faces_.end(),
        [](const MeshFace& face) { return !face.deleted && face.selected; }));
}

void EditableMesh::recalculateNormals() noexcept {
    for (auto& face : faces_) {
        if (face.deleted || face.vertices.size() < 3U) { face.normal = {}; continue; }
        const Vec3 a = vertices_[face.vertices[0]].position;
        const Vec3 b = vertices_[face.vertices[1]].position;
        const Vec3 c = vertices_[face.vertices[2]].position;
        face.normal = normalized(cross(subtract(b,a), subtract(c,a)));
    }
}

MeshStats EditableMesh::stats() const {
    MeshStats out;
    out.vertices = vertices_.size();
    std::set<std::pair<VertexId,VertexId>> edges;
    for (const auto& face : faces_) {
        if (face.deleted) continue;
        ++out.faces;
        out.triangles += face.vertices.size() - 2U;
        for (std::size_t i=0; i<face.vertices.size(); ++i) {
            auto a=face.vertices[i], b=face.vertices[(i+1U)%face.vertices.size()];
            if (a>b) std::swap(a,b);
            edges.emplace(a,b);
        }
    }
    out.edges = edges.size();
    return out;
}

bool EditableMesh::validate() const noexcept {
    for (const auto& face : faces_) {
        if (face.deleted) continue;
        if (face.vertices.size() < 3U) return false;
        std::set<VertexId> unique;
        for (const auto id : face.vertices) {
            if (id >= vertices_.size()) return false;
            unique.insert(id);
        }
        if (unique.size() < 3U) return false;
    }
    return true;
}

MeshHistory::MeshHistory(std::size_t memoryBudgetBytes)
    : memoryBudgetBytes_(std::max<std::size_t>(memoryBudgetBytes, 1U)) {}

std::size_t MeshHistory::estimateBytes(const EditableMesh& mesh) noexcept {
    std::size_t bytes = mesh.vertices().size() * sizeof(MeshVertex) +
                        mesh.faces().size() * sizeof(MeshFace);
    for (const auto& face : mesh.faces()) bytes += face.vertices.size() * sizeof(VertexId);
    bytes += mesh.selectedEdges().size() * sizeof(std::pair<VertexId, VertexId>);
    return bytes;
}

bool MeshHistory::commit(EditableMesh before, EditableMesh after) {
    if (!before.validate() || !after.validate()) return false;
    while (entries_.size() > cursor_) {
        memoryUsed_ -= entries_.back().bytes;
        entries_.pop_back();
    }
    const std::size_t bytes = estimateBytes(before) + estimateBytes(after);
    entries_.push_back({std::move(before), std::move(after), bytes});
    memoryUsed_ += bytes;
    cursor_ = entries_.size();
    trimToBudget();
    return true;
}

bool MeshHistory::undo(EditableMesh& mesh) {
    if (!canUndo()) return false;
    mesh = entries_[--cursor_].before;
    return true;
}

bool MeshHistory::redo(EditableMesh& mesh) {
    if (!canRedo()) return false;
    mesh = entries_[cursor_++].after;
    return true;
}

void MeshHistory::clear() noexcept {
    entries_.clear(); cursor_ = 0; memoryUsed_ = 0;
}

void MeshHistory::trimToBudget() {
    while (memoryUsed_ > memoryBudgetBytes_ && entries_.size() > 1U) {
        memoryUsed_ -= entries_.front().bytes;
        entries_.erase(entries_.begin());
        if (cursor_ > 0U) --cursor_;
    }
}

} // namespace a3d
