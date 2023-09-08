#version 460

#extension GL_EXT_shader_16bit_storage : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 texCoord;

layout(location = 0) out vec4 oColor;

layout(binding = 0) uniform sampler2D textures[7];

layout(push_constant) uniform PushConstants {
	layout(offset = 80) vec3 cameraPosition;
	vec4 color;
	mat3 normalMatrix;
	uint16_t colorIndex;
	uint16_t normalIndex;
} pushConstants;

#define PI 3.14159265358979323

void main() {
	vec2 normalXY = texture(textures[uint(pushConstants.normalIndex)], texCoord).rg * 2.0 - 1.0;
	float normalZ = sqrt(1.0 - normalXY.x * normalXY.x + normalXY.y * normalXY.y);
	vec3 normalMap = vec3(normalXY, normalZ);

	vec3 norm = normalize(normal);
	vec3 tang = normalize(tangent.xyz);
	vec3 bitangent = normalize(cross(norm, tang) * tangent.w);
	mat3 tbn = mat3(tang, bitangent, norm);

	vec4 baseColor = pushConstants.color * texture(textures[uint(pushConstants.colorIndex)], texCoord);

	vec3 N = normalize(pushConstants.normalMatrix * tbn * normalMap);
	vec3 V = normalize(pushConstants.cameraPosition - position);
	vec3 L = normalize(vec3(0.36, 0.80, 0.48));

	oColor = vec4(N * 0.5 + 0.5, 1.0);
}
