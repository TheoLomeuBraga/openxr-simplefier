

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

static XrPosef identity_pose = {.orientation = {.x = 0, .y = 0, .z = 0, .w = 1.0},
								.position = {.x = 0, .y = 0, .z = 0}};

// small helper so we don't forget whether we treat 0 as left or right hand
enum OPENXR_HANDS
{
	HAND_LEFT = 0,
	HAND_RIGHT = 1,
	HAND_COUNT
};

std::string h_str(int hand)
{
	if (hand == HAND_LEFT)
		return "left";
	else if (hand == HAND_RIGHT)
		return "right";
	else
		return "invalid";
}

std::string h_p_str(int hand)
{
	if (hand == HAND_LEFT)
		return "/user/hand/left";
	else if (hand == HAND_RIGHT)
		return "/user/hand/right";
	else
		return "invalid";
}

class XrExample
{
public:
	// every OpenXR app that displays something needs at least an instance and a session
	XrInstance instance = XR_NULL_HANDLE;
	XrSession session;
	XrSystemId system_id;
	XrSessionState state;

	// Play space is usually local (head is origin, seated) or stage (room scale)
	XrSpace play_space;

	// Each physical Display/Eye is described by a view
	std::vector<XrViewConfigurationView> viewconfig_views;
	std::vector<XrCompositionLayerProjectionView> projection_views;
	std::vector<XrView> views;

// The runtime interacts with the OpenGL images (textures) via a Swapchain.
#ifdef _WIN32
	XrGraphicsBindingOpenGLWin32KHR graphics_binding_gl;
#endif
#ifdef X11
	XrGraphicsBindingOpenGLXlibKHR graphics_binding_gl;
#endif

	int64_t swapchain_format;
	// one array of images per view.
	std::vector<std::vector<XrSwapchainImageOpenGLKHR>> images;
	// one swapchain per view. Using only one and rendering l/r to the same image is also possible.
	std::vector<XrSwapchain> swapchains;

	int64_t depth_swapchain_format;
	std::vector<std::vector<XrSwapchainImageOpenGLKHR>> depth_images;
	std::vector<XrSwapchain> depth_swapchains;

	// quad layers are placed into world space, no need to render them per eye
	int64_t quad_swapchain_format;
	uint32_t quad_pixel_width, quad_pixel_height;
	uint32_t quad_swapchain_length;
	std::vector<XrSwapchainImageOpenGLKHR> quad_images;
	XrSwapchain quad_swapchain;

	float near_z;
	float far_z;

	// depth layer data
	struct
	{
		bool supported;
		std::vector<XrCompositionLayerDepthInfoKHR> infos;
	} depth;

	// cylinder layer extension data
	struct
	{
		bool supported;
		int64_t format;
		uint32_t swapchain_width, swapchain_height;
		uint32_t swapchain_length;
		std::vector<XrSwapchainImageOpenGLKHR> images;
		XrSwapchain swapchain;
	} cylinder;

	// To render into a texture we need a framebuffer (one per texture to make it easy)
	std::vector<std::vector<GLuint>> framebuffers;

	std::array<XrPath, HAND_COUNT> hand_paths;

	// hand tracking extension data
	struct
	{
		bool supported;
		// whether the current VR system in use has hand tracking
		bool system_supported;
		PFN_xrLocateHandJointsEXT pfnLocateHandJointsEXT;
		std::array<XrHandTrackerEXT, HAND_COUNT> trackers;
	} hand_tracking;
} xr_example;

bool xr_result(XrInstance instance, XrResult result, const char *format, ...)
{
	if (XR_SUCCEEDED(result))
		return true;

	char resultString[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(instance, result, resultString);

	size_t len1 = strlen(format);
	size_t len2 = strlen(resultString) + 1;
	char *formatRes = new char[len1 + len2 + 4]; // + " []\n"
	sprintf(formatRes, "%s [%s]\n", format, resultString);

	va_list args;
	va_start(args, format);
	vprintf(formatRes, args);
	va_end(args);

	delete[] formatRes;
	return false;
}

void sdl_handle_events(SDL_Event event, bool *running);

// some optional OpenXR calls demonstrated in functions to clutter the main app less
void get_instance_properties(XrInstance instance)
{
	XrResult result;
	XrInstanceProperties instance_props = {
		.type = XR_TYPE_INSTANCE_PROPERTIES,
		.next = NULL,
	};

	result = xrGetInstanceProperties(instance, &instance_props);
	if (!xr_result(NULL, result, "Failed to get instance info"))
		return;

	std::cout
		<< "Runtime Name: " << instance_props.runtimeName << "\n"
		<< "Runtime Version: " << XR_VERSION_MAJOR(instance_props.runtimeVersion) << "." << XR_VERSION_MINOR(instance_props.runtimeVersion) << "." << XR_VERSION_PATCH(instance_props.runtimeVersion) << std::endl;
}

void print_system_properties(XrSystemProperties *system_properties, bool hand_tracking_ext)
{
	std::cout
		<< "System properties for system " << system_properties->systemId << " \""
		<< system_properties->systemName << "\", vendor ID " << system_properties->vendorId << "\n"
		<< "\tMax layers          : " << system_properties->graphicsProperties.maxLayerCount << "\n"
		<< "\tMax swapchain height: " << system_properties->graphicsProperties.maxSwapchainImageHeight << "\n"
		<< "\tMax swapchain width : " << system_properties->graphicsProperties.maxSwapchainImageWidth << "\n"
		<< "\tOrientation Tracking: " << system_properties->trackingProperties.orientationTracking << "\n"
		<< "\tPosition Tracking   : " << system_properties->trackingProperties.positionTracking << std::endl;

	if (hand_tracking_ext)
	{
		XrSystemHandTrackingPropertiesEXT *ht = (XrSystemHandTrackingPropertiesEXT *)system_properties->next;
		std::cout << "\tHand Tracking       : " << ht->supportsHandTracking << std::endl;
	}
}

void print_supported_view_configs(XrExample *self)
{
	XrResult result;

	uint32_t view_config_count;
	result = xrEnumerateViewConfigurations(self->instance, self->system_id, 0, &view_config_count, NULL);
	if (!xr_result(self->instance, result, "Failed to get view configuration count"))
		return;

	std::cout << "Runtime supports " << view_config_count << " view configurations\n";

	std::vector<XrViewConfigurationType> view_configs(view_config_count);
	result = xrEnumerateViewConfigurations(self->instance, self->system_id, view_config_count, &view_config_count, view_configs.data());
	if (!xr_result(self->instance, result, "Failed to enumerate view configurations!"))
		return;

	std::cout << "Runtime supports view configurations:\n";
	for (uint32_t i = 0; i < view_config_count; ++i)
	{
		XrViewConfigurationProperties props = {.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
											   .next = NULL};

		result = xrGetViewConfigurationProperties(self->instance, self->system_id, view_configs[i], &props);
		if (!xr_result(self->instance, result, "Failed to get view configuration info %d!", i))
			return;

		std::cout << props.viewConfigurationType << ": FOV mutable: " << props.fovMutable << "\n";
	}
}

void print_viewconfig_view_info(XrExample *self)
{
	for (uint32_t i = 0; i < self->viewconfig_views.size(); i++)
	{
		printf("View Configuration View %d:\n", i);
		printf("\tResolution       : Recommended %dx%d, Max: %dx%d\n",
			   self->viewconfig_views[0].recommendedImageRectWidth,
			   self->viewconfig_views[0].recommendedImageRectHeight,
			   self->viewconfig_views[0].maxImageRectWidth,
			   self->viewconfig_views[0].maxImageRectHeight);
		printf("\tSwapchain Samples: Recommended: %d, Max: %d)\n",
			   self->viewconfig_views[0].recommendedSwapchainSampleCount,
			   self->viewconfig_views[0].maxSwapchainSampleCount);
	}
}

bool check_opengl_version(XrGraphicsRequirementsOpenGLKHR *opengl_reqs)
{
	XrVersion desired_opengl_version = XR_MAKE_VERSION(4, 3, 0);
	if (desired_opengl_version > opengl_reqs->maxApiVersionSupported ||
		desired_opengl_version < opengl_reqs->minApiVersionSupported)
	{
		printf(
			"We want OpenGL %d.%d.%d, but runtime only supports OpenGL %d.%d.%d - %d.%d.%d!\n",
			XR_VERSION_MAJOR(desired_opengl_version), XR_VERSION_MINOR(desired_opengl_version),
			XR_VERSION_PATCH(desired_opengl_version),
			XR_VERSION_MAJOR(opengl_reqs->minApiVersionSupported),
			XR_VERSION_MINOR(opengl_reqs->minApiVersionSupported),
			XR_VERSION_PATCH(opengl_reqs->minApiVersionSupported),
			XR_VERSION_MAJOR(opengl_reqs->maxApiVersionSupported),
			XR_VERSION_MINOR(opengl_reqs->maxApiVersionSupported),
			XR_VERSION_PATCH(opengl_reqs->maxApiVersionSupported));
		return false;
	}
	return true;
}

void print_reference_spaces(XrExample *self)
{
	XrResult result;

	uint32_t ref_space_count;
	result = xrEnumerateReferenceSpaces(self->session, 0, &ref_space_count, NULL);
	if (!xr_result(self->instance, result, "Getting number of reference spaces failed!"))
		return;

	std::vector<XrReferenceSpaceType> ref_spaces(ref_space_count);
	result = xrEnumerateReferenceSpaces(self->session, ref_space_count, &ref_space_count, ref_spaces.data());
	if (!xr_result(self->instance, result, "Enumerating reference spaces failed!"))
		return;

	printf("Runtime supports %d reference spaces:\n", ref_space_count);
	for (uint32_t i = 0; i < ref_space_count; i++)
	{
		if (ref_spaces[i] == XR_REFERENCE_SPACE_TYPE_LOCAL)
		{
			printf("\tXR_REFERENCE_SPACE_TYPE_LOCAL\n");
		}
		else if (ref_spaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE)
		{
			printf("\tXR_REFERENCE_SPACE_TYPE_STAGE\n");
		}
		else if (ref_spaces[i] == XR_REFERENCE_SPACE_TYPE_VIEW)
		{
			printf("\tXR_REFERENCE_SPACE_TYPE_VIEW\n");
		}
		else
		{
			printf("\tOther (extension?) refspace %u\\n", ref_spaces[i]);
		}
	}
}


void start_vr(void(start_render)(void)){
    
}

void update_vr(void(update_render)(glm::mat4,glm::mat4)){

}

XrHandJointLocationsEXT *get_joints_infos(){
    return NULL;
};

void end_vr(void(end_render)(void)){

};