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




#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>
#include <cstring>

GLFWwindow* window = nullptr;

const void* GetGraphicsBinding()
{
#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = wglGetCurrentDC();
    graphicsBinding.hGLRC = wglGetCurrentContext();
    return &graphicsBinding;
#endif

#ifdef X11
    XrGraphicsBindingOpenGLXlibKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    graphicsBinding.xDisplay = glfwGetX11Display();
    int fbCount;
    GLXFBConfig* fbConfigs = glXGetFBConfigs(graphicsBinding.xDisplay, DefaultScreen(graphicsBinding.xDisplay), &fbCount);
    if (fbConfigs && fbCount > 0)
    {
        graphicsBinding.glxFBConfig = fbConfigs[0];
        XFree(fbConfigs);
    }
    else
    {
        std::cerr << "Failed to get GLXFBConfig" << std::endl;
        return nullptr;
    }
    graphicsBinding.visualid = XVisualIDFromVisual(DefaultVisual(graphicsBinding.xDisplay, DefaultScreen(graphicsBinding.xDisplay)));
    graphicsBinding.glxDrawable = glfwGetX11Window(window);
    graphicsBinding.glxContext = glXGetCurrentContext();
    return &graphicsBinding;
#endif
    return nullptr;
}
