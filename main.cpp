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

#ifdef WAYLAND
#include <wayland-client.h>
#include <wayland-egl.h>
#define XR_USE_PLATFORM_WAYLAND
#define XR_USE_GRAPHICS_API_OPENGL
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <iostream>
#include <cstring>

GLFWwindow *window = nullptr;

const void *GetGraphicsBinding()
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
    GLXFBConfig *fbConfigs = glXGetFBConfigs(graphicsBinding.xDisplay, DefaultScreen(graphicsBinding.xDisplay), &fbCount);
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

#ifdef WAYLAND
    XrGraphicsBindingOpenGLWaylandKHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR};
    graphicsBinding.display = glfwGetWaylandDisplay();
    graphicsBinding.eglSurface = glfwGetWaylandWindow(window);
    graphicsBinding.eglContext = eglGetCurrentContext();
    return &graphicsBinding;
#endif
    return nullptr;
}

void SetupOpenXR()
{
    XrInstance instance;
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy(createInfo.applicationInfo.applicationName, "OpenXR Example");
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy(createInfo.applicationInfo.engineName, "No Engine");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    if (xrCreateInstance(&createInfo, &instance) != XR_SUCCESS)
    {
        std::cerr << "Failed to create XR instance." << std::endl;
        exit(EXIT_FAILURE);
    }

    // More OpenXR setup code goes here
}

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }



    glfwWindowHint(GLFW_SAMPLES, 4);               // 4x antialiasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL

    // Open a window and create its OpenGL context
    GLFWwindow *window; // (In the accompanying source code, this variable is global for simplicity)
    window = glfwCreateWindow(1024, 768, "Tutorial 01", NULL, NULL);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window); // Initialize GLEW
    glewExperimental = true;        // Needed in core profile
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    // Initialize GLEW

    // Compile the vertex shader
    const GLchar *vertexSource =
        R"glsl(
        #version 150 core
        in vec2 position;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    )glsl";
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, nullptr);
    glCompileShader(vertexShader);

    // Check for compile errors
    GLint status;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        char buffer[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, buffer);
        std::cerr << "Vertex Shader Compile Error: " << buffer << std::endl;
        return -1;
    }

    // Compile the fragment shader
    const GLchar *fragmentSource =
        R"glsl(
        #version 150 core
        out vec4 outColor;
        void main() {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )glsl";
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
    glCompileShader(fragmentShader);

    // Check for compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
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
    if (status != GL_TRUE)
    {
        char buffer[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, buffer);
        std::cerr << "Shader Link Error: " << buffer << std::endl;
        return -1;
    }

    // Set up vertex data (and buffer(s)) and attribute pointers
    GLfloat vertices[] = {
        0.0f, 0.5f,
        0.5f, -0.5f,
        -0.5f, -0.5f};

    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Initialize OpenXR
    SetupOpenXR();

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Check for events
        glfwPollEvents();

        // Render
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // Submit frame to OpenXR
        const void *graphicsBinding = GetGraphicsBinding();
        // Use graphicsBinding with OpenXR to submit frame...

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
