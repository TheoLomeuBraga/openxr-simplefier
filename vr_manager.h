#pragma once
#include "glm/mat4x4.hpp"
#include <map>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <GL/glew.h>

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

void start_vr(void(start_render)(void));

void update_vr(void(update_render)(glm::mat4,glm::mat4));

XrHandJointLocationsEXT *get_joints_infos();

void end_vr(void(end_render)(void));

