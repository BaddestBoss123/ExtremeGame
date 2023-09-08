#version 460

layout(location = 0) in vec2 position;

layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint image;

layout(push_constant) uniform PushConstants {
	mat4 transform;
} pushConstants;

void main() {
	gl_Position = pushConstants.transform * vec4(position, 0.0, 1.0);

	uint idx = gl_VertexIndex % 4;
	uv = vec2((idx & 1), ((idx >> 1) & 1));
	image = gl_InstanceIndex;
}
