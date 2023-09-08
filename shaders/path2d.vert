#version 460

layout(location = 0) in vec2 position;

layout(push_constant) uniform PushConstants {
	mat4 transform;
} pushConstants;

void main() {
	gl_Position = pushConstants.transform * vec4(position, 0.0, 1.0);
}
