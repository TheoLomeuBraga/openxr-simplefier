#include <GL/glew.h>

#define XR_USE_GRAPHICS_API_OPENGL
#ifdef _WIN32
#include <Windows.h>
#include <GL/wglew.h>
#define XR_USE_PLATFORM_WIN32
#else
#include <X11/Xlib.h>
#include <GL/glx.h>
#define XR_USE_PLATFORM_XLIB
#endif


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

#ifdef _WIN32
HGLRC hGLRC = nullptr;
HDC hDC = nullptr;
HWND hwnd = nullptr;

bool InitializeOpenGL() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "OpenGL";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, "OpenGL", "OpenXR + OpenGL", WS_OVERLAPPEDWINDOW,
                          CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                          nullptr, nullptr, wc.hInstance, nullptr);

    hDC = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pixelFormat, &pfd);

    hGLRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hGLRC);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    return true;
}

#else
Display* xDisplay = nullptr;
GLXContext glxContext = nullptr;
Window xWindow;

bool InitializeOpenGL() {
    xDisplay = XOpenDisplay(nullptr);
    if (!xDisplay) {
        std::cerr << "Failed to open X display" << std::endl;
        return false;
    }

    int screen = DefaultScreen(xDisplay);
    int attribs[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    XVisualInfo* vi = glXChooseVisual(xDisplay, screen, attribs);
    if (!vi) {
        std::cerr << "No appropriate visual found" << std::endl;
        return false;
    }

    XSetWindowAttributes swa = {};
    swa.colormap = XCreateColormap(xDisplay, RootWindow(xDisplay, vi->screen), vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask;

    xWindow = XCreateWindow(xDisplay, RootWindow(xDisplay, vi->screen),
                            0, 0, 800, 600, 0, vi->depth, InputOutput,
                            vi->visual, CWColormap | CWEventMask, &swa);

    XMapWindow(xDisplay, xWindow);

    glxContext = glXCreateContext(xDisplay, vi, nullptr, GL_TRUE);
    glXMakeCurrent(xDisplay, xWindow, glxContext);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }

    return true;
}
#endif

std::vector<const char*> GetGraphicsAPIInstanceExtensionString() {
    std::vector<const char*> extensions;
    extensions.push_back(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
    extensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME); // Para depuração
    return extensions;
}

XrResult CreateDebugUtilsMessenger(XrInstance instance) {
    XrDebugUtilsMessengerCreateInfoEXT debugInfo = {};
    debugInfo.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                             XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    debugInfo.userCallback = DebugCallback;

    PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT = nullptr;
    xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)(&xrCreateDebugUtilsMessengerEXT));
    if (xrCreateDebugUtilsMessengerEXT != nullptr) {
        return xrCreateDebugUtilsMessengerEXT(instance, &debugInfo, &debugMessenger);
    } else {
        return XR_ERROR_FUNCTION_UNSUPPORTED;
    }
}

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

    // Setup Debug Messenger
    if (CreateDebugUtilsMessenger(instance) != XR_SUCCESS) {
        std::cerr << "Failed to create debug utils messenger" << std::endl;
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

#ifdef _WIN32
XrGraphicsBindingOpenGLWin32KHR GetGraphicsBindingWin32() {
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = hDC;
    graphicsBinding.hGLRC = hGLRC;
    if (graphicsBinding.hDC == nullptr || graphicsBinding.hGLRC == nullptr) {
        std::cerr << "Failed to get current DC or GLRC" << std::endl;
    }
    return graphicsBinding;
}
#else
XrGraphicsBindingOpenGLXlibKHR GetGraphicsBindingXlib() {
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    graphicsBinding.xDisplay = xDisplay;
    graphicsBinding.glxFBConfig = nullptr;
    graphicsBinding.glxDrawable = xWindow;
    graphicsBinding.glxContext = glxContext;
    if (graphicsBinding.xDisplay == nullptr || graphicsBinding.glxContext == nullptr) {
        std::cerr << "Failed to get current X Display or GLX Context" << std::endl;
    }
    return graphicsBinding;
}
#endif

bool CreateOpenXRSession(XrInstance instance, XrSystemId systemId, XrSession& session) {
    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    #ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = GetGraphicsBindingWin32();
    sessionCreateInfo.next = &graphicsBinding;
    #else
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = GetGraphicsBindingXlib();
    sessionCreateInfo.next = &graphicsBinding;
    #endif
    sessionCreateInfo.systemId = systemId;

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
void Cleanup(XrInstance instance, XrSession session) {
    if (session != XR_NULL_HANDLE) {
        xrDestroySession(session);
    }
    if (instance != XR_NULL_HANDLE) {
        xrDestroyInstance(instance);
    }
    #ifdef _WIN32
    if (hGLRC) {
        wglDeleteContext(hGLRC);
    }
    if (hDC) {
        ReleaseDC(hwnd, hDC);
    }
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    #else
    if (glxContext) {
        glXDestroyContext(xDisplay, glxContext);
    }
    if (xDisplay) {
        XCloseDisplay(xDisplay);
    }
    #endif

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
bool SetupOpenXRSession(XrInstance& instance, XrSystemId& systemId, XrSession& session) {
    if (!InitializeOpenGL()) return false;

    if (!InitializeOpenXR(instance, systemId)) return false;

    if (!CreateOpenXRSession(instance, systemId, session)) return false;

    InitializeRendering();

    return true;
}

int main() {
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;

    if (!SetupOpenXRSession(instance, systemId, session)) {
        Cleanup(instance, session);
        return -1;
    }

    // Main loop
    bool running = true;
    while (running) {
        #ifdef _WIN32
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        #else
        XEvent xEvent;
        while (XPending(xDisplay)) {
            XNextEvent(xDisplay, &xEvent);
            if (xEvent.type == ClientMessage) {
                running = false;
            }
        }
        #endif

        RenderFrame();
        #ifdef _WIN32
        SwapBuffers(hDC);
        #else
        glXSwapBuffers(xDisplay, xWindow);
        #endif
    }

    Cleanup(instance, session);
    return 0;
}
