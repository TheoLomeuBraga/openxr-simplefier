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
GLFWwindow* window = NULL;
void InitializeGLFW() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(800, 600, "OpenGL Triangle", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
}

void InitializeOpenGL(){
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
}

// Configura as ligações gráficas para Windows
#ifdef _WIN32
XrGraphicsBindingOpenGLWin32KHR graphicsBinding;
const void* XR_MAY_ALIAS GetGraphicsBinding() {
    graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = wglGetCurrentDC();
    graphicsBinding.hGLRC = wglGetCurrentContext();
    return &graphicsBinding;
}
#else
// Configura as ligações gráficas para Linux
XrGraphicsBindingOpenGLXlibKHR graphicsBinding;
const void* XR_MAY_ALIAS GetGraphicsBinding() {
    graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR};
    graphicsBinding.xDisplay = glfwGetX11Display();
    graphicsBinding.glxFBConfig = nullptr;
    graphicsBinding.glxDrawable = glfwGetX11Window(window);
    graphicsBinding.glxContext = glXGetCurrentContext();
    return &graphicsBinding;
}
#endif

void loopGLFW(){
    glfwSwapBuffers(window);
    glfwPollEvents();
}
void loopOpenGL(){
    glClear(GL_COLOR_BUFFER_BIT);
}

int main() {
    InitializeGLFW();
    InitializeOpenGL();

    do  {
        loopOpenGL();
        loopGLFW();
    } while (!glfwWindowShouldClose(window));

    return 0;
}

