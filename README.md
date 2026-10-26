# A3D Modeler

Native Android 3D editor technical spike. The current scaffold proves the Compose → GLSurfaceView → JNI → C++/OpenGL ES pipeline on both 32-bit and 64-bit ARM.

## Current state

- Adaptive Compose editor shell
- Native C++ renderer with a rotating test triangle
- OpenGL ES 3.1 compatibility baseline
- `armeabi-v7a` and `arm64-v8a` ABI filters
- Android 10 (API 29) minimum

## Build

Requirements: JDK 17, Android SDK 35, NDK, and CMake 3.22.1.

Open the folder in Android Studio and run the `app` configuration, or generate/use a Gradle wrapper and run:

```bash
./gradlew :app:assembleDebug
```

## Next milestone

Replace the test triangle with camera math, a grid, and an indexed cube; add renderer capability detection and unit-testable editor-core modules.
