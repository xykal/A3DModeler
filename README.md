# A3D Modeler

Native Android 3D editor technical spike. The current scaffold proves the Compose → GLSurfaceView → JNI → C++/OpenGL ES pipeline on both 32-bit and 64-bit ARM.

## Current state

- Adaptive Compose editor shell
- Native C++ OpenGL ES 3.1 renderer
- Perspective camera, world grid, colored axes, and indexed cube
- One-finger orbit, two-finger pan, and pinch-to-zoom navigation
- Runtime GPU/OpenGL ES capability logging
- Lifecycle-safe native GPU resource cleanup
- GPU ID-buffer object picking by tapping the viewport
- Orange wireframe selection highlight
- Platform-independent C++ editor core with scene/object IDs
- Memory-budgeted command history with transform undo/redo
- Host-side editor-core tests in CI
- Renderer cube backed by editor-core transform state
- X/Y/Z move controls with native undo and redo actions
- Editable polygon mesh core with vertex/face IDs, selection, topology validation, normals, statistics, deletion, and face extrusion
- Procedural editable cube, plane, UV sphere, cylinder, and cone primitives
- Multi-object `ModelingScene` core with per-object editable meshes, active object, duplicate, delete, and stable IDs
- Scene-level transform undo/redo and Outliner-ready object summaries with names, state, primitive type, and mesh statistics
- Native renderer state migrated from a standalone mesh/editor pair to `ModelingScene` with an active per-object mesh
- JNI scene bridge for Add Primitive, active-object switching, object duplicate/delete, and Outliner summary data
- Scrollable Add Cube/Plane/Sphere/Cylinder/Cone object controls
- Renderer geometry generated from `EditableMesh` instead of a hardcoded cube
- Edit-mode extrusion rebuilds triangle and wireframe GPU buffers at runtime
- Per-face GPU ID picking with active-face highlight
- Face inset operation with generated inner face and surrounding ring topology
- Memory-budgeted mesh snapshot history for topology undo/redo
- Unified action ordering across object transforms and mesh operations
- Face delete and normal-flip operations with topology history
- Horizontally scrollable modeling tool shelf for phone layouts
- Core Vertex/Edge/Face selection modes with select-all, clear, invert, counts, and topology-aware edge validation
- GPU ID picking and orange viewport highlights for vertices, edges, and faces
- Scrollable Vertex/Edge/Face mode controls plus All/Invert/Clear selection actions
- Interactive screen-plane Move gesture for selected vertices, edges, or faces
- Begin/preview/commit/cancel transform transactions producing one undo entry per drag
- Viewport Move gizmo rendered at selection center with red X, green Y, and blue Z axes
- Free screen-plane movement plus constrained Move X/Y/Z drag modes
- Direct gizmo-handle hit testing in screen space with automatic axis locking
- Rotate X/Y/Z gestures around selection center
- Uniform and per-axis Scale gestures with transaction undo/redo
- Toggleable transform snapping: 0.25-unit move, 15-degree rotate, and 0.1 scale increments
- Additive multi-selection with tap-to-toggle for vertices, topology edges, and faces
- Drag-box selection for vertex, edge-midpoint, and face-centroid workflows
- Merge selected vertices at their shared center with face cleanup and topology undo/redo
- Duplicate selected face islands while preserving shared vertices and selecting the new geometry
- Face bevel using inset-ring topology plus normal-based depth offset, fully undoable
- Fill Face from three or more selected vertices with planar ordering and automatic triangulation
- Triangulate selected n-gons into selected triangle faces with topology history
- Push/PR automation is verification-only; APK/AAB builds are manually gated until the modeling milestone is approved
- `armeabi-v7a` and `arm64-v8a` ABI filters
- Signed release pipeline producing separate 32-bit/64-bit APKs plus Play Store AAB
- R8 code shrinking, resource shrinking, signature verification, and mapping artifacts
- Branded launcher icon and source artwork in `branding/`
- Android 10 (API 29) minimum

## Build

Requirements: JDK 17, Android SDK 35, NDK, and CMake 3.22.1.

Open the folder in Android Studio and run the `app` configuration, or generate/use a Gradle wrapper and run:

```bash
./gradlew :app:assembleDebug
```

## Next milestone

Replace the temporary axis-step controls with draggable Move/Rotate/Scale gizmos and command transactions.
