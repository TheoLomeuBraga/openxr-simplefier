

#include "vr_manager.h"




GLuint shaderProgramID = 0;
GLuint VAOs[1] = {0};

static const char* vertexshader =
    "#version 300 es\n"
    "in vec3 aPos;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 proj;\n"
    "in vec2 aColor;\n"
    "out vec2 vertexColor;\n"
    "void main() {\n"
    "    gl_Position = proj * view * model * vec4(aPos, 1.0);\n"
    "    vertexColor = aColor;\n"
    "}\n";



static const char* fragmentshader =
    "#version 300 es\n"
    "precision mediump float;\n"
    "out vec4 FragColor;\n"
    "uniform vec3 uniformColor;\n"
    "in vec2 vertexColor;\n"
    "void main() {\n"
    "    FragColor = (uniformColor.x < 0.01 && uniformColor.y < 0.01 && uniformColor.z < 0.01) ? vec4(vertexColor, 1.0, 1.0) : vec4(uniformColor, 1.0);\n"
    "}\n";



static SDL_Window* desktop_window;
static SDL_GLContext gl_context;

void GLAPIENTRY
MessageCallback(GLenum source,
				GLenum type,
				GLuint id,
				GLenum severity,
				GLsizei length,
				const GLchar* message,
				const void* userParam)
{
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
			(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

#ifdef _WIN32
bool init_sdl_window(HDC& xDisplay, HGLRC& glxContext, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Unable to initialize SDL");
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

    /* Create our window centered at half the VR resolution */
    desktop_window = SDL_CreateWindow("OpenXR Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        w / 2, h / 2, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!desktop_window) {
        printf("Unable to create window");
        return false;
    }

    gl_context = SDL_GL_CreateContext(desktop_window);
    auto err = glewInit();

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    SDL_GL_SetSwapInterval(0);

    // HACK? OpenXR wants us to report these values, so "work around" SDL a
    // bit and get the underlying glx stuff. Does this still work when e.g.
    // SDL switches to xcb?
    xDisplay = wglGetCurrentDC();
    glxContext = wglGetCurrentContext();

    return true;
}
#endif

#ifdef X11

bool init_sdl_window(Display*& xDisplay, GLXContext& glxContext, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Unable to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

    /* Create our window centered at half the VR resolution */
    desktop_window = SDL_CreateWindow("OpenXR Example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        w / 2, h / 2, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!desktop_window) {
        printf("Unable to create window: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    gl_context = SDL_GL_CreateContext(desktop_window);
    if (!gl_context) {
        printf("Unable to create OpenGL context: %s\n", SDL_GetError());
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        printf("GLEW initialization failed: %s\n", glewGetErrorString(err));
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    SDL_GL_SetSwapInterval(0);

    xDisplay = XOpenDisplay(NULL);
    if (!xDisplay) {
        printf("Unable to open X display\n");
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    // Create an X window using the visual info
    int screen = DefaultScreen(xDisplay);
    int glxAttribs[] = {
        GLX_RGBA,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_DOUBLEBUFFER,
        None
    };
    
    XVisualInfo* vi = glXChooseVisual(xDisplay, screen, glxAttribs);
    if (!vi) {
        printf("No appropriate visual found\n");
        XCloseDisplay(xDisplay);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    glxContext = glXCreateContext(xDisplay, vi, NULL, GL_TRUE);
    if (!glxContext) {
        printf("Unable to create GLX context\n");
        XFree(vi);
        XCloseDisplay(xDisplay);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    Window root = RootWindow(xDisplay, vi->screen);
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(xDisplay, root, vi->visual, AllocNone);
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
    swa.border_pixel = 0;

    Window win = XCreateWindow(xDisplay, root, 0, 0, w / 2, h / 2, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask | CWBorderPixel, &swa);
    if (!win) {
        printf("Unable to create X window\n");
        glXDestroyContext(xDisplay, glxContext);
        XFree(vi);
        XCloseDisplay(xDisplay);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    XMapWindow(xDisplay, win);
    XStoreName(xDisplay, win, "OpenXR Example");

    if (!glXMakeCurrent(xDisplay, win, glxContext)) {
        printf("Unable to make GLX context current\n");
        XDestroyWindow(xDisplay, win);
        glXDestroyContext(xDisplay, glxContext);
        XFree(vi);
        XCloseDisplay(xDisplay);
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(desktop_window);
        SDL_Quit();
        return false;
    }

    XFree(vi);
    return true;
}


#endif


void start_vr(void(start_render)(void)){

}

void update_vr(void(update_render)(glm::mat4,glm::mat4)){

}

XrHandJointLocationsEXT *get_joints_infos(){
    return NULL;
};

void end_vr(void(end_render)(void)){

};