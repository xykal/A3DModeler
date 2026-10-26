#include <jni.h>
#include <GLES3/gl31.h>
#include <android/log.h>
#include "a3d/editor_core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr char kTag[] = "A3DNative";
constexpr float kPi = 3.14159265358979323846f;

struct Mat4 { float m[16]{}; };
struct Vertex { float x, y, z, r, g, b; };
struct Vec3 { float x, y, z; };

GLuint program = 0;
GLuint gridVao = 0, gridVbo = 0;
GLuint cubeVao = 0, cubeVbo = 0, cubeEbo = 0, cubeEdgeEbo = 0;
GLuint pickFbo = 0, pickColor = 0, pickDepth = 0;
GLsizei gridVertexCount = 0;
GLint mvpLocation = -1, overrideLocation = -1, overrideColorLocation = -1;
bool cubeSelected = false;
a3d::Editor editor;
a3d::ObjectId cubeObjectId = 0;
int viewportWidth = 1, viewportHeight = 1;
float yaw = 0.75f, pitch = 0.48f, distanceToTarget = 6.5f;
Vec3 cameraTarget {0.0f, 1.0f, 0.0f};

constexpr char kVertexShader[] = R"(#version 300 es
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main() {
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vColor = aColor;
})";

constexpr char kFragmentShader[] = R"(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 fragColor;
uniform bool uOverride;
uniform vec3 uOverrideColor;
void main() { fragColor = vec4(uOverride ? uOverrideColor : vColor, 1.0); }
)";

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                out.m[column * 4 + row] += a.m[k * 4 + row] * b.m[column * 4 + k];
    return out;
}

Mat4 translation(float x, float y, float z) {
    Mat4 out;
    out.m[0] = out.m[5] = out.m[10] = out.m[15] = 1.0f;
    out.m[12] = x; out.m[13] = y; out.m[14] = z;
    return out;
}

Mat4 perspective(float fovY, float aspect, float nearPlane, float farPlane) {
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 out;
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return out;
}

Vec3 subtract(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
Vec3 normalize(Vec3 v) { const float l = std::sqrt(dot(v,v)); return {v.x/l,v.y/l,v.z/l}; }

Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 f = normalize(subtract(target, eye));
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 out;
    out.m[0]=s.x; out.m[1]=u.x; out.m[2]=-f.x;
    out.m[4]=s.y; out.m[5]=u.y; out.m[6]=-f.y;
    out.m[8]=s.z; out.m[9]=u.z; out.m[10]=-f.z;
    out.m[12]=-dot(s,eye); out.m[13]=-dot(u,eye); out.m[14]=dot(f,eye); out.m[15]=1.0f;
    return out;
}

GLuint compileShader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Shader error: %s", log);
    }
    return shader;
}

void bindMesh(GLuint& vao, GLuint& vbo, const void* vertices, size_t bytes) {
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void createGrid() {
    std::vector<Vertex> vertices;
    constexpr int half = 10;
    for (int i = -half; i <= half; ++i) {
        const float c = i == 0 ? 0.42f : (i % 5 == 0 ? 0.25f : 0.16f);
        vertices.push_back({static_cast<float>(i), 0, -half, c, c, c});
        vertices.push_back({static_cast<float>(i), 0,  half, c, c, c});
        vertices.push_back({-half, 0, static_cast<float>(i), c, c, c});
        vertices.push_back({ half, 0, static_cast<float>(i), c, c, c});
    }
    // World axes: X red, Z blue.
    vertices.insert(vertices.end(), {{-half,0,0,0.75f,0.18f,0.18f},{half,0,0,0.75f,0.18f,0.18f},
                                     {0,0,-half,0.18f,0.35f,0.85f},{0,0,half,0.18f,0.35f,0.85f}});
    gridVertexCount = static_cast<GLsizei>(vertices.size());
    bindMesh(gridVao, gridVbo, vertices.data(), vertices.size() * sizeof(Vertex));
}

void createCube() {
    constexpr Vertex vertices[] = {
        {-1,0,-1,0.38f,0.58f,1.0f},{1,0,-1,0.38f,0.58f,1.0f},{1,2,-1,0.50f,0.70f,1.0f},{-1,2,-1,0.50f,0.70f,1.0f},
        {-1,0, 1,0.28f,0.48f,0.90f},{1,0, 1,0.28f,0.48f,0.90f},{1,2, 1,0.62f,0.78f,1.0f},{-1,2, 1,0.62f,0.78f,1.0f}
    };
    constexpr GLushort indices[] = {
        0,1,2, 0,2,3, 5,4,7, 5,7,6, 4,0,3, 4,3,7,
        1,5,6, 1,6,2, 3,2,6, 3,6,7, 4,5,1, 4,1,0
    };
    constexpr GLushort edges[] = {
        0,1, 1,2, 2,3, 3,0, 4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };
    bindMesh(cubeVao, cubeVbo, vertices, sizeof(vertices));
    glGenBuffers(1, &cubeEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glGenBuffers(1, &cubeEdgeEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(edges), edges, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glBindVertexArray(0);
}

void createPickTarget(int width, int height) {
    if (pickDepth) glDeleteRenderbuffers(1, &pickDepth);
    if (pickColor) glDeleteTextures(1, &pickColor);
    if (pickFbo) glDeleteFramebuffers(1, &pickFbo);
    glGenFramebuffers(1, &pickFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pickFbo);
    glGenTextures(1, &pickColor);
    glBindTexture(GL_TEXTURE_2D, pickColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickColor, 0);
    glGenRenderbuffers(1, &pickDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, pickDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pickDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Picking framebuffer is incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void releaseResources() {
    if (pickDepth) glDeleteRenderbuffers(1, &pickDepth);
    if (pickColor) glDeleteTextures(1, &pickColor);
    if (pickFbo) glDeleteFramebuffers(1, &pickFbo);
    if (cubeEdgeEbo) glDeleteBuffers(1, &cubeEdgeEbo);
    if (cubeEbo) glDeleteBuffers(1, &cubeEbo);
    if (cubeVbo) glDeleteBuffers(1, &cubeVbo);
    if (gridVbo) glDeleteBuffers(1, &gridVbo);
    if (cubeVao) glDeleteVertexArrays(1, &cubeVao);
    if (gridVao) glDeleteVertexArrays(1, &gridVao);
    if (program) glDeleteProgram(program);
    program = gridVao = gridVbo = cubeVao = cubeVbo = cubeEbo = cubeEdgeEbo = 0;
    pickFbo = pickColor = pickDepth = 0;
}

void createResources() {
    releaseResources();
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    mvpLocation = glGetUniformLocation(program, "uMvp");
    overrideLocation = glGetUniformLocation(program, "uOverride");
    overrideColorLocation = glGetUniformLocation(program, "uOverrideColor");
    createGrid();
    createCube();
}

Mat4 cameraMvp() {
    const float cp = std::cos(pitch);
    const Vec3 eye {cameraTarget.x + distanceToTarget * cp * std::sin(yaw),
                    cameraTarget.y + distanceToTarget * std::sin(pitch),
                    cameraTarget.z + distanceToTarget * cp * std::cos(yaw)};
    const Mat4 view = lookAt(eye, cameraTarget, {0,1,0});
    const Mat4 projection = perspective(50.0f * kPi / 180.0f,
        static_cast<float>(viewportWidth) / static_cast<float>(std::max(1, viewportHeight)), 0.05f, 100.0f);
    return multiply(projection, view);
}
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceCreated(JNIEnv*, jobject) {
    if (cubeObjectId == 0) cubeObjectId = editor.scene().createObject();
    createResources();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    __android_log_print(ANDROID_LOG_INFO, kTag, "Renderer: %s", glGetString(GL_RENDERER));
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    viewportWidth = width; viewportHeight = height;
    glViewport(0, 0, width, height);
    createPickTarget(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDrawFrame(JNIEnv*, jobject) {
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    const Mat4 viewProjection = cameraMvp();
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, viewProjection.m);
    glUniform1i(overrideLocation, GL_FALSE);
    glBindVertexArray(gridVao);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    a3d::Vec3 position{};
    if (const auto* cube = editor.scene().find(cubeObjectId)) position = cube->transform.position;
    const Mat4 cubeMvp = multiply(viewProjection, translation(position.x, position.y, position.z));
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, cubeMvp.m);
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);
    if (cubeSelected) {
        glDisable(GL_CULL_FACE);
        glLineWidth(3.0f);
        glUniform1i(overrideLocation, GL_TRUE);
        glUniform3f(overrideColorLocation, 1.0f, 0.62f, 0.08f);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_SHORT, nullptr);
        glUniform1i(overrideLocation, GL_FALSE);
        glEnable(GL_CULL_FACE);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeOrbit(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    yaw -= dx * 0.006f;
    pitch = std::clamp(pitch - dy * 0.006f, -1.35f, 1.35f);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeZoom(JNIEnv*, jobject, jfloat scale) {
    if (scale > 0.0f) distanceToTarget = std::clamp(distanceToTarget / scale, 2.5f, 25.0f);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativePan(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    const float scale = distanceToTarget * 0.0015f;
    const Vec3 right {std::cos(yaw), 0.0f, -std::sin(yaw)};
    const Vec3 forward {-std::sin(yaw), 0.0f, -std::cos(yaw)};
    cameraTarget.x += (-dx * right.x + dy * forward.x) * scale;
    cameraTarget.z += (-dx * right.z + dy * forward.z) * scale;
    cameraTarget.y += dy * scale * 0.35f;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativePick(JNIEnv*, jobject, jfloat x, jfloat y) {
    if (!pickFbo) return JNI_FALSE;
    glBindFramebuffer(GL_FRAMEBUFFER, pickFbo);
    glViewport(0, 0, viewportWidth, viewportHeight);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    const Mat4 viewProjection = cameraMvp();
    a3d::Vec3 position{};
    if (const auto* cube = editor.scene().find(cubeObjectId)) position = cube->transform.position;
    const Mat4 mvp = multiply(viewProjection, translation(position.x, position.y, position.z));
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, mvp.m);
    glUniform1i(overrideLocation, GL_TRUE);
    glUniform3f(overrideColorLocation, 1.0f, 0.0f, 0.0f);
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);
    unsigned char pixel[4]{};
    const int px = std::clamp(static_cast<int>(x), 0, viewportWidth - 1);
    const int py = std::clamp(viewportHeight - 1 - static_cast<int>(y), 0, viewportHeight - 1);
    glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    cubeSelected = pixel[0] > 127;
    editor.select(cubeSelected ? std::optional<a3d::ObjectId>{cubeObjectId} : std::nullopt);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportWidth, viewportHeight);
    glUniform1i(overrideLocation, GL_FALSE);
    return cubeSelected ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeNudge(JNIEnv*, jobject, jint axis, jfloat amount) {
    if (!cubeSelected) return JNI_FALSE;
    const auto* cube = editor.scene().find(cubeObjectId);
    if (!cube) return JNI_FALSE;
    a3d::Transform next = cube->transform;
    if (axis == 0) next.position.x += amount;
    else if (axis == 1) next.position.y += amount;
    else if (axis == 2) next.position.z += amount;
    else return JNI_FALSE;
    return editor.applyTransform(cubeObjectId, next) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeUndo(JNIEnv*, jobject) {
    return editor.history().undo(editor.scene()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeRedo(JNIEnv*, jobject) {
    return editor.history().redo(editor.scene()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeCapabilities(JNIEnv* env, jobject) {
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::string result = std::string(renderer ? renderer : "Unknown GPU") + " | " +
                         (version ? version : "Unknown OpenGL ES");
    return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeRelease(JNIEnv*, jobject) {
    releaseResources();
}
