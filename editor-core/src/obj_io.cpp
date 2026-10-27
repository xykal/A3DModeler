#include "a3d/obj_io.hpp"
#include <cmath>
#include <charconv>
#include <iomanip>
#include <sstream>
#include <vector>
namespace a3d {
namespace {
Vec3 transformPoint(Vec3 p,const Transform& t) {
    p={p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z};
    const float cx=std::cos(t.rotationEuler.x),sx=std::sin(t.rotationEuler.x);
    const float cy=std::cos(t.rotationEuler.y),sy=std::sin(t.rotationEuler.y);
    const float cz=std::cos(t.rotationEuler.z),sz=std::sin(t.rotationEuler.z);
    p={p.x,p.y*cx-p.z*sx,p.y*sx+p.z*cx};
    p={p.x*cy+p.z*sy,p.y,-p.x*sy+p.z*cy};
    p={p.x*cz-p.y*sz,p.x*sz+p.y*cz,p.z};
    return {p.x+t.position.x,p.y+t.position.y,p.z+t.position.z};
}
void fail(std::string* error,std::string message) { if (error) *error=std::move(message); }
}
std::string exportObj(const ModelingScene& scene) {
    std::ostringstream out; out<<"# A3DModeler OBJ\n"<<std::setprecision(9);
    std::size_t offset=1;
    for (const auto id:scene.objectIds()) {
        const auto* object=scene.object(id); const auto* mesh=scene.mesh(id);
        if (!object || !mesh || !object->visible) continue;
        out<<"o "<<object->name<<'\n';
        for (const auto& vertex:mesh->vertices()) { const auto p=transformPoint(vertex.position,object->transform); out<<"v "<<p.x<<' '<<p.y<<' '<<p.z<<'\n'; }
        for (const auto& face:mesh->faces()) if (!face.deleted) {
            out<<'f'; for (const auto vertex:face.vertices) out<<' '<<(offset+vertex); out<<'\n';
        }
        offset+=mesh->vertices().size();
    }
    return out.str();
}
std::optional<EditableMesh> importObj(std::string_view text,std::string* error) {
    constexpr std::size_t maxVertices=5'000'000,maxFaces=5'000'000;
    EditableMesh mesh; std::istringstream input{std::string(text)}; std::string line;
    std::size_t lineNumber=0;
    while (std::getline(input,line)) {
        ++lineNumber; std::istringstream row(line); std::string tag; row>>tag;
        if (tag.empty() || tag[0]=='#' || tag=="o" || tag=="g" || tag=="s" || tag=="usemtl" || tag=="mtllib" || tag=="vt" || tag=="vn") continue;
        if (tag=="v") {
            Vec3 p{}; if (!(row>>p.x>>p.y>>p.z)) { fail(error,"invalid vertex at line "+std::to_string(lineNumber)); return std::nullopt; }
            if (mesh.vertices().size()>=maxVertices) { fail(error,"OBJ vertex limit exceeded"); return std::nullopt; }
            (void)mesh.addVertex(p);
        } else if (tag=="f") {
            std::vector<VertexId> face; std::string token;
            while (row>>token) {
                const auto slash=token.find('/'); const std::string indexText=token.substr(0,slash);
                int index=0; const auto parsed=std::from_chars(indexText.data(),indexText.data()+indexText.size(),index);
                if (parsed.ec!=std::errc{} || parsed.ptr!=indexText.data()+indexText.size() || index==0) { fail(error,"invalid face index at line "+std::to_string(lineNumber)); return std::nullopt; }
                const long long resolved=index>0?static_cast<long long>(index)-1:static_cast<long long>(mesh.vertices().size())+index;
                if (resolved<0 || resolved>=static_cast<long long>(mesh.vertices().size())) { fail(error,"face index out of range at line "+std::to_string(lineNumber)); return std::nullopt; }
                face.push_back(static_cast<VertexId>(resolved));
            }
            if (mesh.stats().faces>=maxFaces || !mesh.addFace(std::move(face))) { fail(error,"invalid face at line "+std::to_string(lineNumber)); return std::nullopt; }
        }
    }
    if (mesh.vertices().empty() || mesh.stats().faces==0) { fail(error,"OBJ contains no mesh faces"); return std::nullopt; }
    mesh.recalculateNormals(); return mesh;
}
} // namespace a3d
