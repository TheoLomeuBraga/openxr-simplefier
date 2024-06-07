#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "vr_manager.h"

void event_manager(SDL_Event event)
{
}

namespace render_shapes
{
    const GLfloat vertices[] = {
        // Posições        // Normais
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,

        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,

        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f};
    GLuint cubeVAO, cubeVBO;
    GLuint shaderProgram;

    void setup_cube_render()
    {
        // Compilar e linkar os shaders
        const char *vertexShaderSource = R"(
        #version 300 es

layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
    )";
        const char *fragmentShaderSource = R"(
        #version 300 es
precision mediump float;
out vec4 FragColor;
uniform vec4 color;

void main()
{
    FragColor = color;
}
    )";

        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);
        // Checar erros de compilação...

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);
        // Checar erros de compilação...

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        // Checar erros de linkagem...

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Configurar VAO e VBO para o cubo
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);

        glBindVertexArray(cubeVAO);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Posições
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void *)0);
        glEnableVertexAttribArray(0);

        // Normais
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void *)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void render_cube(glm::mat4 view, glm::mat4 projection, glm::vec3 position, glm::vec4 color = glm::vec4(1.0, 1.0, 1.0, 1.0), glm::vec3 scale = glm::vec3(1.0, 1.0, 1.0), glm::quat orientation = glm::quat(1.0, 0.0, 0.0, 0.0))
    {
        glUseProgram(shaderProgram);

        // Configurar as matrizes de modelo, view e projeção
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = model * glm::toMat4(orientation);
        model = glm::scale(model, scale);

        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
        GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);
        glUniform4f(glGetUniformLocation(shaderProgram, "color"), color.x, color.y, color.z, color.w);

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }

};

GLuint depthBuffer;
void start_vr_render()
{
    //deph
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    

    glClearColor(0.0f, 0.0f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_shapes::setup_cube_render();
}

void before_vr_render()
{
    if(get_vr_action(vr_menu) == 1.0){
        std::cout << "i will stop now\n";
        stop_vr();
    }

    if(get_vr_action(vr_teleport)){
        std::cout << "teleport\n";
        reorientate(glm::vec3(0,2,0),glm::quat(1.0,0.0,0.0,0.0));
    }else{
        reorientate(glm::vec3(0,0,0),glm::quat(1.0,0.0,0.0,0.0));
    }

    std::cout << "vr_rotate: " << get_vr_action(vr_rotate) << std::endl;


}

unsigned char c = 0;
void update_vr_render(unsigned int frame_buffer, glm::ivec2 resolution, glm::mat4 view, glm::mat4 projection)
{

    //deph

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_shapes::render_cube(view, projection, glm::vec3(0.0, 0.0, 0.0), glm::vec4(1.0, 1.0, 1.0, 1.0), glm::vec3(2.0, 0.05, 2.0));

    vr_pose pose = get_vr_traker_pose(vr_left_hand);
    render_shapes::render_cube(view, projection, pose.position, glm::vec4(1.0, 0.0, 0.0, 1.0), glm::vec3(0.02, 0.02, 0.1), pose.quaternion);
    pose = get_vr_traker_pose(vr_right_hand);
    render_shapes::render_cube(view, projection, pose.position, glm::vec4(1.0, 0.0, 0.0, 1.0), glm::vec3(0.02, 0.02, 0.1), pose.quaternion);

    int i = 0;
    std::vector<vr_pose> joint_list = get_vr_joints_infos(vr_left_hand);
    for (vr_pose p : joint_list)
    {
        i++;
        float color_power =  (float)joint_list.size() / (float)i;
        render_shapes::render_cube(view, projection, p.position, glm::vec4(0.5 / color_power, 1.0 / color_power, 0.5 / color_power, 1.0), glm::vec3(0.01, 0.01, 0.01), p.quaternion);
    }

    i = 0;
    joint_list = get_vr_joints_infos(vr_right_hand);
    for (vr_pose p : joint_list)
    {
        i++;
        float color_power =  (float)joint_list.size() / (float)i;
        render_shapes::render_cube(view, projection, p.position, glm::vec4(0.0, 1.0 / color_power, 0.0, 1.0), glm::vec3(0.01, 0.01, 0.01), p.quaternion);
    }
    
    ////deph
}

void after_vr_render() {}

void end_vr_render()
{
}

int main()
{
    std::cout << "Hello World!\n";
    set_sdl_event_manager(event_manager);
    start_vr("test vr", start_vr_render);
    update_vr(before_vr_render, update_vr_render, after_vr_render);
    end_vr(end_vr_render);

    return 0;
}