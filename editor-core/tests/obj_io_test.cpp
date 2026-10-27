#include "a3d/obj_io.hpp"
#include <cassert>
#include <string>
int main() {
    a3d::ModelingScene scene;
    const auto cube=scene.addPrimitive(a3d::PrimitiveType::Cube,"Export Cube");
    scene.object(cube)->transform.position={2.0F,3.0F,4.0F};
    const std::string text=a3d::exportObj(scene);
    assert(text.find("o Export Cube")!=std::string::npos);
    assert(text.find("v 1 2 3")!=std::string::npos);
    std::string error;
    auto imported=a3d::importObj(text,&error);
    assert(imported && error.empty());
    assert(imported->validate());
    assert(imported->stats().vertices==8);
    assert(imported->stats().faces==6);
    a3d::ModelingScene importedScene;
    const auto importedId=importedScene.addMesh(std::move(*imported),"OBJ Asset");
    assert(importedScene.activeObject()==importedId);
    assert(importedScene.object(importedId)->name=="OBJ Asset");
    assert(importedScene.mesh(importedId)->stats().faces==6);
    const auto negative=a3d::importObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3/-1 -2/-1 -1/-1\n",&error);
    assert(negative && negative->stats().triangles==1);
    assert(!a3d::importObj("v 0 0 0\nf 1 2 3\n",&error));
    assert(!error.empty());
}
