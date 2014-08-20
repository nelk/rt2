#version 330 core

layout(location = 0) in vec3 vertexPositionModelspace;

void main(){
  gl_Position = vec4(vertexPositionModelspace, 1);
}

