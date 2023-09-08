#pragma comment(lib, "kernel32")
#pragma comment(lib, "user32")
#pragma comment(lib, "vcruntime.lib")
#pragma comment(lib, "ucrt.lib")

#include "math.h"
#include <Windows.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define STR(...) #__VA_ARGS__

struct KTX2 {
	char identifier[12];
	u32 vkFormat;
	u32 typeSize;
	u32 pixelWidth;
	u32 pixelHeight;
	u32 pixelDepth;
	u32 layerCount;
	u32 faceCount;
	u32 levelCount;
	u32 supercompressionScheme;
	u32 dfdByteOffset;
	u32 dfdByteLength;
	u32 kvdByteOffset;
	u32 kvdByteLength;
	u64 sgdByteOffset;
	u64 sgdByteLength;
	struct {
		u64 byteOffset;
		u64 byteLength;
		u64 uncompressedByteLength;
	} levels[];
};

struct WAV {
    char riff[4];
    u32 fileSize;
    char wave[4];
    char fmt[4];
    u32 length;
    u16 audioFormat;
    u16 numChannels;
    u32 sampleRate;
    u32 byteRate;
    u16 blockAlign;
    u16 bitsPerSample;
    char data[4];
    u32 dataSize;
};

struct VertexPosition {
	u16 x, y, z;
};

struct VertexAttributes {
	u16 nx, ny, nz;
	u16 tx, ty, tz, tw;
	u16 u, v;
};

#define INCBIN(name, file) \
	__asm__(".section .rdata, \"dr\"\n" \
			".global incbin_" #name "_start\n" \
			".balign 16\n" \
			"incbin_" #name "_start:\n" \
			".incbin \"" file "\"\n" \
			".global incbin_" #name "_end\n" \
			".balign 1\n" \
			"incbin_" #name "_end:\n" \
	); \
	extern __attribute__((aligned(16))) const char incbin_ ## name ## _start[]; \
	extern const char incbin_ ## name ## _end[]

// INCBIN(planks033B_1K_Color, "Planks033B_1K_Color.ktx2");
INCBIN(gear, "gear.ktx2");
INCBIN(discord, "discord.ktx2");

INCBIN(tada, "C:/Windows/Media/tada.wav");

static inline void writeNode(cgltf_node* node, int depth) {
	char indent[128];
	for (int i = 0; i < 1 + depth * 2; i++)
		indent[i] = '\t';
	indent[1 + depth * 2] = '\0';

	__builtin_printf("{\n");
	printf("%s\t.translation = { %ff, %ff, %ff },\n", indent, (double)node->translation[0], (double)node->translation[1], (double)node->translation[2]);
	printf("%s\t.rotation = { %ff, %ff, %ff, %ff },\n", indent, (double)node->rotation[0], (double)node->rotation[1], (double)node->rotation[2], (double)node->rotation[3]);
	printf("%s\t.scale = { %ff, %ff, %ff },\n", indent, (double)node->scale[0], (double)node->scale[1], (double)node->scale[2]);

	if (node->mesh)
		printf("%s\t.mesh = %s,\n", indent, node->mesh->name);

	if (node->children_count) {
		printf("%s\t.childCount = %zi,\n", indent, node->children_count);
		printf("%s\t.children = (struct Node[]){\n%s\t\t", indent, indent);

		for (cgltf_size i = 0; i < node->children_count; i++)
			writeNode(node->children[i], depth + 1);
	}

	printf("%s}, ", indent);
}

int _fltused;

__attribute__((noreturn)) void WinMainCRTStartup(void) {
	FILE* geometry_bin = fopen("geometry", "wb");
	FILE* icons_bin = fopen("icons", "wb");
	FILE* textures_bin = fopen("textures", "wb");

	FILE* tada_bin = fopen("tada", "wb");
	struct WAV* tada = incbin_tada_start;

	struct KTX2* gear = incbin_gear_start;
	struct KTX2* discord = incbin_discord_start;

	fwrite(incbin_gear_start + gear->levels[0].byteOffset, 1, gear->levels[0].byteLength, icons_bin);
	fwrite(incbin_discord_start + discord->levels[0].byteOffset, 1, discord->levels[0].byteLength, icons_bin);

	// struct KTX2* planks033B_1K_Color = incbin_planks033B_1K_Color_start;
	// for (u32 i = 0; i < planks033B_1K_Color->levelCount; i++) {
	// 	fwrite(incbin_planks033B_1K_Color_start + planks033B_1K_Color->levels[i].byteOffset, 1, planks033B_1K_Color->levels[i].byteLength, textures_bin);
	// }

	static u16 indices[0x1000000];
	static struct VertexPosition vertexPositions[0x1000000];
	static struct VertexAttributes vertexAttributes[0x1000000];
	u32 indexCount = 6 * 12288 * sizeof(u16);
	u32 vertexCount = 0;

	printf("[PRIMITIVE_QUAD] = {\n"
		"\t\t.min = { 0.f, 0.f, 0.f },\n"
		"\t\t.max = { 1.f, 0.f, 1.f },\n"
		"\t\t.indexCount = 6,\n"
		"\t\t.firstIndex = BUFFER_OFFSET_QUAD_INDICES / sizeof(u16),\n"
		"\t\t.vertexOffset = 0,\n"
		"\t\t.indexed = true\n"
		"\t}, ");

	vertexPositions[vertexCount] = (struct VertexPosition){ 0, 0, 0 };
	vertexPositions[vertexCount + 1] = (struct VertexPosition){ UINT16_MAX, 0, 0 };
	vertexPositions[vertexCount + 2] = (struct VertexPosition){ 0, 0, UINT16_MAX };
	vertexPositions[vertexCount + 3] = (struct VertexPosition){ UINT16_MAX, 0, UINT16_MAX };

	u16 nx = 0;
	u16 ny = UINT16_MAX;
	u16 nz = 0;
	u16 tx = UINT16_MAX;
	u16 ty = 0;
	u16 tw = 0;
	u16 tz = UINT16_MAX;
	vertexAttributes[vertexCount] = (struct VertexAttributes){ nx, ny, nz, tx, ty, tz, tw, 0, 0 };
	vertexAttributes[vertexCount + 1] = (struct VertexAttributes){ nx, ny, nz, tx, ty, tz, tw, UINT16_MAX, 0 };
	vertexAttributes[vertexCount + 2] = (struct VertexAttributes){ nx, ny, nz, tx, ty, tz, tw, 0, UINT16_MAX };
	vertexAttributes[vertexCount + 3] = (struct VertexAttributes){ nx, ny, nz, tx, ty, tz, tw, UINT16_MAX, UINT16_MAX };

	vertexCount += 4;

	printf("[PRIMITIVE_CUBE] = {\n"
		"\t\t.min = { -0.5f, -0.5f, -0.5f },\n"
		"\t\t.max = { 0.5f, 0.5f, 0.5f },\n"
		"\t\t.indexCount = 36,\n"
		"\t\t.firstIndex = BUFFER_OFFSET_QUAD_INDICES / sizeof(u16),\n"
		"\t\t.vertexOffset = %u,\n"
		"\t\t.indexed = true\n"
		"\t}, ", vertexCount);

	for (u32 i = 0; i < 24; i++) {
		u32 b = 1 << i;
		vertexPositions[vertexCount] = (struct VertexPosition){
			.x = (b & 0b110000111100110011110000) ? UINT16_MAX : 0,
			.y = (b & 0b010101011111000001010101) ? UINT16_MAX : 0,
			.z = (b & 0b111100001010010100111100) ? UINT16_MAX : 0
		};

		vec3 normal = (vec3[]){
			{ -1.0, 0.0, 0.0 },
			{ 1.0, 0.0, 0.0 },
			{ 0.0, -1.0, 0.0 },
			{ 0.0, 1.0, 0.0 },
			{ 0.0, 0.0, -1.0 },
			{ 0.0, 0.0, 1.0 }
		}[(i / 4) % 6];

		vec3 tangent = (vec3[]){
			{ 0.0, 0.0, -1.0 },
			{ 0.0, 1.0, 0.0 },
			{ -1.0, 0.0, 0.0 },
			{ 0.0, 0.0, 1.0 },
			{ 0.0, 1.0, 0.0 },
			{ -1.0, 0.0, 0.0 }
		}[(i / 4) % 6];

		vertexAttributes[vertexCount++] = (struct VertexAttributes){
			.nx = (u16)(normalize(normal.x, -1.f, 1.f) * UINT16_MAX),
			.ny = (u16)(normalize(normal.y, -1.f, 1.f) * UINT16_MAX),
			.nz = (u16)(normalize(normal.z, -1.f, 1.f) * UINT16_MAX),
			.tx = (u16)(normalize(tangent.x, -1.f, 1.f) * UINT16_MAX),
			.ty = (u16)(normalize(tangent.y, -1.f, 1.f) * UINT16_MAX),
			.tz = (u16)(normalize(tangent.z, -1.f, 1.f) * UINT16_MAX),
			.tw = (u16)(normalize(1.f, -1.f, 1.f) * UINT16_MAX),
			.u = ((i & 2) >> 1) * UINT16_MAX,
			.v = (i & 1) * UINT16_MAX
		};
	}

	float z = (1 + __builtin_sqrtf(5)) / 2;

	printf("[PRIMITIVE_ICOSAHEDRON] = {\n"
		"\t\t.min = { %ff, %ff, %ff },\n"
		"\t\t.max = { %ff, %ff, %ff },\n"
		"\t\t.vertexCount = 60,\n"
		"\t\t.firstVertex = %u,\n"
		"\t\t.indexed = false\n"
		"\t}, ", -(double)z, -(double)z, -(double)z, (double)z, (double)z, (double)z, vertexCount);

	vec3 icosahedronPositions[] = {
		{ -1, 0, z },
		{ 1, 0, z },
		{ -1, 0, -z },
		{ 1, 0, -z },
		{ 0, z, 1 },
		{ 0, z, -1 },
		{ 0, -z, 1 },
		{ 0, -z, -1 },
		{ z, 1, 0 },
		{ -z, 1, 0 },
		{ z, -1, 0 },
		{ -z, -1, 0 }
	};

	vec2 icosahedronUVs[_countof(icosahedronPositions)];

	for (u32 i = 0; i < _countof(icosahedronPositions); i++) {
		float r = vec3Length(icosahedronPositions[i]);
		float theta = __builtin_atan2f(icosahedronPositions[i].y, icosahedronPositions[i].x);
		float phi = __builtin_acosf(icosahedronPositions[i].z / r);

		icosahedronUVs[i] = (vec2){ theta / (2.f * M_PI) + 0.5f, phi / M_PI };
	}

	u8 icosahedronIndices[] = {
		0, 1, 4, 0, 4, 9, 9, 4, 5, 4, 8, 5, 4, 1, 8,
		8, 1, 10, 8, 10, 3, 5, 8, 3, 5, 3, 2, 2, 3, 7,
		7, 3, 10, 7, 10, 6, 7, 6, 11, 11, 6, 0, 0, 6, 1,
		6, 10, 1, 9, 11, 0, 9, 2, 11, 9, 5, 2, 7, 11, 2
	};

	for (u32 i = 0; i < 20; i++) {
		u8 i0 = icosahedronIndices[i * 3 + 0];
		u8 i1 = icosahedronIndices[i * 3 + 1];
		u8 i2 = icosahedronIndices[i * 3 + 2];

		vec3 a = icosahedronPositions[i0];
		vec3 b = icosahedronPositions[i1];
		vec3 c = icosahedronPositions[i2];

		vertexPositions[vertexCount] = (struct VertexPosition){
			.x = (u16)(normalize(a.x, -z, z) * UINT16_MAX),
			.y = (u16)(normalize(a.y, -z, z) * UINT16_MAX),
			.z = (u16)(normalize(a.z, -z, z) * UINT16_MAX)
		};

		vertexPositions[vertexCount + 1] = (struct VertexPosition){
			.x = (u16)(normalize(b.x, -z, z) * UINT16_MAX),
			.y = (u16)(normalize(b.y, -z, z) * UINT16_MAX),
			.z = (u16)(normalize(b.z, -z, z) * UINT16_MAX)
		};

		vertexPositions[vertexCount + 2] = (struct VertexPosition){
			.x = (u16)(normalize(c.x, -z, z) * UINT16_MAX),
			.y = (u16)(normalize(c.y, -z, z) * UINT16_MAX),
			.z = (u16)(normalize(c.z, -z, z) * UINT16_MAX)
		};

		vec2 uv0 = icosahedronUVs[i0];
		vec2 uv1 = icosahedronUVs[i1];
		vec2 uv2 = icosahedronUVs[i2];

		vec3 edge1 = b - a;
		vec3 edge2 = c - a;
		vec3 normal = vec3Normalize(vec3Cross(edge1, edge2));

		vec2 deltaUV1 = uv1 - uv0;
		vec2 deltaUV2 = uv2 - uv0;

		float f = 1.f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
		vec3 tangent = {
			f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x),
			f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y),
			f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z)
		};
		tangent = vec3Normalize(tangent);

		vertexAttributes[vertexCount] = (struct VertexAttributes){
			.nx = (u16)(normalize(normal.x, -1.f, 1.f) * UINT16_MAX),
			.ny = (u16)(normalize(normal.y, -1.f, 1.f) * UINT16_MAX),
			.nz = (u16)(normalize(normal.z, -1.f, 1.f) * UINT16_MAX),
			.tx = (u16)(normalize(tangent.x, -1.f, 1.f) * UINT16_MAX),
			.ty = (u16)(normalize(tangent.y, -1.f, 1.f) * UINT16_MAX),
			.tz = (u16)(normalize(tangent.z, -1.f, 1.f) * UINT16_MAX),
			.tw = (u16)(normalize(1.f, -1.f, 1.f) * UINT16_MAX),
			.u = (u16)(uv0.x * UINT16_MAX),
			.v = (u16)(uv0.y * UINT16_MAX)
		};

		vertexAttributes[vertexCount + 1] = (struct VertexAttributes){
			.nx = (u16)(normalize(normal.x, -1.f, 1.f) * UINT16_MAX),
			.ny = (u16)(normalize(normal.y, -1.f, 1.f) * UINT16_MAX),
			.nz = (u16)(normalize(normal.z, -1.f, 1.f) * UINT16_MAX),
			.tx = (u16)(normalize(tangent.x, -1.f, 1.f) * UINT16_MAX),
			.ty = (u16)(normalize(tangent.y, -1.f, 1.f) * UINT16_MAX),
			.tz = (u16)(normalize(tangent.z, -1.f, 1.f) * UINT16_MAX),
			.tw = (u16)(normalize(1.f, -1.f, 1.f) * UINT16_MAX),
			.u = (u16)(uv1.x * UINT16_MAX),
			.v = (u16)(uv1.y * UINT16_MAX)
		};

		vertexAttributes[vertexCount + 2] = (struct VertexAttributes){
			.nx = (u16)(normalize(normal.x, -1.f, 1.f) * UINT16_MAX),
			.ny = (u16)(normalize(normal.y, -1.f, 1.f) * UINT16_MAX),
			.nz = (u16)(normalize(normal.z, -1.f, 1.f) * UINT16_MAX),
			.tx = (u16)(normalize(tangent.x, -1.f, 1.f) * UINT16_MAX),
			.ty = (u16)(normalize(tangent.y, -1.f, 1.f) * UINT16_MAX),
			.tz = (u16)(normalize(tangent.z, -1.f, 1.f) * UINT16_MAX),
			.tw = (u16)(normalize(1.f, -1.f, 1.f) * UINT16_MAX),
			.u = (u16)(uv1.x * UINT16_MAX),
			.v = (u16)(uv1.y * UINT16_MAX)
		};

		vertexCount += 3;
	}

	uint16_t major = 12;
	uint16_t minor = 12;
	float minorRadius = 0.3f;
	float majorRadius = 1.f;

	printf("[PRIMITIVE_TORUS] = {\n"
		"\t\t.min = { %ff, %ff, %ff },\n"
		"\t\t.max = { %ff, %ff, %ff },\n"
		"\t\t.indexCount = %u,\n"
		"\t\t.firstIndex = BUFFER_OFFSET_QUAD_INDICES / sizeof(u16),\n"
		"\t\t.vertexOffset = %u,\n"
		"\t\t.indexed = true\n"
		"\t}, ", (double)(-majorRadius - minorRadius), (double)(-majorRadius - minorRadius), (double)(-minorRadius),
			(double)(majorRadius + minorRadius), (double)(majorRadius + minorRadius), (double)(minorRadius), major * minor * 6, vertexCount);

	float majorStep = 2 * M_PI / major;
	float minorStep = 2 * M_PI / minor;

	for (uint16_t i = 0; i < major; i++) {
		float majorAngle = i * majorStep;
		float u = majorAngle + 0.5f * majorStep;

		for (uint16_t j = 0; j < minor; j++) {
			float minorAngle = j * minorStep;
			float v = minorAngle + 0.5f * minorStep;

			vec3 facePositions[4];
			vec2 faceUVs[4];
			for (uint32_t k = 0; k < 4; k++) {
				float uu = majorAngle + (k & 1) * majorStep;
				float vv = minorAngle + (k >= 2) * minorStep;

				facePositions[k] = (vec3){
					(majorRadius + minorRadius * __builtin_cosf(vv)) * __builtin_cosf(uu),
					(majorRadius + minorRadius * __builtin_cosf(vv)) * __builtin_sinf(uu),
					minorRadius * __builtin_sinf(vv)
				};

				faceUVs[k] = (vec2){
					u / (2 * M_PI),
					v / (2 * M_PI)
				};
			}

			vec3 normal = {
				__builtin_cosf(v) * __builtin_cosf(u),
            	__builtin_cosf(v) * __builtin_sinf(u),
            	__builtin_sinf(v)
			};

			vec3 tangent = vec3Normalize((vec3){
				(majorRadius + minorRadius * __builtin_cosf(v)) * __builtin_sinf(u),
				(majorRadius + minorRadius * __builtin_cosf(v)) * __builtin_cosf(u),
				0.f
			});

			for (uint32_t k = 0; k < 4; k++) {
				vertexPositions[vertexCount] = (struct VertexPosition){
					.x = (u16)(normalize(facePositions[k].x, -majorRadius - minorRadius, majorRadius + minorRadius) * UINT16_MAX),
					.y = (u16)(normalize(facePositions[k].y, -majorRadius - minorRadius, majorRadius + minorRadius) * UINT16_MAX),
					.z = (u16)(normalize(facePositions[k].z, -minorRadius, minorRadius) * UINT16_MAX)
				};

				float uu = majorAngle + (k >= 2) * majorStep;
				float vv = minorAngle + (k & 1) * minorStep;

				vertexAttributes[vertexCount++] = (struct VertexAttributes){
					.nx = (u16)(normalize(normal.x, -1.f, 1.f) * UINT16_MAX),
					.ny = (u16)(normalize(normal.y, -1.f, 1.f) * UINT16_MAX),
					.nz = (u16)(normalize(normal.z, -1.f, 1.f) * UINT16_MAX),
					.tx = (u16)(normalize(tangent.x, -1.f, 1.f) * UINT16_MAX),
					.ty = (u16)(normalize(tangent.y, -1.f, 1.f) * UINT16_MAX),
					.tz = (u16)(normalize(tangent.z, -1.f, 1.f) * UINT16_MAX),
					.tw = (u16)(normalize(1.f, -1.f, 1.f) * UINT16_MAX),
					.u = (uint16_t)((uu / (2 * M_PI)) * UINT16_MAX),
					.v = (uint16_t)((vv / (2 * M_PI)) * UINT16_MAX)
				};
			}
		}
	}

	printf("indexCount: %u vertexCount: %u\n", (u32)(indexCount - (6 * 12288 * sizeof(u16))), vertexCount);

	fwrite(indices, sizeof(u16), indexCount - (6 * 12288 * sizeof(u16)), geometry_bin);
	fwrite(vertexPositions, sizeof(struct VertexPosition), vertexCount, geometry_bin);
	fwrite(vertexAttributes, sizeof(struct VertexAttributes), vertexCount, geometry_bin);

	ExitProcess(0);

// 	cgltf_options options = { 0 };
// 	cgltf_data* data;
// 	if (cgltf_parse_file(&options, "assets/barrel.glb", &data) != cgltf_result_success)
// 		ExitProcess(EXIT_FAILURE);

// 	if (cgltf_load_buffers(&options, data, "assets"))
// 		ExitProcess(EXIT_FAILURE);

// 	for (cgltf_size i = 0; i < data->materials_count; i++) {
// 		cgltf_material material = data->materials[i];

// 		u8 r = (u8)(material.pbr_metallic_roughness.base_color_factor[0] * 255);
// 		u8 g = (u8)(material.pbr_metallic_roughness.base_color_factor[1] * 255);
// 		u8 b = (u8)(material.pbr_metallic_roughness.base_color_factor[2] * 255);
// 		u8 a = (u8)(material.pbr_metallic_roughness.base_color_factor[3] * 255);

// 		const char* color = material.pbr_metallic_roughness.base_color_texture.texture == NULL ? "IMAGE_VIEW_WHITE" : material.pbr_metallic_roughness.base_color_texture.texture->image->name;

// 		const char* normal = material.normal_texture.texture == NULL ? "IMAGE_VIEW_NORMAL" : material.normal_texture.texture->image->name;

// 		printf("[%s] = {\n\
// 	.rgba = { %i, %i, %i, %i },\n\
// 	.color = %s,\n\
// 	.normal = %s\n\
// }", material.name, r, g, b, a, color, normal);
// 	}

// 	for (cgltf_size i = 0; i < data->meshes_count; i++) {
// 		cgltf_mesh mesh = data->meshes[i];

// 		for (cgltf_size j = 0; j < mesh.primitives_count; j++) {
// 			cgltf_primitive primitive = mesh.primitives[j];

// 			u64 indexCount = 0;
// 			u64 vertexCount = 0;
// 			u64 indexOffset = 0;
// 			u64 firstVertex = totalVertexCount;
// 			float miX = 0, miY = 0, miZ = 0;
// 			float maX = 0, maY = 0, maZ = 0;
// 			bool indexed = primitive.indices != NULL;

// 			if (indexed) {
// 				i64 accessorIndex = primitive.indices - data->accessors;
// 				if (!accessorVisits[accessorIndex].visited) {
// 					accessorVisits[accessorIndex].visited = true;
// 					void* indexData = ((char*)primitive.indices->buffer_view->buffer->data + primitive.indices->buffer_view->offset + primitive.indices->offset);

// 					static u16 indices[UINT16_MAX];

// 					if (primitive.indices->stride == 4) {
// 						u32* idxs = (u32*)indexData;
// 						for (cgltf_size k = 0; k < primitive.indices->count; k++)
// 							indices[k] = (u16)idxs[k];
// 					} else if (primitive.indices->stride == 2) {
// 						u16* idxs = (u16*)indexData;
// 						for (cgltf_size k = 0; k < primitive.indices->count; k++)
// 							indices[k] = (u16)idxs[k];
// 					} else if (primitive.indices->stride == 1) {
// 						u8* idxs = (u8*)indexData;
// 						for (cgltf_size k = 0; k < primitive.indices->count; k++)
// 							indices[k] = (u16)idxs[k];
// 					} else {

// 					}

// 					fwrite(indices, sizeof(u16), primitive.indices->count, indices_bin);

// 					accessorVisits[accessorIndex].indexOffset = (u32)totalIndexCount;

// 					indexCount = primitive.indices->count;
// 					indexOffset = totalIndexCount;
// 					totalIndexCount += indexCount;
// 				} else {
// 					indexCount = primitive.indices->count;
// 					indexOffset = accessorVisits[accessorIndex].indexOffset;
// 				}
// 			}

// 			static struct VertexAttributes attributes[UINT16_MAX];

// 			u64 vertexOffset = totalVertexCount;

// 			for (cgltf_size k = 0; k < primitive.attributes_count; k++) {
// 				cgltf_attribute attribute = primitive.attributes[k];
// 				i64 accessorIndex = attribute.data - data->accessors;

// 				float minX = attribute.data->min[0];
// 				float minY = attribute.data->min[1];
// 				float minZ = attribute.data->min[2];
// 				float maxX = attribute.data->max[0];
// 				float maxY = attribute.data->max[1];
// 				float maxZ = attribute.data->max[2];

// 				switch (attribute.type) {
// 					case cgltf_attribute_type_position: {
// 						void* positions = ((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);

// 						totalVertexCount += attribute.data->count;
// 						vertexCount = attribute.data->count;

// 						miX = minX;
// 						miY = minY;
// 						miZ = minZ;
// 						maX = maxX;
// 						maY = maxY;
// 						maZ = maxZ;

// 						for (u32 l = 0; l < vertexCount; l++) {
// 							u16 vertex[3];
// 							vertex[0] = (u16)(normalize(((float*)positions)[(l * 3) + 0], minX, maxX) * UINT16_MAX);
// 							vertex[1] = (u16)(normalize(((float*)positions)[(l * 3) + 1], minY, maxY) * UINT16_MAX);
// 							vertex[2] = (u16)(normalize(((float*)positions)[(l * 3) + 2], minZ, maxZ) * UINT16_MAX);
// 							fwrite(vertex, sizeof(vertex), 1, positions_bin);
// 						}
// 					} break;
// 					case cgltf_attribute_type_normal: {
// 						void* normals = ((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
// 						for (cgltf_size l = 0; l < attribute.data->count; l++) {
// 							attributes[l].nx = (u16)(normalize(((float*)normals)[(l * 3) + 0], -1.f, 1.f) * UINT16_MAX);
// 							attributes[l].ny = (u16)(normalize(((float*)normals)[(l * 3) + 1], -1.f, 1.f) * UINT16_MAX);
// 							attributes[l].nz = (u16)(normalize(((float*)normals)[(l * 3) + 2], -1.f, 1.f) * UINT16_MAX);
// 						}
// 					} break;
// 					case cgltf_attribute_type_tangent: {
// 						void* tangents = ((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);
// 						for (cgltf_size l = 0; l < attribute.data->count; l++) {
// 							attributes[l].tx = (u16)(normalize(((float*)tangents)[(l * 4) + 0], -1.f, 1.f) * UINT16_MAX);
// 							attributes[l].ty = (u16)(normalize(((float*)tangents)[(l * 4) + 1], -1.f, 1.f) * UINT16_MAX);
// 							attributes[l].tz = (u16)(normalize(((float*)tangents)[(l * 4) + 2], -1.f, 1.f) * UINT16_MAX);
// 							attributes[l].tw = (u16)(normalize(((float*)tangents)[(l * 4) + 3], -1.f, 1.f) * UINT16_MAX);
// 						}
// 					} break;
// 					case cgltf_attribute_type_texcoord: {
// 						void* texcoords = ((char*)attribute.data->buffer_view->buffer->data + attribute.data->buffer_view->offset + attribute.data->offset);

// 						for (cgltf_size l = 0; l < attribute.data->count; l++) {
// 							attributes[l].u = (u16)(normalize(((float*)texcoords)[(l * 2) + 0], 0.f, 1.f) * UINT16_MAX);
// 							attributes[l].v = (u16)(normalize(((float*)texcoords)[(l * 2) + 1], 0.f, 1.f) * UINT16_MAX);
// 						}
// 					} break;
// 					default: {

// 					} break;
// 				}
// 			}

// 			if (indexed) {
// 				printf("[%s] = {\n"
// 					"\t\t.indexed = true,\n"
// 					"\t\t.indexCount = %zu,\n"
// 					"\t\t.firstIndex = %zu,\n"
// 					"\t\t.vertexOffset = %zu,\n"
// 					"\t\t.min = { %ff, %ff, %ff },\n"
// 					"\t\t.max = { %ff, %ff, %ff }\n"
// 					"\t}, ", mesh.name, indexCount, indexOffset / sizeof(u16), vertexOffset, (double)miX, (double)miY, (double)miZ, (double)maX, (double)maY, (double)maZ);
// 			} else {
// 				printf("[%s] = {\n"
// 					"\t\t.indexed = false,\n"
// 					"\t\t.vertexCount = %zu,\n"
// 					"\t\t.firstVertex = %zu,\n"
// 					"\t\t.min = { %ff, %ff, %ff },\n"
// 					"\t\t.max = { %ff, %ff, %ff }\n"
// 					"\t}, ", mesh.name, vertexCount, firstVertex, (double)miX, (double)miY, (double)miZ, (double)maX, (double)maY, (double)maZ);
// 			}

// 			fwrite(attributes, sizeof(struct VertexAttributes), vertexCount, attributes_bin);
// 		}
// 	}

// 	for (cgltf_size i = 0, k = 0; i < data->meshes_count; i++) {
// 		cgltf_mesh mesh = data->meshes[i];

// 		printf("[%s] = {\n"
// 		"\t\t.count = %zu,\n"
// 		"\t\t.elements = (struct MeshElement[]){\n\t\t\t", mesh.name, mesh.primitives_count);

// 		for (cgltf_size j = 0; j < mesh.primitives_count; j++) {
// 			cgltf_primitive primitive = mesh.primitives[j];

// 			printf("{\n\
// 				.primitive = %zu,\n\
// 				.material = %zu\n\
// 			}, ", 1 + k, primitive.material - data->materials);
// 			k++;
// 		}
// 	}

// 	// fprintf(assets_h, "static struct Node nodes[] = {\n\t");

// 	// for (cgltf_size i = 0; i < data->scenes_count; i++) {
// 	// 	cgltf_scene scene = data->scenes[i];

// 	// 	for (cgltf_size j = 0; j < scene.nodes_count; j++) {
// 	// 		cgltf_node* node = scene.nodes[j];

// 	// 		writeNode(node, 0);
// 	// 	}
// 	// 	fseek(assets_h, -2, SEEK_CUR);
// 	// 	fprintf(assets_h, "\n");
// 	// }
// 	// fseek(assets_h, -2, SEEK_CUR);
// 	// fprintf(assets_h, "\n};\n");

// 	ExitProcess(EXIT_SUCCESS);
}
