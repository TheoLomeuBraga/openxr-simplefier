

#include "vr_manager.h"

#ifdef _WIN32
#define XR_USE_PLATFORM_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GL/gl.h>
#include <GL/wglext.h>
#endif

#ifdef X11
#define XR_USE_PLATFORM_XLIB
#define GLFW_EXPOSE_NATIVE_X11
#include <GL/glx.h>
#endif

#define XR_USE_GRAPHICS_API_OPENGL
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

std::string window_name = "window";

namespace my_math
{
	typedef enum
	{
		GRAPHICS_VULKAN,
		GRAPHICS_OPENGL,
		GRAPHICS_OPENGL_ES
	} GraphicsAPI;

	void XrMatrix4x4f_CreateProjectionFov(glm::mat4 &result,
										  const XrFovf &fov,
										  const float nearZ,
										  const float farZ)
	{
		const float tanAngleLeft = tanf(fov.angleLeft);
		const float tanAngleRight = tanf(fov.angleRight);
		const float tanAngleDown = tanf(fov.angleDown);
		const float tanAngleUp = tanf(fov.angleUp);

		const float tanAngleWidth = tanAngleRight - tanAngleLeft;
		const float tanAngleHeight = GRAPHICS_OPENGL_ES == GRAPHICS_VULKAN ? (tanAngleDown - tanAngleUp) : (tanAngleUp - tanAngleDown);
		const float offsetZ = (GRAPHICS_OPENGL_ES == GRAPHICS_OPENGL || GRAPHICS_OPENGL_ES == GRAPHICS_OPENGL_ES) ? nearZ : 0.0f;

		if (farZ <= nearZ)
		{
			// Infinite far plane
			result = glm::mat4(0.0f);
			result[0][0] = 2.0f / tanAngleWidth;
			result[1][1] = 2.0f / tanAngleHeight;
			result[2][2] = -1.0f;
			result[2][3] = -1.0f;
			result[3][2] = -(nearZ + offsetZ);
		}
		else
		{
			// Normal projection
			result = glm::mat4(0.0f);
			result[0][0] = 2.0f / tanAngleWidth;
			result[1][1] = 2.0f / tanAngleHeight;
			result[2][2] = -(farZ + offsetZ) / (farZ - nearZ);
			result[2][3] = -1.0f;
			result[3][2] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);
		}
	}

	void XrMatrix4x4f_CreateFromQuaternion(glm::mat4 &result, const glm::quat &quat)
	{
		glm::quat q(quat.w, quat.x, quat.y, quat.z);
		result = glm::mat4_cast(q);
	}

	/*
	void XrMatrix4x4f_CreateViewMatrix(glm::mat4 &result, const glm::vec3 &translation, const glm::quat &rotation)
	{
		glm::mat4 rotationMatrix;
		XrMatrix4x4f_CreateFromQuaternion(rotationMatrix, rotation);

		glm::mat4 translationMatrix;
		translationMatrix = glm::translate(glm::mat4(1.0f), translation);

		glm::mat4 viewMatrix = translationMatrix * rotationMatrix;

		glm::inverse(viewMatrix);
	}
	*/

	void XrMatrix4x4f_CreateViewMatrix(glm::mat4 &result, const glm::vec3 &translation, const glm::quat &rotation)
	{
		// Converte a rotação de quaternion para uma matriz de rotação
		glm::mat4 rotationMatrix = glm::toMat4(rotation);

		// Cria uma matriz de translação
		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);

		// A matriz de visão é o inverso da matriz de modelo
		result = glm::inverse(translationMatrix * rotationMatrix);
	}

	void XrMatrix4x4f_CreateModelMatrix(glm::mat4 &result, const glm::vec3 &translation, const glm::quat &rotation, const glm::vec3 &scale)
	{
		glm::mat4 scaleMatrix;
		scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

		glm::mat4 rotationMatrix;
		XrMatrix4x4f_CreateFromQuaternion(rotationMatrix, rotation);

		glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);

		glm::mat4 combinedMatrix = rotationMatrix * scaleMatrix;
		result = translationMatrix * combinedMatrix;
	}

	void printXrMatrix4x4(const glm::mat4 &matrix)
	{
		const float *m = glm::value_ptr(matrix);
		std::cout << m[0] << " " << m[1] << " " << m[2] << " " << m[3] << "\n"
				  << m[4] << " " << m[5] << " " << m[6] << " " << m[7] << "\n"
				  << m[8] << " " << m[9] << " " << m[10] << " " << m[11] << "\n"
				  << m[12] << " " << m[13] << " " << m[14] << " " << m[15] << "\n";
	}

};

GLuint shaderProgramID = 0;
GLuint VAOs[1] = {0};

static SDL_Window *desktop_window;
static SDL_GLContext gl_context;

void GLAPIENTRY
MessageCallback(GLenum source,
				GLenum type,
				GLuint id,
				GLenum severity,
				GLsizei length,
				const GLchar *message,
				const void *userParam)
{
	fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
			(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);
}

#ifdef _WIN32
bool init_sdl_window(HDC &xDisplay, HGLRC &glxContext, int w, int h)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Unable to initialize SDL");
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

	/* Create our window centered at half the VR resolution */
	desktop_window = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									  w / 2, h / 2, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!desktop_window)
	{
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

bool init_sdl_window(Display *&xDisplay, GLXContext &glxContext, int w, int h)
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Unable to initialize SDL: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

	/* Create our window centered at half the VR resolution */
	desktop_window = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
									  w / 2, h / 2, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!desktop_window)
	{
		printf("Unable to create window: %s\n", SDL_GetError());
		SDL_Quit();
		return false;
	}

	gl_context = SDL_GL_CreateContext(desktop_window);
	if (!gl_context)
	{
		printf("Unable to create OpenGL context: %s\n", SDL_GetError());
		SDL_DestroyWindow(desktop_window);
		SDL_Quit();
		return false;
	}

	GLenum err = glewInit();
	if (err != GLEW_OK)
	{
		printf("GLEW initialization failed: %s\n", glewGetErrorString(err));
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(desktop_window);
		SDL_Quit();
		return false;
	}

	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);

	SDL_GL_SetSwapInterval(0);

	xDisplay = glXGetCurrentDisplay();
	glxContext = glXGetCurrentContext();

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

XrExample self;



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

void reorientate(glm::vec3 new_position, glm::quat new_rotation)
{

	XrPosef teleport_pose = {
		.orientation = {.x = new_rotation.x, .y = new_rotation.y, .z = new_rotation.z, .w = new_rotation.w},
		.position = {.x = -new_position.x, .y = -new_position.y, .z = -new_position.z}};

	// Atualizando o espaço de referência com a nova pose de teleporte
	XrReferenceSpaceCreateInfo teleport_space_create_info = {
		.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next = NULL,
		.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL,
		.poseInReferenceSpace = teleport_pose};

	XrSpace teleport_space;
	XrResult result = xrCreateReferenceSpace(self.session, &teleport_space_create_info, &teleport_space);
	if (!xr_result(self.instance, result, "Failed to create teleport reference space!"))
	{
		return;
	}
	self.play_space = teleport_space;
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

void print_supported_view_configs(XrExample self)
{
	XrResult result;

	uint32_t view_config_count;
	result = xrEnumerateViewConfigurations(self.instance, self.system_id, 0, &view_config_count, NULL);
	if (!xr_result(self.instance, result, "Failed to get view configuration count"))
		return;

	std::cout << "Runtime supports " << view_config_count << " view configurations\n";

	std::vector<XrViewConfigurationType> view_configs(view_config_count);
	result = xrEnumerateViewConfigurations(self.instance, self.system_id, view_config_count, &view_config_count, view_configs.data());
	if (!xr_result(self.instance, result, "Failed to enumerate view configurations!"))
		return;

	std::cout << "Runtime supports view configurations:\n";
	for (uint32_t i = 0; i < view_config_count; ++i)
	{
		XrViewConfigurationProperties props = {.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
											   .next = NULL};

		result = xrGetViewConfigurationProperties(self.instance, self.system_id, view_configs[i], &props);
		if (!xr_result(self.instance, result, "Failed to get view configuration info %d!", i))
			return;

		std::cout << props.viewConfigurationType << ": FOV mutable: " << props.fovMutable << "\n";
	}
}

void print_viewconfig_view_info(XrExample self)
{
	for (uint32_t i = 0; i < self.viewconfig_views.size(); i++)
	{
		printf("View Configuration View %d:\n", i);
		printf("\tResolution       : Recommended %dx%d, Max: %dx%d\n",
			   self.viewconfig_views[0].recommendedImageRectWidth,
			   self.viewconfig_views[0].recommendedImageRectHeight,
			   self.viewconfig_views[0].maxImageRectWidth,
			   self.viewconfig_views[0].maxImageRectHeight);
		printf("\tSwapchain Samples: Recommended: %d, Max: %d)\n",
			   self.viewconfig_views[0].recommendedSwapchainSampleCount,
			   self.viewconfig_views[0].maxSwapchainSampleCount);
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

void print_reference_spaces(XrExample self)
{
	XrResult result;

	uint32_t ref_space_count;
	result = xrEnumerateReferenceSpaces(self.session, 0, &ref_space_count, NULL);
	if (!xr_result(self.instance, result, "Getting number of reference spaces failed!"))
		return;

	std::vector<XrReferenceSpaceType> ref_spaces(ref_space_count);
	result = xrEnumerateReferenceSpaces(self.session, ref_space_count, &ref_space_count, ref_spaces.data());
	if (!xr_result(self.instance, result, "Enumerating reference spaces failed!"))
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

void defout_event_manager(SDL_Event e) {}
void (*event_manager)(SDL_Event) = defout_event_manager;
void set_sdl_event_manager(void(sdl_event_manager)(SDL_Event))
{
	event_manager = sdl_event_manager;
}


void start_vr(std::string name, void(start_render)(void))
{
	window_name = name;

	XrResult result;

	// --- Make sure runtime supports the OpenGL extension

	// xrEnumerate*() functions are usually called once with CapacityInput = 0.
	// The function will write the required amount into CountOutput. We then have
	// to allocate an array to hold CountOutput elements and call the function
	// with CountOutput as CapacityInput.
	uint32_t ext_count = 0;
	result = xrEnumerateInstanceExtensionProperties(NULL, 0, &ext_count, NULL);

	/* TODO: instance null will not be able to convert XrResult to string */
	if (!xr_result(NULL, result, "Failed to enumerate number of extension properties"))
		return;

	printf("Runtime supports %d extensions\n", ext_count);

	std::vector<XrExtensionProperties> extensionProperties(ext_count, {XR_TYPE_EXTENSION_PROPERTIES, nullptr});
	result = xrEnumerateInstanceExtensionProperties(NULL, ext_count, &ext_count, extensionProperties.data());
	if (!xr_result(NULL, result, "Failed to enumerate extension properties"))
		return;

	bool opengl_ext = false;
	for (uint32_t i = 0; i < ext_count; i++)
	{
		printf("\t%s v%d\n", extensionProperties[i].extensionName, extensionProperties[i].extensionVersion);
		if (strcmp(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, extensionProperties[i].extensionName) == 0)
		{
			opengl_ext = true;
		}

		if (strcmp(XR_EXT_HAND_TRACKING_EXTENSION_NAME, extensionProperties[i].extensionName) == 0)
		{
			self.hand_tracking.supported = true;
		}

		if (strcmp(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME, extensionProperties[i].extensionName) == 0)
		{
			self.cylinder.supported = true;
		}

		if (strcmp(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, extensionProperties[i].extensionName) == 0)
		{
			self.depth.supported = true;
		}
	}

	// A graphics extension like OpenGL is required to draw anything in VR
	if (!opengl_ext)
	{
		printf("Runtime does not support OpenGL extension!\n");
		return;
	}

	printf("Runtime supports extensions:\n");
	printf("\t%s: %d\n", XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, opengl_ext);
	printf("\t%s: %d\n", XR_EXT_HAND_TRACKING_EXTENSION_NAME, self.hand_tracking.supported);
	printf("\t%s: %d\n", XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME, self.cylinder.supported);
	printf("\t%s: %d\n", XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME, self.depth.supported);

	// --- Create XrInstance
	int enabled_ext_count = 1;
	const char *enabled_exts[3] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};

	if (self.hand_tracking.supported)
	{
		enabled_exts[enabled_ext_count++] = XR_EXT_HAND_TRACKING_EXTENSION_NAME;
	}
	if (self.cylinder.supported)
	{
		enabled_exts[enabled_ext_count++] = XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME;
	}

	// same can be done for API layers, but API layers can also be enabled by env var

	XrInstanceCreateInfo instance_create_info = {
		XR_TYPE_INSTANCE_CREATE_INFO,
		nullptr,
		0,
		{
			"app_name",
			1,
			"Custom",
			0,
			XR_CURRENT_API_VERSION,
		},
		0,
		NULL,
		(uint32_t)enabled_ext_count,
		enabled_exts};

	strncpy(instance_create_info.applicationInfo.applicationName, window_name.c_str(), sizeof(instance_create_info.applicationInfo.applicationName));

	result = xrCreateInstance(&instance_create_info, &self.instance);
	if (!xr_result(NULL, result, "Failed to create XR instance."))
		return;

	// Optionally get runtime name and version
	get_instance_properties(self.instance);

	// --- Create XrSystem
	XrSystemGetInfo system_get_info = {.type = XR_TYPE_SYSTEM_GET_INFO,
									   .next = NULL,
									   .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY};

	result = xrGetSystem(self.instance, &system_get_info, &self.system_id);
	if (!xr_result(self.instance, result, "Failed to get system for HMD form factor."))
		return;

	printf("Successfully got XrSystem with id %lu for HMD form factor\n", self.system_id);

	// checking system properties is generally  optional, but we are interested in hand tracking
	// support
	{
		XrSystemProperties system_props = {
			.type = XR_TYPE_SYSTEM_PROPERTIES,
			.next = NULL,
			.graphicsProperties = {0},
			.trackingProperties = {0},
		};

		XrSystemHandTrackingPropertiesEXT ht = {.type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
												.next = NULL};
		if (self.hand_tracking.supported)
		{
			system_props.next = &ht;
		}

		result = xrGetSystemProperties(self.instance, self.system_id, &system_props);
		if (!xr_result(self.instance, result, "Failed to get System properties"))
			return;

		self.hand_tracking.system_supported = self.hand_tracking.supported && ht.supportsHandTracking;

		print_system_properties(&system_props, self.hand_tracking.supported);
	}

	print_supported_view_configs(self);
	// Stereo is most common for VR. We could check if stereo is supported and maybe choose another
	// one, but as this app is only tested with stereo, we assume it is (next call will error anyway
	// if not).
	XrViewConfigurationType view_type = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

	uint32_t view_count = 0;
	result = xrEnumerateViewConfigurationViews(self.instance, self.system_id, view_type, 0, &view_count, NULL);
	if (!xr_result(self.instance, result, "Failed to get view configuration view count!"))
		return;

	self.viewconfig_views.resize(view_count, {XR_TYPE_VIEW_CONFIGURATION_VIEW, nullptr});

	result = xrEnumerateViewConfigurationViews(self.instance, self.system_id, view_type, view_count, &view_count, self.viewconfig_views.data());
	if (!xr_result(self.instance, result, "Failed to enumerate view configuration views!"))
		return;
	print_viewconfig_view_info(self);

	// OpenXR requires checking graphics requirements before creating a session.
	XrGraphicsRequirementsOpenGLKHR opengl_reqs = {.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR, .next = NULL};

	PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
	{
		result = xrGetInstanceProcAddr(self.instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&pfnGetOpenGLGraphicsRequirementsKHR);
		if (!xr_result(self.instance, result, "Failed to get OpenGL graphics requirements function!"))
			return;
	}

	result = pfnGetOpenGLGraphicsRequirementsKHR(self.instance, self.system_id, &opengl_reqs);
	if (!xr_result(self.instance, result, "Failed to get OpenGL graphics requirements!"))
		return;

	// On OpenGL we never fail this check because the version requirement is not useful.
	// Other APIs may have more useful requirements.
	check_opengl_version(&opengl_reqs);

#ifdef _WIN32
	self.graphics_binding_gl = XrGraphicsBindingOpenGLWin32KHR{
		.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
	};

	// create SDL window the size of the left eye & fill GL graphics binding info

	if (!init_sdl_window(self.graphics_binding_gl.hDC, self.graphics_binding_gl.hGLRC,
						 self.viewconfig_views[0].recommendedImageRectWidth,
						 self.viewconfig_views[0].recommendedImageRectHeight))
	{
		printf("GLX init failed!\n");
		return;
	}

#endif

#ifdef X11
	self.graphics_binding_gl = XrGraphicsBindingOpenGLXlibKHR{
		.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR,
	};

	// create SDL window the size of the left eye & fill GL graphics binding info
	if (!init_sdl_window(self.graphics_binding_gl.xDisplay, self.graphics_binding_gl.glxContext,
						 self.viewconfig_views[0].recommendedImageRectWidth,
						 self.viewconfig_views[0].recommendedImageRectHeight))
	{
		printf("X11 init failed!\n");
		return;
	}
#endif

	printf("Using OpenGL version: %s\n", glGetString(GL_VERSION));
	printf("Using OpenGL Renderer: %s\n", glGetString(GL_RENDERER));

	// Set up rendering (compile shaders, ...)
	start_render();

	self.state = XR_SESSION_STATE_UNKNOWN;

	XrSessionCreateInfo session_create_info = {.type = XR_TYPE_SESSION_CREATE_INFO,
											   .next = &self.graphics_binding_gl,
											   .systemId = self.system_id};

	result = xrCreateSession(self.instance, &session_create_info, &self.session);
	if (!xr_result(self.instance, result, "Failed to create session"))
		return;

	printf("Successfully created a session with OpenGL!\n");

	if (self.hand_tracking.system_supported)
	{
		result = xrGetInstanceProcAddr(self.instance, "xrLocateHandJointsEXT", (PFN_xrVoidFunction *)&self.hand_tracking.pfnLocateHandJointsEXT);
		xr_result(self.instance, result, "Failed to get xrLocateHandJointsEXT function!");

		PFN_xrCreateHandTrackerEXT pfnCreateHandTrackerEXT = NULL;
		result = xrGetInstanceProcAddr(self.instance, "xrCreateHandTrackerEXT", (PFN_xrVoidFunction *)&pfnCreateHandTrackerEXT);

		if (!xr_result(self.instance, result, "Failed to get xrCreateHandTrackerEXT function!"))
			return;

		{
			XrHandTrackerCreateInfoEXT hand_tracker_create_info = {
				.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
				.next = NULL,
				.hand = XR_HAND_LEFT_EXT,
				.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT};
			result = pfnCreateHandTrackerEXT(self.session, &hand_tracker_create_info,
											 &self.hand_tracking.trackers[HAND_LEFT]);
			if (!xr_result(self.instance, result, "Failed to create left hand tracker"))
			{
				return;
			}
			printf("Created hand tracker for left hand\n");
		}
		{
			XrHandTrackerCreateInfoEXT hand_tracker_create_info = {
				.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
				.next = NULL,
				.hand = XR_HAND_RIGHT_EXT,
				.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT};
			result = pfnCreateHandTrackerEXT(self.session, &hand_tracker_create_info,
											 &self.hand_tracking.trackers[HAND_RIGHT]);
			if (!xr_result(self.instance, result, "Failed to create right hand tracker"))
			{
				return;
			}
			printf("Created hand tracker for right hand\n");
		}
	}

	XrReferenceSpaceType play_space_type = XR_REFERENCE_SPACE_TYPE_LOCAL;
	// We could check if our ref space type is supported, but next call will error anyway if not
	print_reference_spaces(self);

	XrReferenceSpaceCreateInfo play_space_create_info = {.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
														 .next = NULL,
														 .referenceSpaceType = play_space_type,
														 .poseInReferenceSpace = identity_pose};

	result = xrCreateReferenceSpace(self.session, &play_space_create_info, &self.play_space);
	if (!xr_result(self.instance, result, "Failed to create play space!"))
		return;

	// --- Begin session
	XrSessionBeginInfo session_begin_info = {.type = XR_TYPE_SESSION_BEGIN_INFO, .next = NULL, .primaryViewConfigurationType = view_type};
	result = xrBeginSession(self.session, &session_begin_info);
	if (!xr_result(self.instance, result, "Failed to begin session!"))
		return;
	printf("Session started!\n");

	// --- Create Swapchains
	uint32_t swapchain_format_count;
	result = xrEnumerateSwapchainFormats(self.session, 0, &swapchain_format_count, NULL);
	if (!xr_result(self.instance, result, "Failed to get number of supported swapchain formats"))
		return;

	printf("Runtime supports %d swapchain formats\n", swapchain_format_count);
	std::vector<int64_t> swapchain_formats(swapchain_format_count);
	result = xrEnumerateSwapchainFormats(self.session, swapchain_format_count, &swapchain_format_count, swapchain_formats.data());
	if (!xr_result(self.instance, result, "Failed to enumerate swapchain formats"))
		return;

	// SRGB is usually the best choice. Selection logic should be expanded though.
	int64_t preferred_swapchain_format = GL_SRGB8_ALPHA8;
	// Using a depth format that directly maps to vulkan is a good idea:
	// GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT32F
	int64_t preferred_depth_swapchain_format = GL_DEPTH_COMPONENT32F;
	int64_t preferred_quad_swapchain_format = GL_RGBA8_EXT;

	self.swapchain_format = swapchain_formats[0];
	self.quad_swapchain_format = swapchain_formats[0];
	self.cylinder.format = swapchain_formats[0];
	self.depth_swapchain_format = -1;
	for (auto &swapchain_format : swapchain_formats)
	{
		printf("Supported GL format: %#lx\n", swapchain_format);
		if (swapchain_format == preferred_swapchain_format)
		{
			self.swapchain_format = swapchain_format;
			printf("Using preferred swapchain format %#lx\n", self.swapchain_format);
		}
		if (swapchain_format == preferred_depth_swapchain_format)
		{
			self.depth_swapchain_format = swapchain_format;
			printf("Using preferred depth swapchain format %#lx\n", self.depth_swapchain_format);
		}
		if (swapchain_format == preferred_quad_swapchain_format)
		{
			self.quad_swapchain_format = swapchain_format;
			self.cylinder.format = swapchain_format;
			printf("Using preferred quad swapchain format %#lx\n", self.quad_swapchain_format);
		}
	}

	if (self.swapchain_format != preferred_swapchain_format)
	{
		printf("Using non preferred swapchain format %#lx\n", self.swapchain_format);
	}
	/* All OpenGL textures that will be submitted in xrEndFrame are created by the runtime here.
	 * The runtime will give us a number (not controlled by us) of OpenGL textures per swapchain
	 * and tell us with xrAcquireSwapchainImage, which of those we can render to per frame.
	 * Here we use one swapchain per view (eye), and for example 3 ("triple buffering") images per
	 * swapchain.
	 */
	self.swapchains.resize(view_count);
	self.images.resize(view_count);
	for (uint32_t i = 0; i < view_count; i++)
	{
		XrSwapchainCreateInfo swapchain_create_info;
		swapchain_create_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
		swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_create_info.createFlags = 0;
		swapchain_create_info.format = self.swapchain_format;
		swapchain_create_info.sampleCount = self.viewconfig_views[i].recommendedSwapchainSampleCount;
		swapchain_create_info.width = self.viewconfig_views[i].recommendedImageRectWidth;
		swapchain_create_info.height = self.viewconfig_views[i].recommendedImageRectHeight;
		swapchain_create_info.faceCount = 1;
		swapchain_create_info.arraySize = 1;
		swapchain_create_info.mipCount = 1;
		swapchain_create_info.next = NULL;

		result = xrCreateSwapchain(self.session, &swapchain_create_info, &self.swapchains[i]);
		if (!xr_result(self.instance, result, "Failed to create swapchain %d!", i))
			return;

		uint32_t swapchain_length;
		result = xrEnumerateSwapchainImages(self.swapchains[i], 0, &swapchain_length, nullptr);
		if (!xr_result(self.instance, result, "Failed to enumerate swapchains"))
			return;

		// these are wrappers for the actual OpenGL texture id
		self.images[i].resize(swapchain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});
		result = xrEnumerateSwapchainImages(self.swapchains[i], swapchain_length, &swapchain_length, (XrSwapchainImageBaseHeader *)self.images[i].data());
		if (!xr_result(self.instance, result, "Failed to enumerate swapchain images"))
			return;
	}

	/* Allocate resources that we use for our own rendering.
	 * We will bind framebuffers to the runtime provided textures for rendering.
	 * For this, we create one framebuffer per OpenGL texture.
	 * This is not mandated by OpenXR, other ways to render to textures will work too.
	 */
	self.framebuffers.resize(view_count);
	for (uint32_t i = 0; i < view_count; i++)
	{
		self.framebuffers[i].resize(self.images[i].size());
		glGenFramebuffers(self.framebuffers[i].size(), self.framebuffers[i].data());
	}

	if (self.depth_swapchain_format == -1)
	{
		printf("Preferred depth swapchain format %#lx not supported!\n",
			   preferred_depth_swapchain_format);
	}

	if (self.depth_swapchain_format != -1)
	{
		self.depth_swapchains.resize(view_count);
		self.depth_images.resize(view_count);
		for (uint32_t i = 0; i < view_count; i++)
		{
			XrSwapchainCreateInfo swapchain_create_info;
			swapchain_create_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
			swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			swapchain_create_info.createFlags = 0;
			swapchain_create_info.format = self.depth_swapchain_format;
			swapchain_create_info.sampleCount = self.viewconfig_views[i].recommendedSwapchainSampleCount;
			swapchain_create_info.width = self.viewconfig_views[i].recommendedImageRectWidth;
			swapchain_create_info.height = self.viewconfig_views[i].recommendedImageRectHeight;
			swapchain_create_info.faceCount = 1;
			swapchain_create_info.arraySize = 1;
			swapchain_create_info.mipCount = 1;
			swapchain_create_info.next = NULL;

			result = xrCreateSwapchain(self.session, &swapchain_create_info, &self.depth_swapchains[i]);
			if (!xr_result(self.instance, result, "Failed to create swapchain %d!", i))
				return;

			uint32_t depth_swapchain_length;
			result = xrEnumerateSwapchainImages(self.depth_swapchains[i], 0, &depth_swapchain_length, nullptr);
			if (!xr_result(self.instance, result, "Failed to enumerate swapchains"))
				return;

			// these are wrappers for the actual OpenGL texture id
			self.depth_images[i].resize(depth_swapchain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});
			result = xrEnumerateSwapchainImages(self.depth_swapchains[i], depth_swapchain_length, &depth_swapchain_length, (XrSwapchainImageBaseHeader *)self.depth_images[i].data());
			if (!xr_result(self.instance, result, "Failed to enumerate swapchain images"))
				return;
		}
	}

	{
		self.quad_pixel_width = 800;
		self.quad_pixel_height = 600;
		XrSwapchainCreateInfo swapchain_create_info;
		swapchain_create_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
		swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_create_info.createFlags = 0;
		swapchain_create_info.format = self.quad_swapchain_format;
		swapchain_create_info.sampleCount = 1;
		swapchain_create_info.width = self.quad_pixel_width;
		swapchain_create_info.height = self.quad_pixel_height;
		swapchain_create_info.faceCount = 1;
		swapchain_create_info.arraySize = 1;
		swapchain_create_info.mipCount = 1;
		swapchain_create_info.next = NULL;

		result = xrCreateSwapchain(self.session, &swapchain_create_info, &self.quad_swapchain);
		if (!xr_result(self.instance, result, "Failed to create swapchain!"))
			return;

		result = xrEnumerateSwapchainImages(self.quad_swapchain, 0, &self.quad_swapchain_length, NULL);
		if (!xr_result(self.instance, result, "Failed to enumerate swapchains"))
			return;

		// these are wrappers for the actual OpenGL texture id
		self.quad_images.resize(self.quad_swapchain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});
		result = xrEnumerateSwapchainImages(self.quad_swapchain, self.quad_swapchain_length,
											&self.quad_swapchain_length,
											(XrSwapchainImageBaseHeader *)self.quad_images.data());
		if (!xr_result(self.instance, result, "Failed to enumerate swapchain images"))
			return;
	}

	if (self.cylinder.supported)
	{
		self.cylinder.swapchain_width = 800;
		self.cylinder.swapchain_height = 600;
		XrSwapchainCreateInfo swapchain_create_info;
		swapchain_create_info.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
		swapchain_create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_create_info.createFlags = 0;
		swapchain_create_info.format = self.cylinder.format;
		swapchain_create_info.sampleCount = 1;
		swapchain_create_info.width = self.cylinder.swapchain_width;
		swapchain_create_info.height = self.cylinder.swapchain_height;
		swapchain_create_info.faceCount = 1;
		swapchain_create_info.arraySize = 1;
		swapchain_create_info.mipCount = 1;
		swapchain_create_info.next = NULL;

		result = xrCreateSwapchain(self.session, &swapchain_create_info, &self.cylinder.swapchain);
		if (!xr_result(self.instance, result, "Failed to create swapchain!"))
			return;

		result = xrEnumerateSwapchainImages(self.cylinder.swapchain, 0,
											&self.cylinder.swapchain_length, NULL);
		if (!xr_result(self.instance, result, "Failed to enumerate swapchains"))
			return;

		// these are wrappers for the actual OpenGL texture id
		self.cylinder.images.resize(self.cylinder.swapchain_length, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR, nullptr});
		result = xrEnumerateSwapchainImages(self.cylinder.swapchain, self.cylinder.swapchain_length,
											&self.cylinder.swapchain_length,
											(XrSwapchainImageBaseHeader *)self.cylinder.images.data());
		if (!xr_result(self.instance, result, "Failed to enumerate swapchain images"))
			return;
	}

	self.near_z = 0.01f;
	self.far_z = 100.f;

	// A stereo view config implies two views, but our code is set up for a dynamic amount of views.
	// So we need to allocate a bunch of memory for data structures dynamically.
	self.views.resize(view_count, {XR_TYPE_VIEW, nullptr});
	self.projection_views.resize(view_count);
	for (uint32_t i = 0; i < view_count; i++)
	{
		self.projection_views[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		self.projection_views[i].next = NULL;

		
		self.projection_views[i].subImage.swapchain = self.swapchains[i];
		self.projection_views[i].subImage.imageArrayIndex = 0;
		self.projection_views[i].subImage.imageRect.offset.x = 0;
		self.projection_views[i].subImage.imageRect.offset.y = 0;
		self.projection_views[i].subImage.imageRect.extent.width = self.viewconfig_views[i].recommendedImageRectWidth;
		self.projection_views[i].subImage.imageRect.extent.height = self.viewconfig_views[i].recommendedImageRectHeight;

		// projection_views[i].pose (and fov) have to be filled every frame in frame loop
	};

	// analog to projection layer allocation, though we can actually fill everything in here
	if (self.depth.supported)
	{
		self.depth.infos.resize(view_count);
		for (uint32_t i = 0; i < view_count; i++)
		{
			self.depth.infos[i].type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
			self.depth.infos[i].next = NULL;
			self.depth.infos[i].minDepth = 0.f;
			self.depth.infos[i].maxDepth = 1.f;
			self.depth.infos[i].nearZ = self.near_z;
			self.depth.infos[i].farZ = self.far_z;

			self.depth.infos[i].subImage.swapchain = self.depth_swapchains[i];

			self.depth.infos[i].subImage.imageArrayIndex = 0;
			self.depth.infos[i].subImage.imageRect.offset.x = 0;
			self.depth.infos[i].subImage.imageRect.offset.y = 0;
			self.depth.infos[i].subImage.imageRect.extent.width = self.viewconfig_views[i].recommendedImageRectWidth;
			self.depth.infos[i].subImage.imageRect.extent.height = self.viewconfig_views[i].recommendedImageRectHeight;

			self.projection_views[i].next = &self.depth.infos[i];
		};
	}
}

bool continue_vr = true;

void render_quad(int w,
				 int h,
				 int64_t swapchain_format,
				 XrSwapchainImageOpenGLKHR image,
				 XrTime predictedDisplayTime)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, image.image);

	glViewport(0, 0, w, h);
	glScissor(0, 0, w, h);

	uint8_t *rgb = new uint8_t[w * h * 4];
	for (int row = 0; row < h; row++)
	{
		for (int col = 0; col < w; col++)
		{
			uint8_t *base = &rgb[(row * w * 4 + col * 4)];
			*(base + 0) = (((float)row / (float)h)) * 255.;
			*(base + 1) = 0;
			*(base + 2) = 0;
			*(base + 3) = 0;

			if (abs(row - col) < 3)
			{
				*(base + 0) = 255.;
				*(base + 1) = 255;
				*(base + 2) = 255;
				*(base + 3) = 0;
			}

			if (abs((w - col) - (row)) < 3)
			{
				*(base + 0) = 0.;
				*(base + 1) = 0;
				*(base + 2) = 0;
				*(base + 3) = 0;
			}
		}
	}

	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE,
					(GLvoid *)rgb);
	delete[] rgb;
}

std::map<unsigned char, float> actions_map = {
	std::pair<unsigned char, float>(0, 0.0),
	std::pair<unsigned char, float>(1, 0.0),
	std::pair<unsigned char, float>(2, 0.0),
	std::pair<unsigned char, float>(3, 0.0),
	std::pair<unsigned char, float>(4, 0.0),
	std::pair<unsigned char, float>(5, 0.0),
	std::pair<unsigned char, float>(6, 0.0),
	std::pair<unsigned char, float>(7, 0.0),
	std::pair<unsigned char, float>(8, 0.0),
	std::pair<unsigned char, float>(9, 0.0),
	std::pair<unsigned char, float>(10, 0.0),
	std::pair<unsigned char, float>(11, 0.0),
	std::pair<unsigned char, float>(12, 0.0),
	std::pair<unsigned char, float>(13, 0.0),
};

std::map<unsigned char, vr_pose> traker_pose_map = {
	std::pair<unsigned char, vr_pose>(0, {glm::vec3(0, 0, 0), glm::quat(0, 0, 0, 1)}),
	std::pair<unsigned char, vr_pose>(1, {glm::vec3(0, 0, 0), glm::quat(0, 0, 0, 1)}),
	std::pair<unsigned char, vr_pose>(2, {glm::vec3(0, 0, 0), glm::quat(0, 0, 0, 1)}),

};

XrHandJointLocationEXT joints[HAND_COUNT][XR_HAND_JOINT_COUNT_EXT];
XrHandJointLocationsEXT joint_locations[HAND_COUNT] = {{}};
std::vector<vr_pose> get_vr_joints_infos(vr_traker_type hand)
{
	XrHandJointLocationEXT *hand_joints = nullptr;
	if (hand == vr_left_hand)
	{
		hand_joints = joints[HAND_LEFT];
	}
	else if (hand == vr_right_hand)
	{
		hand_joints = joints[HAND_RIGHT];
	}
	else
	{
		return {};
	}
	std::vector<vr_pose> ret;
	for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++)
	{
		XrPosef p = hand_joints[i].pose;
		ret.push_back({glm::vec3(p.position.x, p.position.y, p.position.z), glm::quat(p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z)});
	}
	return ret;
}

void stop_vr()
{
	continue_vr = false;
}

void update_vr(void(before_render)(void), void(update_render)(unsigned int, glm::ivec2, glm::mat4, glm::mat4), void(after_render)(void))
{

	XrResult result;

	XrActionSetCreateInfo main_actionset_info = {.type = XR_TYPE_ACTION_SET_CREATE_INFO, .next = NULL, .priority = 0};
	strcpy(main_actionset_info.actionSetName, "mainactions");
	strcpy(main_actionset_info.localizedActionSetName, "Main Actions");

	XrActionSet main_actionset;
	result = xrCreateActionSet(self.instance, &main_actionset_info, &main_actionset);
	if (!xr_result(self.instance, result, "failed to create actionset"))
		return;

	xrStringToPath(self.instance, "/user/hand/left", &self.hand_paths[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right", &self.hand_paths[HAND_RIGHT]);

	XrAction pose_action;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_POSE_INPUT,
										  .countSubactionPaths = HAND_COUNT,
										  .subactionPaths = self.hand_paths.data()};
		strcpy(action_info.actionName, "handpose");
		strcpy(action_info.localizedActionName, "Hand Pose");

		result = xrCreateAction(main_actionset, &action_info, &pose_action);
		if (!xr_result(self.instance, result, "failed to create pose action"))
			return;
	}

	XrAction grab_action_float;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_FLOAT_INPUT,
										  .countSubactionPaths = HAND_COUNT,
										  .subactionPaths = self.hand_paths.data()};
		strcpy(action_info.actionName, "grab");
		strcpy(action_info.localizedActionName, "Grab");

		result = xrCreateAction(main_actionset, &action_info, &grab_action_float);
		if (!xr_result(self.instance, result, "failed to create grab action"))
			return;
	}

	XrAction use_action_float;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_FLOAT_INPUT,
										  .countSubactionPaths = HAND_COUNT,
										  .subactionPaths = self.hand_paths.data()};
		strcpy(action_info.actionName, "use");
		strcpy(action_info.localizedActionName, "Use");

		result = xrCreateAction(main_actionset, &action_info, &use_action_float);
		if (!xr_result(self.instance, result, "failed to create use action"))
			return;
	}

	XrAction use_2_action_boolean;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_BOOLEAN_INPUT,
										  .countSubactionPaths = HAND_COUNT,
										  .subactionPaths = self.hand_paths.data()};
		strcpy(action_info.actionName, "use_2");
		strcpy(action_info.localizedActionName, "Use 2");

		result = xrCreateAction(main_actionset, &action_info, &use_2_action_boolean);
		if (!xr_result(self.instance, result, "failed to create use 2 action"))
			return;
	}

	XrAction use_3_action_boolean;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_BOOLEAN_INPUT,
										  .countSubactionPaths = HAND_COUNT,
										  .subactionPaths = self.hand_paths.data()};
		strcpy(action_info.actionName, "use_3");
		strcpy(action_info.localizedActionName, "Use 3");

		result = xrCreateAction(main_actionset, &action_info, &use_3_action_boolean);
		if (!xr_result(self.instance, result, "failed to create use 3 action"))
			return;
	}

	XrAction vibration_action;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT,
										  .countSubactionPaths = HAND_COUNT,
										  .subactionPaths = self.hand_paths.data()};
		strcpy(action_info.actionName, "vibration");
		strcpy(action_info.localizedActionName, "Vibration");

		result = xrCreateAction(main_actionset, &action_info, &vibration_action);
		if (!xr_result(self.instance, result, "failed to create vibration action"))
			return;
	}

	XrAction move_x_z_action_vec_2;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_VECTOR2F_INPUT,
										  .countSubactionPaths = 1,
										  .subactionPaths = &self.hand_paths[HAND_LEFT]};
		strcpy(action_info.actionName, "move_x_y");
		strcpy(action_info.localizedActionName, "Move x y");

		result = xrCreateAction(main_actionset, &action_info, &move_x_z_action_vec_2);
		if (!xr_result(self.instance, result, "failed to create Move x y action"))
			return;
	}

	XrAction move_rotation_y_action_vec_2;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_VECTOR2F_INPUT,
										  .countSubactionPaths = 1,
										  .subactionPaths = &self.hand_paths[HAND_RIGHT]};
		strcpy(action_info.actionName, "move_y");
		strcpy(action_info.localizedActionName, "Move y");

		result = xrCreateAction(main_actionset, &action_info, &move_rotation_y_action_vec_2);
		if (!xr_result(self.instance, result, "failed to create move y action"))
			return;
	}

	XrAction menu_action_boolean;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_BOOLEAN_INPUT,
										  .countSubactionPaths = 1,
										  .subactionPaths = &self.hand_paths[HAND_LEFT]};
		strcpy(action_info.actionName, "menu");
		strcpy(action_info.localizedActionName, "Menu");

		result = xrCreateAction(main_actionset, &action_info, &menu_action_boolean);
		if (!xr_result(self.instance, result, "failed to create menu action"))
			return;
	}

	XrAction teleport_action_boolean;
	{
		XrActionCreateInfo action_info = {.type = XR_TYPE_ACTION_CREATE_INFO,
										  .next = NULL,
										  .actionType = XR_ACTION_TYPE_BOOLEAN_INPUT,
										  .countSubactionPaths = 1,
										  .subactionPaths = &self.hand_paths[HAND_LEFT]};
		strcpy(action_info.actionName, "teleport");
		strcpy(action_info.localizedActionName, "Teleport");

		result = xrCreateAction(main_actionset, &action_info, &teleport_action_boolean);
		if (!xr_result(self.instance, result, "failed to create teleport action"))
			return;
	}

	// quest 2
	/*
	User Paths

		/user/hand/left
		/user/hand/right

	Input Component Paths
	Left Hand

		/user/hand/left/input/trigger
		/user/hand/left/input/trigger/value
		/user/hand/left/input/squeeze
		/user/hand/left/input/squeeze/value
		/user/hand/left/input/thumbstick
		/user/hand/left/input/thumbstick/x
		/user/hand/left/input/thumbstick/y
		/user/hand/left/input/thumbstick/click
		/user/hand/left/input/thumbrest
		/user/hand/left/input/thumbrest/touch
		/user/hand/left/input/system
		/user/hand/left/input/menu
		/user/hand/left/input/b
		/user/hand/left/input/b/touch
		/user/hand/left/input/x
		/user/hand/left/input/x/touch

	Right Hand

		/user/hand/right/input/trigger
		/user/hand/right/input/trigger/value
		/user/hand/right/input/squeeze
		/user/hand/right/input/squeeze/value
		/user/hand/right/input/thumbstick
		/user/hand/right/input/thumbstick/x
		/user/hand/right/input/thumbstick/y
		/user/hand/right/input/thumbstick/click
		/user/hand/right/input/thumbrest
		/user/hand/right/input/thumbrest/touch
		/user/hand/right/input/system
		/user/hand/right/input/menu
		/user/hand/right/input/a
		/user/hand/right/input/a/touch
		/user/hand/right/input/y
		/user/hand/right/input/y/touch

	Output Component Paths
	Haptics

		/user/hand/left/output/haptic
		/user/hand/right/output/haptic

	Pose Paths

		/user/hand/left/input/grip/pose
		/user/hand/left/input/aim/pose
		/user/hand/right/input/grip/pose
		/user/hand/right/input/aim/pose
	*/

	// oque o path /user/hand/left/input/grip/pose leva e qual e o path da posição da mão ?

	XrPath grip_pose_path[HAND_COUNT];
	xrStringToPath(self.instance, "/user/hand/left/input/grip/pose", &grip_pose_path[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right/input/grip/pose", &grip_pose_path[HAND_RIGHT]);

	XrPath grab_path[HAND_COUNT];
	xrStringToPath(self.instance, "/user/hand/left/input/squeeze/value", &grab_path[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right/input/squeeze/value", &grab_path[HAND_RIGHT]);

	XrPath use_path[HAND_COUNT];
	xrStringToPath(self.instance, "/user/hand/left/input/trigger/value", &use_path[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right/input/trigger/value", &use_path[HAND_RIGHT]);

	XrPath use_2_path[HAND_COUNT];
	xrStringToPath(self.instance, "/user/hand/left/input/x/click", &use_2_path[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right/input/a/click", &use_2_path[HAND_RIGHT]);

	XrPath use_3_path[HAND_COUNT];
	xrStringToPath(self.instance, "/user/hand/left/input/y/click", &use_3_path[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right/input/b/click", &use_3_path[HAND_RIGHT]);

	XrPath vibrate_path[HAND_COUNT];
	xrStringToPath(self.instance, "/user/hand/left/output/haptic", &vibrate_path[HAND_LEFT]);
	xrStringToPath(self.instance, "/user/hand/right/output/haptic", &vibrate_path[HAND_RIGHT]);

	XrPath movement_x_z_path;
	xrStringToPath(self.instance, "/user/hand/left/input/thumbstick", &movement_x_z_path);

	XrPath movement_rotation_y_path;
	xrStringToPath(self.instance, "/user/hand/right/input/thumbstick", &movement_rotation_y_path);

	XrPath teleport_path;
	xrStringToPath(self.instance, "/user/hand/left/input/thumbstick/click", &teleport_path);

	XrPath special_path;
	xrStringToPath(self.instance, "/user/hand/right/input/b", &special_path);

	XrPath menu_path;
	xrStringToPath(self.instance, "/user/hand/left/input/menu", &menu_path);

	{
		XrPath interaction_profile_path;
		result = xrStringToPath(self.instance, "/interaction_profiles/oculus/touch_controller",
								&interaction_profile_path);
		if (!xr_result(self.instance, result, "failed to get interaction profile"))
			return;

		const XrActionSuggestedBinding bindings[] = {
			{.action = pose_action, .binding = grip_pose_path[HAND_LEFT]},
			{.action = pose_action, .binding = grip_pose_path[HAND_RIGHT]},

			{.action = grab_action_float, .binding = grab_path[HAND_LEFT]},
			{.action = grab_action_float, .binding = grab_path[HAND_RIGHT]},

			{.action = use_action_float, .binding = use_path[HAND_LEFT]},
			{.action = use_action_float, .binding = use_path[HAND_RIGHT]},

			{.action = use_2_action_boolean, .binding = use_2_path[HAND_LEFT]},
			{.action = use_2_action_boolean, .binding = use_2_path[HAND_RIGHT]},

			{.action = use_3_action_boolean, .binding = use_3_path[HAND_LEFT]},
			{.action = use_3_action_boolean, .binding = use_3_path[HAND_RIGHT]},

			{.action = vibration_action, .binding = vibrate_path[HAND_LEFT]},
			{.action = vibration_action, .binding = vibrate_path[HAND_RIGHT]},

			{.action = move_x_z_action_vec_2, .binding = movement_x_z_path},
			{.action = move_rotation_y_action_vec_2, .binding = movement_rotation_y_path},
			{.action = teleport_action_boolean, .binding = teleport_path},

			{.action = menu_action_boolean, .binding = menu_path},

		};

		const XrInteractionProfileSuggestedBinding suggested_bindings = {
			.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
			.next = NULL,
			.interactionProfile = interaction_profile_path,
			.countSuggestedBindings = sizeof(bindings) / sizeof(bindings[0]),
			.suggestedBindings = bindings};

		xrSuggestInteractionProfileBindings(self.instance, &suggested_bindings);
		if (!xr_result(self.instance, result, "failed to suggest bindings"))
			return;
	}

	XrSpace pose_action_spaces[HAND_COUNT];
	{
		XrActionSpaceCreateInfo action_space_info;
		action_space_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
		action_space_info.next = NULL;
		action_space_info.action = pose_action;
		action_space_info.poseInActionSpace = identity_pose;
		action_space_info.subactionPath = self.hand_paths[HAND_LEFT];

		result = xrCreateActionSpace(self.session, &action_space_info, &pose_action_spaces[HAND_LEFT]);
		if (!xr_result(self.instance, result, "failed to create left hand pose space"))
			return;
	}
	{
		XrActionSpaceCreateInfo action_space_info;
		action_space_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
		action_space_info.next = NULL;
		action_space_info.action = pose_action;
		action_space_info.poseInActionSpace = identity_pose;
		action_space_info.subactionPath = self.hand_paths[HAND_RIGHT];

		result =
			xrCreateActionSpace(self.session, &action_space_info, &pose_action_spaces[HAND_RIGHT]);
		if (!xr_result(self.instance, result, "failed to create left hand pose space"))
			return;
	}

	XrSessionActionSetsAttachInfo actionset_attach_info = {
		.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
		.next = NULL,
		.countActionSets = 1,
		.actionSets = &main_actionset};
	result = xrAttachSessionActionSets(self.session, &actionset_attach_info);
	if (!xr_result(self.instance, result, "failed to attach action set"))
		return;

	int loop_count = 0;
	while (continue_vr)
	{
		loop_count++;

		SDL_Event sdl_event;
		while (SDL_PollEvent(&sdl_event))
		{
			if (sdl_event.type == SDL_QUIT)
			{
				stop_vr();
				break;
			}
			event_manager(sdl_event);
		}
		if (!continue_vr)
		{
			break;
		}

		before_render();

		bool session_stopping = false;

		XrEventDataBuffer runtime_event = {.type = XR_TYPE_EVENT_DATA_BUFFER, .next = NULL};
		XrResult poll_result = xrPollEvent(self.instance, &runtime_event);
		while (poll_result == XR_SUCCESS)
		{
			switch (runtime_event.type)
			{
			case XR_TYPE_EVENT_DATA_EVENTS_LOST:
			{
				XrEventDataEventsLost *event = (XrEventDataEventsLost *)&runtime_event;
				printf("EVENT: %d events data lost!\n", event->lostEventCount);
				// do we care if the runtime loses events?
				break;
			}
			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
			{
				XrEventDataInstanceLossPending *event = (XrEventDataInstanceLossPending *)&runtime_event;
				printf("EVENT: instance loss pending at %lu! Destroying instance.\n", event->lossTime);
				session_stopping = true;
				break;
			}
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				XrEventDataSessionStateChanged *event = (XrEventDataSessionStateChanged *)&runtime_event;
				printf("EVENT: session state changed from %d to %d\n", self.state, event->state);

				self.state = event->state;

				if (event->state >= XR_SESSION_STATE_STOPPING)
				{
					printf("Session is stopping...\n");
					// still handle rest of the events instead of immediately quitting
					session_stopping = true;
				}
				break;
			}
			case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
			{
				printf("EVENT: reference space change pending!\n");
				XrEventDataReferenceSpaceChangePending *event =
					(XrEventDataReferenceSpaceChangePending *)&runtime_event;
				(void)event;
				// TODO: do something
				break;
			}
			case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
			{
				printf("EVENT: interaction profile changed!\n");
				XrEventDataInteractionProfileChanged *event =
					(XrEventDataInteractionProfileChanged *)&runtime_event;
				(void)event;

				XrInteractionProfileState state = {.type = XR_TYPE_INTERACTION_PROFILE_STATE};

				for (int i = 0; i < 2; i++)
				{
					XrResult res = xrGetCurrentInteractionProfile(self.session, self.hand_paths[i], &state);
					if (!xr_result(self.instance, res, "Failed to get interaction profile for %d", i))
						continue;

					XrPath prof = state.interactionProfile;

					uint32_t strl;
					char profile_str[XR_MAX_PATH_LENGTH];
					res = xrPathToString(self.instance, prof, XR_MAX_PATH_LENGTH, &strl, profile_str);
					if (!xr_result(self.instance, res, "Failed to get interaction profile path str for %s",
								   h_p_str(i)))
						continue;

					printf("Event: Interaction profile changed for %s: %s\n", h_p_str(i), profile_str);
				}
				// TODO: do something
				break;
			}

			case XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR:
			{
				printf("EVENT: visibility mask changed!!\n");
				XrEventDataVisibilityMaskChangedKHR *event =
					(XrEventDataVisibilityMaskChangedKHR *)&runtime_event;
				(void)event;
				// this event is from an extension
				break;
			}
			case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT:
			{
				printf("EVENT: perf settings!\n");
				XrEventDataPerfSettingsEXT *event = (XrEventDataPerfSettingsEXT *)&runtime_event;
				(void)event;
				// this event is from an extension
				break;
			}
			default:
				printf("Unhandled event type %d\n", runtime_event.type);
			}

			runtime_event.type = XR_TYPE_EVENT_DATA_BUFFER;
			poll_result = xrPollEvent(self.instance, &runtime_event);
		}
		if (poll_result == XR_EVENT_UNAVAILABLE)
		{
			// processed all events in the queue
		}
		else
		{
			printf("Failed to poll events!\n");
			break;
		}

		if (session_stopping)
		{
			printf("Quitting main render loop\n");
			return;
		}

		// --- Wait for our turn to do head-pose dependent computation and render a frame
		XrFrameState frameState = {.type = XR_TYPE_FRAME_STATE, .next = NULL};
		XrFrameWaitInfo frameWaitInfo = {.type = XR_TYPE_FRAME_WAIT_INFO, .next = NULL};
		result = xrWaitFrame(self.session, &frameWaitInfo, &frameState);
		if (!xr_result(self.instance, result, "xrWaitFrame() was not successful, exiting..."))
			break;

		if (self.hand_tracking.system_supported)
		{

			for (int i = 0; i < HAND_COUNT; i++)
			{

				joint_locations[i] = XrHandJointLocationsEXT{
					.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
					.jointCount = XR_HAND_JOINT_COUNT_EXT,
					.jointLocations = joints[i],
				};

				if (self.hand_tracking.trackers[i] == NULL)
					continue;

				XrHandJointsLocateInfoEXT locateInfo = {.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
														.next = NULL,
														.baseSpace = self.play_space,
														.time = frameState.predictedDisplayTime};

				result = self.hand_tracking.pfnLocateHandJointsEXT(self.hand_tracking.trackers[i],
																   &locateInfo, &joint_locations[i]);
				if (!xr_result(self.instance, result, "failed to locate hand %d joints!", i))
					break;

				/*
				if (joint_locations[i].isActive) {
				  printf("located hand %d joints", i);
				  for (uint32_t j = 0; j < joint_locations[i].jointCount; j++) {
					printf("%f ", joint_locations[i].jointLocations[j].radius);
				  }
				  printf("\n");
				} else {
				  printf("hand %d joints inactive\n", i);
				}
				*/
			}
		}

		XrViewLocateInfo view_locate_info = {.type = XR_TYPE_VIEW_LOCATE_INFO,
											 .next = NULL,
											 .viewConfigurationType =
												 XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
											 .displayTime = frameState.predictedDisplayTime,
											 .space = self.play_space};

		uint32_t view_count = self.viewconfig_views.size();
		std::vector<XrView> views(view_count);
		for (uint32_t i = 0; i < view_count; i++)
		{
			views[i].type = XR_TYPE_VIEW;
			views[i].next = NULL;
		};

		XrViewState view_state = {.type = XR_TYPE_VIEW_STATE, .next = NULL};
		result = xrLocateViews(self.session, &view_locate_info, &view_state, view_count,
							   &view_count, views.data());
		if (!xr_result(self.instance, result, "Could not locate views"))
			break;
		const XrActiveActionSet active_actionsets[] = {
			{.actionSet = main_actionset, .subactionPath = XR_NULL_PATH}};

		XrActionsSyncInfo actions_sync_info = {
			.type = XR_TYPE_ACTIONS_SYNC_INFO,
			.countActiveActionSets = sizeof(active_actionsets) / sizeof(active_actionsets[0]),
			.activeActionSets = active_actionsets,
		};
		result = xrSyncActions(self.session, &actions_sync_info);
		xr_result(self.instance, result, "failed to sync actions!");

		// query each value / location with a subaction path != XR_NULL_PATH
		// resulting in individual values per hand/.
		XrActionStateFloat grab_value[HAND_COUNT];
		XrActionStateFloat use_value[HAND_COUNT];
		XrActionStateBoolean use_2_value[HAND_COUNT];
		XrActionStateBoolean use_3_value[HAND_COUNT];

		XrActionStateBoolean menu_value;
		XrActionStateBoolean teleport_value;
		XrActionStateFloat rotate_value;
		XrActionStateVector2f movement_x_z_value;
		XrActionStateVector2f movement_rotation_y_value;

		XrSpaceLocation hand_locations[HAND_COUNT];
		bool hand_locations_valid[HAND_COUNT];

		{
			XrActionStateBoolean menu_state;
			XrActionStateGetInfo get_info = {
				.type = XR_TYPE_ACTION_STATE_GET_INFO,
				.next = NULL,
				.action = menu_action_boolean,
				.subactionPath = self.hand_paths[HAND_LEFT] // Ou o path apropriado se for para outra mão
			};
			result = xrGetActionStateBoolean(self.session, &get_info, &menu_state);
			if (menu_state.isActive){
				actions_map[vr_menu] = menu_state.currentState;
			}
		}

		{
			XrActionStateGetInfo get_info = {
				.type = XR_TYPE_ACTION_STATE_GET_INFO,
				.next = NULL,
				.action = teleport_action_boolean,
				.subactionPath = self.hand_paths[HAND_LEFT] // Ou o path apropriado se for para outra mão
			};
			result = xrGetActionStateBoolean(self.session, &get_info, &teleport_value);
			xr_result(self.instance, result, "failed to get teleport action state");

			if (teleport_value.isActive)
			{
				bool is_teleport_pressed = teleport_value.currentState;
				if (is_teleport_pressed)
				{
					actions_map[vr_teleport] = 1.0;
				}
				else
				{
					actions_map[vr_teleport] = 0.0;
				}
			}
		}

		{
			XrActionStateGetInfo get_info = {
				.type = XR_TYPE_ACTION_STATE_GET_INFO,
				.next = NULL,
				.action = move_x_z_action_vec_2,
				.subactionPath = self.hand_paths[HAND_LEFT] // Ou o path apropriado se for para outra mão
			};
			result = xrGetActionStateVector2f(self.session, &get_info, &movement_x_z_value);
			xr_result(self.instance, result, "failed to get movement x y action state");

			if (movement_x_z_value.isActive)
			{
				XrVector2f movement = movement_x_z_value.currentState;
				actions_map[vr_move_x] = movement.y;
				actions_map[vr_move_z] = movement.x;
			}
			else
			{
				// A ação de movimento não está ativa
				actions_map[vr_move_x] = 0.0;
				actions_map[vr_move_z] = 0.0;
			}
		}

		{
			XrActionStateGetInfo get_info = {
				.type = XR_TYPE_ACTION_STATE_GET_INFO,
				.next = NULL,
				.action = move_rotation_y_action_vec_2,
				.subactionPath = self.hand_paths[HAND_RIGHT] // Ou o path apropriado se for para outra mão
			};
			result = xrGetActionStateVector2f(self.session, &get_info, &movement_rotation_y_value);
			xr_result(self.instance, result, "failed to get rotation y action state");

			if (movement_rotation_y_value.isActive)
			{
				XrVector2f movement = movement_rotation_y_value.currentState;
				actions_map[vr_rotate] = movement.x;
				actions_map[vr_move_y] = movement.y;
			}
			else
			{
				// A ação de movimento não está ativa
				actions_map[vr_rotate] = 0.0;
				actions_map[vr_move_y] = 0.0;
			}
		}

		for (int i = 0; i < HAND_COUNT; i++)
		{
			// hands
			XrActionStatePose pose_state = {.type = XR_TYPE_ACTION_STATE_POSE, .next = NULL};
			{
				XrActionStateGetInfo get_info = {.type = XR_TYPE_ACTION_STATE_GET_INFO,
												 .next = NULL,
												 .action = pose_action,
												 .subactionPath = self.hand_paths[i]};
				result = xrGetActionStatePose(self.session, &get_info, &pose_state);
				xr_result(self.instance, result, "failed to get pose value!");
			}

			hand_locations[i].type = XR_TYPE_SPACE_LOCATION;
			hand_locations[i].next = NULL;

			result = xrLocateSpace(pose_action_spaces[i], self.play_space, frameState.predictedDisplayTime, &hand_locations[i]);
			xr_result(self.instance, result, "failed to locate space %d!", i);
			hand_locations_valid[i] = (hand_locations[i].locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;

			if (i == HAND_LEFT)
			{

				traker_pose_map[vr_left_hand].position = glm::vec3(hand_locations[i].pose.position.x, hand_locations[i].pose.position.y, hand_locations[i].pose.position.z);
				traker_pose_map[vr_left_hand].quaternion = glm::quat(hand_locations[i].pose.orientation.w, hand_locations[i].pose.orientation.x, hand_locations[i].pose.orientation.y, hand_locations[i].pose.orientation.z);
			}
			else if (i == HAND_RIGHT)
			{
				traker_pose_map[vr_right_hand].position = glm::vec3(hand_locations[i].pose.position.x, hand_locations[i].pose.position.y, hand_locations[i].pose.position.z);
				traker_pose_map[vr_right_hand].quaternion = glm::quat(hand_locations[i].pose.orientation.w, hand_locations[i].pose.orientation.x, hand_locations[i].pose.orientation.y, hand_locations[i].pose.orientation.z);
			}
		}

		// --- Begin frame
		XrFrameBeginInfo frame_begin_info = {.type = XR_TYPE_FRAME_BEGIN_INFO, .next = NULL};

		result = xrBeginFrame(self.session, &frame_begin_info);
		if (!xr_result(self.instance, result, "failed to begin frame!"))
			break;

		for (uint32_t i = 0; i < view_count; i++)
		{
			glm::mat4 projection_matrix(1.0f);
			my_math::XrMatrix4x4f_CreateProjectionFov(projection_matrix, views[i].fov,
													  self.near_z, self.far_z);

			glm::mat4 view_matrix(1.0f);
			const glm::vec3 position(views[i].pose.position.x, views[i].pose.position.y, views[i].pose.position.z);
			const glm::quat orientation(views[i].pose.orientation.w, views[i].pose.orientation.x, views[i].pose.orientation.y, views[i].pose.orientation.z);

			traker_pose_map[vr_headset] = {position, orientation};

			my_math::XrMatrix4x4f_CreateViewMatrix(view_matrix, position, orientation);

			XrSwapchainImageAcquireInfo acquire_info = {.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
														.next = NULL};
			uint32_t acquired_index;
			result = xrAcquireSwapchainImage(self.swapchains[i], &acquire_info, &acquired_index);
			if (!xr_result(self.instance, result, "failed to acquire swapchain image!"))
				break;

			XrSwapchainImageWaitInfo wait_info = {
				.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next = NULL, .timeout = 1000};
			result = xrWaitSwapchainImage(self.swapchains[i], &wait_info);
			if (!xr_result(self.instance, result, "failed to wait for swapchain image!"))
				break;

			uint32_t depth_acquired_index = UINT32_MAX;
			if (self.depth_swapchain_format != -1)
			{
				XrSwapchainImageAcquireInfo depth_acquire_info = {
					.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, .next = NULL};
				result = xrAcquireSwapchainImage(self.depth_swapchains[i], &depth_acquire_info,
												 &depth_acquired_index);
				if (!xr_result(self.instance, result, "failed to acquire swapchain image!"))
					break;

				XrSwapchainImageWaitInfo depth_wait_info = {
					.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next = NULL, .timeout = 1000};
				result = xrWaitSwapchainImage(self.depth_swapchains[i], &depth_wait_info);
				if (!xr_result(self.instance, result, "failed to wait for swapchain image!"))
					break;
			}

			self.projection_views[i].pose = views[i].pose;
			self.projection_views[i].fov = views[i].fov;

			GLuint depth_image = self.depth_swapchain_format != -1
									 ? self.depth_images[i][depth_acquired_index].image
									 : UINT32_MAX;

			glBindFramebuffer(GL_FRAMEBUFFER, self.framebuffers[i][acquired_index]);

			glm::ivec2 resolution(self.viewconfig_views[i].recommendedImageRectWidth, self.viewconfig_views[i].recommendedImageRectHeight);

			glViewport(0, 0, resolution.x, resolution.y);
			glScissor(0, 0, resolution.x, resolution.y);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self.images[i][acquired_index].image, 0);
			if (depth_image != UINT32_MAX)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_image, 0);
			}
			else
			{
				// TODO: need a depth attachment for depth test when rendering to fbo
			}
			/*
		   render_frame(self.viewconfig_views[i].recommendedImageRectWidth,
						self.viewconfig_views[i].recommendedImageRectHeight, projection_matrix,
						view_matrix, hand_locations, hand_locations_valid, joint_locations,
						self.framebuffers[i][acquired_index], depth_image,
						self.images[i][acquired_index], i, frameState.predictedDisplayTime);
		   */
			update_render(self.framebuffers[i][acquired_index], resolution, view_matrix, projection_matrix);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			if (i == 1)
			{
				update_render(self.framebuffers[i][acquired_index], resolution, view_matrix, projection_matrix);
			}

			glFinish();
			XrSwapchainImageReleaseInfo release_info = {.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
														.next = NULL};
			result = xrReleaseSwapchainImage(self.swapchains[i], &release_info);
			if (!xr_result(self.instance, result, "failed to release swapchain image!"))
				break;

			if (self.depth_swapchain_format != -1)
			{
				XrSwapchainImageReleaseInfo depth_release_info = {
					.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, .next = NULL};
				result = xrReleaseSwapchainImage(self.depth_swapchains[i], &depth_release_info);
				if (!xr_result(self.instance, result, "failed to release swapchain image!"))
					break;
			}
		}

		XrSwapchainImageAcquireInfo acquire_info = {.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
													.next = NULL};
		uint32_t acquired_index;
		result = xrAcquireSwapchainImage(self.quad_swapchain, &acquire_info, &acquired_index);
		if (!xr_result(self.instance, result, "failed to acquire swapchain image!"))
			break;

		XrSwapchainImageWaitInfo wait_info = {
			.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next = NULL, .timeout = 1000};
		result = xrWaitSwapchainImage(self.quad_swapchain, &wait_info);
		if (!xr_result(self.instance, result, "failed to wait for swapchain image!"))
			break;

		/**/
		render_quad(self.quad_pixel_width, self.quad_pixel_height, self.swapchain_format,
					self.quad_images[acquired_index], frameState.predictedDisplayTime);

		XrSwapchainImageReleaseInfo release_info = {.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
													.next = NULL};
		result = xrReleaseSwapchainImage(self.quad_swapchain, &release_info);
		if (!xr_result(self.instance, result, "failed to release swapchain image!"))
			break;

		if (self.cylinder.supported)
		{
			acquire_info = {.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
							.next = NULL};
			uint32_t acquired_index;
			result = xrAcquireSwapchainImage(self.cylinder.swapchain, &acquire_info, &acquired_index);
			if (!xr_result(self.instance, result, "failed to acquire swapchain image!"))
				break;

			XrSwapchainImageWaitInfo wait_info = {
				.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next = NULL, .timeout = 1000};
			result = xrWaitSwapchainImage(self.cylinder.swapchain, &wait_info);
			if (!xr_result(self.instance, result, "failed to wait for swapchain image!"))
				break;

			/**/
			render_quad(self.cylinder.swapchain_width, self.cylinder.swapchain_height,
						self.cylinder.format, self.cylinder.images[acquired_index],
						frameState.predictedDisplayTime);

			XrSwapchainImageReleaseInfo release_info = {.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
														.next = NULL};
			result = xrReleaseSwapchainImage(self.cylinder.swapchain, &release_info);
			if (!xr_result(self.instance, result, "failed to release swapchain image!"))
				break;
		}

		// projectionLayers struct reused for every frame
		XrCompositionLayerProjection projection_layer = {
			.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
			.next = NULL,
			.layerFlags = 0,
			.space = self.play_space,
			.viewCount = view_count,
			.views = self.projection_views.data(),
		};

		float aspect = (float)self.quad_pixel_width / (float)self.quad_pixel_height;
		float quad_width = 1.f;
		XrCompositionLayerQuad quad_layer;

		quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD,
		quad_layer.next = NULL,
		quad_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
		quad_layer.space = self.play_space,
		quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH,
		quad_layer.pose = {.orientation = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 1.f},
						   .position = {.x = 1.5f, .y = .7f, .z = -1.5f}},
		quad_layer.size = {.width = quad_width, .height = quad_width / aspect},
		quad_layer.subImage = {
			.swapchain = self.quad_swapchain,
			.imageRect = {
				.offset = {.x = 0, .y = 0},
				.extent = {.width = (int32_t)self.quad_pixel_width,
						   .height = (int32_t)self.quad_pixel_height},
			}};

		float cylinder_aspect =
			(float)self.cylinder.swapchain_width / (float)self.cylinder.swapchain_height;

		float threesixty = M_PI * 2 - 0.0001; /* TODO: spec issue range [0, 2π)*/

		float angleratio = 1 + (loop_count % 1000) / 50.;
		XrCompositionLayerCylinderKHR cylinder_layer = {
			.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR,
			.next = NULL,
			.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
			.space = self.play_space,
			.eyeVisibility = XR_EYE_VISIBILITY_BOTH,
			.subImage = {.swapchain = self.cylinder.swapchain,
						 .imageRect = {.offset = {.x = 0, .y = 0},
									   .extent = {.width = (int32_t)self.cylinder.swapchain_width,
												  .height = (int32_t)self.cylinder.swapchain_height}}},
			.pose = {.orientation = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 1.f},
					 .position = {.x = 1.5f, .y = 0.f, .z = -1.5f}},
			.radius = 0.5,
			.centralAngle = threesixty / 3,
			.aspectRatio = cylinder_aspect};

		int submitted_layer_count = 1;
		const XrCompositionLayerBaseHeader *submittedLayers[3] = {
			(const XrCompositionLayerBaseHeader *const)&projection_layer};

		if (false)
		{
			submittedLayers[submitted_layer_count++] =
				(const XrCompositionLayerBaseHeader *const)&quad_layer;
		}
		if (self.cylinder.supported)
		{
			submittedLayers[submitted_layer_count++] =
				(const XrCompositionLayerBaseHeader *const)&cylinder_layer;
		};

		XrFrameEndInfo frameEndInfo;
		frameEndInfo.type = XR_TYPE_FRAME_END_INFO;
		frameEndInfo.displayTime = frameState.predictedDisplayTime;
		frameEndInfo.layerCount = submitted_layer_count;
		frameEndInfo.layers = submittedLayers;
		frameEndInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		frameEndInfo.next = NULL;
		result = xrEndFrame(self.session, &frameEndInfo);
		if (!xr_result(self.instance, result, "failed to end frame!"))
			break;

		after_render();
	}
}

vr_pose get_vr_traker_pose(vr_traker_type traker)
{
	return traker_pose_map[traker];
}

float get_vr_action(vr_action action)
{
	return actions_map[action];
}

void vibrate_traker(vr_traker_type traker, float power)
{
}

void end_vr(void(clean_render)(void))
{
	XrResult result;

	xrEndSession(self.session);

	if (self.hand_tracking.system_supported)
	{
		PFN_xrDestroyHandTrackerEXT pfnDestroyHandTrackerEXT = NULL;
		result = xrGetInstanceProcAddr(self.instance, "xrDestroyHandTrackerEXT",
									   (PFN_xrVoidFunction *)&pfnDestroyHandTrackerEXT);

		xr_result(self.instance, result, "Failed to get xrDestroyHandTrackerEXT function!");

		if (self.hand_tracking.trackers[HAND_LEFT])
		{
			result = pfnDestroyHandTrackerEXT(self.hand_tracking.trackers[HAND_LEFT]);
			if (xr_result(self.instance, result, "Failed to destroy left hand tracker"))
			{
				printf("Destroyed hand tracker for left hand\n");
			}
		}
		if (self.hand_tracking.trackers[HAND_RIGHT])
		{
			result = pfnDestroyHandTrackerEXT(self.hand_tracking.trackers[HAND_RIGHT]);
			if (xr_result(self.instance, result, "Failed to destroy right hand tracker"))
			{
				printf("Destroyed hand tracker for right hand\n");
			}
		}
	}

	xrDestroySession(self.session);

	for (auto &frame_buffer : self.framebuffers)
	{
		glDeleteFramebuffers(frame_buffer.size(), frame_buffer.data());
	}
	xrDestroyInstance(self.instance);

	clean_render();

	SDL_DestroyWindow(desktop_window);

	SDL_Quit();
};