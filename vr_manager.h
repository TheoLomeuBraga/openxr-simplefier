#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include "glm/mat4x4.hpp"
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

void update_vr(void(before_render)(void),void(update_render)(glm::mat4,glm::mat4),void(after_render)(void));

XrHandJointLocationsEXT *get_joints_infos();

void stop_vr();

void end_vr(void(clean_render)(void));

