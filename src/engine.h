#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ucrt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "vcruntime.lib")

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#define VK_ENABLE_BETA_EXTENSIONS
#define STRICT
#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "vulkan/vulkan.h"
#include <windowsx.h>
#include "hidusage.h"
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <stdio.h>

#include "math.h"

#define FRAMES_IN_FLIGHT 2

#define BEFORE_INSTANCE_FUNCS(F) \
	F(vkCreateInstance)

#define INSTANCE_FUNCS(F) \
	F(vkCreateDevice) \
	F(vkCreateWin32SurfaceKHR) \
	F(vkEnumeratePhysicalDevices) \
	F(vkGetDeviceProcAddr) \
	F(vkGetPhysicalDeviceFeatures) \
	F(vkGetPhysicalDeviceMemoryProperties) \
	F(vkGetPhysicalDeviceProperties) \
	F(vkGetPhysicalDeviceQueueFamilyProperties) \
	F(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
	F(vkGetPhysicalDeviceSurfaceFormatsKHR) \
	F(vkGetPhysicalDeviceSurfaceSupportKHR)

#define DEVICE_FUNCS(F) \
	F(vkAcquireNextImageKHR) \
	F(vkAllocateCommandBuffers) \
	F(vkAllocateDescriptorSets) \
	F(vkAllocateMemory) \
	F(vkBeginCommandBuffer) \
	F(vkBindBufferMemory) \
	F(vkBindImageMemory) \
	F(vkCmdBeginRenderPass) \
	F(vkCmdBindDescriptorSets) \
	F(vkCmdBindIndexBuffer) \
	F(vkCmdBindPipeline) \
	F(vkCmdBindVertexBuffers) \
	F(vkCmdCopyBuffer) \
	F(vkCmdCopyBufferToImage) \
	F(vkCmdDraw) \
	F(vkCmdDrawIndirect) \
	F(vkCmdDrawIndexed) \
	F(vkCmdDrawIndexedIndirect) \
	F(vkCmdEndRenderPass) \
	F(vkCmdPipelineBarrier) \
	F(vkCmdPushConstants) \
	F(vkCmdSetStencilReference) \
	F(vkCreateBuffer) \
	F(vkCreateCommandPool) \
	F(vkCreateComputePipelines) \
	F(vkCreateDescriptorPool) \
	F(vkCreateDescriptorSetLayout) \
	F(vkCreateFence) \
	F(vkCreateFramebuffer) \
	F(vkCreateGraphicsPipelines) \
	F(vkCreateImage) \
	F(vkCreateImageView) \
	F(vkCreatePipelineCache) \
	F(vkCreatePipelineLayout) \
	F(vkCreateRenderPass) \
	F(vkCreateSampler) \
	F(vkCreateSemaphore) \
	F(vkCreateShaderModule) \
	F(vkCreateSwapchainKHR) \
	F(vkEndCommandBuffer) \
	F(vkGetBufferMemoryRequirements) \
	F(vkGetDeviceQueue) \
	F(vkGetImageMemoryRequirements) \
	F(vkGetPipelineCacheData) \
	F(vkGetSwapchainImagesKHR) \
	F(vkMapMemory) \
	F(vkQueuePresentKHR) \
	F(vkQueueSubmit) \
	F(vkResetCommandPool) \
	F(vkResetFences) \
	F(vkUpdateDescriptorSets) \
	F(vkWaitForFences)

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

enum AnimationTarget : u16 {
	ANIMATION_TARGET_TRANSLATION,
	ANIMATION_TARGET_ROTATION,
	ANIMATION_TARGET_SCALE
};

enum AnimationInterpolation : u16 {
	ANIMATION_INTERPOLATION_STEP,
	ANIMATION_INTERPOLATION_LINEAR,
	ANIMATION_INTERPOLATION_CUBIC_SPLINE
};

struct AnimationChannel {
	enum AnimationTarget target;
	u16 node;
	u16 sampler;
};

struct AnimationSampler {
	float* input;
	void* output;
	enum AnimationInterpolation interpolation;
};

struct Animation {
	u16* channels;
	u16* samplers;
	u16 channelCount;
	u16 samplerCount;
};

struct VertexPosition {
	u16 x, y, z;
};

struct VertexAttributes {
	u16 nx, ny, nz;
	u16 tx, ty, tz, tw;
	u16 u, v;
};

INCBIN(icons, "icons");
INCBIN(textures, "textures");
INCBIN(geometry, "geometry");
INCBIN(shaders, "shaders.spv");

enum BufferRange {
	BUFFER_RANGE_QUAD_INDICES = 6 * 12288 * sizeof(u16),
	BUFFER_RANGE_VERTEX_INDICES = 0 * sizeof(u16),
	BUFFER_RANGE_VERTEX_POSITIONS = 660 * sizeof(struct VertexPosition),
	BUFFER_RANGE_VERTEX_ATTRIBUTES = 660 * sizeof(struct VertexAttributes),

	BUFFER_RANGE_MODEL_MATRICES = 16 * 1024 * sizeof(mat4),
	BUFFER_RANGE_INDICES_2D = 1024 * 1024,
	BUFFER_RANGE_VERTICES_2D = 1024 * 1024,
	BUFFER_RANGE_TEXT = 1024 * 1024,
};

enum BufferOffset {
	BUFFER_OFFSET_QUAD_INDICES = 0,
	BUFFER_OFFSET_VERTEX_INDICES = BUFFER_OFFSET_QUAD_INDICES + BUFFER_RANGE_QUAD_INDICES,
	BUFFER_OFFSET_VERTEX_POSITIONS = BUFFER_OFFSET_VERTEX_INDICES + BUFFER_RANGE_VERTEX_INDICES,
	BUFFER_OFFSET_VERTEX_ATTRIBUTES = BUFFER_OFFSET_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_POSITIONS,

	BUFFER_OFFSET_MODEL_MATRICES = 0,
	BUFFER_OFFSET_INDICES_2D = BUFFER_OFFSET_MODEL_MATRICES + FRAMES_IN_FLIGHT * BUFFER_RANGE_MODEL_MATRICES,
	BUFFER_OFFSET_VERTICES_2D = BUFFER_OFFSET_INDICES_2D + FRAMES_IN_FLIGHT * BUFFER_RANGE_INDICES_2D,
	BUFFER_OFFSET_TEXT = BUFFER_OFFSET_VERTICES_2D + FRAMES_IN_FLIGHT * BUFFER_RANGE_VERTICES_2D,
};

int _fltused;

#define F(fn) static PFN_##fn fn;
BEFORE_INSTANCE_FUNCS(F)
INSTANCE_FUNCS(F)
DEVICE_FUNCS(F)

static VkCommandBuffer commandBuffer;
static VkPipelineLayout pipelineLayout;
static VkSurfaceCapabilitiesKHR surfaceCapabilities;

#define windowWidth surfaceCapabilities.currentExtent.width
#define windowHeight surfaceCapabilities.currentExtent.height

static VkMemoryRequirements memoryRequirements;
static VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
static VkPhysicalDeviceFeatures physicalDeviceFeatures;
static VkPhysicalDeviceProperties physicalDeviceProperties;

static HWND hwnd;
static HANDLE cursor;
static HANDLE cursorNormal;
static HANDLE cursorHand;
static HANDLE cursorBeam;

static SOCKET sock;
static struct sockaddr_in serverAddress;
static struct sockaddr_storage gameHostAddress;
static bool isHost;

static u32 msElapsed;
static float deltaTime;
static u32 ticksElapsed;
static u32 frame;

static POINT mouse;
static BOOL lButtonDown;
static BOOL rButtonDown;
static BOOL mButtonDown;
static BOOL xButton1Down;
static BOOL xButton2Down;
static BOOL cursorLocked;

static bool keys[0xFF];

enum Pipelines : u8 {
	PIPELINE_IMAGE2D,
	PIPELINE_PATH2D,
	PIPELINE_PATH2D_CLIP,
	PIPELINE_TEXT,
	PIPELINE_TRIANGLE,
	PIPELINE_SKYBOX,
	PIPELINE_MAX_ENUM
};

static VkPipeline pipelines[PIPELINE_MAX_ENUM];

static inline u32 vkGetMemoryTypeIndex(VkMemoryPropertyFlags required, VkMemoryPropertyFlags optional) {
	for (u32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && BITS_SET(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags, required | optional))
			return i;

	for (u32 i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && BITS_SET(physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags, required))
			return i;

	__builtin_unreachable();
}

static inline const char* vkResultToString(VkResult r) {
	switch (r) {
		#define CASE(r) case r: return #r
		CASE(VK_SUCCESS);
		CASE(VK_NOT_READY);
		CASE(VK_TIMEOUT);
		CASE(VK_EVENT_SET);
		CASE(VK_EVENT_RESET);
		CASE(VK_INCOMPLETE);
		CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
		CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
		CASE(VK_ERROR_INITIALIZATION_FAILED);
		CASE(VK_ERROR_DEVICE_LOST);
		CASE(VK_ERROR_MEMORY_MAP_FAILED);
		CASE(VK_ERROR_LAYER_NOT_PRESENT);
		CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
		CASE(VK_ERROR_FEATURE_NOT_PRESENT);
		CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
		CASE(VK_ERROR_TOO_MANY_OBJECTS);
		CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
		CASE(VK_ERROR_FRAGMENTED_POOL);
		CASE(VK_ERROR_UNKNOWN);
		CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
		CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
		CASE(VK_ERROR_FRAGMENTATION);
		CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
		CASE(VK_PIPELINE_COMPILE_REQUIRED);
		CASE(VK_ERROR_SURFACE_LOST_KHR);
		CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
		CASE(VK_SUBOPTIMAL_KHR);
		CASE(VK_ERROR_OUT_OF_DATE_KHR);
		CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
		CASE(VK_ERROR_VALIDATION_FAILED_EXT);
		CASE(VK_ERROR_INVALID_SHADER_NV);
		CASE(VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR);
		CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
		CASE(VK_ERROR_NOT_PERMITTED_KHR);
		CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
		CASE(VK_THREAD_IDLE_KHR);
		CASE(VK_THREAD_DONE_KHR);
		CASE(VK_OPERATION_DEFERRED_KHR);
		CASE(VK_OPERATION_NOT_DEFERRED_KHR);
		CASE(VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR);
		CASE(VK_ERROR_COMPRESSION_EXHAUSTED_EXT);
		CASE(VK_ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
		default: __builtin_unreachable();
	}
}

__attribute__((noreturn)) static inline void vkFatal(const char* fn, VkResult r) {
	char buff[1024];
	__builtin_sprintf(buff, "%s: %s\n", fn, vkResultToString(r));
	MessageBoxA(NULL, buff, NULL, MB_OK | MB_ICONEXCLAMATION);
	ExitProcess(EXIT_FAILURE);
}

__attribute__((noreturn)) static inline void win32Fatal(const char* fn, DWORD err) {
	char buff[1024];
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), buff, sizeof(buff), NULL)) {
		char buff2[1024];
		__builtin_sprintf(buff2, "%s: %s", fn, buff);
		MessageBoxA(NULL, buff2, NULL, MB_OK | MB_ICONEXCLAMATION);
	}

	ExitProcess(EXIT_FAILURE);
}
