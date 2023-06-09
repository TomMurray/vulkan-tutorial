#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec3 in_colour;

layout(location = 0) out vec3 frag_colour;

void main() {
  gl_Position = vec4(in_pos, 0.0, 1.0);
  frag_colour = in_colour;
}