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
    return 0;
}
