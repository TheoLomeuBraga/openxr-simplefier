#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "vr_manager.h"

void event_manager(SDL_Event event){

}

GLuint VAO,EBO, VBO, shaderProgram;

std::string readShaderFile(const char* filePath) {
    std::ifstream shaderFile(filePath);
    std::stringstream shaderStream;
    shaderStream << shaderFile.rdbuf();
    return shaderStream.str();
}

// Função para compilar um shader
GLuint compileShader(const char* shaderSource, GLenum shaderType) {
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    // Verifica erros de compilação
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Erro na compilação do shader: " << infoLog << std::endl;
    }

    return shader;
}

// Função para criar o programa shader
GLuint createShaderProgram(const char* vertexPath, const char* fragmentPath) {
    // Lê o código do shader dos arquivos
    std::string vertexCode = readShaderFile(vertexPath);
    std::string fragmentCode = readShaderFile(fragmentPath);

    const char* vertexShaderSource = vertexCode.c_str();
    const char* fragmentShaderSource = fragmentCode.c_str();

    // Compila os shaders
    GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
    GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);

    // Linka os shaders no programa
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Verifica erros de linkagem
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Erro na linkagem do programa shader: " << infoLog << std::endl;
    }

    // Exclui os shaders pois já estão linkados no programa
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}



void start_vr_render(){

    glClearColor(0.0f, 0.0f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
}

void before_vr_render(){}




unsigned char c = 0;
void update_vr_render(unsigned int frame_buffer,glm::ivec2 resolution,glm::mat4 view,glm::mat4 projection){


    
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