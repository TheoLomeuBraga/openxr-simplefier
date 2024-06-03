#version 300 es

layout(location = 0) in vec4 vPosition;
uniform mat4 uMVPMatrix; // Matriz de transformação

void main() {
    gl_Position = uMVPMatrix * vPosition; // Aplique a matriz de transformação
}
