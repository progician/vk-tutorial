#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_colour;
layout(location = 0) in vec3 fragment_colour;

void main() {
    out_colour = vec4(fragment_colour, 1.0);
}