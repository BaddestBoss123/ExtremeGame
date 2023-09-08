#version 460

#define LIGHT vec3(0.36, 0.80, 0.48)
#define SUN_SIZE 0.002
#define SUN_SHARPNESS 1.0

layout(location = 0) in vec3 direction;

layout(location = 0) out vec4 oColor;

void main() {
	vec3 dir = normalize(direction);

	float h = (1.0 - dir.y) * (1.0 - dir.y) * 0.5;
	vec3 sky = vec3(0.2 + h, 0.5 + h, 1.0);

	float s = dot(dir, LIGHT) - 1.0 + SUN_SIZE;
	float sun = min(exp(s * SUN_SHARPNESS / SUN_SIZE), 1.0);

	oColor = vec4(max(sky, sun), 1.0);
}
