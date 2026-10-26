#include "a3d/editor_core.hpp"

#include <cassert>
#include <memory>

int main() {
    a3d::Editor editor;
    const auto cube = editor.scene().createObject();
    editor.select(cube);
    assert(editor.selection() == cube);

    a3d::Transform moved;
    moved.position = {2.0F, 1.0F, -3.0F};
    assert(editor.applyTransform(cube, moved));
    assert(editor.scene().find(cube)->transform.nearlyEquals(moved));
    assert(editor.history().canUndo());

    assert(editor.history().undo(editor.scene()));
    assert(editor.scene().find(cube)->transform.position.nearlyEquals({0.0F, 0.0F, 0.0F}));
    assert(editor.history().redo(editor.scene()));
    assert(editor.scene().find(cube)->transform.nearlyEquals(moved));

    // A new command after undo must invalidate the redo branch.
    assert(editor.history().undo(editor.scene()));
    a3d::Transform scaled;
    scaled.scale = {2.0F, 2.0F, 2.0F};
    assert(editor.applyTransform(cube, scaled));
    assert(!editor.history().canRedo());

    const auto sphere = editor.scene().createObject(a3d::PrimitiveType::UVSphere, "Sphere");
    const auto sphere2 = editor.scene().createObject(a3d::PrimitiveType::UVSphere, "Sphere");
    assert(editor.scene().find(sphere)->name == "Sphere");
    assert(editor.scene().find(sphere2)->name == "Sphere.1");
    assert(editor.scene().renameObject(sphere2, "Detail Sphere"));
    assert(!editor.scene().renameObject(sphere2, "Sphere"));

    editor.scene().find(sphere)->transform.position = {4.0F, 0.0F, 2.0F};
    const auto copy = editor.scene().duplicateObject(sphere);
    assert(copy.has_value());
    assert(editor.scene().find(*copy)->primitive == a3d::PrimitiveType::UVSphere);
    assert(editor.scene().find(*copy)->transform.position.nearlyEquals({4.0F, 0.0F, 2.0F}));

    assert(editor.scene().setVisible(sphere, false));
    assert(!editor.scene().find(sphere)->visible);
    assert(editor.scene().setLocked(sphere, true));
    a3d::Transform blocked;
    blocked.position = {99.0F, 0.0F, 0.0F};
    assert(!editor.applyTransform(sphere, blocked));

    const auto ids = editor.scene().objectIds();
    assert(ids.size() == editor.scene().size());
    for (std::size_t i = 1; i < ids.size(); ++i) assert(ids[i - 1] < ids[i]);
    assert(editor.scene().removeObject(*copy));
    assert(!editor.scene().find(*copy));
    return 0;
}
