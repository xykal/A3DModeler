#include "a3d/modeling_scene.hpp"
#include <algorithm>
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

    assert(scene.mesh(cylinder)->setFaceSelected(0,true));
    const auto separated=scene.separateSelectedFaces(cylinder);
    assert(separated.has_value());
    assert(scene.mesh(*separated)->stats().faces==1);
    assert(scene.mesh(cylinder)->stats().faces>1);

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
    a3d::Transform moved=scene.object(sphere)->transform;
    moved.position={2.0F,3.0F,4.0F};
    assert(scene.applyTransform(sphere,moved));
    assert(scene.object(sphere)->transform.nearlyEquals(moved));
    assert(scene.undoTransform());
    assert(scene.object(sphere)->transform.position.nearlyEquals({0.0F,0.0F,0.0F}));
    assert(scene.redoTransform());
    assert(scene.object(sphere)->transform.nearlyEquals(moved));
    const auto summaries=scene.summaries();
    assert(summaries.size()==5);
    bool foundActive=false;
    for (const auto& summary:summaries) if (summary.id==sphere) {
        assert(summary.active);
        assert(summary.name=="Sphere");
        assert(summary.meshStats.faces>6);
        foundActive=true;
    }
    assert(foundActive);
    assert(scene.scene().setVisible(cube,false));
    assert(scene.scene().setLocked(cube,true));
    assert(scene.scene().renameObject(cube,"Hero Mesh"));
    const auto updated=scene.summaries();
    const auto hero=std::find_if(updated.begin(),updated.end(),[&](const auto& item){ return item.id==cube; });
    assert(hero!=updated.end());
    assert(hero->name=="Hero Mesh" && !hero->visible && hero->locked);
    assert(scene.removeObject(sphere));
    assert(!scene.mesh(sphere));
    assert(!scene.object(sphere));
    assert(!scene.removeObject(sphere));
    return 0;
}
