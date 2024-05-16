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


#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>

#include <vector>




// Inicializa GLFW
GLFWwindow* InitializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenXR + OpenGL", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return nullptr;
    }

    return window;
}

// Inicializa o OpenXR
bool InitializeOpenXR(XrInstance& instance, XrSystemId& systemId) {
    XrApplicationInfo appInfo = {"OpenXRExample", 1, "", 0, XR_CURRENT_API_VERSION};

    XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.applicationInfo = appInfo;

    if (xrCreateInstance(&createInfo, &instance) != XR_SUCCESS) {
        std::cerr << "Failed to create OpenXR instance" << std::endl;
        return false;
    }

    XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    if (xrGetSystem(instance, &systemInfo, &systemId) != XR_SUCCESS) {
        std::cerr << "Failed to get OpenXR system" << std::endl;
        return false;
    }

    return true;
}

// Configura as ligações gráficas para Windows
#ifdef _WIN32
XrGraphicsBindingOpenGLWin32KHR GetGraphicsBinding() {
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = wglGetCurrentDC();
    graphicsBinding.hGLRC = wglGetCurrentContext();
    return graphicsBinding;
}
#else
// Configura as ligações gráficas para Linux
XrGraphicsBindingOpenGLXlibKHR GetGraphicsBinding(GLFWwindow* window) {
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    graphicsBinding.xDisplay = glfwGetX11Display();
    graphicsBinding.glxFBConfig = nullptr;
    graphicsBinding.glxDrawable = glfwGetX11Window(window);
    graphicsBinding.glxContext = glXGetCurrentContext();
    return graphicsBinding;
}
#endif

// Cria a sessão OpenXR
bool CreateOpenXRSession(XrInstance instance, XrSystemId systemId, XrSession& session, GLFWwindow* window) {
    XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
    #ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = GetGraphicsBinding();
    sessionCreateInfo.next = &graphicsBinding;
    #else
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = GetGraphicsBinding(window);
    sessionCreateInfo.next = &graphicsBinding;
    #endif
    sessionCreateInfo.systemId = systemId;

    if (xrCreateSession(instance, &sessionCreateInfo, &session) != XR_SUCCESS) {
        std::cerr << "Failed to create OpenXR session" << std::endl;
        return false;
    }

    return true;
}

GLuint shaderProgram;
GLuint vertexArrayObject;
GLuint vertexBufferObject;

// Limpa os recursos alocados
void Cleanup(XrInstance instance, XrSession session, GLFWwindow* window) {
    if (session != XR_NULL_HANDLE) {
        xrDestroySession(session);
    }
    if (instance != XR_NULL_HANDLE) {
        xrDestroyInstance(instance);
    }
    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();

    glDeleteVertexArrays(1, &vertexArrayObject);
    glDeleteBuffers(1, &vertexBufferObject);
    glDeleteProgram(shaderProgram);
}


// Vertex shader source code
const char* vertexShaderSource = R"glsl(
    #version 450 core
    layout(location = 0) in vec3 aPos;
    void main() {
        gl_Position = vec4(aPos, 1.0);
    }
)glsl";

// Fragment shader source code
const char* fragmentShaderSource = R"glsl(
    #version 450 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
    }
)glsl";









// Cria e compila os shaders
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

// Inicializa a renderização
void InitializeRendering() {
    shaderProgram = CreateShaderProgram();

    GLfloat vertices[] = {
        0.0f,  0.5f, 0.0f,
       -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f
    };

    glGenVertexArrays(1, &vertexArrayObject);
    glGenBuffers(1, &vertexBufferObject);

    glBindVertexArray(vertexArrayObject);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Função principal que engloba toda a configuração e inicialização
bool SetupOpenXRSession(XrInstance& instance, XrSystemId& systemId, XrSession& session, GLFWwindow*& window) {
    window = InitializeGLFW();
    if (!window) return false;

    if (!InitializeOpenXR(instance, systemId)) return false;

    if (!CreateOpenXRSession(instance, systemId, session, window)) return false;

    InitializeRendering();

    return true;
}

// Renderiza um frame
void RenderFrame() {
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glBindVertexArray(vertexArrayObject);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}





int main() {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    GLFWwindow* window = nullptr;

    if (!SetupOpenXRSession(instance, systemId, session, window)) {
        Cleanup(instance, session, window);
        return -1;
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        RenderFrame();
        glfwSwapBuffers(window);
    }

    Cleanup(instance, session, window);
    return 0;
}

