#version 460

layout(location = 0) in mat4 model;
layout(location = 4) in vec3 position;
layout(location = 5) in vec3 in_normal;
layout(location = 6) in vec4 in_tangent;
layout(location = 7) in vec2 in_texCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 normal;
layout(location = 2) out vec4 tangent;
layout(location = 3) out vec2 texCoord;

layout(push_constant) uniform PushConstants {
	mat4 viewProjection;
	vec4 clippingPlane;
} pushConstants;

void main() {
	vec4 pos = model * vec4(position, 1.0);
	fragPos = pos.xyz;

	gl_Position = pushConstants.viewProjection * pos;
	gl_ClipDistance[0] = dot(pushConstants.clippingPlane, gl_Position);

	normal = normalize(in_normal * 2.0 - 1.0);
	tangent = normalize(in_tangent * 2.0 - 1.0);

	texCoord = in_texCoord;
}