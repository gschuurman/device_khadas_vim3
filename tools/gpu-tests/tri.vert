#version 450
layout(location = 0) out vec3 col;
const vec2 p[3] = vec2[](vec2(0.0,-0.8), vec2(0.8,0.8), vec2(-0.8,0.8));
void main() { gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0); col = vec3(0.0, 1.0, 0.0); }
