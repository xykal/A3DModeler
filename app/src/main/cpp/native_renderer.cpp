#include <jni.h>
#include <GLES3/gl31.h>
#include <android/log.h>
#include "a3d/editor_core.hpp"
#include "a3d/editable_mesh.hpp"
#include <algorithm>
#include <array>
#include <set>
#include <cmath>
#include <string>
#include <vector>

namespace {
constexpr char kTag[] = "A3DNative";
constexpr float kPi = 3.14159265358979323846f;

struct Mat4 { float m[16]{}; };
struct Vertex { float x, y, z, r, g, b; };
struct Vec3 { float x, y, z; };
struct FaceDrawRange { a3d::FaceId face; GLsizei firstIndex; GLsizei indexCount; };

GLuint program = 0;
GLuint gridVao = 0, gridVbo = 0;
GLuint cubeVao = 0, cubeVbo = 0, cubeEbo = 0, cubeEdgeEbo = 0;
GLuint pickFbo = 0, pickColor = 0, pickDepth = 0;
GLsizei gridVertexCount = 0, cubeIndexCount = 0, cubeEdgeIndexCount = 0;
GLint mvpLocation = -1, overrideLocation = -1, overrideColorLocation = -1;
bool cubeSelected = false;
a3d::Editor editor;
a3d::EditableMesh editableMesh = a3d::EditableMesh::cube();
a3d::ObjectId cubeObjectId = 0;
a3d::FaceId activeFace = 4;
std::vector<FaceDrawRange> faceDrawRanges;
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
    if (cubeEdgeEbo) glDeleteBuffers(1, &cubeEdgeEbo);
    if (cubeEbo) glDeleteBuffers(1, &cubeEbo);
    if (cubeVbo) glDeleteBuffers(1, &cubeVbo);
    if (cubeVao) glDeleteVertexArrays(1, &cubeVao);
    cubeVao = cubeVbo = cubeEbo = cubeEdgeEbo = 0;

    std::vector<Vertex> vertices;
    vertices.reserve(editableMesh.vertices().size());
    for (const auto& vertex : editableMesh.vertices()) {
        const float shade = std::clamp((vertex.position.y + 1.0f) * 0.16f, 0.0f, 0.32f);
        vertices.push_back({vertex.position.x, vertex.position.y + 1.0f, vertex.position.z,
                            0.30f + shade, 0.50f + shade, 0.92f + shade * 0.2f});
    }

    std::vector<GLushort> triangles;
    std::set<std::pair<GLushort, GLushort>> uniqueEdges;
    faceDrawRanges.clear();
    for (std::size_t faceIndex = 0; faceIndex < editableMesh.faces().size(); ++faceIndex) {
        const auto& face = editableMesh.faces()[faceIndex];
        if (face.deleted || face.vertices.size() < 3U) continue;
        const GLsizei firstIndex = static_cast<GLsizei>(triangles.size());
        const auto first = static_cast<GLushort>(face.vertices[0]);
        for (std::size_t i = 1; i + 1 < face.vertices.size(); ++i) {
            triangles.push_back(first);
            triangles.push_back(static_cast<GLushort>(face.vertices[i]));
            triangles.push_back(static_cast<GLushort>(face.vertices[i + 1]));
        }
        faceDrawRanges.push_back({static_cast<a3d::FaceId>(faceIndex), firstIndex,
                                  static_cast<GLsizei>(triangles.size()) - firstIndex});
        for (std::size_t i = 0; i < face.vertices.size(); ++i) {
            auto a = static_cast<GLushort>(face.vertices[i]);
            auto b = static_cast<GLushort>(face.vertices[(i + 1U) % face.vertices.size()]);
            if (a > b) std::swap(a, b);
            uniqueEdges.emplace(a, b);
        }
    }
    std::vector<GLushort> edges;
    edges.reserve(uniqueEdges.size() * 2U);
    for (const auto& [a, b] : uniqueEdges) { edges.push_back(a); edges.push_back(b); }
    cubeIndexCount = static_cast<GLsizei>(triangles.size());
    cubeEdgeIndexCount = static_cast<GLsizei>(edges.size());

    bindMesh(cubeVao, cubeVbo, vertices.data(), vertices.size() * sizeof(Vertex));
    glGenBuffers(1, &cubeEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(triangles.size() * sizeof(GLushort)),
                 triangles.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &cubeEdgeEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(edges.size() * sizeof(GLushort)),
                 edges.data(), GL_STATIC_DRAW);
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
    glDrawElements(GL_TRIANGLES, cubeIndexCount, GL_UNSIGNED_SHORT, nullptr);
    if (cubeSelected) {
        const auto selected = std::find_if(faceDrawRanges.begin(), faceDrawRanges.end(),
            [](const FaceDrawRange& range) { return range.face == activeFace; });
        if (selected != faceDrawRanges.end()) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -1.0f);
            glUniform1i(overrideLocation, GL_TRUE);
            glUniform3f(overrideColorLocation, 1.0f, 0.48f, 0.06f);
            glDrawElements(GL_TRIANGLES, selected->indexCount, GL_UNSIGNED_SHORT,
                           reinterpret_cast<void*>(static_cast<std::size_t>(selected->firstIndex) * sizeof(GLushort)));
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
        glDisable(GL_CULL_FACE);
        glLineWidth(3.0f);
        glUniform1i(overrideLocation, GL_TRUE);
        glUniform3f(overrideColorLocation, 1.0f, 0.62f, 0.08f);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
        glDrawElements(GL_LINES, cubeEdgeIndexCount, GL_UNSIGNED_SHORT, nullptr);
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
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    for (const auto& range : faceDrawRanges) {
        const std::uint32_t id = static_cast<std::uint32_t>(range.face) + 1U;
        glUniform3f(overrideColorLocation,
                   static_cast<float>(id & 0xFFU) / 255.0f,
                   static_cast<float>((id >> 8U) & 0xFFU) / 255.0f,
                   static_cast<float>((id >> 16U) & 0xFFU) / 255.0f);
        glDrawElements(GL_TRIANGLES, range.indexCount, GL_UNSIGNED_SHORT,
                       reinterpret_cast<void*>(static_cast<std::size_t>(range.firstIndex) * sizeof(GLushort)));
    }
    unsigned char pixel[4]{};
    const int px = std::clamp(static_cast<int>(x), 0, viewportWidth - 1);
    const int py = std::clamp(viewportHeight - 1 - static_cast<int>(y), 0, viewportHeight - 1);
    glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    const std::uint32_t encoded = static_cast<std::uint32_t>(pixel[0]) |
        (static_cast<std::uint32_t>(pixel[1]) << 8U) |
        (static_cast<std::uint32_t>(pixel[2]) << 16U);
    cubeSelected = encoded > 0U;
    if (cubeSelected) activeFace = static_cast<a3d::FaceId>(encoded - 1U);
    editableMesh.clearSelection();
    if (cubeSelected) (void)editableMesh.setFaceSelected(activeFace, true);
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
Java_ai_arena_a3d_NativeRenderer_nativeExtrude(JNIEnv*, jobject, jfloat distance) {
    if (!cubeSelected) return JNI_FALSE;
    editableMesh.clearSelection();
    if (!editableMesh.setFaceSelected(activeFace, true)) return JNI_FALSE;
    const auto result = editableMesh.extrudeFace(activeFace, distance);
    if (!result) return JNI_FALSE;
    activeFace = *result;
    createCube();
    return JNI_TRUE;
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
