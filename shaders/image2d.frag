#version 460

layout(location = 0) in vec2 uv;
layout(location = 1) flat in uint image;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D images[7];

void main() {
	oColor = texture(images[image], uv);
}
