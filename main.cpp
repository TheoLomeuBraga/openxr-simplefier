#include <GL/glew.h>



#ifdef _WIN32

#include <windows.h>
#include <GL/gl.h>
#include <GL/wglext.h>
#define XR_USE_PLATFORM_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#define XR_USE_GRAPHICS_API_OPENGL

#endif

#ifdef X11

#include <GL/glx.h>
#define XR_USE_PLATFORM_XLIB
#define GLFW_EXPOSE_NATIVE_X11
#define XR_USE_GRAPHICS_API_OPENGL

#endif

#if WAYLAND 

#include <wayland-client.h>
#include <wayland-egl.h>
#define XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR
#define XR_USE_GRAPHICS_API_OPENGL

#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>

#include <vector>
#include <string>
#include <map>
#include <unordered_map>

// Inicializa GLFW
GLFWwindow *window = NULL;
void InitializeGLFW()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(800, 600, "OpenGL Triangle", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK)
    {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
}

void APIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
    std::cerr << "OpenGL Debug Message:" << std::endl;
    std::cerr << "Source: " << source << std::endl;
    std::cerr << "Type: " << type << std::endl;
    std::cerr << "ID: " << id << std::endl;
    std::cerr << "Severity: " << severity << std::endl;
    std::cerr << "Message: " << message << std::endl;
}

void InitializeOpenGL()
{
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(GLDebugMessageCallback, nullptr);
}
namespace openxr_helper
{

    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;

    // XR_DOCS_TAG_BEGIN_Helper_Functions0
    void OpenXRDebugBreak()
    {
        std::cerr << "Breakpoint here to debug." << std::endl;
    }

    const char *GetXRErrorString(XrInstance xrInstance, XrResult result)
    {
        static char string[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(xrInstance, result, string);
        return string;
    }

#define OPENXR_CHECK(x, y)                                                                                                                                  \
    {                                                                                                                                                       \
        XrResult result = (x);                                                                                                                              \
        if (!XR_SUCCEEDED(result))                                                                                                                          \
        {                                                                                                                                                   \
            std::cerr << "ERROR: OPENXR: " << int(result) << "(" << (m_xrInstance ? GetXRErrorString(m_xrInstance, result) : "") << ") " << y << std::endl; \
            OpenXRDebugBreak();                                                                                                                             \
        }                                                                                                                                                   \
    }

    std::string GetGraphicsAPIInstanceExtensionString()
    {
        return XR_KHR_OPENGL_ENABLE_EXTENSION_NAME;
    }

#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding;
    const void *XR_MAY_ALIAS GetGraphicsBinding()
    {
        graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        graphicsBinding.hDC = wglGetCurrentDC();
        graphicsBinding.hGLRC = wglGetCurrentContext();
        return &graphicsBinding;
    }
#endif
#ifdef X11
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding;
    const void *XR_MAY_ALIAS GetGraphicsBinding()
    {
        graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
        graphicsBinding.xDisplay = glfwGetX11Display();
        graphicsBinding.glxFBConfig = nullptr;
        graphicsBinding.glxDrawable = glfwGetX11Window(window);
        graphicsBinding.glxContext = glXGetCurrentContext();
        return &graphicsBinding;
    }
#endif
#ifdef WAYLAND


    XrGraphicsBindingOpenGLWaylandKHR graphicsBinding;
    const void *XR_MAY_ALIAS GetGraphicsBinding()
    {
        graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR};
        graphicsBinding.display = glfwGetWaylandDisplay();
        graphicsBinding.eglSurface = glfwGetWaylandWindow(window);
        graphicsBinding.eglContext = eglGetCurrentContext();
        return &graphicsBinding;
    }
#endif

    bool InitializeOpenXR()
    {
        XrApplicationInfo appInfo = {"OpenXRExample", 1, "", 0, XR_CURRENT_API_VERSION};

        XrInstanceCreateInfo createInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.applicationInfo = appInfo;

        // Criação da instância OpenXR
        XrResult result = xrCreateInstance(&createInfo, &instance);
        if (result != XR_SUCCESS)
        {
            std::cerr << "Failed to create OpenXR instance: " << result << std::endl;
            return false;
        }

        // Obtenção do sistema OpenXR
        XrSystemGetInfo systemInfo = {XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

        result = xrGetSystem(instance, &systemInfo, &systemId);
        if (result != XR_SUCCESS)
        {
            std::cerr << "Failed to get OpenXR system: " << result << std::endl;
            switch (result)
            {
            case XR_ERROR_FORM_FACTOR_UNSUPPORTED:
                std::cerr << "Unsupported form factor" << std::endl;
                break;
            case XR_ERROR_INSTANCE_LOST:
                std::cerr << "Instance lost" << std::endl;
                break;
            case XR_ERROR_RUNTIME_FAILURE:
                std::cerr << "Runtime failure" << std::endl;
                break;
            case XR_ERROR_INITIALIZATION_FAILED:
                std::cerr << "Initialization failed" << std::endl;
                break;
            case XR_ERROR_FUNCTION_UNSUPPORTED:
                std::cerr << "Function unsupported" << std::endl;
                break;
            default:
                std::cerr << "Unknown error" << std::endl;
                break;
            }
            return false;
        }

        std::cout << "OpenXR instance created and system found!" << std::endl;
        return true;
    }

    bool create_openxr_session()
    {
        // Prepara a estrutura de informações da sessão
        XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
        sessionCreateInfo.next = GetGraphicsBinding();
        sessionCreateInfo.systemId = systemId;

        // Cria a sessão OpenXR
        XrResult result = xrCreateSession(instance, &sessionCreateInfo, &session);
        if (result != XR_SUCCESS)
        {
            std::cerr << "Failed to create OpenXR session: " << result << std::endl;
            return false;
        }

        std::cout << "OpenXR session created!" << std::endl;
        return true;
    }

    void start_OpenXR()
    {
        if (!InitializeOpenXR())
            return;
        if (!create_openxr_session())
            return;
    }

    int SelectColorSwapchainFormat()
    {
        return GL_RGBA8;
    }

    int SelectDepthSwapchainFormat()
    {
        return GL_DEPTH_COMPONENT24;
    }

}

void loopGLFW()
{
    glfwSwapBuffers(window);
    glfwPollEvents();
}
void loopOpenGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

int main()
{
    InitializeGLFW();
    openxr_helper::start_OpenXR();
    InitializeOpenGL();

    do
    {
        loopOpenGL();
        loopGLFW();
    } while (!glfwWindowShouldClose(window));

    return 0;
}
