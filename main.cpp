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

void InitializeOpenGL()
{
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
}
namespace openxr_helper
{
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
#else
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

    int SelectColorSwapchainFormat()
    {
        return GL_RGBA8;
    }

    int SelectDepthSwapchainFormat()
    {
        return GL_DEPTH_COMPONENT24;
    }

    enum class SwapchainType : uint8_t
    {
        COLOR,
        DEPTH
    };

    std::unordered_map<XrSwapchain, std::pair<SwapchainType, std::vector<XrSwapchainImageOpenGLKHR>>> swapchainImagesMap{};

    XrSwapchainImageBaseHeader *AllocateSwapchainImageData(XrSwapchain swapchain, SwapchainType type, uint32_t count)
    {
        swapchainImagesMap[swapchain].first = type;
        swapchainImagesMap[swapchain].second.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        return reinterpret_cast<XrSwapchainImageBaseHeader *>(swapchainImagesMap[swapchain].second.data());
    }
    XrSwapchainImageBaseHeader *GetSwapchainImageData(XrSwapchain swapchain, uint32_t index) { return (XrSwapchainImageBaseHeader *)&swapchainImagesMap[swapchain].second[index]; }
    void *GetSwapchainImage(XrSwapchain swapchain, uint32_t index) { return (void *)(uint64_t)swapchainImagesMap[swapchain].second[index].image; }

    
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
    InitializeOpenGL();

    do
    {
        loopOpenGL();
        loopGLFW();
    } while (!glfwWindowShouldClose(window));

    return 0;
}
