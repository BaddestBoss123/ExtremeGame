#include "engine.h"

static struct {
	u8vec4 black;
	u8vec4 white;
	u8vec4 gray; // settings button
	u8vec4 red; // exit button
	u8vec4 yellow; // menu buttons
	u8vec4 green; // ok/ready button
	u8vec4 discordBlue;
} colors = {
	.black = { 0, 0, 0, 255 },
	.white = { 255, 255, 255, 255 },
	.gray = { 178, 178, 178, 255 },
	.red = { 187, 85, 85, 255 },
	.yellow = { 209, 203, 29, 255 },
	.green = { 29, 209, 41, 255 },
	.discordBlue = { 88, 101, 242, 255 }
};

struct Buffer {
	VkBuffer handle;
	VkDeviceSize size;
	VkBufferUsageFlags usage;
	VkMemoryPropertyFlags requiredMemoryPropertyFlagBits;
	VkMemoryPropertyFlags optionalMemoryPropertyFlagBits;
	void* data;
};

enum Buffers : u8 {
	BUFFER_STAGING,
	BUFFER_DEVICE,
	BUFFER_FRAME
};

static struct Buffer buffers[] = {
	[BUFFER_STAGING] = {
		.size = 128 * 1024 * 1024,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	}, [BUFFER_DEVICE] = {
		.size = BUFFER_RANGE_QUAD_INDICES + BUFFER_RANGE_VERTEX_INDICES + BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_ATTRIBUTES,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	}, [BUFFER_FRAME] = {
		.size = FRAMES_IN_FLIGHT * (BUFFER_RANGE_MODEL_MATRICES + BUFFER_RANGE_INDICES_2D + BUFFER_RANGE_VERTICES_2D + BUFFER_RANGE_TEXT),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.requiredMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		.optionalMemoryPropertyFlagBits = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	}
};

struct Image {
	VkImage handle;
	VkFormat format;
	u16 width, height;
	u16 mipLevels, arrayLayers;
};

enum Images : u8 {
	IMAGE_FONT,
	IMAGE_SKYBOX,
	IMAGE_DEFAULT,
	IMAGE_ICONS,
	IMAGE_TEXTURES,
};

static struct Image images[] = {
	[IMAGE_FONT] = {
		.format = VK_FORMAT_R8_UNORM,
		.width = 2048,
		.height = 2048,
		.mipLevels = 1,
		.arrayLayers = 1
	}, [IMAGE_SKYBOX] = {
		.format = VK_FORMAT_BC6H_UFLOAT_BLOCK,
		.width = 2048,
		.height = 2048,
		.mipLevels = 1,
		.arrayLayers = 6
	}, [IMAGE_DEFAULT] = {
		.format = VK_FORMAT_R32G32_SFLOAT,
		.width = 1,
		.height = 1,
		.mipLevels = 1,
		.arrayLayers = 1
	}, [IMAGE_ICONS] = {
		.format = VK_FORMAT_BC4_UNORM_BLOCK,
		.width = 48,
		.height = 48,
		.mipLevels = 1,
		.arrayLayers = 4
	}, [IMAGE_TEXTURES] = {
		.format = VK_FORMAT_BC7_UNORM_BLOCK,
		.width = 1024,
		.height = 1024,
		.mipLevels = 1,
		.arrayLayers = 1
	}
};

struct ImageView {
	enum Images image;
	u16 baseArrayLayer;
	u16 layerCount;
	VkComponentMapping components;
};

enum ImageViews : u16 {
	IMAGE_VIEW_FONT,
	IMAGE_VIEW_SKYBOX,
	IMAGE_VIEW_BLACK,
	IMAGE_VIEW_WHITE,
	IMAGE_VIEW_NORMAL,
	IMAGE_VIEW_GEAR,
	IMAGE_VIEW_DISCORD,
	IMAGE_VIEW_UNDISPUTED,
};

static struct ImageView imageViews[] = {
	[IMAGE_VIEW_FONT] = {
		.image = IMAGE_FONT
	}, [IMAGE_VIEW_SKYBOX] = {
		.image = IMAGE_SKYBOX,
		.layerCount = 6
	}, [IMAGE_VIEW_BLACK] = {
		.image = IMAGE_DEFAULT,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_ZERO,
			.g = VK_COMPONENT_SWIZZLE_ZERO,
			.b = VK_COMPONENT_SWIZZLE_ZERO,
			.a = VK_COMPONENT_SWIZZLE_ONE
		}
	}, [IMAGE_VIEW_WHITE] = {
		.image = IMAGE_DEFAULT,
		.components = {
			.r = VK_COMPONENT_SWIZZLE_ONE,
			.g = VK_COMPONENT_SWIZZLE_ONE,
			.b = VK_COMPONENT_SWIZZLE_ONE,
			.a = VK_COMPONENT_SWIZZLE_ONE
		}
	}, [IMAGE_VIEW_NORMAL] = {
		.image = IMAGE_DEFAULT,
		.components = {
			.g = VK_COMPONENT_SWIZZLE_R
		}
	}, [IMAGE_VIEW_GEAR] = {
		.image = IMAGE_ICONS,
		.baseArrayLayer = 0,
		.components = {
			.g = VK_COMPONENT_SWIZZLE_R,
			.b = VK_COMPONENT_SWIZZLE_R,
			.a = VK_COMPONENT_SWIZZLE_R
		}
	}, [IMAGE_VIEW_DISCORD] = {
		.image = IMAGE_ICONS,
		.baseArrayLayer = 1,
		.components = {
			.g = VK_COMPONENT_SWIZZLE_R,
			.b = VK_COMPONENT_SWIZZLE_R,
			.a = VK_COMPONENT_SWIZZLE_R
		}
	}, [IMAGE_VIEW_UNDISPUTED] = {
		.image = IMAGE_TEXTURES,
		.baseArrayLayer = 0
	}
};

struct Material {
	u8vec4 rgba;
	enum ImageViews color;
	enum ImageViews normal;
	enum ImageViews metalRough;
};

struct ShaderMaterial {
	vec4 color;
	mat3 normalMatrix;
	enum ImageViews colorIndex;
	enum ImageViews normalIndex;
};

enum Materials : u8 {
	MATERIAL_PURPLE,
	MATERIAL_ROCK,
	MATERIAL_WOOD_DARK,
	MATERIAL_METAL
};

static struct Material materials[] = {
	[MATERIAL_PURPLE] = {
		.rgba = { 127, 0, 127, 255 },
		.color = IMAGE_VIEW_WHITE,
		.normal = IMAGE_VIEW_NORMAL,
		.metalRough = IMAGE_VIEW_BLACK
	}, [MATERIAL_ROCK] = {
		.rgba = { 102, 109, 135, 255 },
		.color = IMAGE_VIEW_WHITE,
		.normal = IMAGE_VIEW_NORMAL,
		.metalRough = IMAGE_VIEW_BLACK
	}, [MATERIAL_WOOD_DARK] = {
		.rgba = { 89, 31, 10, 255 },
		.color = IMAGE_VIEW_WHITE,
		.normal = IMAGE_VIEW_NORMAL,
		.metalRough = IMAGE_VIEW_BLACK
	}, [MATERIAL_METAL] = {
		.rgba = { 147, 157, 195, 255 },
		.color = IMAGE_VIEW_WHITE,
		.normal = IMAGE_VIEW_NORMAL,
		.metalRough = IMAGE_VIEW_BLACK
	}
};

struct Primitive {
	vec3 min;
	vec3 max;
	union {
		struct {
			u32 indexCount;
			u32 firstIndex;
			i32 vertexOffset;
		};
		struct {
			u32 vertexCount;
			u32 firstVertex;
		};
	};
	bool indexed;
};

enum Primitives : u8 {
	PRIMITIVE_QUAD,
	PRIMITIVE_CUBE,
	PRIMITIVE_ICOSAHEDRON,
	PRIMITIVE_TORUS
};

static struct Primitive primitives[] = {
	[PRIMITIVE_QUAD] = {
		.min = { -1.f, 0.f, -1.f },
		.max = { 1.f, 0.f, 1.f },
		.indexCount = 6,
		.firstIndex = 0,
		.vertexOffset = 0,
		.indexed = true
	}, [PRIMITIVE_CUBE] = {
		.min = { -0.5f, -0.5f, -0.5f },
		.max = { 0.5f, 0.5f, 0.5f },
		.indexCount = 36,
		.firstIndex = BUFFER_OFFSET_QUAD_INDICES / sizeof(uint16_t),
		.vertexOffset = 4,
		.indexed = true
	}, [PRIMITIVE_ICOSAHEDRON] = {
		.min = { -1.618034f, -1.618034f, -1.618034f },
		.max = { 1.618034f, 1.618034f, 1.618034f },
		.vertexCount = 60,
		.firstVertex = 28,
		.indexed = false
	}, [PRIMITIVE_TORUS] = {
		.min = { -1.300000f, -1.300000f, -0.300000f },
		.max = { 1.300000f, 1.300000f, 0.300000f },
		.indexCount = 864,
		.firstIndex = BUFFER_OFFSET_QUAD_INDICES / sizeof(uint16_t),
		.vertexOffset = 88,
		.indexed = true
	}
};

struct MeshElement {
	enum Primitives primitive;
	enum Materials material;
};

struct Mesh {
	u16 count;
	struct MeshElement* elements;
};

enum Meshes : u8 {
	MESH_PURPLE_CUBE,
	MESH_BARREL
};

static struct Mesh meshes[] = {
	[MESH_PURPLE_CUBE] = {
		.count = 1,
		.elements = &(struct MeshElement){
			.primitive = PRIMITIVE_CUBE,
			.material = MATERIAL_PURPLE
		},
	}
};

struct Node {
	vec3 translation;
	quat rotation;
	vec3 scale;
	u16 mesh;
	u16 childCount;
	u16 firstChild;
};

enum Nodes : u8 {
	NODE_PURPLE_CUBE
};

static struct Node nodes[] = {
	[NODE_PURPLE_CUBE] = {
		.translation = { 0.000000f, 0.000000f, 0.000000f },
		.rotation = { 0.000000f, 0.000000f, 0.000000f, 1.000000f },
		.scale = { 1.000000f, 1.000000f, 1.000000f },
		.mesh = MESH_PURPLE_CUBE,
	}
};
