#include <jni.h>
#include <GLES3/gl31.h>
#include <android/log.h>
#include "a3d/editor_core.hpp"
#include "a3d/editable_mesh.hpp"
#include "a3d/modeling_scene.hpp"
#include "a3d/obj_io.hpp"
#include "a3d/project_io.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
constexpr char kTag[] = "A3DNative";
constexpr float kPi = 3.14159265358979323846f;

struct Mat4 { float m[16]{}; };
struct Vertex { float x, y, z, r, g, b; };
struct Vec3 { float x, y, z; };
struct FaceDrawRange { a3d::FaceId face; GLsizei firstIndex; GLsizei indexCount; };
struct EdgeDrawRange { a3d::VertexId first; a3d::VertexId second; GLsizei firstIndex; };
struct SceneGpuMesh { GLuint vao{}; GLuint vbo{}; GLuint ebo{}; GLsizei indexCount{}; };

GLuint program = 0;
GLuint gridVao = 0, gridVbo = 0;
GLuint gizmoVao = 0, gizmoVbo = 0;
GLuint cubeVao = 0, cubeVbo = 0, cubeEbo = 0, cubeEdgeEbo = 0;
GLuint pickFbo = 0, pickColor = 0, pickDepth = 0;
GLsizei gridVertexCount = 0, cubeIndexCount = 0, cubeEdgeIndexCount = 0;
GLint mvpLocation = -1, overrideLocation = -1, overrideColorLocation = -1;
bool cubeSelected = false;
enum class ActionKind { ObjectTransform, MeshTopology };

a3d::ModelingScene modelingScene;
a3d::EditableMesh* activeMesh = nullptr;
a3d::MeshHistory meshHistory;
std::vector<ActionKind> undoActions;
std::vector<ActionKind> redoActions;
std::optional<a3d::EditableMesh> transformBefore;
bool transformChanged = false;
int transformAxis = -1;
bool snappingEnabled = false;
bool additiveSelection = false;
float moveSnapStep = 0.25f;
float angleSnapRadians = 15.0f * kPi / 180.0f;
float scaleSnapStep = 0.1f;
Vec3 rawMoveDelta{};
Vec3 appliedMoveDelta{};
float rawTransformScalar = 0.0f;
float appliedTransformScalar = 0.0f;
float appliedScaleFactor = 1.0f;
a3d::ObjectId cubeObjectId = 0;
a3d::FaceId activeFace = 4;
a3d::SelectionMode selectionMode = a3d::SelectionMode::Face;
std::vector<FaceDrawRange> faceDrawRanges;
std::vector<EdgeDrawRange> edgeDrawRanges;
std::unordered_map<a3d::ObjectId, SceneGpuMesh> sceneGpuMeshes;
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
    gl_PointSize = 14.0;
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

void destroySceneGpuMesh(SceneGpuMesh& gpu) {
    if (gpu.ebo) glDeleteBuffers(1,&gpu.ebo);
    if (gpu.vbo) glDeleteBuffers(1,&gpu.vbo);
    if (gpu.vao) glDeleteVertexArrays(1,&gpu.vao);
    gpu={};
}

SceneGpuMesh uploadSceneMesh(const a3d::EditableMesh& mesh) {
    SceneGpuMesh gpu;
    if (mesh.vertices().size()>65535U) return gpu;
    std::vector<Vertex> vertices;
    vertices.reserve(mesh.vertices().size());
    for (const auto& vertex:mesh.vertices())
        vertices.push_back({vertex.position.x,vertex.position.y+1.0f,vertex.position.z,0.32f,0.48f,0.82f});
    std::vector<GLushort> indices;
    for (const auto& face:mesh.faces()) {
        if (face.deleted || face.vertices.size()<3U) continue;
        for (std::size_t i=1;i+1<face.vertices.size();++i) {
            indices.push_back(static_cast<GLushort>(face.vertices[0]));
            indices.push_back(static_cast<GLushort>(face.vertices[i]));
            indices.push_back(static_cast<GLushort>(face.vertices[i+1]));
        }
    }
    if (vertices.empty() || indices.empty()) return gpu;
    bindMesh(gpu.vao,gpu.vbo,vertices.data(),vertices.size()*sizeof(Vertex));
    glGenBuffers(1,&gpu.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(indices.size()*sizeof(GLushort)),indices.data(),GL_STATIC_DRAW);
    gpu.indexCount=static_cast<GLsizei>(indices.size());
    glBindVertexArray(0);
    return gpu;
}

void refreshSceneGpuMeshes() {
    for (auto& [id,gpu]:sceneGpuMeshes) { (void)id; destroySceneGpuMesh(gpu); }
    sceneGpuMeshes.clear();
    for (const auto id:modelingScene.objectIds())
        if (const auto* mesh=modelingScene.mesh(id)) sceneGpuMeshes.emplace(id,uploadSceneMesh(*mesh));
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

void createGizmo() {
    constexpr Vertex axes[] = {
        {0,0,0,1.0f,0.15f,0.12f},{1.5f,0,0,1.0f,0.15f,0.12f},
        {0,0,0,0.20f,0.95f,0.30f},{0,1.5f,0,0.20f,0.95f,0.30f},
        {0,0,0,0.20f,0.45f,1.0f},{0,0,1.5f,0.20f,0.45f,1.0f}
    };
    bindMesh(gizmoVao, gizmoVbo, axes, sizeof(axes));
}

bool hasMeshSelection() {
    return activeMesh->selectedVertexCount() > 0U || activeMesh->selectedEdgeCount() > 0U ||
           activeMesh->selectedFaceCount() > 0U;
}

void recordAction(ActionKind kind) {
    undoActions.push_back(kind);
    redoActions.clear();
}

void syncActiveFace() {
    for (std::size_t i = 0; i < activeMesh->faces().size(); ++i) {
        const auto& face = activeMesh->faces()[i];
        if (!face.deleted && face.selected) {
            activeFace = static_cast<a3d::FaceId>(i);
            return;
        }
    }
    for (std::size_t i = 0; i < activeMesh->faces().size(); ++i) {
        if (!activeMesh->faces()[i].deleted) {
            activeFace = static_cast<a3d::FaceId>(i);
            return;
        }
    }
}

void createCube() {
    if (cubeEdgeEbo) glDeleteBuffers(1, &cubeEdgeEbo);
    if (cubeEbo) glDeleteBuffers(1, &cubeEbo);
    if (cubeVbo) glDeleteBuffers(1, &cubeVbo);
    if (cubeVao) glDeleteVertexArrays(1, &cubeVao);
    cubeVao = cubeVbo = cubeEbo = cubeEdgeEbo = 0;

    std::vector<Vertex> vertices;
    vertices.reserve(activeMesh->vertices().size());
    for (const auto& vertex : activeMesh->vertices()) {
        const float shade = std::clamp((vertex.position.y + 1.0f) * 0.16f, 0.0f, 0.32f);
        vertices.push_back({vertex.position.x, vertex.position.y + 1.0f, vertex.position.z,
                            0.30f + shade, 0.50f + shade, 0.92f + shade * 0.2f});
    }

    std::vector<GLushort> triangles;
    std::set<std::pair<GLushort, GLushort>> uniqueEdges;
    faceDrawRanges.clear();
    for (std::size_t faceIndex = 0; faceIndex < activeMesh->faces().size(); ++faceIndex) {
        const auto& face = activeMesh->faces()[faceIndex];
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
    edgeDrawRanges.clear();
    for (const auto& [a, b] : uniqueEdges) {
        const GLsizei firstIndex = static_cast<GLsizei>(edges.size());
        edges.push_back(a); edges.push_back(b);
        edgeDrawRanges.push_back({a, b, firstIndex});
    }
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

bool activateObject(a3d::ObjectId id) {
    auto* mesh=modelingScene.mesh(id);
    if (!mesh || !modelingScene.selectObject(id)) return false;
    refreshSceneGpuMeshes();
    cubeObjectId=id;
    activeMesh=mesh;
    meshHistory.clear();
    undoActions.clear(); redoActions.clear();
    transformBefore.reset(); transformChanged=false; transformAxis=-1;
    cubeSelected=hasMeshSelection();
    syncActiveFace();
    createCube();
    return true;
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
    for (auto& [id,gpu]:sceneGpuMeshes) { (void)id; destroySceneGpuMesh(gpu); }
    sceneGpuMeshes.clear();
    if (pickDepth) glDeleteRenderbuffers(1, &pickDepth);
    if (pickColor) glDeleteTextures(1, &pickColor);
    if (pickFbo) glDeleteFramebuffers(1, &pickFbo);
    if (cubeEdgeEbo) glDeleteBuffers(1, &cubeEdgeEbo);
    if (cubeEbo) glDeleteBuffers(1, &cubeEbo);
    if (cubeVbo) glDeleteBuffers(1, &cubeVbo);
    if (gizmoVbo) glDeleteBuffers(1, &gizmoVbo);
    if (gridVbo) glDeleteBuffers(1, &gridVbo);
    if (cubeVao) glDeleteVertexArrays(1, &cubeVao);
    if (gizmoVao) glDeleteVertexArrays(1, &gizmoVao);
    if (gridVao) glDeleteVertexArrays(1, &gridVao);
    if (program) glDeleteProgram(program);
    program = gridVao = gridVbo = gizmoVao = gizmoVbo = cubeVao = cubeVbo = cubeEbo = cubeEdgeEbo = 0;
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
    createGizmo();
    createCube();
    refreshSceneGpuMeshes();
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

std::optional<Vec3> projectToScreen(Vec3 point) {
    const Mat4 mvp = cameraMvp();
    const float clipX = mvp.m[0]*point.x + mvp.m[4]*point.y + mvp.m[8]*point.z + mvp.m[12];
    const float clipY = mvp.m[1]*point.x + mvp.m[5]*point.y + mvp.m[9]*point.z + mvp.m[13];
    const float clipW = mvp.m[3]*point.x + mvp.m[7]*point.y + mvp.m[11]*point.z + mvp.m[15];
    if (clipW <= 0.0001f) return std::nullopt;
    return Vec3{(clipX/clipW*0.5f+0.5f)*viewportWidth,
                (1.0f-(clipY/clipW*0.5f+0.5f))*viewportHeight, 0.0f};
}

float pointSegmentDistance(float px, float py, Vec3 a, Vec3 b) {
    const float dx=b.x-a.x, dy=b.y-a.y;
    const float length2=dx*dx+dy*dy;
    const float t=length2 > 0.0001f ? std::clamp(((px-a.x)*dx+(py-a.y)*dy)/length2,0.0f,1.0f) : 0.0f;
    const float ex=px-(a.x+t*dx), ey=py-(a.y+t*dy);
    return std::sqrt(ex*ex+ey*ey);
}

int pickGizmoAxis(float x, float y) {
    const auto center = activeMesh->selectionCenter(selectionMode);
    if (!center) return -1;
    a3d::Vec3 objectPosition{};
    if (const auto* object = modelingScene.object(cubeObjectId)) objectPosition = object->transform.position;
    const Vec3 origin{objectPosition.x+center->x, objectPosition.y+center->y+1.0f, objectPosition.z+center->z};
    const auto screenOrigin=projectToScreen(origin);
    if (!screenOrigin) return -1;
    constexpr Vec3 axes[]={{1.5f,0,0},{0,1.5f,0},{0,0,1.5f}};
    int best=-1; float bestDistance=32.0f;
    for (int axis=0; axis<3; ++axis) {
        const auto endpoint=projectToScreen({origin.x+axes[axis].x,origin.y+axes[axis].y,origin.z+axes[axis].z});
        if (!endpoint) continue;
        const float distance=pointSegmentDistance(x,y,*screenOrigin,*endpoint);
        if (distance<bestDistance) { bestDistance=distance; best=axis; }
    }
    return best;
}
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceCreated(JNIEnv*, jobject) {
    if (cubeObjectId == 0) cubeObjectId = modelingScene.addPrimitive(a3d::PrimitiveType::Cube, "Cube");
    activeMesh = modelingScene.mesh(cubeObjectId);
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

extern "C" JNIEXPORT jlong JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeAddPrimitive(JNIEnv*, jobject, jint type) {
    const auto primitive=type==1?a3d::PrimitiveType::Plane:type==2?a3d::PrimitiveType::UVSphere:
        type==3?a3d::PrimitiveType::Cylinder:type==4?a3d::PrimitiveType::Cone:a3d::PrimitiveType::Cube;
    const char* name=type==1?"Plane":type==2?"Sphere":type==3?"Cylinder":type==4?"Cone":"Cube";
    const auto id=modelingScene.addPrimitive(primitive,name);
    if (auto* object=modelingScene.object(id)) object->transform.position.x=
        static_cast<float>(modelingScene.objectIds().size()-1U)*2.5f;
    if (!activateObject(id)) return 0;
    return static_cast<jlong>(id);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSelectObject(JNIEnv*, jobject, jlong id) {
    return activateObject(static_cast<a3d::ObjectId>(id))?JNI_TRUE:JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDuplicateObject(JNIEnv*, jobject) {
    const auto copy=modelingScene.duplicateObject(cubeObjectId);
    if (!copy) return 0;
    if (auto* object=modelingScene.object(*copy)) object->transform.position.x+=1.0f;
    if (!activateObject(*copy)) return 0;
    return static_cast<jlong>(*copy);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDeleteObject(JNIEnv*, jobject) {
    if (modelingScene.objectIds().size()<=1U) return JNI_FALSE;
    const auto deleting=cubeObjectId;
    if (!modelingScene.removeObject(deleting)) return JNI_FALSE;
    const auto active=modelingScene.activeObject();
    return active && activateObject(*active)?JNI_TRUE:JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSetObjectVisible(JNIEnv*, jobject, jlong id, jboolean visible) {
    return modelingScene.scene().setVisible(static_cast<a3d::ObjectId>(id),visible==JNI_TRUE)?JNI_TRUE:JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSetObjectLocked(JNIEnv*, jobject, jlong id, jboolean locked) {
    return modelingScene.scene().setLocked(static_cast<a3d::ObjectId>(id),locked==JNI_TRUE)?JNI_TRUE:JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeRenameObject(JNIEnv* env, jobject, jlong id, jstring name) {
    const char* raw=env->GetStringUTFChars(name,nullptr);
    const std::string candidate=raw?raw:"";
    const bool safe=!candidate.empty() && candidate.find_first_of("|\r\n")==std::string::npos;
    const bool renamed=safe && modelingScene.scene().renameObject(static_cast<a3d::ObjectId>(id),candidate);
    if (raw) env->ReleaseStringUTFChars(name,raw);
    return renamed?JNI_TRUE:JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeExportObj(JNIEnv* env, jobject) {
    const std::string text=a3d::exportObj(modelingScene);
    if (text.size()>static_cast<std::size_t>(std::numeric_limits<jsize>::max())) return nullptr;
    auto result=env->NewByteArray(static_cast<jsize>(text.size()));
    if (result && !text.empty()) env->SetByteArrayRegion(result,0,static_cast<jsize>(text.size()),reinterpret_cast<const jbyte*>(text.data()));
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeImportObj(JNIEnv* env, jobject, jbyteArray input) {
    if (!input) return JNI_FALSE;
    const jsize size=env->GetArrayLength(input);
    if (size<=0 || size>64*1024*1024) return JNI_FALSE;
    std::string text(static_cast<std::size_t>(size),'\0');
    env->GetByteArrayRegion(input,0,size,reinterpret_cast<jbyte*>(text.data()));
    if (env->ExceptionCheck()) return JNI_FALSE;
    std::string error;
    auto mesh=a3d::importObj(text,&error);
    if (!mesh) return JNI_FALSE;
    const auto id=modelingScene.addMesh(std::move(*mesh),"Imported OBJ");
    return activateObject(id)?JNI_TRUE:JNI_FALSE;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSaveProject(JNIEnv* env, jobject) {
    std::ostringstream output(std::ios::binary);
    if (!a3d::ProjectIo::save(modelingScene,output)) return nullptr;
    const std::string bytes=output.str();
    if (bytes.size()>static_cast<std::size_t>(std::numeric_limits<jsize>::max())) return nullptr;
    jbyteArray result=env->NewByteArray(static_cast<jsize>(bytes.size()));
    if (!result) return nullptr;
    env->SetByteArrayRegion(result,0,static_cast<jsize>(bytes.size()),reinterpret_cast<const jbyte*>(bytes.data()));
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeLoadProject(JNIEnv* env, jobject, jbyteArray data) {
    if (!data) return JNI_FALSE;
    const jsize size=env->GetArrayLength(data);
    if (size<=0 || size>256*1024*1024) return JNI_FALSE;
    std::string bytes(static_cast<std::size_t>(size),'\0');
    env->GetByteArrayRegion(data,0,size,reinterpret_cast<jbyte*>(bytes.data()));
    std::istringstream input(bytes,std::ios::binary);
    auto loaded=a3d::ProjectIo::load(input);
    if (!loaded || loaded.scene->objectIds().empty()) return JNI_FALSE;
    releaseResources();
    modelingScene=std::move(*loaded.scene);
    const auto active=modelingScene.activeObject().value_or(modelingScene.objectIds().front());
    cubeObjectId=active;
    activeMesh=modelingScene.mesh(active);
    meshHistory.clear(); undoActions.clear(); redoActions.clear();
    transformBefore.reset(); transformChanged=false; transformAxis=-1;
    createResources();
    createPickTarget(viewportWidth,viewportHeight);
    (void)modelingScene.selectObject(active);
    cubeSelected=hasMeshSelection();
    syncActiveFace();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSceneSummary(JNIEnv* env, jobject) {
    std::ostringstream output;
    bool first=true;
    for (const auto& item:modelingScene.summaries()) {
        if (!first) output<<'\n';
        first=false;
        output<<item.id<<'|'<<item.name<<'|'<<static_cast<int>(item.primitive)<<'|'
              <<item.visible<<'|'<<item.locked<<'|'<<item.active<<'|'
              <<item.meshStats.vertices<<'|'<<item.meshStats.edges<<'|'
              <<item.meshStats.faces<<'|'<<item.meshStats.triangles;
    }
    return env->NewStringUTF(output.str().c_str());
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
    for (const auto id:modelingScene.objectIds()) {
        if (id==cubeObjectId) continue;
        const auto* object=modelingScene.object(id);
        const auto found=sceneGpuMeshes.find(id);
        if (!object || !object->visible || found==sceneGpuMeshes.end() || !found->second.vao) continue;
        const auto& gpu=found->second;
        const Mat4 objectMvp=multiply(viewProjection,translation(object->transform.position.x,
            object->transform.position.y,object->transform.position.z));
        glUniformMatrix4fv(mvpLocation,1,GL_FALSE,objectMvp.m);
        glUniform1i(overrideLocation,GL_FALSE);
        glBindVertexArray(gpu.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gpu.ebo);
        glDrawElements(GL_TRIANGLES,gpu.indexCount,GL_UNSIGNED_SHORT,nullptr);
    }
    a3d::Vec3 position{};
    if (const auto* cube = modelingScene.object(cubeObjectId)) position = cube->transform.position;
    const Mat4 cubeMvp = multiply(viewProjection, translation(position.x, position.y, position.z));
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, cubeMvp.m);
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glDrawElements(GL_TRIANGLES, cubeIndexCount, GL_UNSIGNED_SHORT, nullptr);
    if (cubeSelected) {
        glUniform1i(overrideLocation, GL_TRUE);
        glUniform3f(overrideColorLocation, 1.0f, 0.48f, 0.06f);
        if (selectionMode == a3d::SelectionMode::Face) {
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1.0f, -1.0f);
            for (const auto& range : faceDrawRanges) {
                if (range.face < activeMesh->faces().size() && activeMesh->faces()[range.face].selected)
                    glDrawElements(GL_TRIANGLES, range.indexCount, GL_UNSIGNED_SHORT,
                        reinterpret_cast<void*>(static_cast<std::size_t>(range.firstIndex) * sizeof(GLushort)));
            }
            glDisable(GL_POLYGON_OFFSET_FILL);
        } else if (selectionMode == a3d::SelectionMode::Edge) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
            glLineWidth(5.0f);
            for (const auto& edge : edgeDrawRanges) {
                if (activeMesh->isEdgeSelected(edge.first, edge.second))
                    glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT,
                        reinterpret_cast<void*>(static_cast<std::size_t>(edge.firstIndex) * sizeof(GLushort)));
            }
        } else {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            for (std::size_t i = 0; i < activeMesh->vertices().size(); ++i)
                if (activeMesh->vertices()[i].selected) glDrawArrays(GL_POINTS, static_cast<GLint>(i), 1);
        }
        glDisable(GL_CULL_FACE);
        glLineWidth(2.0f);
        glUniform3f(overrideColorLocation, 1.0f, 0.62f, 0.08f);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
        glDrawElements(GL_LINES, cubeEdgeIndexCount, GL_UNSIGNED_SHORT, nullptr);
        glUniform1i(overrideLocation, GL_FALSE);
        glEnable(GL_CULL_FACE);

        if (const auto center = activeMesh->selectionCenter(selectionMode)) {
            const Mat4 gizmoMvp = multiply(viewProjection,
                translation(position.x + center->x, position.y + center->y + 1.0f, position.z + center->z));
            glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, gizmoMvp.m);
            glBindVertexArray(gizmoVao);
            glLineWidth(5.0f);
            glDrawArrays(GL_LINES, 0, 6);
        }
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
    if (const auto* cube = modelingScene.object(cubeObjectId)) position = cube->transform.position;
    const Mat4 mvp = multiply(viewProjection, translation(position.x, position.y, position.z));
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, mvp.m);
    glUniform1i(overrideLocation, GL_TRUE);
    glBindVertexArray(cubeVao);
    const auto setPickColor = [](std::uint32_t id) {
        glUniform3f(overrideColorLocation,
                   static_cast<float>(id & 0xFFU) / 255.0f,
                   static_cast<float>((id >> 8U) & 0xFFU) / 255.0f,
                   static_cast<float>((id >> 16U) & 0xFFU) / 255.0f);
    };
    if (selectionMode == a3d::SelectionMode::Face) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
        for (const auto& range : faceDrawRanges) {
            setPickColor(static_cast<std::uint32_t>(range.face) + 1U);
            glDrawElements(GL_TRIANGLES, range.indexCount, GL_UNSIGNED_SHORT,
                           reinterpret_cast<void*>(static_cast<std::size_t>(range.firstIndex) * sizeof(GLushort)));
        }
    } else if (selectionMode == a3d::SelectionMode::Edge) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEdgeEbo);
        glLineWidth(10.0f);
        for (std::size_t i = 0; i < edgeDrawRanges.size(); ++i) {
            setPickColor(static_cast<std::uint32_t>(i) + 1U);
            glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT,
                           reinterpret_cast<void*>(static_cast<std::size_t>(edgeDrawRanges[i].firstIndex) * sizeof(GLushort)));
        }
    } else {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        for (std::size_t i = 0; i < activeMesh->vertices().size(); ++i) {
            setPickColor(static_cast<std::uint32_t>(i) + 1U);
            glDrawArrays(GL_POINTS, static_cast<GLint>(i), 1);
        }
    }
    unsigned char pixel[4]{};
    const int px = std::clamp(static_cast<int>(x), 0, viewportWidth - 1);
    const int py = std::clamp(viewportHeight - 1 - static_cast<int>(y), 0, viewportHeight - 1);
    glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    const std::uint32_t encoded = static_cast<std::uint32_t>(pixel[0]) |
        (static_cast<std::uint32_t>(pixel[1]) << 8U) |
        (static_cast<std::uint32_t>(pixel[2]) << 16U);
    if (!additiveSelection) activeMesh->clearSelection();
    if (encoded > 0U) {
        const std::uint32_t picked = encoded - 1U;
        if (selectionMode == a3d::SelectionMode::Face && picked < activeMesh->faces().size()) {
            activeFace = static_cast<a3d::FaceId>(picked);
            const bool next = additiveSelection ? !activeMesh->faces()[picked].selected : true;
            (void)activeMesh->setFaceSelected(activeFace, next);
        } else if (selectionMode == a3d::SelectionMode::Edge && picked < edgeDrawRanges.size()) {
            const auto& edge = edgeDrawRanges[picked];
            const bool next = additiveSelection ? !activeMesh->isEdgeSelected(edge.first, edge.second) : true;
            (void)activeMesh->setEdgeSelected(edge.first, edge.second, next);
        } else if (selectionMode == a3d::SelectionMode::Vertex && picked < activeMesh->vertices().size()) {
            const auto vertex = static_cast<a3d::VertexId>(picked);
            const bool next = additiveSelection ? !activeMesh->vertices()[picked].selected : true;
            (void)activeMesh->setVertexSelected(vertex, next);
        }
    }
    cubeSelected = hasMeshSelection();
    (void)modelingScene.selectObject(cubeSelected ? std::optional<a3d::ObjectId>{cubeObjectId} : std::nullopt);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, viewportWidth, viewportHeight);
    glUniform1i(overrideLocation, GL_FALSE);
    return cubeSelected ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeBoxSelect(JNIEnv*, jobject, jfloat x1, jfloat y1, jfloat x2, jfloat y2) {
    const float left=std::min(x1,x2), right=std::max(x1,x2);
    const float top=std::min(y1,y2), bottom=std::max(y1,y2);
    const auto inside=[&](const std::optional<Vec3>& p) {
        return p && p->x>=left && p->x<=right && p->y>=top && p->y<=bottom;
    };
    a3d::Vec3 objectPosition{};
    if (const auto* object=modelingScene.object(cubeObjectId)) objectPosition=object->transform.position;
    const auto projectVertex=[&](a3d::VertexId id) {
        const auto p=activeMesh->vertices()[id].position;
        return projectToScreen({objectPosition.x+p.x,objectPosition.y+p.y+1.0f,objectPosition.z+p.z});
    };
    if (!additiveSelection) activeMesh->clearSelection();
    if (selectionMode==a3d::SelectionMode::Vertex) {
        for (std::size_t i=0;i<activeMesh->vertices().size();++i)
            if (inside(projectVertex(static_cast<a3d::VertexId>(i))))
                (void)activeMesh->setVertexSelected(static_cast<a3d::VertexId>(i),true);
    } else if (selectionMode==a3d::SelectionMode::Edge) {
        for (const auto& edge:edgeDrawRanges) {
            const auto a=projectVertex(edge.first), b=projectVertex(edge.second);
            if (a && b && inside(Vec3{(a->x+b->x)*0.5f,(a->y+b->y)*0.5f,0}))
                (void)activeMesh->setEdgeSelected(edge.first,edge.second,true);
        }
    } else {
        for (std::size_t i=0;i<activeMesh->faces().size();++i) {
            const auto& face=activeMesh->faces()[i];
            if (face.deleted) continue;
            Vec3 center{}; int count=0;
            for (const auto id:face.vertices) if (const auto p=projectVertex(id)) { center.x+=p->x; center.y+=p->y; ++count; }
            if (count>0) { center.x/=count; center.y/=count; if (inside(center)) (void)activeMesh->setFaceSelected(static_cast<a3d::FaceId>(i),true); }
        }
    }
    cubeSelected=hasMeshSelection();
    (void)modelingScene.selectObject(cubeSelected ? std::optional<a3d::ObjectId>{cubeObjectId} : std::nullopt);
    return cubeSelected ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeNudge(JNIEnv*, jobject, jint axis, jfloat amount) {
    if (!cubeSelected) return JNI_FALSE;
    const auto* cube = modelingScene.object(cubeObjectId);
    if (!cube) return JNI_FALSE;
    a3d::Transform next = cube->transform;
    if (axis == 0) next.position.x += amount;
    else if (axis == 1) next.position.y += amount;
    else if (axis == 2) next.position.z += amount;
    else return JNI_FALSE;
    if (!modelingScene.applyTransform(cubeObjectId, next)) return JNI_FALSE;
    recordAction(ActionKind::ObjectTransform);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeExtrude(JNIEnv*, jobject, jfloat distance) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    activeMesh->clearSelection();
    if (!activeMesh->setFaceSelected(activeFace, true)) return JNI_FALSE;
    const auto result = activeMesh->extrudeFace(activeFace, distance);
    if (!result) return JNI_FALSE;
    activeFace = *result;
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeInset(JNIEnv*, jobject, jfloat amount) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    activeMesh->clearSelection();
    if (!activeMesh->setFaceSelected(activeFace, true)) return JNI_FALSE;
    const auto result = activeMesh->insetFace(activeFace, amount);
    if (!result) return JNI_FALSE;
    activeFace = *result;
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeFillFace(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Vertex) return JNI_FALSE;
    const a3d::EditableMesh before=*activeMesh;
    const auto result=activeMesh->fillSelectedVertices();
    if (!result) return JNI_FALSE;
    activeFace=*result;
    selectionMode=a3d::SelectionMode::Face;
    if (!meshHistory.commit(before,*activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeTriangulateFaces(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before=*activeMesh;
    const auto created=activeMesh->triangulateSelectedFaces();
    if (created.empty()) return JNI_FALSE;
    activeFace=created.back();
    if (!meshHistory.commit(before,*activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSubdivideFaces(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before=*activeMesh;
    const auto created=activeMesh->subdivideSelectedFaces();
    if (created.empty()) return JNI_FALSE;
    activeFace=created.back();
    if (!meshHistory.commit(before,*activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology); createCube(); return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDissolveFaces(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before=*activeMesh;
    const auto result=activeMesh->dissolveSelectedFaces();
    if (!result) return JNI_FALSE;
    activeFace=*result;
    if (!meshHistory.commit(before,*activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology); createCube(); return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDuplicateFaces(JNIEnv*, jobject, jfloat offset) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    const auto created = activeMesh->duplicateSelectedFaces({offset,0.0f,0.0f});
    if (created.empty()) return JNI_FALSE;
    activeFace = created.back();
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeMergeVertices(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Vertex) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    if (!activeMesh->mergeSelectedVertices()) return JNI_FALSE;
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeBevelFace(JNIEnv*, jobject, jfloat amount, jfloat depth) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    const auto result = activeMesh->bevelFace(activeFace, amount, depth);
    if (!result) return JNI_FALSE;
    activeFace = *result;
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSeparateFaces(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const auto separated=modelingScene.separateSelectedFaces(cubeObjectId);
    if (!separated || !activateObject(*separated)) return JNI_FALSE;
    cubeSelected=hasMeshSelection();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDeleteFace(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    if (!activeMesh->deleteFace(activeFace)) return JNI_FALSE;
    syncActiveFace();
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeFlipFace(JNIEnv*, jobject) {
    if (!cubeSelected || selectionMode != a3d::SelectionMode::Face) return JNI_FALSE;
    const a3d::EditableMesh before = *activeMesh;
    if (!activeMesh->flipFace(activeFace)) return JNI_FALSE;
    if (!meshHistory.commit(before, *activeMesh)) return JNI_FALSE;
    recordAction(ActionKind::MeshTopology);
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSetSelectionMode(JNIEnv*, jobject, jint mode) {
    selectionMode = mode == 0 ? a3d::SelectionMode::Vertex
                  : mode == 1 ? a3d::SelectionMode::Edge
                              : a3d::SelectionMode::Face;
    activeMesh->clearSelection();
    cubeSelected = false;
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSetAdditiveSelection(JNIEnv*, jobject, jboolean enabled) {
    additiveSelection = enabled == JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSetSnapping(JNIEnv*, jobject, jboolean enabled, jfloat moveStep,
                                                    jfloat angleDegrees, jfloat scaleStep) {
    snappingEnabled = enabled == JNI_TRUE;
    if (moveStep > 0.0001f) moveSnapStep = moveStep;
    if (angleDegrees > 0.0001f) angleSnapRadians = angleDegrees * kPi / 180.0f;
    if (scaleStep > 0.0001f) scaleSnapStep = scaleStep;
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSelectAll(JNIEnv*, jobject) {
    activeMesh->selectAll(selectionMode);
    cubeSelected = true;
    (void)modelingScene.selectObject(cubeObjectId);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeInvertSelection(JNIEnv*, jobject) {
    activeMesh->invertSelection(selectionMode);
    cubeSelected = hasMeshSelection();
    (void)modelingScene.selectObject(cubeSelected ? std::optional<a3d::ObjectId>{cubeObjectId} : std::nullopt);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeClearSelection(JNIEnv*, jobject) {
    activeMesh->clearSelection();
    cubeSelected = false;
    (void)modelingScene.selectObject(std::nullopt);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeBeginMeshTransform(JNIEnv*, jobject, jfloat x, jfloat y, jint preferredAxis) {
    if (transformBefore || !activeMesh->selectionCenter(selectionMode).has_value()) return JNI_FALSE;
    transformAxis = preferredAxis >= 0 && preferredAxis <= 2 ? preferredAxis : pickGizmoAxis(x, y);
    transformBefore = *activeMesh;
    transformChanged = false;
    rawMoveDelta = {}; appliedMoveDelta = {};
    rawTransformScalar = appliedTransformScalar = 0.0f;
    appliedScaleFactor = 1.0f;
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeTranslateSelection(JNIEnv*, jobject, jfloat x, jfloat y, jfloat z) {
    if (!transformBefore || !activeMesh->translateSelected(selectionMode, {x, y, z})) return JNI_FALSE;
    transformChanged = true;
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDragMeshTransform(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    const float horizontal = dx * 0.01f;
    const float vertical = -dy * 0.01f;
    const Vec3 input = transformAxis == 0 ? Vec3{horizontal,0,0}
                     : transformAxis == 1 ? Vec3{0,vertical,0}
                     : transformAxis == 2 ? Vec3{0,0,horizontal}
                                          : Vec3{horizontal,vertical,0};
    if (!transformBefore) return JNI_FALSE;
    rawMoveDelta = {rawMoveDelta.x+input.x, rawMoveDelta.y+input.y, rawMoveDelta.z+input.z};
    const auto snap = [](float value, float step) { return std::round(value/step)*step; };
    const Vec3 target = snappingEnabled
        ? Vec3{snap(rawMoveDelta.x,moveSnapStep),snap(rawMoveDelta.y,moveSnapStep),snap(rawMoveDelta.z,moveSnapStep)}
        : rawMoveDelta;
    const Vec3 delta{target.x-appliedMoveDelta.x,target.y-appliedMoveDelta.y,target.z-appliedMoveDelta.z};
    if (std::fabs(delta.x)+std::fabs(delta.y)+std::fabs(delta.z) < 0.00001f) return JNI_TRUE;
    if (!activeMesh->translateSelected(selectionMode, {delta.x,delta.y,delta.z})) return JNI_FALSE;
    appliedMoveDelta = target;
    transformChanged = true;
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDragRotateTransform(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    if (!transformBefore) return JNI_FALSE;
    const int axis = transformAxis >= 0 ? transformAxis : 1;
    rawTransformScalar += (dx - dy) * 0.01f;
    const float target = snappingEnabled
        ? std::round(rawTransformScalar/angleSnapRadians)*angleSnapRadians : rawTransformScalar;
    const float delta = target-appliedTransformScalar;
    if (std::fabs(delta) < 0.00001f) return JNI_TRUE;
    if (!activeMesh->rotateSelected(selectionMode, axis, delta)) return JNI_FALSE;
    appliedTransformScalar = target;
    transformChanged = true;
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDragScaleTransform(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    if (!transformBefore) return JNI_FALSE;
    rawTransformScalar += (dx - dy) * 0.005f;
    float targetFactor = std::exp(rawTransformScalar);
    if (snappingEnabled) targetFactor = std::max(scaleSnapStep,
        std::round(targetFactor/scaleSnapStep)*scaleSnapStep);
    const float factor = targetFactor/appliedScaleFactor;
    if (std::fabs(factor-1.0f) < 0.00001f) return JNI_TRUE;
    const Vec3 factors = transformAxis == 0 ? Vec3{factor,1,1}
                       : transformAxis == 1 ? Vec3{1,factor,1}
                       : transformAxis == 2 ? Vec3{1,1,factor}
                                            : Vec3{factor,factor,factor};
    if (!activeMesh->scaleSelected(selectionMode, {factors.x,factors.y,factors.z})) return JNI_FALSE;
    appliedScaleFactor = targetFactor;
    transformChanged = true;
    createCube();
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeEndMeshTransform(JNIEnv*, jobject) {
    if (!transformBefore) return JNI_FALSE;
    bool committed = false;
    if (transformChanged) {
        committed = meshHistory.commit(*transformBefore, *activeMesh);
        if (committed) recordAction(ActionKind::MeshTopology);
    }
    transformBefore.reset();
    transformChanged = false;
    transformAxis = -1;
    return committed ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeCancelMeshTransform(JNIEnv*, jobject) {
    if (transformBefore) {
        *activeMesh = *transformBefore;
        createCube();
    }
    transformBefore.reset();
    transformChanged = false;
    transformAxis = -1;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeUndo(JNIEnv*, jobject) {
    if (undoActions.empty()) return JNI_FALSE;
    const ActionKind kind = undoActions.back();
    const bool changed = kind == ActionKind::MeshTopology
        ? meshHistory.undo(*activeMesh)
        : modelingScene.undoTransform();
    if (!changed) return JNI_FALSE;
    undoActions.pop_back();
    redoActions.push_back(kind);
    if (kind == ActionKind::MeshTopology) { syncActiveFace(); createCube(); }
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeRedo(JNIEnv*, jobject) {
    if (redoActions.empty()) return JNI_FALSE;
    const ActionKind kind = redoActions.back();
    const bool changed = kind == ActionKind::MeshTopology
        ? meshHistory.redo(*activeMesh)
        : modelingScene.redoTransform();
    if (!changed) return JNI_FALSE;
    redoActions.pop_back();
    undoActions.push_back(kind);
    if (kind == ActionKind::MeshTopology) { syncActiveFace(); createCube(); }
    return JNI_TRUE;
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
