#include "a3d/modeling_scene.hpp"
#include <cassert>

int main() {
    a3d::ModelingScene scene;
    const auto cube=scene.addPrimitive(a3d::PrimitiveType::Cube,"Cube");
    const auto sphere=scene.addPrimitive(a3d::PrimitiveType::UVSphere,"Sphere");
    const auto cylinder=scene.addPrimitive(a3d::PrimitiveType::Cylinder,"Cylinder");
    assert(scene.objectIds().size()==3);
    assert(scene.activeObject()==cylinder);
    assert(scene.mesh(cube)->stats().faces==6);
    assert(scene.mesh(sphere)->stats().faces>6);
    assert(scene.mesh(cylinder)->stats().faces>6);

    scene.object(cube)->transform.position={3.0F,0.0F,0.0F};
    assert(scene.mesh(cube)->setFaceSelected(0,true));
    const auto copy=scene.duplicateObject(cube);
    assert(copy.has_value());
    assert(scene.object(*copy)->transform.position.nearlyEquals({3.0F,0.0F,0.0F}));
    assert(scene.mesh(*copy)->stats().faces==6);
    assert(scene.mesh(*copy)->selectedFaceCount()==1);

    assert(scene.selectObject(sphere));
    assert(scene.activeObject()==sphere);
    assert(!scene.selectObject(999999));
    assert(scene.removeObject(sphere));
    assert(!scene.mesh(sphere));
    assert(!scene.object(sphere));
    assert(!scene.removeObject(sphere));
    return 0;
}
