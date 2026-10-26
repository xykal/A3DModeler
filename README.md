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
- `armeabi-v7a` and `arm64-v8a` ABI filters
- Android 10 (API 29) minimum

## Build

Requirements: JDK 17, Android SDK 35, NDK, and CMake 3.22.1.

Open the folder in Android Studio and run the `app` configuration, or generate/use a Gradle wrapper and run:

```bash
./gradlew :app:assembleDebug
```

## Next milestone

Add transform gizmos, renderer feature tiers, and unit-testable editor-core modules.
