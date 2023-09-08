#version 460

layout(location = 0) in vec4 vertex;

layout(location = 0) out vec2 uv;

layout(push_constant) uniform PushConstants {
	mat4 transform;
} pushConstants;

void main() {
	gl_Position = pushConstants.transform * vec4(vertex.xy, 0.0, 1.0);

	uv = vertex.zw;
}
