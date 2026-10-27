#include "a3d/project_io.hpp"
#include <array>
#include <cstdint>
#include <limits>

namespace a3d {
namespace {
template<class T> bool writeValue(std::ostream& out,const T& value){out.write(reinterpret_cast<const char*>(&value),sizeof(T));return out.good();}
template<class T> bool readValue(std::istream& in,T& value){in.read(reinterpret_cast<char*>(&value),sizeof(T));return in.good();}
bool writeString(std::ostream& out,const std::string& value){const auto n=static_cast<std::uint32_t>(value.size());return writeValue(out,n)&&(out.write(value.data(),n),out.good());}
bool readString(std::istream& in,std::string& value){std::uint32_t n{};if(!readValue(in,n)||n>4096U)return false;value.resize(n);in.read(value.data(),n);return in.good();}
constexpr std::uint32_t maxObjects=10000,maxVertices=2000000,maxFaces=2000000,maxFaceVertices=65535;
}

bool ProjectIo::save(const ModelingScene& scene,std::ostream& out){
    const std::array<char,4> magic{'A','3','D','P'};out.write(magic.data(),magic.size());
    if(!writeValue(out,currentVersion))return false;
    const auto ids=scene.objectIds();const auto count=static_cast<std::uint32_t>(ids.size());if(!writeValue(out,count))return false;
    std::int32_t active=-1;for(std::size_t i=0;i<ids.size();++i)if(scene.activeObject()==ids[i])active=static_cast<std::int32_t>(i);
    if(!writeValue(out,active))return false;
    for(const auto id:ids){
        const auto* object=scene.object(id);const auto* mesh=scene.mesh(id);if(!object||!mesh)return false;
        if(!writeString(out,object->name))return false;
        const auto primitive=static_cast<std::uint8_t>(object->primitive);const std::uint8_t flags=(object->visible?1U:0U)|(object->locked?2U:0U);
        if(!writeValue(out,primitive)||!writeValue(out,flags)||!writeValue(out,object->transform))return false;
        const auto vertexCount=static_cast<std::uint32_t>(mesh->vertices().size());if(!writeValue(out,vertexCount))return false;
        for(const auto& vertex:mesh->vertices())if(!writeValue(out,vertex))return false;
        std::uint32_t faceCount=0;for(const auto& face:mesh->faces())if(!face.deleted)++faceCount;if(!writeValue(out,faceCount))return false;
        for(const auto& face:mesh->faces())if(!face.deleted){const auto n=static_cast<std::uint32_t>(face.vertices.size());if(!writeValue(out,n))return false;for(const auto v:face.vertices)if(!writeValue(out,v))return false;if(!writeValue(out,face.selected))return false;}
        const auto edgeCount=static_cast<std::uint32_t>(mesh->selectedEdges().size());if(!writeValue(out,edgeCount))return false;
        for(const auto& edge:mesh->selectedEdges())if(!writeValue(out,edge))return false;
    }
    return out.good();
}

ProjectLoadResult ProjectIo::load(std::istream& in){
    std::array<char,4> magic{};in.read(magic.data(),magic.size());if(magic!=std::array<char,4>{'A','3','D','P'})return {nullptr,"Invalid project signature"};
    std::uint32_t version{},count{};std::int32_t active{};if(!readValue(in,version)||version!=currentVersion)return {nullptr,"Unsupported project version"};
    if(!readValue(in,count)||count>maxObjects||!readValue(in,active))return {nullptr,"Invalid object table"};
    auto scene=std::make_unique<ModelingScene>();std::vector<ObjectId> loadedIds;
    for(std::uint32_t i=0;i<count;++i){
        std::string name;std::uint8_t primitive{},flags{};Transform transform{};if(!readString(in,name)||!readValue(in,primitive)||primitive>4||!readValue(in,flags)||!readValue(in,transform))return {nullptr,"Invalid object record"};
        const auto id=scene->addPrimitive(static_cast<PrimitiveType>(primitive),name);loadedIds.push_back(id);auto* object=scene->object(id);auto* mesh=scene->mesh(id);object->transform=transform;object->visible=(flags&1U)!=0;object->locked=(flags&2U)!=0;
        EditableMesh restored;std::uint32_t vertices{};if(!readValue(in,vertices)||vertices>maxVertices)return {nullptr,"Invalid vertex count"};
        for(std::uint32_t v=0;v<vertices;++v){MeshVertex vertex{};if(!readValue(in,vertex))return {nullptr,"Truncated vertex data"};const auto newId=restored.addVertex(vertex.position);if(vertex.selected)(void)restored.setVertexSelected(newId,true);}
        std::uint32_t faces{};if(!readValue(in,faces)||faces>maxFaces)return {nullptr,"Invalid face count"};
        for(std::uint32_t f=0;f<faces;++f){std::uint32_t n{};if(!readValue(in,n)||n<3||n>maxFaceVertices)return {nullptr,"Invalid polygon"};std::vector<VertexId> ids(n);for(auto& v:ids)if(!readValue(in,v)||v>=vertices)return {nullptr,"Invalid polygon index"};bool selected{};if(!readValue(in,selected))return {nullptr,"Truncated face data"};const auto face=restored.addFace(std::move(ids));if(!face)return {nullptr,"Invalid face topology"};if(selected)(void)restored.setFaceSelected(*face,true);}
        std::uint32_t edges{};if(!readValue(in,edges)||edges>maxVertices*4U)return {nullptr,"Invalid edge selection"};for(std::uint32_t e=0;e<edges;++e){std::pair<VertexId,VertexId> edge{};if(!readValue(in,edge)||!restored.setEdgeSelected(edge.first,edge.second,true))return {nullptr,"Invalid selected edge"};}
        restored.recalculateNormals();*mesh=std::move(restored);
    }
    if(active>=0&&static_cast<std::uint32_t>(active)<loadedIds.size())(void)scene->selectObject(loadedIds[static_cast<std::size_t>(active)]);
    if(!in.good()&&!in.eof()) return {nullptr,"Project read failure"};
    return {std::move(scene),{}};
}
} // namespace a3d
