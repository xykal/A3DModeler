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
    return 0;
}
