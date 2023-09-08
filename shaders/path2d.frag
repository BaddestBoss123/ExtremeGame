#version 460

layout(location = 0) out vec4 oColor;

layout(push_constant) uniform PushConstants {
	layout(offset = 96) vec4 color;
} pushConstants;

void main() {
	oColor = pushConstants.color;
}