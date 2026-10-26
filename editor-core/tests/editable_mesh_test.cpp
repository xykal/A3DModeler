#include "a3d/editable_mesh.hpp"
#include <cassert>
#include <cmath>

int main() {
    auto cube = a3d::EditableMesh::cube();
    assert(cube.validate());
    auto stats = cube.stats();
    assert(stats.vertices == 8 && stats.edges == 12 && stats.faces == 6 && stats.triangles == 12);

    assert(cube.setFaceSelected(1, true));
    const auto top = cube.extrudeFace(1, 1.0F);
    assert(top.has_value());
    assert(cube.validate());
    stats = cube.stats();
    assert(stats.vertices == 12);
    assert(stats.faces == 10);
    assert(stats.triangles == 20);
    assert(cube.faces()[*top].selected);

    cube.clearSelection();
    assert(!cube.faces()[*top].selected);
    assert(cube.deleteFace(*top));
    assert(!cube.deleteFace(*top));

    auto plane = a3d::EditableMesh::plane(4.0F);
    const auto planeStats = plane.stats();
    assert(planeStats.vertices == 4 && planeStats.edges == 4 && planeStats.faces == 1);
    assert(std::fabs(plane.faces()[0].normal.y + 1.0F) < 0.0001F ||
           std::fabs(plane.faces()[0].normal.y - 1.0F) < 0.0001F);

    assert(!plane.addFace({0, 0, 1}).has_value());
    assert(!plane.addFace({0, 1, 99}).has_value());

    auto insetPlane = a3d::EditableMesh::plane(4.0F);
    assert(!insetPlane.insetFace(0, 0.0F).has_value());
    assert(!insetPlane.insetFace(0, 1.0F).has_value());
    const auto inset = insetPlane.insetFace(0, 0.25F);
    assert(inset.has_value());
    assert(insetPlane.validate());
    const auto insetStats = insetPlane.stats();
    assert(insetStats.vertices == 8);
    assert(insetStats.faces == 5);
    assert(insetStats.triangles == 10);
    assert(insetPlane.faces()[*inset].selected);

    a3d::MeshHistory history(1024U * 1024U);
    auto historyMesh = a3d::EditableMesh::cube();
    const auto beforeExtrude = historyMesh;
    assert(historyMesh.extrudeFace(4, 0.5F).has_value());
    assert(history.commit(beforeExtrude, historyMesh));
    assert(history.canUndo() && !history.canRedo());
    assert(history.undo(historyMesh));
    assert(historyMesh.stats().vertices == 8);
    assert(historyMesh.stats().faces == 6);
    assert(history.redo(historyMesh));
    assert(historyMesh.stats().vertices == 12);
    assert(historyMesh.stats().faces == 10);

    assert(history.undo(historyMesh));
    const auto beforeInset = historyMesh;
    assert(historyMesh.insetFace(4, 0.2F).has_value());
    assert(history.commit(beforeInset, historyMesh));
    assert(!history.canRedo());
    assert(history.memoryUsed() > 0U);

    auto normalMesh = a3d::EditableMesh::plane();
    const auto beforeNormal = normalMesh.faces()[0].normal;
    assert(normalMesh.flipFace(0));
    const auto afterNormal = normalMesh.faces()[0].normal;
    assert(std::fabs(beforeNormal.x + afterNormal.x) < 0.0001F);
    assert(std::fabs(beforeNormal.y + afterNormal.y) < 0.0001F);
    assert(std::fabs(beforeNormal.z + afterNormal.z) < 0.0001F);
    assert(!normalMesh.flipFace(99));

    const auto beforeDelete = normalMesh;
    assert(normalMesh.deleteFace(0));
    assert(history.commit(beforeDelete, normalMesh));
    assert(normalMesh.stats().faces == 0);
    assert(history.undo(normalMesh));
    assert(normalMesh.stats().faces == 1);
    return 0;
}
