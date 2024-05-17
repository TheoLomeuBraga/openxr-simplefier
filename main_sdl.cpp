#include <SDL2/SDL.h>
#include <GL/glew.h>

#ifdef _WIN32

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#define GLFW_EXPOSE_NATIVE_WIN32

#else

#include <GL/glx.h>
#define XR_USE_PLATFORM_XLIB
#define XR_USE_GRAPHICS_API_OPENGL
#define GLFW_EXPOSE_NATIVE_X11

#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>

// OpenGL shader sources
const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout(location = 0) in vec3 aPos;
    void main() {
        gl_Position = vec4(aPos, 1.0);
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
    }
)glsl";

// OpenXR globals
XrInstance xrInstance = XR_NULL_HANDLE;
XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
XrSession xrSession = XR_NULL_HANDLE;

// SDL2 window and OpenGL context
SDL_Window* window = nullptr;
SDL_GLContext glContext;

// Function to check OpenXR results
bool checkXrResult(XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        std::cerr << "Failed to " << operation << ": " << result << std::endl;
        return false;
    }
    return true;
}

// Initialize SDL2 and OpenGL
bool InitializeSDL2OpenGL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow("OpenXR + OpenGL + SDL2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_GL_MakeCurrent(window, glContext);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    return true;
}

// Initialize OpenXR
XrInstance xrInstance = XR_NULL_HANDLE;
XrSystemId xrSystemId = XR_NULL_SYSTEM_ID;
XrSession xrSession = XR_NULL_HANDLE;

bool checkXrResult(XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        std::cerr << "Failed to " << operation << ": " << result << std::endl;
        return false;
    }
    return true;
}

bool InitializeOpenXR() {
    XrApplicationInfo appInfo = {};
    strcpy(appInfo.applicationName, "OpenXRExample");
    appInfo.applicationVersion = 1;
    strcpy(appInfo.engineName, "NoEngine");
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.applicationInfo = appInfo;

    XrResult result = xrCreateInstance(&createInfo, &xrInstance);
    if (!checkXrResult(result, "create OpenXR instance")) {
        return false;
    }

    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    result = xrGetSystem(xrInstance, &systemInfo, &xrSystemId);
    if (!checkXrResult(result, "get OpenXR system")) {
        return false;
    }

#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = wglGetCurrentDC();
    graphicsBinding.hGLRC = wglGetCurrentContext();
#else
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    graphicsBinding.xDisplay = XOpenDisplay(NULL);
    graphicsBinding.glxFBConfig = NULL;
    graphicsBinding.glxDrawable = glXGetCurrentDrawable();
    graphicsBinding.glxContext = glXGetCurrentContext();
#endif

    XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.systemId = xrSystemId;
    sessionCreateInfo.next = &graphicsBinding;

    result = xrCreateSession(xrInstance, &sessionCreateInfo, &xrSession);
    if (!checkXrResult(result, "create OpenXR session")) {
        return false;
    }

    return true;
}

// Create and compile shader program
GLuint CreateShaderProgram() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Initialize OpenGL resources
GLuint InitializeOpenGLResources() {
    GLuint shaderProgram = CreateShaderProgram();

    GLfloat vertices[] = {
        0.0f,  0.5f, 0.0f,
       -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return shaderProgram;
}

// Main loop
void MainLoop(GLuint shaderProgram) {
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(shaderProgram);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        SDL_GL_SwapWindow(window);
    }
}

// Cleanup
void Cleanup(GLuint shaderProgram) {
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (xrSession != XR_NULL_HANDLE) {
        xrDestroySession(xrSession);
    }
    if (xrInstance != XR_NULL_HANDLE) {
        xrDestroyInstance(xrInstance);
    }
}

int main() {
    if (!InitializeSDL2OpenGL()) {
        return -1;
    }

    if (!InitializeOpenXR()) {
        Cleanup(0);
        return -1;
    }

    GLuint shaderProgram = InitializeOpenGLResources();
    MainLoop(shaderProgram);
    Cleanup(shaderProgram);

    return 0;
}
