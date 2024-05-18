#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <openvr/openvr.h>
#include <iostream>
#include <vector>

// Vertex Shader
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
void main() {
    gl_Position = vec4(aPos, 1.0);
}
)";

// Fragment Shader
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.5, 0.2, 1.0);
}
)";

// Function to compile shader
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

// Function to create shader program
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

struct FramebufferDesc {
    GLuint renderFramebufferId;
    GLuint renderTextureId;
    GLuint depthBufferId;
    GLuint renderFramebufferDesc;
};

bool CreateFrameBuffer(int width, int height, FramebufferDesc& framebufferDesc) {
    glGenFramebuffers(1, &framebufferDesc.renderFramebufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.renderFramebufferId);

    glGenRenderbuffers(1, &framebufferDesc.depthBufferId);
    glBindRenderbuffer(GL_RENDERBUFFER, framebufferDesc.depthBufferId);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebufferDesc.depthBufferId);

    glGenTextures(1, &framebufferDesc.renderTextureId);
    glBindTexture(GL_TEXTURE_2D, framebufferDesc.renderTextureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.renderTextureId, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void RenderScene(GLuint shaderProgram, GLuint VAO) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    vr::EVRInitError eError = vr::VRInitError_None;
    vr::IVRSystem* vrSystem = vr::VR_Init(&eError, vr::VRApplication_Scene);

    if (eError != vr::VRInitError_None) {
        std::cerr << "Unable to init VR runtime: " << vr::VR_GetVRInitErrorAsEnglishDescription(eError) << std::endl;
        return -1;
    }

    if (!vr::VRCompositor()) {
        std::cerr << "Compositor initialization failed. See log file for details" << std::endl;
        vr::VR_Shutdown();
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenVR Example", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        vr::VR_Shutdown();
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewInit();

    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    int width = 1280;
    int height = 720;

    FramebufferDesc leftEyeDesc;
    FramebufferDesc rightEyeDesc;
    if (!CreateFrameBuffer(width, height, leftEyeDesc)) {
        std::cerr << "Failed to create framebuffer for left eye" << std::endl;
        return -1;
    }
    if (!CreateFrameBuffer(width, height, rightEyeDesc)) {
        std::cerr << "Failed to create framebuffer for right eye" << std::endl;
        return -1;
    }

    // Set the clear color to gray
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);

    // Ensure compositor is ready
    std::this_thread::sleep_for(std::chrono::seconds(2));

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vr::TrackedDevicePose_t trackedDevicePose[vr::k_unMaxTrackedDeviceCount];
        vr::VRCompositor()->WaitGetPoses(trackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

        // Render left eye
        glBindFramebuffer(GL_FRAMEBUFFER, leftEyeDesc.renderFramebufferId);
        glViewport(0, 0, width, height);
        RenderScene(shaderProgram, VAO);

        vr::Texture_t leftEyeTexture = { (void*)(uintptr_t)leftEyeDesc.renderTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
        vr::EVRCompositorError error = vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture);
        if (error != vr::VRCompositorError_None) {
            std::cerr << "Failed to submit left eye texture: " << error << std::endl;
        }

        // Render right eye
        glBindFramebuffer(GL_FRAMEBUFFER, rightEyeDesc.renderFramebufferId);
        glViewport(0, 0, width, height);
        RenderScene(shaderProgram, VAO);

        vr::Texture_t rightEyeTexture = { (void*)(uintptr_t)rightEyeDesc.renderTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
        error = vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture);
        if (error != vr::VRCompositorError_None) {
            std::cerr << "Failed to submit right eye texture: " << error << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glfwDestroyWindow(window);
    vr::VR_Shutdown();
    glfwTerminate();

    return 0;
}