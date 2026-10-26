#include <jni.h>
#include <GLES3/gl31.h>
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {
constexpr char kTag[] = "A3DNative";
constexpr float kPi = 3.14159265358979323846f;

struct Mat4 { float m[16]{}; };
struct Vertex { float x, y, z, r, g, b; };

GLuint program = 0;
GLuint gridVao = 0, gridVbo = 0;
GLuint cubeVao = 0, cubeVbo = 0, cubeEbo = 0;
GLsizei gridVertexCount = 0;
GLint mvpLocation = -1;
int viewportWidth = 1, viewportHeight = 1;
float yaw = 0.75f, pitch = 0.48f, distanceToTarget = 6.5f;

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
void main() { fragColor = vec4(vColor, 1.0); }
)";

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                out.m[column * 4 + row] += a.m[k * 4 + row] * b.m[column * 4 + k];
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

struct Vec3 { float x, y, z; };
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
    bindMesh(cubeVao, cubeVbo, vertices, sizeof(vertices));
    glGenBuffers(1, &cubeEbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void createResources() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    mvpLocation = glGetUniformLocation(program, "uMvp");
    createGrid();
    createCube();
}

Mat4 cameraMvp() {
    const float cp = std::cos(pitch);
    const Vec3 eye {distanceToTarget * cp * std::sin(yaw),
                    1.0f + distanceToTarget * std::sin(pitch),
                    distanceToTarget * cp * std::cos(yaw)};
    const Mat4 view = lookAt(eye, {0,1,0}, {0,1,0});
    const Mat4 projection = perspective(50.0f * kPi / 180.0f,
        static_cast<float>(viewportWidth) / static_cast<float>(std::max(1, viewportHeight)), 0.05f, 100.0f);
    return multiply(projection, view);
}
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceCreated(JNIEnv*, jobject) {
    createResources();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    __android_log_print(ANDROID_LOG_INFO, kTag, "Renderer: %s", glGetString(GL_RENDERER));
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    viewportWidth = width; viewportHeight = height; glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDrawFrame(JNIEnv*, jobject) {
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    const Mat4 mvp = cameraMvp();
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, mvp.m);
    glBindVertexArray(gridVao);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(cubeVao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, nullptr);
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
