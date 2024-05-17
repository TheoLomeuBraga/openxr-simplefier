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

XrDebugUtilsMessengerEXT debugMessenger = XR_NULL_HANDLE;

XrBool32 XRAPI_PTR DebugCallback(XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
                                 XrDebugUtilsMessageTypeFlagsEXT messageTypes,
                                 const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
                                 void* userData) {
    std::cerr << "Debug: " << callbackData->message << std::endl;
    return XR_FALSE;
}




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


// Obtém as extensões necessárias para a API gráfica
std::vector<const char*> GetGraphicsAPIInstanceExtensionString() {
    std::vector<const char*> extensions;
    extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
    extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME); // Para depuração
    
    return extensions;
}


// Inicializa o OpenXR
bool InitializeOpenXR(XrInstance& instance, XrSystemId& systemId) {
    // Application Info
    XrApplicationInfo appInfo = {};
    strncpy(appInfo.applicationName, "OpenXRExample", XR_MAX_APPLICATION_NAME_SIZE - 1);
    appInfo.applicationVersion = 1;
    strncpy(appInfo.engineName, "NoEngine", XR_MAX_ENGINE_NAME_SIZE - 1);
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;

    // Get required extensions for the graphics API
    std::vector<const char*> extensions = GetGraphicsAPIInstanceExtensionString();

    // Create Instance Info
    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    createInfo.applicationInfo = appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();

    // Create Instance
    XrResult result = xrCreateInstance(&createInfo, &instance);
    if (result != XR_SUCCESS) {
        std::cerr << "Failed to create OpenXR instance: " << result << std::endl;
        if (result == XR_ERROR_RUNTIME_FAILURE) {
            std::cerr << "Runtime failure. Please ensure the OpenXR runtime is installed and active." << std::endl;
        }
        return false;
    }

    // Get System Info
    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    result = xrGetSystem(instance, &systemInfo, &systemId);
    if (result != XR_SUCCESS) {
        std::cerr << "Failed to get OpenXR system: " << result << std::endl;
        return false;
    }

    return true;
}

// Configura as ligações gráficas para Windows
#ifdef _WIN32
XrGraphicsBindingOpenGLWin32KHR GetGraphicsBindingWin32() {
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = wglGetCurrentDC();
    graphicsBinding.hGLRC = wglGetCurrentContext();
    if (graphicsBinding.hDC == nullptr || graphicsBinding.hGLRC == nullptr) {
        std::cerr << "Failed to get current DC or GLRC" << std::endl;
    }
    return graphicsBinding;
}
#else
XrGraphicsBindingOpenGLXlibKHR GetGraphicsBindingXlib(GLFWwindow* window) {
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    graphicsBinding.xDisplay = glfwGetX11Display();
    graphicsBinding.glxFBConfig = nullptr;
    graphicsBinding.glxDrawable = glfwGetX11Window(window);
    graphicsBinding.glxContext = glXGetCurrentContext();
    if (graphicsBinding.xDisplay == nullptr || graphicsBinding.glxContext == nullptr) {
        std::cerr << "Failed to get current X Display or GLX Context" << std::endl;
    }
    return graphicsBinding;
}
#endif


// Cria a sessão OpenXR
bool CreateOpenXRSession(XrInstance instance, XrSystemId systemId, XrSession& session, GLFWwindow* window) {
    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    #ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = GetGraphicsBindingWin32();
    sessionCreateInfo.next = &graphicsBinding;
    #else
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = GetGraphicsBindingXlib(window);
    sessionCreateInfo.next = &graphicsBinding;
    #endif
    sessionCreateInfo.systemId = systemId;
    sessionCreateInfo.type  = XR_TYPE_SESSION_CREATE_INFO; 

    XrResult result = xrCreateSession(instance, &sessionCreateInfo, &session);
    if (result != XR_SUCCESS) {
        std::cerr << "Failed to create OpenXR session: " << result << std::endl;
        switch (result) {
            case XR_ERROR_VALIDATION_FAILURE:
                std::cerr << "Validation failure" << std::endl;
                break;
            case XR_ERROR_RUNTIME_FAILURE:
                std::cerr << "Runtime failure" << std::endl;
                break;
            case XR_ERROR_OUT_OF_MEMORY:
                std::cerr << "Out of memory" << std::endl;
                break;
            case XR_ERROR_API_VERSION_UNSUPPORTED:
                std::cerr << "API version unsupported" << std::endl;
                break;
            case XR_ERROR_INITIALIZATION_FAILED:
                std::cerr << "Initialization failed" << std::endl;
                break;
            case XR_ERROR_FUNCTION_UNSUPPORTED:
                std::cerr << "Function unsupported" << std::endl;
                break;
            case XR_ERROR_FEATURE_UNSUPPORTED:
                std::cerr << "Feature unsupported" << std::endl;
                break;
            case XR_ERROR_EXTENSION_NOT_PRESENT:
                std::cerr << "Extension not present" << std::endl;
                break;
            default:
                std::cerr << "Unknown error" << std::endl;
                break;
        }
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

const char* fragmentShaderSource = R"glsl(
    #version 450 core
    out vec4 FragColor;
    void main() {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
    }
)glsl";

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

void RenderFrame() {
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glBindVertexArray(vertexArrayObject);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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
