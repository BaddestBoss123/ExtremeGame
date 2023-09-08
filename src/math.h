#include <stdint.h>
#include <stdbool.h>

#define M_PI 3.14159265358979323846264338327950288f

#define BITS_SET(variable, bits) ((variable & (bits)) == (bits))

#define ALIGN_FORWARD(value, alignment) ((alignment) > 0) ? ((value) + (alignment) - 1) & ~((alignment) - 1) : (value)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef __uint128_t u128;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef __int128_t i128;

typedef float mat3 __attribute__((matrix_type(4, 3)));
typedef float mat4 __attribute__((matrix_type(4, 4)));
typedef float vec2 __attribute__((ext_vector_type(2)));
typedef float vec3 __attribute__((ext_vector_type(3)));
typedef float vec4 __attribute__((ext_vector_type(4)));
typedef i32 ivec2 __attribute__((ext_vector_type(2)));
typedef i32 ivec3 __attribute__((ext_vector_type(3)));
typedef u8 u8vec2 __attribute__((ext_vector_type(2)));
typedef u8 u8vec3 __attribute__((ext_vector_type(3)));
typedef u8 u8vec4 __attribute__((ext_vector_type(4)));
typedef u16 u16vec2 __attribute__((ext_vector_type(2)));
typedef u16 u16vec3 __attribute__((ext_vector_type(3)));
typedef i16 i16vec2 __attribute__((ext_vector_type(2)));
typedef i16 i16vec3 __attribute__((ext_vector_type(3)));
typedef u32 uvec3 __attribute__((ext_vector_type(3)));
typedef u32 uvec4 __attribute__((ext_vector_type(4)));
typedef float quat __attribute__((ext_vector_type(4)));

static inline float step(float edge, float x) {
    return x < edge ? 0.f : 1.f;
}

static inline float easeOutQuadratic(float x) {
	return 1.f -__builtin_powf(1.f - x, 2.f);
}

static inline float easeInOutQuad(float x) {
   float inValue = 2.f * x  *x;
   float outValue = 1.f - __builtin_powf(-2.f * x + 2.f, 2.f) / 2.f;
   float inStep = step(inValue, 0.5) * inValue;
   float outStep = step(0.5, outValue ) * outValue;

   return inStep + outStep;
}

static inline float easeOutCubic(float x) {
	return 1.f -__builtin_powf(1.f - x, 3.f);
}

static inline float degToRad(float deg) {
	return deg * M_PI / 180.f;
}

static inline float radToDeg(float rad) {
	return rad * 180.f / M_PI;
}

static inline float i8ToRad(i8 x) {
	return x * M_PI / 127.5f;
}

static inline i8 radToI8(float rad) {
	return (i8)(rad * (127.5f / M_PI));
}

static inline float normalize(float x, float a, float b) {
	return (x - a) / (b - a);
}

static inline float lerp(float a, float b, float t) {
	return a + t * (b - a);
}

static inline vec2 vec2Lerp(vec2 a, vec2 b, float t) {
	return a + t * (b - a);
}

static inline vec3 vec3Lerp(vec3 a, vec3 b, float t) {
	return a + t * (b - a);
}

static inline vec4 vec4Lerp(vec4 a, vec4 b, float t) {
	return a + t * (b - a);
}

static inline u32 clamp(u32 v, u32 min, u32 max) {
	v = v < min ? min : v;
	return v > max ? max : v;
}

static inline float fclampf(float v, float min, float max) {
	return __builtin_fminf(__builtin_fmaxf(v, min), max);
}

static inline vec3 vec3Clamp(vec3 v, vec3 min, vec3 max) {
	return __builtin_elementwise_min(__builtin_elementwise_max(v, min), max);
}

static inline float normalizeAngle(float a) {
	while (a < -M_PI) a += 2.f * M_PI;
	while (a > M_PI) a -= 2.f * M_PI;
	return a;
}

static inline vec2 vec2Perp(vec2 v) {
	return (vec2){ -v.y, v.x };
}

static inline float vec2Length(vec2 a) {
	return __builtin_sqrtf(a.x * a.x + a.y * a.y);
}

static inline float vec3Length(vec3 a) {
	return __builtin_sqrtf(a.x * a.x + a.y * a.y + a.z * a.z);
}

static inline float vec4Length(vec4 a) {
	return __builtin_sqrtf(a.x * a.x + a.y * a.y + a.z * a.z + a.w * a.w);
}

static inline vec2 vec2Normalize(vec2 a) {
	return a / vec2Length(a);
}

static inline vec3 vec3Normalize(vec3 a) {
	return a / vec3Length(a);
}

static inline vec4 vec4Normalize(vec4 a) {
	return a / vec4Length(a);
}

static inline float vec2Cross(vec2 a, vec2 b) {
	return (a.x * b.y) - (a.y * b.x);
}

static inline vec3 vec3Cross(vec3 a, vec3 b) {
	return (vec3){
		(a.y * b.z) - (a.z * b.y),
		(a.z * b.x) - (a.x * b.z),
		(a.x * b.y) - (a.y * b.x)
	};
}

static inline float vec2Dot(vec2 a, vec2 b) {
	return a.x * b.x + a.y * b.y;
}

static inline float vec3Dot(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec4Dot(vec4 a, vec4 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

static inline vec3 vec3TransformQuat(vec3 v, quat q) {
	vec3 uv = vec3Cross(q.xyz, v);
	return v + (uv * (2 * q.w)) + (vec3Cross(q.xyz, uv) * 2);
}

static inline vec4 vec4TransformMat4(vec4 a, mat4 m) {
	return (vec4){
		m[0][0] * a.x + m[0][1] * a.y + m[0][2] * a.z + m[0][3] * a.w,
		m[1][0] * a.x + m[1][1] * a.y + m[1][2] * a.z + m[1][3] * a.w,
		m[2][0] * a.x + m[2][1] * a.y + m[2][2] * a.z + m[2][3] * a.w,
		m[3][0] * a.x + m[3][1] * a.y + m[3][2] * a.z + m[3][3] * a.w
	};
}

static inline quat quatConjugate(quat a) {
	return (quat){ -a.x, -a.y, -a.z, a.w };
}

static inline quat quatFromAxisAngle(vec3 axis, float rad) {
	rad *= 0.5f;
	float s = __builtin_sinf(rad);

	return (quat){ s * axis.x, s * axis.y, s * axis.z, __builtin_cosf(rad) };
}

static inline quat quatMultiply(quat a, quat b) {
	return (quat){
		a.x * b.w + a.w * b.x + a.y * b.z - a.z * b.y,
		a.y * b.w + a.w * b.y + a.z * b.x - a.x * b.z,
		a.z * b.w + a.w * b.z + a.x * b.y - a.y * b.x,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
	};
}

static inline quat quatRotateX(quat a, float radians) {
	float bx = __builtin_sinf(radians);
	float bw = __builtin_cosf(radians);

	return (quat){
		a.x * bw + a.w * bx,
		a.y * bw + a.z * bx,
		a.z * bw - a.y * bx,
		a.w * bw - a.x * bx,
	};
}

static inline quat quatRotateY(quat a, float radians) {
	float by = __builtin_sinf(radians);
	float bw = __builtin_cosf(radians);

	return (quat){
		a.x * bw - a.z * by,
		a.y * bw + a.w * by,
		a.z * bw + a.x * by,
		a.w * bw - a.y * by,
	};
}

static inline quat quatRotateZ(quat a, float radians) {
	float bz = __builtin_sinf(radians);
	float bw = __builtin_cosf(radians);

	return (quat){
		a.x * bw + a.y * bz,
		a.y * bw - a.x * bz,
		a.z * bw + a.w * bz,
		a.w * bw - a.z * bz,
	};
}

static inline quat quatFromMat3(mat3 m) {
	return (quat){
		__builtin_copysignf(__builtin_sqrtf(__builtin_fmaxf(0.f, 1.f + m[0][0] - m[1][1] - m[2][2])) * 0.5f, m[2][1] - m[1][2]),
		__builtin_copysignf(__builtin_sqrtf(__builtin_fmaxf(0.f, 1.f - m[0][0] + m[1][1] - m[2][2])) * 0.5f, m[0][2] - m[2][0]),
		__builtin_copysignf(__builtin_sqrtf(__builtin_fmaxf(0.f, 1.f - m[0][0] - m[1][1] + m[2][2])) * 0.5f, m[1][0] - m[0][1]),
		__builtin_sqrtf(__builtin_fmaxf(0.f, 1.f + m[0][0] + m[1][1] + m[2][2])) * 0.5f
	};
}

static inline quat slerp(quat a, quat b, float t) {
	float d = vec4Dot(a, b);

	if (d < 0.f) {
		b = -b;
		d = -d;
	}

	if (d > 0.9995f)
		return vec4Lerp(a, b, t);

	float theta_0 = __builtin_acosf(d);
	float theta = t * theta_0;
	float sin_theta = __builtin_sinf(theta);
	float sin_theta_0 = __builtin_sinf(theta_0);

	float scalePreviousQuat = __builtin_cosf(theta) - d * sin_theta / sin_theta_0;
	float scaleNextQuat = sin_theta / sin_theta_0;
	return scalePreviousQuat * a + scaleNextQuat * b;
}

static inline mat3 mat3Adjoint(mat3 m) {
	vec3 a = { m[0][0], m[1][0], m[2][0] };
	vec3 b = { m[0][1], m[1][1], m[2][1] };
	vec3 c = { m[0][2], m[1][2], m[2][2] };

	vec3 d = vec3Cross(b, c);
	vec3 e = vec3Cross(c, a);
	vec3 f = vec3Cross(a, b);

	mat3 ret;
	ret[0][0] = d.x;
	ret[1][0] = d.y;
	ret[2][0] = d.z;
	ret[0][1] = e.x;
	ret[1][1] = e.y;
	ret[2][1] = e.z;
	ret[0][2] = f.x;
	ret[1][2] = f.y;
	ret[2][2] = f.z;

	return ret;
}

static inline mat3 mat3FromMat4(mat4 m) {
	mat3 ret;
	ret[0][0] = m[0][0];
	ret[0][1] = m[0][1];
	ret[0][2] = m[0][2];
	ret[1][0] = m[1][0];
	ret[1][1] = m[1][1];
	ret[1][2] = m[1][2];
	ret[2][0] = m[2][0];
	ret[2][1] = m[2][1];
	ret[2][2] = m[2][2];

	return ret;
}

static inline mat3 mat3FromQuat(quat q) {
	vec3 right = vec3TransformQuat((vec3){ 1.f, 0.f, 0.f }, q);
	vec3 up = vec3TransformQuat((vec3){ 0.f, 1.f, 0.f }, q);
	vec3 forward = vec3TransformQuat((vec3){ 0.f, 0.f, 1.f }, q);

	mat3 ret;
	ret[0][0] = right.x;
	ret[0][1] = up.x;
	ret[0][2] = forward.x;
	ret[1][0] = right.y;
	ret[1][1] = up.y;
	ret[1][2] = forward.y;
	ret[2][0] = right.z;
	ret[2][1] = up.z;
	ret[2][2] = forward.z;

	return ret;
}

static inline mat4 mat4TranslateScale(mat4 m, vec3 t, vec3 s) {
	mat4 ret;

	ret[0][0] = m[0][0] * s.x;
	ret[1][0] = m[1][0] * s.x;
	ret[2][0] = m[2][0] * s.x;
	ret[3][0] = m[3][0] * s.x;
	ret[0][1] = m[0][1] * s.y;
	ret[1][1] = m[1][1] * s.y;
	ret[2][1] = m[2][1] * s.y;
	ret[3][1] = m[3][1] * s.y;
	ret[0][2] = m[0][2] * s.z;
	ret[1][2] = m[1][2] * s.z;
	ret[2][2] = m[2][2] * s.z;
	ret[3][2] = m[3][2] * s.z;
	ret[0][3] = m[0][0] * t.x + m[0][1] * t.y + m[0][2] * t.z + m[0][3];
	ret[1][3] = m[1][0] * t.x + m[1][1] * t.y + m[1][2] * t.z + m[1][3];
	ret[2][3] = m[2][0] * t.x + m[2][1] * t.y + m[2][2] * t.z + m[2][3];
	ret[3][3] = m[3][0] * t.x + m[3][1] * t.y + m[3][2] * t.z + m[3][3];

	return ret;
}

static inline mat4 mat4Inverse(mat4 a) {
	float b00 = a[0][0] * a[1][1] - a[1][0] * a[0][1];
	float b01 = a[0][0] * a[2][1] - a[2][0] * a[0][1];
	float b02 = a[0][0] * a[3][1] - a[3][0] * a[0][1];
	float b03 = a[1][0] * a[2][1] - a[2][0] * a[1][1];
	float b04 = a[1][0] * a[3][1] - a[3][0] * a[1][1];
	float b05 = a[2][0] * a[3][1] - a[3][0] * a[2][1];
	float b06 = a[0][2] * a[1][3] - a[1][2] * a[0][3];
	float b07 = a[0][2] * a[2][3] - a[2][2] * a[0][3];
	float b08 = a[0][2] * a[3][3] - a[3][2] * a[0][3];
	float b09 = a[1][2] * a[2][3] - a[2][2] * a[1][3];
	float b10 = a[1][2] * a[3][3] - a[3][2] * a[1][3];
	float b11 = a[2][2] * a[3][3] - a[3][2] * a[2][3];

	float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
	det = 1.f / det;

	mat4 ret;
	ret[0][0] = (a[1][1] * b11 - a[2][1] * b10 + a[3][1] * b09) * det;
	ret[1][0] = (a[2][0] * b10 - a[1][0] * b11 - a[3][0] * b09) * det;
	ret[2][0] = (a[1][3] * b05 - a[2][3] * b04 + a[3][3] * b03) * det;
	ret[3][0] = (a[2][2] * b04 - a[1][2] * b05 - a[3][2] * b03) * det;
	ret[0][1] = (a[2][1] * b08 - a[0][1] * b11 - a[3][1] * b07) * det;
	ret[1][1] = (a[0][0] * b11 - a[2][0] * b08 + a[3][0] * b07) * det;
	ret[2][1] = (a[2][3] * b02 - a[0][3] * b05 - a[3][3] * b01) * det;
	ret[3][1] = (a[0][2] * b05 - a[2][2] * b02 + a[3][2] * b01) * det;
	ret[0][2] = (a[0][1] * b10 - a[1][1] * b08 + a[3][1] * b06) * det;
	ret[1][2] = (a[1][0] * b08 - a[0][0] * b10 - a[3][0] * b06) * det;
	ret[2][2] = (a[0][3] * b04 - a[1][3] * b02 + a[3][3] * b00) * det;
	ret[3][2] = (a[1][2] * b02 - a[0][2] * b04 - a[3][2] * b00) * det;
	ret[0][3] = (a[1][1] * b07 - a[0][1] * b09 - a[2][1] * b06) * det;
	ret[1][3] = (a[0][0] * b09 - a[1][0] * b07 + a[2][0] * b06) * det;
	ret[2][3] = (a[1][3] * b01 - a[0][3] * b03 - a[2][3] * b00) * det;
	ret[3][3] = (a[0][2] * b03 - a[1][2] * b01 + a[2][2] * b00) * det;

	return ret;
}

static inline mat4 mat4FromTranslation(vec3 t) {
	mat4 ret;
	ret[0][0] = 1.f;
	ret[1][0] = 0.f;
	ret[2][0] = 0.f;
	ret[3][0] = 0.f;
	ret[0][1] = 0.f;
	ret[1][1] = 1.f;
	ret[2][1] = 0.f;
	ret[3][1] = 0.f;
	ret[0][2] = 0.f;
	ret[1][2] = 0.f;
	ret[2][2] = 1.f;
	ret[3][2] = 0.f;
	ret[0][3] = t.x;
	ret[1][3] = t.y;
	ret[2][3] = t.z;
	ret[3][3] = 1.f;

	return ret;
}

static inline mat4 mat4FromRotationTranslationScale(quat r, vec3 t, vec3 s) {
	mat4 ret;
	ret[0][0] = (1.f - ((r.y * (r.y + r.y)) + (r.z * (r.z + r.z)))) * s.x;
	ret[1][0] = ((r.x * (r.y + r.y)) + (r.w * (r.z + r.z))) * s.x;
	ret[2][0] = ((r.x * (r.z + r.z)) - (r.w * (r.y + r.y))) * s.x;
	ret[3][0] = 0.f;
	ret[0][1] = ((r.x * (r.y + r.y)) - (r.w * (r.z + r.z))) * s.y;
	ret[1][1] = (1.f - ((r.x * (r.x + r.x)) + (r.z * (r.z + r.z)))) * s.y;
	ret[2][1] = ((r.y * (r.z + r.z)) + (r.w * (r.x + r.x))) * s.y;
	ret[3][1] = 0.f;
	ret[0][2] = ((r.x * (r.z + r.z)) + (r.w * (r.y + r.y))) * s.z;
	ret[1][2] = ((r.y * (r.z + r.z)) - (r.w * (r.x + r.x))) * s.z;
	ret[2][2] = (1.f - ((r.x * (r.x + r.x)) + (r.y * (r.y + r.y)))) * s.z;
	ret[3][2] = 0.f;
	ret[0][3] = t.x;
	ret[1][3] = t.y;
	ret[2][3] = t.z;
	ret[3][3] = 1.f;

	return ret;
}

static inline mat4 mat4From2DAffine(float a, float b, float c, float d, float e, float f) {
	mat4 ret;
	ret[0][0] = a;
	ret[0][1] = b;
	ret[0][2] = 0.f;
	ret[0][3] = e;
	ret[1][0] = c;
	ret[1][1] = d;
	ret[1][2] = 0.f;
	ret[1][3] = f;
	ret[2][0] = 0.f;
	ret[2][1] = 0.f;
	ret[2][2] = 1.f;
	ret[2][3] = 0.f;
	ret[3][0] = 0.f;
	ret[3][1] = 0.f;
	ret[3][2] = 0.f;
	ret[3][3] = 1.f;

	return ret;
}

static inline u8vec4 blendColor(u8vec4 c0, u8vec4 c1, float t) {
	return (u8vec4){
		(u8)lerp(c0.r, c1.r, t),
		(u8)lerp(c0.g, c1.g, t),
		(u8)lerp(c0.b, c1.b, t),
		(u8)lerp(c0.a, c1.a, t)
	};
}

static inline u32 u32random(void) {
	static u32 s[] = { 0x6A8D3F21, 0xE57B9C14 };

	u32 x = s[0];
	u32 y = s[1];

	s[0] = y;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= y;
	x ^= y >> 5;
	s[1] = x;

	return s[0];
}

static inline float rayPlane(vec3 origin, vec3 direction, vec4 plane) {
	return -(vec3Dot(origin, plane.xyz) + plane.w) / vec3Dot(direction, plane.xyz);
}
