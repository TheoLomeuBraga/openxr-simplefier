#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <openvr/openvr.h>
#include <iostream>

// Shader sources
const GLchar* vertexSource =
R"glsl(
    #version 150 core
    in vec2 position;
    void main() {
        gl_Position = vec4(position, 0.0, 1.0);
    }
)glsl";

const GLchar* fragmentSource =
R"glsl(
    #version 150 core
    out vec4 outColor;
    void main() {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
)glsl";

vr::IVRSystem* vrSystem = nullptr;
vr::IVRCompositor* vrCompositor = nullptr;

void SetupOpenVR() {
    vr::EVRInitError eError = vr::VRInitError_None;
    vrSystem = vr::VR_Init(&eError, vr::VRApplication_Scene);

    if (eError != vr::VRInitError_None) {
        std::cerr << "Unable to init VR runtime: "
                  << vr::VR_GetVRInitErrorAsEnglishDescription(eError) << std::endl;
        exit(EXIT_FAILURE);
    }

    vrCompositor = vr::VRCompositor();
    if (!vrCompositor) {
        std::cerr << "Compositor initialization failed. See log file for details" << std::endl;
        vr::VR_Shutdown();
        exit(EXIT_FAILURE);
    }
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a GLFW window
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenVR Triangle", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Compile the vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    // Check for compile errors
    GLint status;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, buffer);
        std::cerr << "Vertex Shader Compile Error: " << buffer << std::endl;
        return -1;
    }

    // Compile the fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    // Check for compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, buffer);
        std::cerr << "Fragment Shader Compile Error: " << buffer << std::endl;
        return -1;
    }

    // Link the vertex and fragment shader into a shader program
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char buffer[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, buffer);
        std::cerr << "Shader Link Error: " << buffer << std::endl;
        return -1;
    }

    // Set up vertex data (and buffer(s)) and attribute pointers
    GLfloat vertices[] = {
        0.0f,  0.5f,
        0.5f, -0.5f,
       -0.5f, -0.5f
    };

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Initialize OpenVR
    SetupOpenVR();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Check for events
        glfwPollEvents();

        // Render
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // Submit frame to OpenVR
        vr::Texture_t leftEyeTexture = { (void*)(uintptr_t)0, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
        vr::Texture_t rightEyeTexture = { (void*)(uintptr_t)0, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
        vrCompositor->Submit(vr::Eye_Left, &leftEyeTexture);
        vrCompositor->Submit(vr::Eye_Right, &rightEyeTexture);

        // Swap the buffers
        glfwSwapBuffers(window);
    }

    // Clean up
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
