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

    auto selectionMesh = a3d::EditableMesh::cube();
    assert(selectionMesh.setVertexSelected(0, true));
    assert(!selectionMesh.setVertexSelected(99, true));
    assert(selectionMesh.selectedVertexCount() == 1);
    assert(selectionMesh.setEdgeSelected(0, 1, true));
    assert(selectionMesh.isEdgeSelected(1, 0));
    assert(!selectionMesh.setEdgeSelected(0, 6, true));
    assert(selectionMesh.selectedEdgeCount() == 1);
    assert(selectionMesh.setFaceSelected(0, true));
    assert(selectionMesh.selectedFaceCount() == 1);

    selectionMesh.clearSelection();
    assert(selectionMesh.selectedVertexCount() == 0);
    assert(selectionMesh.selectedEdgeCount() == 0);
    assert(selectionMesh.selectedFaceCount() == 0);
    selectionMesh.selectAll(a3d::SelectionMode::Vertex);
    assert(selectionMesh.selectedVertexCount() == 8);
    selectionMesh.invertSelection(a3d::SelectionMode::Vertex);
    assert(selectionMesh.selectedVertexCount() == 0);
    selectionMesh.selectAll(a3d::SelectionMode::Edge);
    assert(selectionMesh.selectedEdgeCount() == 12);
    selectionMesh.invertSelection(a3d::SelectionMode::Edge);
    assert(selectionMesh.selectedEdgeCount() == 0);
    selectionMesh.selectAll(a3d::SelectionMode::Face);
    assert(selectionMesh.selectedFaceCount() == 6);
    selectionMesh.invertSelection(a3d::SelectionMode::Face);
    assert(selectionMesh.selectedFaceCount() == 0);

    selectionMesh.clearSelection();
    assert(selectionMesh.setVertexSelected(0, true));
    const auto vertexCenter = selectionMesh.selectionCenter(a3d::SelectionMode::Vertex);
    assert(vertexCenter.has_value());
    const auto oldVertex = selectionMesh.vertices()[0].position;
    assert(selectionMesh.translateSelected(a3d::SelectionMode::Vertex, {1.0F, 2.0F, 3.0F}));
    const auto movedVertex = selectionMesh.vertices()[0].position;
    assert(std::fabs(movedVertex.x - oldVertex.x - 1.0F) < 0.0001F);
    assert(std::fabs(movedVertex.y - oldVertex.y - 2.0F) < 0.0001F);
    assert(std::fabs(movedVertex.z - oldVertex.z - 3.0F) < 0.0001F);

    selectionMesh.clearSelection();
    assert(selectionMesh.setEdgeSelected(0, 1, true));
    const auto edgeCenter = selectionMesh.selectionCenter(a3d::SelectionMode::Edge);
    assert(edgeCenter.has_value());
    assert(selectionMesh.translateSelected(a3d::SelectionMode::Edge, {0.0F, 1.0F, 0.0F}));

    selectionMesh.clearSelection();
    assert(!selectionMesh.selectionCenter(a3d::SelectionMode::Face).has_value());
    assert(!selectionMesh.translateSelected(a3d::SelectionMode::Face, {1.0F, 0.0F, 0.0F}));

    auto transformMesh = a3d::EditableMesh::plane(2.0F);
    transformMesh.selectAll(a3d::SelectionMode::Vertex);
    const auto centerBefore = transformMesh.selectionCenter(a3d::SelectionMode::Vertex);
    assert(transformMesh.rotateSelected(a3d::SelectionMode::Vertex, 1, 3.14159265F * 0.5F));
    const auto centerAfterRotate = transformMesh.selectionCenter(a3d::SelectionMode::Vertex);
    assert(centerBefore->nearlyEquals(*centerAfterRotate));
    assert(!transformMesh.rotateSelected(a3d::SelectionMode::Vertex, 4, 1.0F));
    assert(transformMesh.scaleSelected(a3d::SelectionMode::Vertex, {2.0F, 1.0F, 0.5F}));
    const auto centerAfterScale = transformMesh.selectionCenter(a3d::SelectionMode::Vertex);
    assert(centerBefore->nearlyEquals(*centerAfterScale));
    assert(!transformMesh.scaleSelected(a3d::SelectionMode::Vertex, {0.0F, 1.0F, 1.0F}));

    auto mergeMesh = a3d::EditableMesh::cube();
    assert(!mergeMesh.mergeSelectedVertices().has_value());
    assert(mergeMesh.setVertexSelected(0, true));
    assert(mergeMesh.setVertexSelected(1, true));
    const auto merged = mergeMesh.mergeSelectedVertices();
    assert(merged.has_value() && *merged == 0);
    assert(mergeMesh.validate());
    assert(mergeMesh.selectedVertexCount() == 1);
    assert(mergeMesh.vertices()[0].position.nearlyEquals({0.0F, -1.0F, -1.0F}));
    assert(mergeMesh.stats().faces == 6);
    assert(mergeMesh.stats().triangles == 10);

    auto duplicateMesh = a3d::EditableMesh::cube();
    assert(duplicateMesh.duplicateSelectedFaces({1.0F,0.0F,0.0F}).empty());
    assert(duplicateMesh.setFaceSelected(0,true));
    assert(duplicateMesh.setFaceSelected(2,true));
    const auto duplicatedFaces = duplicateMesh.duplicateSelectedFaces({0.5F,0.0F,0.0F});
    assert(duplicatedFaces.size() == 2);
    assert(duplicateMesh.validate());
    assert(duplicateMesh.stats().vertices == 14);
    assert(duplicateMesh.stats().faces == 8);
    assert(duplicateMesh.selectedFaceCount() == 2);

    auto bevelMesh = a3d::EditableMesh::plane(4.0F);
    assert(!bevelMesh.bevelFace(0, 0.0F, 0.2F).has_value());
    const auto beveled = bevelMesh.bevelFace(0, 0.25F, 0.5F);
    assert(beveled.has_value());
    assert(bevelMesh.validate());
    assert(bevelMesh.stats().vertices == 8);
    assert(bevelMesh.stats().faces == 5);
    assert(bevelMesh.selectedFaceCount() == 1);
    const auto bevelCenter = bevelMesh.selectionCenter(a3d::SelectionMode::Face);
    assert(bevelCenter.has_value());
    assert(std::fabs(bevelCenter->y) > 0.49F);

    a3d::EditableMesh fillMesh;
    const auto f0=fillMesh.addVertex({-1.0F,0.0F,-1.0F});
    const auto f1=fillMesh.addVertex({1.0F,0.0F,-1.0F});
    const auto f2=fillMesh.addVertex({1.0F,0.0F,1.0F});
    const auto f3=fillMesh.addVertex({-1.0F,0.0F,1.0F});
    assert(!fillMesh.fillSelectedVertices().has_value());
    assert(fillMesh.setVertexSelected(f2,true));
    assert(fillMesh.setVertexSelected(f0,true));
    assert(fillMesh.setVertexSelected(f3,true));
    assert(fillMesh.setVertexSelected(f1,true));
    const auto filled=fillMesh.fillSelectedVertices();
    assert(filled.has_value());
    assert(fillMesh.validate());
    assert(fillMesh.stats().faces==1);
    assert(fillMesh.stats().triangles==2);
    assert(fillMesh.selectedFaceCount()==1);
    return 0;
}
