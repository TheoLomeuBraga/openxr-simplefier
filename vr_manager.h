#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <chrono>

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include "glm/mat4x4.hpp"
#include <glm/gtx/quaternion.hpp>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

void set_sdl_event_manager(void(sdl_event_manager)(SDL_Event));

void start_vr(std::string name,void(start_render)(void));

void update_vr(void(before_render)(void),void(update_render)(unsigned int,glm::ivec2,glm::mat4,glm::mat4),void(after_render)(void));

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
    vr_grab_l = 0,
    vr_grab_r = 1,
    vr_use_l = 2,
    vr_use_r = 3,
    vr_use_2_l = 4,
    vr_use_2_r = 5,
    vr_use_3_l = 6,
    vr_use_3_r = 7,
    vr_menu = 8,
    vr_move_x = 9,
    vr_move_y = 10,
    vr_move_z = 11,
    vr_rotate = 12,
    vr_teleport = 13,
};

float get_vr_action(vr_action action);

void vibrate_traker(vr_traker_type traker,float power);

std::vector<vr_pose> get_vr_joints_infos(vr_traker_type hand);

void reorientate(glm::vec3 new_position, glm::quat new_rotation);



void stop_vr();

void end_vr(void(clean_render)(void));

