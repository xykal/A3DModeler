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
- Procedural editable cube and plane primitives
- Renderer geometry generated from `EditableMesh` instead of a hardcoded cube
- Edit-mode extrusion rebuilds triangle and wireframe GPU buffers at runtime
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
