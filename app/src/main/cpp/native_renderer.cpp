#include <jni.h>
#include <GLES3/gl31.h>
#include <android/log.h>
#include <cmath>

namespace {
constexpr char kTag[] = "A3DNative";
GLuint program = 0;
GLuint vao = 0;
GLuint vbo = 0;
GLint timeLocation = -1;

constexpr char kVertexShader[] = R"(#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec3 aColor;
out vec3 vColor;
uniform float uTime;
void main() {
    float a = uTime * 0.35;
    mat2 rotation = mat2(cos(a), -sin(a), sin(a), cos(a));
    gl_Position = vec4(rotation * aPosition, 0.0, 1.0);
    vColor = aColor;
})";

constexpr char kFragmentShader[] = R"(#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

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

void createResources() {
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    timeLocation = glGetUniformLocation(program, "uTime");

    constexpr float vertices[] = {
         0.0f,  0.65f, 0.45f, 0.62f, 1.0f,
        -0.6f, -0.45f, 0.35f, 0.90f, 0.72f,
         0.6f, -0.45f, 1.00f, 0.48f, 0.55f,
    };
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceCreated(JNIEnv*, jobject) {
    createResources();
    glEnable(GL_DEPTH_TEST);
    __android_log_print(ANDROID_LOG_INFO, kTag, "Renderer initialized: %s", glGetString(GL_RENDERER));
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_ai_arena_a3d_NativeRenderer_nativeDrawFrame(JNIEnv*, jobject, jfloat timeSeconds) {
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glUniform1f(timeLocation, timeSeconds);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
