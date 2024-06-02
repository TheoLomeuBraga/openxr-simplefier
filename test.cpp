
#include "vr_manager.h"

void event_manager(SDL_Event event){

}

void start_vr_render(){
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
}

void before_vr_render(){}

void update_vr_render(glm::ivec2 resolution,glm::mat4 view,glm::mat4 projection){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void after_vr_render(){}

void end_vr_render(){
    
}


int main() {
    std::cout << "Hello World!\n";
    set_sdl_event_manager(event_manager);
    start_vr(start_vr_render);
    update_vr(before_vr_render,update_vr_render,after_vr_render);
    end_vr(end_vr_render);
    return 0;
}