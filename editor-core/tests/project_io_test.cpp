#include "a3d/project_io.hpp"
#include <cassert>
#include <sstream>

int main() {
    a3d::ModelingScene scene;
    const auto cube=scene.addPrimitive(a3d::PrimitiveType::Cube,"Hero");
    const auto sphere=scene.addPrimitive(a3d::PrimitiveType::UVSphere,"Orb");
    scene.object(cube)->transform.position={1,2,3};
    assert(scene.scene().setVisible(cube,false));
    assert(scene.scene().setLocked(cube,true));
    assert(scene.selectObject(cube));
    assert(scene.mesh(sphere)->setFaceSelected(0,true));
    assert(scene.mesh(sphere)->extrudeFace(0,0.25F).has_value());

    std::stringstream data(std::ios::in|std::ios::out|std::ios::binary);
    assert(a3d::ProjectIo::save(scene,data));
    data.seekg(0);
    auto loaded=a3d::ProjectIo::load(data);
    assert(loaded);
    assert(loaded.scene->objectIds().size()==2);
    const auto ids=loaded.scene->objectIds();
    const auto* hero=loaded.scene->object(ids[0]);
    assert(hero->name=="Hero"&&!hero->visible&&hero->locked);
    assert(hero->transform.position.nearlyEquals({1,2,3}));
    assert(loaded.scene->activeObject()==ids[0]);
    assert(loaded.scene->mesh(ids[1])->validate());

    std::stringstream bad("nope");
    assert(!a3d::ProjectIo::load(bad));
    return 0;
}
