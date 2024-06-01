#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/quaternion.hpp"
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

void update_vr(void(before_render)(void),void(update_render)(glm::ivec2,glm::mat4,glm::mat4),void(after_render)(void));

enum vr_traker_type{
    vr_headset = 0,
    vr_left_hand = 1,
    vr_right_hand = 2,
};

struct vr_pose_struct{
    glm::vec3 position;
    glm::quat quaternion;
};
typedef struct vr_pose_struct vr_pose;

vr_pose get_vr_traker_pose(vr_traker_type traker);

enum vr_action{
    vr_grab = 0,
    vr_use = 1,
    vr_use_2 = 2,
    vr_menu = 3,
    vr_menu_2 = 4,
    vr_move_x = 5,
    vr_move_y = 6,
    vr_move_z = 7,
    vr_rotate = 8,
};

float get_vr_action(vr_action action);

void vibrate_traker(vr_traker_type traker,float time);

XrHandJointLocationsEXT *get_vr_joints_infos();

void stop_vr();

void end_vr(void(clean_render)(void));

