#version 450

layout(location = 0) out vec4 outColor;

void main() {
  // Colour any geometry flat red
  outColor = vec4(1.0, 0.0, 0.0, 1.0);
}