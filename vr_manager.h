#pragma once
#include "glm/mat4x4.hpp"
#include <map>

void start_vr(void(start_render)(void));

bool update_vr(void(update_render)(glm::mat4,glm::mat4));

void end_vr(void(end_render)(void));