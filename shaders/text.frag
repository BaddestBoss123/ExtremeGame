#version 460

layout(location = 0) in vec2 texCoord;

layout(binding = 0) uniform sampler2D glyphTexture;

layout(location = 0) out vec4 oColor;

void main() {
	oColor = vec4(1.0, 1.0, 1.0, texture(glyphTexture, texCoord).r);
}