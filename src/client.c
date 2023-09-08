#define TICKS_PER_SECOND 50

#include "gui.h"

static mat4 projection;

static mat4* modelMatrices;
static u32 modelMatricesCount;

static struct {
	float pitch;
	float yaw;
	vec3 position;
	vec3 right;
	vec3 up;
	vec3 forward;
} camera = {
	.right = { 1.f, 0.f, 0.f },
	.up = { 0.f, 1.f, 0.f },
	.forward = { 0.f, 0.f, 1.f }
};

struct PlayingSound {
	u16 sound;
	u16 left, right;
	u16 samplesPlayed;

	u8 next;
};

static struct {
	struct PlayingSound* data;
	u8 firstFree;
	u8 head;
	u8 count;
} playingSounds = {
	.data = (struct PlayingSound[UINT8_MAX]){ },
	.firstFree = (u8)-1,
	.head = (u8)-1
};

struct Client {
	u16 family;
	union {
		char v4[4];
		char v6[16];
	} ip;
	u16 port;

	char name[32];

	u16 entity;

	u16 next;
};

static struct {
	struct Client* data;
	u16 firstFree;
	u16 head;
	u16 count;
} clients = {
	.data = (struct Client[UINT16_MAX]){ },
	.firstFree = (u16)-1,
	.head = (u16)-1
};

static HANDLE saveFile;

static inline void drawScene(vec3 cameraPosition, vec3 right, vec3 up, vec3 forward, u32 stencilReference) {
	mat4 view;
	view[0][0] = right.x;
	view[1][0] = up.x;
	view[2][0] = forward.x;
	view[3][0] = 0.f;
	view[0][1] = right.y;
	view[1][1] = up.y;
	view[2][1] = forward.y;
	view[3][1] = 0.f;
	view[0][2] = right.z;
	view[1][2] = up.z;
	view[2][2] = forward.z;
	view[3][2] = 0.f;
	view[0][3] = -vec3Dot(right, cameraPosition);
	view[1][3] = -vec3Dot(up, cameraPosition);
	view[2][3] = -vec3Dot(forward, cameraPosition);
	view[3][3] = 1.f;

	mat4 viewProjection = projection * view;

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4) + sizeof(vec4), &(struct { mat4 viewProjection; vec4 clippingPlane; }){
		viewProjection,
		(vec4){ 0.f, 0.f, 0.f, 0.f }
	});

	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 80, sizeof(vec3), &cameraPosition);

	vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, stencilReference);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_TRIANGLE]);
	vkCmdBindIndexBuffer(commandBuffer, buffers[BUFFER_DEVICE].handle, 0, VK_INDEX_TYPE_UINT16);

	for (u16 i = 0; i < _countof(primitives); i++) {
		struct Primitive* primitive = &primitives[i];
		struct Material* material = &materials[i];

		mat4 transform = mat4FromRotationTranslationScale((quat){ 0.f, 0.f, 0.f, 1.f }, (vec3){ i * 10.f, 0.f, -10.f }, (vec3){ 1.f, 1.f, 1.f });
		modelMatrices[modelMatricesCount] = mat4TranslateScale(transform, primitive->min, primitive->max - primitive->min);

		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(struct ShaderMaterial), &(struct ShaderMaterial){
			.color = { material->rgba.r / 255.f, material->rgba.g / 255.f, material->rgba.b / 255.f, material->rgba.a / 255.f },
			.normalMatrix = mat3Adjoint(mat3FromMat4(transform)),
			.colorIndex = material->color,
			.normalIndex = material->normal
		});

		if (primitive->indexed)
			vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, primitive->vertexOffset, modelMatricesCount);
		else
			vkCmdDraw(commandBuffer, primitive->vertexCount, 1, primitive->firstVertex, modelMatricesCount);

		modelMatricesCount++;
	}


	u16 idx = entities.head;
	while (idx != (u16)-1) {
		struct Entity* entity = &entities.data[idx];

		quat pitchQuat = quatFromAxisAngle((vec3){ 1.f, 0.f, 0.f }, -entity->pitch);
		quat yawQuat = quatFromAxisAngle((vec3){ 0.f, 1.f, 0.f }, -entity->yaw);
		quat rotation = quatMultiply(yawQuat, pitchQuat);

		vec3 position = (vec3)(entity->position >> 16);

		struct Node* node = &nodes[NODE_PURPLE_CUBE];
		struct Node* tree[64];
		u16 childOffsets[64];
		mat4 transforms[64];
		u16 depth = 0;

		transforms[0] = mat4FromRotationTranslationScale(rotation, position, (vec3){ entity->radius, entity->radius, entity->radius });

		for (;;) {
			transforms[depth + 1] = transforms[depth] * mat4FromRotationTranslationScale(node->rotation, node->translation, node->scale);

			if (node->mesh != (u16)-1) {
				struct Mesh* mesh = &meshes[node->mesh];

				for (u16 i = 0; i < mesh->count; i++) {
					struct Primitive* primitive = &primitives[mesh->elements[i].primitive];
					struct Material* material = &materials[mesh->elements[i].material];

					modelMatrices[modelMatricesCount] = mat4TranslateScale(transforms[depth + 1], primitive->min, primitive->max - primitive->min);

					vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 96, sizeof(struct ShaderMaterial), &(struct ShaderMaterial){
						.color = { material->rgba.r / 255.f, material->rgba.g / 255.f, material->rgba.b / 255.f, material->rgba.a / 255.f },
						.normalMatrix = mat3Adjoint(mat3FromMat4(transforms[depth])),
						.colorIndex = material->color,
						.normalIndex = material->normal
					});

					if (primitive->indexed)
						vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, primitive->vertexOffset, modelMatricesCount);
					else
						vkCmdDraw(commandBuffer, primitive->vertexCount, 1, primitive->firstVertex, modelMatricesCount);

					modelMatricesCount++;
				}
			}

			if (node->childCount) {
				tree[depth] = node;
				childOffsets[depth++] = 0;
				node = &nodes[node->firstChild];
			} else {
				while (depth) {
					node = tree[--depth];
					if (++childOffsets[depth] < node->childCount) {
						node = &nodes[node->firstChild + childOffsets[depth++]];
						break;
					}
				}
				if (!depth)
					break;
			}
		}

		idx = entity->next;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[PIPELINE_SKYBOX]);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

static inline LRESULT CALLBACK wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_DESTROY:
			DWORD bytesWritten;
			if (!WriteFile(saveFile, &saves, sizeof(struct Saves), &bytesWritten, NULL))
				win32Fatal("WriteFile", GetLastError());

			PostQuitMessage(0);
			break;
		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE) {
				for (u8 i = 0; i < _countof(keys); i++)
					keys[i] = false;

				if (cursorLocked) {
					cursorLocked = FALSE;
					ShowCursor(TRUE);
				}
			}
			break;
		case WM_CLOSE:
			if (!DestroyWindow(hWnd))
				win32Fatal("DestroyWindow", GetLastError());
			break;
		case WM_INPUT:
			if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT) {
				if (cursorLocked) {
					RAWINPUT rawInput;
					GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &(UINT){ sizeof(RAWINPUT) }, sizeof(RAWINPUTHEADER));

					camera.pitch -= settings.mouseSensitivity * (float)rawInput.data.mouse.lLastY;
					camera.yaw -= settings.mouseSensitivity * (float)rawInput.data.mouse.lLastX;

					camera.pitch = fclampf(camera.pitch, -M_PI * 0.4f, M_PI * 0.3f);
					camera.yaw = normalizeAngle(camera.yaw);

					float sp = __builtin_sinf(camera.pitch);
					float cp = __builtin_cosf(camera.pitch);
					float sy = __builtin_sinf(camera.yaw);
					float cy = __builtin_cosf(camera.yaw);

					camera.right = (vec3){ cy, 0, -sy };
					camera.up = (vec3){ sy * sp, cp, cy * sp };
					camera.forward = (vec3){ sy * cp, -sp, cp * cy };
				}

				DefWindowProcW(hWnd, msg, wParam, lParam);
			}
			break;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			WORD vkCode = LOWORD(wParam);
			WORD repeatCount = LOWORD(lParam);
			WORD keyFlags = HIWORD(lParam);
			WORD scanCode = LOBYTE(keyFlags);

			BOOL isExtendedKey = (keyFlags & KF_EXTENDED) == KF_EXTENDED;
			BOOL altDown = (keyFlags & KF_ALTDOWN) == KF_ALTDOWN;
			BOOL wasKeyDown = (keyFlags & KF_REPEAT) == KF_REPEAT;
			BOOL isKeyReleased = (keyFlags & KF_UP) == KF_UP;

			if (isExtendedKey)
				scanCode = MAKEWORD(scanCode, 0xE0);

			if (vkCode == VK_SHIFT || vkCode == VK_CONTROL || vkCode == VK_MENU)
				vkCode = LOWORD(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));

			char keyNameText[512];
			if (!GetKeyNameTextA((LONG)lParam, keyNameText, sizeof(keyNameText)))
				win32Fatal("GetKeyNameTextA", GetLastError());

			keys[vkCode] = !isKeyReleased;

			if (!isKeyReleased)
				switch (wParam) {
					case VK_ESCAPE:
						if (cursorLocked) {
							cursorLocked = FALSE;
							ShowCursor(TRUE);
						}
						break;
					case VK_F4:
						if (altDown) {
							if (!isAnimating(&altF4Dialog)) {
								ANIMATE(altF4Dialog.position, ((i16vec2){ ((i16)windowWidth / 2) - 256, ((i16)windowHeight / 2) - 128 }), msElapsed);
								ANIMATE(altF4Dialog.scale, 255, msElapsed);
							}
						}
						break;
					case 'V':
						if (GetKeyState(VK_CONTROL) & 0x8000) {
							if (!OpenClipboard(NULL))
								win32Fatal("OpenClipboard", GetLastError());

							HANDLE hClip;
							if (!(hClip = GetClipboardData(CF_TEXT)))
								win32Fatal("GetClipboardData", GetLastError());

							char* cData;
							if (!(cData = GlobalLock(hClip)))
								win32Fatal("GlobalLock", GetLastError());

							MessageBoxA(NULL, cData, NULL, MB_OK);

							DWORD err;
							if (!GlobalUnlock(hClip) && !(err = GetLastError()))
								win32Fatal("GlobalUnlock", err);

							if (!CloseClipboard())
								win32Fatal("CloseClipboard", GetLastError());
						}
						break;
				}
			break;
		case WM_CHAR:
			TCHAR code = (TCHAR)wParam;

			if (focusedTextfield) {
				focusedTextfield->text[0] = code;
				focusedTextfield->text[1] = '\0';
			}

			break;
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			WORD keyState = GET_KEYSTATE_WPARAM(wParam);

			BOOL ctrlDown = (keyState & MK_CONTROL) == MK_CONTROL;
			BOOL shiftDown = (keyState & MK_SHIFT) == MK_SHIFT;

			lButtonDown = (keyState & MK_LBUTTON) == MK_LBUTTON;
			rButtonDown = (keyState & MK_RBUTTON) == MK_RBUTTON;
			mButtonDown = (keyState & MK_MBUTTON) == MK_MBUTTON;

			xButton1Down = (keyState & MK_XBUTTON1) == MK_XBUTTON1;
			xButton2Down = (keyState & MK_XBUTTON2) == MK_XBUTTON2;

			short zDelta = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;

			if (cursorLocked) {
				RECT clipRect;
				if (!GetWindowRect(hWnd, &clipRect))
					win32Fatal("GetWindowRect", GetLastError());

				if (!SetCursorPos((clipRect.left + clipRect.right) / 2, (clipRect.top + clipRect.bottom) / 2))
					win32Fatal("SetCursorPos", GetLastError());
			} else {
				mouse.x = GET_X_LPARAM(lParam);
				mouse.y = GET_Y_LPARAM(lParam);
			}

			if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP)
				return TRUE;

			break;
		default:
			return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	return 0;
}

__attribute__((noreturn)) void WinMainCRTStartup(void) {
	if (!SetProcessDPIAware())
		win32Fatal("SetProcessDPIAware", GetLastError());

	if (!(cursorNormal = LoadImageW(NULL, MAKEINTRESOURCEW(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_SHARED)))
		win32Fatal("LoadImageW", GetLastError());

	if (!(cursorHand = LoadImageW(NULL, MAKEINTRESOURCEW(OCR_HAND), IMAGE_CURSOR, 0, 0, LR_SHARED)))
		win32Fatal("LoadImageW", GetLastError());

	if (!(cursorBeam = LoadImageW(NULL, MAKEINTRESOURCEW(OCR_IBEAM), IMAGE_CURSOR, 0, 0, LR_SHARED)))
		win32Fatal("LoadImage", GetLastError());

	WNDCLASSEXW class = {
		.cbSize = sizeof(WNDCLASSEXW),
		.lpfnWndProc = wndProc,
		.hInstance = GetModuleHandleW(NULL),
		.lpszClassName = L"Extreme Game",
		.hCursor = cursorNormal
	};

	if (FindWindowW(class.lpszClassName, class.lpszClassName))
		ExitProcess(EXIT_FAILURE);

	if (!RegisterClassExW(&class))
		win32Fatal("RegisterClassExW", GetLastError());

	if (!(hwnd = CreateWindowW(class.lpszClassName, class.lpszClassName, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, NULL, NULL, class.hInstance, NULL)))
		win32Fatal("CreateWindowW", GetLastError());

	if (!RegisterRawInputDevices(&(RAWINPUTDEVICE){
		.usUsagePage = HID_USAGE_PAGE_GENERIC,
		.usUsage = HID_USAGE_GENERIC_MOUSE,
		.dwFlags = 0,
		.hwndTarget = hwnd
	}, 1, sizeof(RAWINPUTDEVICE)))
		win32Fatal("RegisterRawInputDevices", GetLastError());

	HMODULE vulkan;
	if (!(vulkan = LoadLibraryW(L"vulkan-1.dll")))
		win32Fatal("LoadLibraryW", GetLastError());

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(vulkan, "vkGetInstanceProcAddr");
	if (!vkGetInstanceProcAddr)
		win32Fatal("GetProcAddress", GetLastError());

	#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(NULL, #fn);
	BEFORE_INSTANCE_FUNCS(F)

	VkResult r;
	VkInstance instance;
	if ((r = vkCreateInstance(&(VkInstanceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &(VkApplicationInfo){
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.apiVersion = VK_API_VERSION_1_0
		},
		.enabledExtensionCount = 2,
		.ppEnabledExtensionNames = (const char*[]){
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME
		}
	}, NULL, &instance)) != VK_SUCCESS)
		vkFatal("vkCreateInstance", r);

	#define F(fn) fn = (PFN_##fn)vkGetInstanceProcAddr(instance, #fn);
	INSTANCE_FUNCS(F)

	VkSurfaceKHR surface;
	if ((r = vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance = class.hInstance,
		.hwnd = hwnd
	}, NULL, &surface)) != VK_SUCCESS)
		vkFatal("vkCreateWin32SurfaceKHR", r);

	VkPhysicalDevice physicalDevice;
	r = vkEnumeratePhysicalDevices(instance, &(u32){ 1 }, &physicalDevice);
	if (r != VK_SUCCESS && r != VK_INCOMPLETE)
		vkFatal("vkEnumeratePhysicalDevices", r);

	if ((r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) != VK_SUCCESS)
		vkFatal("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", r);

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

	VkSurfaceFormatKHR surfaceFormat;
	r = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &(u32){ 1 }, &surfaceFormat);
	if (r != VK_SUCCESS && r != VK_INCOMPLETE)
		vkFatal("vkGetPhysicalDeviceSurfaceFormatsKHR", r);

	VkQueueFamilyProperties queueFamilyProperties[16];
	u32 queueFamilyPropertyCount = _countof(queueFamilyProperties);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties);

	u32 queueFamilyIndex = 0;
	for (; queueFamilyIndex < queueFamilyPropertyCount; queueFamilyIndex++)
		if (BITS_SET(queueFamilyProperties[queueFamilyIndex].queueFlags, (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))) {
			VkBool32 presentSupport;
			if ((r = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &presentSupport)) != VK_SUCCESS)
				vkFatal("vkGetPhysicalDeviceSurfaceSupportKHR", r);

			if (presentSupport)
				break;
		}

	VkDevice device;
	if ((r = vkCreateDevice(physicalDevice, &(const VkDeviceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &(const VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = queueFamilyIndex,
			.queueCount = 1,
			.pQueuePriorities = &(const float){ 1.f }
		},
		.enabledExtensionCount = 1,
		.ppEnabledExtensionNames = (const char*[]){
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		},
		.pEnabledFeatures = &(VkPhysicalDeviceFeatures){
			.textureCompressionBC = physicalDeviceFeatures.textureCompressionBC,
			.depthClamp = physicalDeviceFeatures.depthClamp,
			.shaderClipDistance = physicalDeviceFeatures.shaderClipDistance,
			.shaderInt16 = physicalDeviceFeatures.shaderClipDistance
			// .samplerAnisotropy = physicalDeviceFeatures.samplerAnisotropy
		}
	}, NULL, &device)) != VK_SUCCESS)
		vkFatal("vkCreateDevice", r);

	#define F(fn) fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);
	DEVICE_FUNCS(F)
	#undef F

	VkRenderPass renderPass;
	if ((r = vkCreateRenderPass(device, &(VkRenderPassCreateInfo){
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 3,
		.pAttachments = (VkAttachmentDescription[]){
			{
				.format = surfaceFormat.format,
				.samples = VK_SAMPLE_COUNT_4_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			}, {
				.format = VK_FORMAT_D24_UNORM_S8_UINT,
				.samples = VK_SAMPLE_COUNT_4_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			}, {
				.format = surfaceFormat.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			}
		},
		.subpassCount = 1,
		.pSubpasses = &(VkSubpassDescription){
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &(VkAttachmentReference){
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			},
			.pDepthStencilAttachment = &(VkAttachmentReference){
				.attachment = 1,
				.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
			},
			.pResolveAttachments = &(VkAttachmentReference){
				.attachment = 2,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			}
		},
		.dependencyCount = 1,
		.pDependencies = &(VkSubpassDependency){
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		}
	}, NULL, &renderPass)) != VK_SUCCESS)
		vkFatal("vkCreateRenderPass", r);

	VkSwapchainKHR swapchain;
	if ((r = vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR){
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = surfaceCapabilities.minImageCount + 1,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = surfaceCapabilities.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR,
		.preTransform = surfaceCapabilities.currentTransform,
		.clipped = VK_TRUE
	}, NULL, &swapchain)) != VK_SUCCESS)
		vkFatal("vkCreateSwapchainKHR", r);

	VkImage swapchainImages[8];
	u32 swapchainImagesCount = _countof(swapchainImages);
	if ((r = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImagesCount, swapchainImages)) != VK_SUCCESS)
		vkFatal("vkGetSwapchainImagesKHR", r);

	VkImage colorImage;
	if ((r = vkCreateImage(device, &(VkImageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = surfaceFormat.format,
		.extent = {
			.width = windowWidth,
			.height = windowHeight,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_4_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
	}, NULL, &colorImage)) != VK_SUCCESS)
		vkFatal("vkCreateImage", r);

	vkGetImageMemoryRequirements(device, colorImage, &memoryRequirements);

	VkDeviceMemory colorImageDeviceMemory;
	if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = vkGetMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
	}, NULL, &colorImageDeviceMemory)) != VK_SUCCESS)
		vkFatal("vkAllocateMemory", r);

	if ((r = vkBindImageMemory(device, colorImage, colorImageDeviceMemory, 0)) != VK_SUCCESS)
		vkFatal("vkBindImageMemory", r);

	VkImageView colorImageView;
	if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = colorImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = surfaceFormat.format,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	}, NULL, &colorImageView)) != VK_SUCCESS)
		vkFatal("vkCreateImageView", r);

	VkImage depthImage;
	if ((r = vkCreateImage(device, &(VkImageCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_D24_UNORM_S8_UINT,
		.extent = {
			.width = windowWidth,
			.height = windowHeight,
			.depth = 1
		},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_4_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
	}, NULL, &depthImage)) != VK_SUCCESS)
		vkFatal("vkCreateImage", r);

	vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);

	VkDeviceMemory depthImageDeviceMemory;
	if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = vkGetMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
	}, NULL, &depthImageDeviceMemory)) != VK_SUCCESS)
		vkFatal("vkAllocateMemory", r);

	if ((r = vkBindImageMemory(device, depthImage, depthImageDeviceMemory, 0)) != VK_SUCCESS)
		vkFatal("vkBindImageMemory", r);

	VkImageView depthImageView;
	if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depthImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = VK_FORMAT_D24_UNORM_S8_UINT,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
			.levelCount = 1,
			.layerCount = 1
		}
	}, NULL, &depthImageView)) != VK_SUCCESS)
		vkFatal("vkCreateImageView", r);

	VkImageView swapchainImageViews[8];
	VkFramebuffer swapchainFramebuffers[8];
	for (u32 i = 0; i < swapchainImagesCount; i++) {
		if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surfaceFormat.format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		}, NULL, &swapchainImageViews[i])) != VK_SUCCESS)
			vkFatal("vkCreateImageView", r);

		if ((r = vkCreateFramebuffer(device, &(VkFramebufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = renderPass,
			.attachmentCount = 3,
			.pAttachments = (VkImageView[]){
				colorImageView,
				depthImageView,
				swapchainImageViews[i]
			},
			.width = windowWidth,
			.height = windowHeight,
			.layers = 1
		}, NULL, &swapchainFramebuffers[i])) != VK_SUCCESS)
			vkFatal("vkCreateFramebuffer", r);
	}

	for (u32 i = 0; i < _countof(buffers); i++) {
		if ((r = vkCreateBuffer(device, &(VkBufferCreateInfo){
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = buffers[i].size,
			.usage = buffers[i].usage
		}, NULL, &buffers[i].handle)) != VK_SUCCESS)
			vkFatal("vkCreateBuffer", r);

		vkGetBufferMemoryRequirements(device, buffers[i].handle, &memoryRequirements);

		VkDeviceMemory deviceMemory;
		if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = vkGetMemoryTypeIndex(buffers[i].requiredMemoryPropertyFlagBits, buffers[i].optionalMemoryPropertyFlagBits)
		}, NULL, &deviceMemory)) != VK_SUCCESS)
			vkFatal("vkAllocateMemory", r);

		if ((r = vkBindBufferMemory(device, buffers[i].handle, deviceMemory, 0)) != VK_SUCCESS)
			vkFatal("vkBindBufferMemory", r);

		if (buffers[i].requiredMemoryPropertyFlagBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			if ((r = vkMapMemory(device, deviceMemory, 0, VK_WHOLE_SIZE, 0, &buffers[i].data)) != VK_SUCCESS)
				vkFatal("vkMapMemory", r);
	}

	VkImageMemoryBarrier imageMemoryBarriers[_countof(images)];
	for (u32 i = 0; i < _countof(images); i++) {
		if ((r = vkCreateImage(device, &(VkImageCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = images[i].format,
			.extent = {
				.width = images[i].width,
				.height = images[i].height,
				.depth = 1
			},
			.mipLevels = images[i].mipLevels,
			.arrayLayers = images[i].arrayLayers,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
		}, NULL, &images[i].handle)) != VK_SUCCESS)
			vkFatal("vkCreateImage", r);

		vkGetImageMemoryRequirements(device, images[i].handle, &memoryRequirements);

		VkDeviceMemory deviceMemory;
		if ((r = vkAllocateMemory(device, &(VkMemoryAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = vkGetMemoryTypeIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0)
		}, NULL, &deviceMemory)) != VK_SUCCESS)
			vkFatal("vkAllocateMemory", r);

		if ((r = vkBindImageMemory(device, images[i].handle, deviceMemory, 0)) != VK_SUCCESS)
			vkFatal("vkBindImageMemory", r);

		imageMemoryBarriers[i] = (VkImageMemoryBarrier){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_NONE,
			.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = images[i].handle,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = images[i].mipLevels,
				.baseArrayLayer = 0,
				.layerCount = images[i].arrayLayers
			}
		};
	}

	VkDescriptorImageInfo descriptorImageInfos[_countof(imageViews)];
	for (u32 i = 0; i < _countof(imageViews); i++) {
		if ((r = vkCreateImageView(device, &(VkImageViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = images[imageViews[i].image].handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = images[imageViews[i].image].format,
			.components = imageViews[i].components,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = images[imageViews[i].image].mipLevels,
				.baseArrayLayer = imageViews[i].baseArrayLayer,
				.layerCount = imageViews[i].layerCount
			}
		}, NULL, &descriptorImageInfos[i].imageView)) != VK_SUCCESS)
			vkFatal("vkCreateImageView", r);

		descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkSampler sampler;
	if ((r = vkCreateSampler(device, &(VkSamplerCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO
	}, NULL, &sampler)) != VK_SUCCESS)
		vkFatal("vkCreateSampler", r);

	VkSampler samplerLinear;
	if ((r = vkCreateSampler(device, &(VkSamplerCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR
	}, NULL, &samplerLinear)) != VK_SUCCESS)
		vkFatal("vkCreateSampler", r);

	VkDescriptorSetLayout descriptorSetLayout;
	if ((r = vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = _countof(descriptorImageInfos),
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = (VkSampler[]){
					[IMAGE_VIEW_FONT] = sampler,
					[IMAGE_VIEW_SKYBOX] = sampler,
					[IMAGE_VIEW_BLACK] = sampler,
					[IMAGE_VIEW_WHITE] = sampler,
					[IMAGE_VIEW_NORMAL] = sampler,
					[IMAGE_VIEW_GEAR] = samplerLinear,
					[IMAGE_VIEW_DISCORD] = samplerLinear,
					[IMAGE_VIEW_UNDISPUTED] = samplerLinear,
				}
			}
		}
	}, NULL, &descriptorSetLayout)) != VK_SUCCESS)
		vkFatal("vkCreateDescriptorSetLayout", r);

	VkDescriptorPool descriptorPool;
	if ((r = vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = (VkDescriptorPoolSize[]){
			{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = _countof(descriptorImageInfos)
			}
		}
	}, NULL, &descriptorPool)) != VK_SUCCESS)
		vkFatal("vkCreateDescriptorPool", r);

	VkDescriptorSet descriptorSet;
	if ((r = vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &descriptorSetLayout
	}, &descriptorSet)) != VK_SUCCESS)
		vkFatal("vkAllocateDescriptorSets", r);

	vkUpdateDescriptorSets(device, 1, (VkWriteDescriptorSet[]){
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = descriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = _countof(descriptorImageInfos),
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = descriptorImageInfos
		}
	}, 0, NULL);

	if ((r = vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &descriptorSetLayout,
		.pushConstantRangeCount = 2,
		.pPushConstantRanges = (VkPushConstantRange[]){
			{
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
				.offset = 0,
				.size = 80
			}, {
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.offset = 80,
				.size = 88
			}
		}
	}, NULL, &pipelineLayout)) != VK_SUCCESS)
		vkFatal("vkCreatePipelineLayout", r);

	VkShaderModule shaderModule;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = (u32*)&incbin_shaders_start,
		.codeSize = (char*)&incbin_shaders_end - (char*)&incbin_shaders_start
	}, NULL, &shaderModule)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "image2d_vert.h"
	VkShaderModule image2dVert;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = image2d_vert,
		.codeSize = sizeof(image2d_vert)
	}, NULL, &image2dVert)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "image2d_frag.h"
	VkShaderModule image2dFrag;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = image2d_frag,
		.codeSize = sizeof(image2d_frag)
	}, NULL, &image2dFrag)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "path2d_frag.h"
	VkShaderModule path2dFrag;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = path2d_frag,
		.codeSize = sizeof(path2d_frag)
	}, NULL, &path2dFrag)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "text_vert.h"
	VkShaderModule textVert;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = text_vert,
		.codeSize = sizeof(text_vert)
	}, NULL, &textVert)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "text_frag.h"
	VkShaderModule textFrag;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = text_frag,
		.codeSize = sizeof(text_frag)
	}, NULL, &textFrag)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "triangle_vert.h"
	VkShaderModule triangleVert;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = triangle_vert,
		.codeSize = sizeof(triangle_vert)
	}, NULL, &triangleVert)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "triangle_frag.h"
	VkShaderModule triangleFrag;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = triangle_frag,
		.codeSize = sizeof(triangle_frag)
	}, NULL, &triangleFrag)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "skybox_vert.h"
	VkShaderModule skyboxVert;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = skybox_vert,
		.codeSize = sizeof(skybox_vert)
	}, NULL, &skyboxVert)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	#include "skybox_frag.h"
	VkShaderModule skyboxFrag;
	if ((r = vkCreateShaderModule(device, &(VkShaderModuleCreateInfo){
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pCode = skybox_frag,
		.codeSize = sizeof(skybox_frag)
	}, NULL, &skyboxFrag)) != VK_SUCCESS)
		vkFatal("vkCreateShaderModule", r);

	VkPipelineShaderStageCreateInfo shaderStagesImage2d[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = image2dVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = image2dFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesPath2d[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName = "path2d_vert"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = path2dFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesPath2dClip[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = shaderModule,
			.pName = "path2d_vert"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = shaderModule,
			.pName = "empty_frag"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesTextsdf[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = textVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = textFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesTriangle[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = triangleVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = triangleFrag,
			.pName = "main"
		}
	};

	VkPipelineShaderStageCreateInfo shaderStagesSkybox[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = skyboxVert,
			.pName = "main"
		}, {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = skyboxFrag,
			.pName = "main"
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState2D = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &(VkVertexInputBindingDescription){
			.binding = 0,
			.stride = sizeof(vec2)
		},
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &(VkVertexInputAttributeDescription){
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = 0
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateTextSDF = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &(VkVertexInputBindingDescription){
			.binding = 1,
			.stride = sizeof(vec4)
		},
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &(VkVertexInputAttributeDescription){
			.location = 0,
			.binding = 1,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = 0
		}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputStateTriangle = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 3,
		.pVertexBindingDescriptions = (VkVertexInputBindingDescription[]){
			{
				.binding = 2,
				.stride = sizeof(mat4),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			}, {
				.binding = 3,
				.stride = sizeof(struct VertexPosition)
			}, {
				.binding = 4,
				.stride = sizeof(struct VertexAttributes)
			}
		},
		.vertexAttributeDescriptionCount = 8,
		.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]){
			{
				.location = 0,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 0
			}, {
				.location = 1,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 4 * sizeof(float)
			}, {
				.location = 2,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 8 * sizeof(float)
			}, {
				.location = 3,
				.binding = 2,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = 12 * sizeof(float)
			}, {
				.location = 4,
				.binding = 3,
				.format = VK_FORMAT_R16G16B16_UNORM,
				.offset = 0
			}, {
				.location = 5,
				.binding = 4,
				.format = VK_FORMAT_R16G16B16_UNORM,
				.offset = offsetof(struct VertexAttributes, nx)
			}, {
				.location = 6,
				.binding = 4,
				.format = VK_FORMAT_R16G16B16A16_UNORM,
				.offset = offsetof(struct VertexAttributes, tx)
			}, {
				.location = 7,
				.binding = 4,
				.format = VK_FORMAT_R16G16_UNORM,
				.offset = offsetof(struct VertexAttributes, u)
			}
		}
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &(VkViewport){
			.width = (float)windowWidth,
			.height = (float)windowHeight,
			.minDepth = 0.f,
			.maxDepth = 1.f
		},
		.scissorCount = 1,
		.pScissors = &(VkRect2D){
			.extent = surfaceCapabilities.currentExtent
		}
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.f
	};

	VkPipelineMultisampleStateCreateInfo multisampleState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState2D = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		// .stencilTestEnable = VK_TRUE,
		// .front = {
		// 	.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
		// 	.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		// 	.compareMask = 0xFF,
		// 	.writeMask = 0xFF
		// }
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState2DClip = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask = 0xFF
		},
		.back = {
			.passOp = VK_STENCIL_OP_INCREMENT_AND_CLAMP,
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF,
			.writeMask = 0xFF
		}
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateTriangle = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_GREATER,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		},
		.back = {
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		}
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilStateSkybox = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_EQUAL,
		.stencilTestEnable = VK_TRUE,
		.front = {
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		},
		.back = {
			.compareOp = VK_COMPARE_OP_EQUAL,
			.compareMask = 0xFF
		}
	};

	VkPipelineColorBlendStateCreateInfo colorBlendState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &(VkPipelineColorBlendAttachmentState){
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		}
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 1,
		.pDynamicStates = &(VkDynamicState){
			VK_DYNAMIC_STATE_STENCIL_REFERENCE
		}
	};

	VkPipelineCache pipelineCache;
	if ((r = vkCreatePipelineCache(device, &(VkPipelineCacheCreateInfo){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
	}, NULL, &pipelineCache)) != VK_SUCCESS)
		vkFatal("vkCreatePipelineCache", r);

	// if ((r = vkGetPipelineCacheData(device, pipelineCache, size_t *pDataSize, void *pData)) != VK_SUCCESS)
	// 	vkFatal("vkGetPipelineCacheData", r);

	if ((r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, _countof(pipelines), (VkGraphicsPipelineCreateInfo[]){
		[PIPELINE_IMAGE2D] = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesImage2d),
			.pStages = shaderStagesImage2d,
			.pVertexInputState = &vertexInputState2D,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilState2D,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, [PIPELINE_PATH2D] = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesPath2d),
			.pStages = shaderStagesPath2d,
			.pVertexInputState = &vertexInputState2D,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilState2D,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, [PIPELINE_PATH2D_CLIP] = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesPath2dClip),
			.pStages = shaderStagesPath2dClip,
			.pVertexInputState = &vertexInputState2D,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilState2DClip,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, [PIPELINE_TEXT] = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesTextsdf),
			.pStages = shaderStagesTextsdf,
			.pVertexInputState = &vertexInputStateTextSDF,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilState2D,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, [PIPELINE_TRIANGLE] = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesTriangle),
			.pStages = shaderStagesTriangle,
			.pVertexInputState = &vertexInputStateTriangle,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStateTriangle,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}, [PIPELINE_SKYBOX] = {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = _countof(shaderStagesSkybox),
			.pStages = shaderStagesSkybox,
			.pVertexInputState = &vertexInputState,
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilStateSkybox,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
			.renderPass = renderPass
		}
	}, NULL, pipelines)) != VK_SUCCESS)
		vkFatal("vkCreateGraphicsPipelines", r);

	VkQueue queue;
	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

	VkCommandPool commandPools[FRAMES_IN_FLIGHT];
	VkFence fences[FRAMES_IN_FLIGHT];
	VkSemaphore semaphores[FRAMES_IN_FLIGHT];
	VkSemaphore imageAcquireSemaphores[FRAMES_IN_FLIGHT];
	VkCommandBuffer commandBuffers[FRAMES_IN_FLIGHT];
	for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
		if ((r = vkCreateCommandPool(device, &(VkCommandPoolCreateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = queueFamilyIndex
		}, NULL, &commandPools[i])) != VK_SUCCESS)
			vkFatal("vkCreateCommandPool", r);

		if ((r = vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = commandPools[i],
			.commandBufferCount = 1
		}, &commandBuffers[i])) != VK_SUCCESS)
			vkFatal("vkAllocateCommandBuffers", r);

		if ((r = vkCreateFence(device, &(VkFenceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		}, NULL, &fences[i])) != VK_SUCCESS)
			vkFatal("vkCreateFence", r);

		if ((r = vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		}, NULL, &imageAcquireSemaphores[i])) != VK_SUCCESS)
			vkFatal("vkCreateSemaphore", r);

		if ((r = vkCreateSemaphore(device, &(VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
		}, NULL, &semaphores[i])) != VK_SUCCESS)
			vkFatal("vkCreateSemaphore", r);
	}

	viewport[0][0] = 2.f / (float)windowWidth;
	viewport[0][1] = 0.f;
	viewport[0][2] = 0.f;
	viewport[0][3] = -1.f;
	viewport[1][0] = 0.f;
	viewport[1][1] = 2.f / (float)windowHeight;
	viewport[1][2] = 0.f;
	viewport[1][3] = -1.f;
	viewport[2][0] = 0.f;
	viewport[2][1] = 0.f;
	viewport[2][2] = 1.f;
	viewport[2][3] = 0.f;
	viewport[3][0] = 0.f;
	viewport[3][1] = 0.f;
	viewport[3][2] = 0.f;
	viewport[3][3] = 1.f;

	float fov = 0.78f;
	float f = 1.f / __builtin_tanf(fov / 2);
	projection[0][0] = f / ((float)windowWidth / (float)windowHeight);
	projection[0][1] = 0.f;
	projection[0][2] = 0.f;
	projection[0][3] = 0.f;
	projection[1][0] = 0.f;
	projection[1][1] = -f;
	projection[1][2] = 0.f;
	projection[1][3] = 0.f;
	projection[2][0] = 0.f;
	projection[2][1] = 0.f;
	projection[2][2] = 0.f;
	projection[2][3] = 0.1f;
	projection[3][0] = 0.f;
	projection[3][1] = 0.f;
	projection[3][2] = -1.f;
	projection[3][3] = 0.f;

	title = (struct UINode){
		.type = UI_TYPE_TEXT,
		.position = {
			.old = { 0, ((i16)windowHeight / 2) - 256 },
			.new = { 0, ((i16)windowHeight / 2) - 256 }
		},
		.scale = { .old = 255, .new = 255 },
		.color = {
			.base = colors.white,
			.border = colors.black,
			.old = colors.white,
			.new = colors.white
		},
		.text = "Extreme Game"
	};
	u16 tw = (u16)textWidth(title.text, 32.f);
	title.position.old.x = title.position.new.x = ((i16)windowWidth / 2) - (tw / 2);

	settingsButton = (struct UINode){
		.type = UI_TYPE_BUTTON,
		.flags = UI_FLAG_ON_HOVER_ROTATE_ICON,
		.body = UI_BODY_CIRCLE,
		.onClick = UI_ON_CLICK_SETTINGS,
		.position = {
			.old = { 16 + 32, -64 - 2 },
			.new = { 16 + 32, 16 + 32 }
		},
		.scale = { .old = 255, .new = 255 },
		.radius = 32,
		.color = {
			.base = colors.gray,
			.border = blendColor(colors.gray, colors.black, 0.25),
			.old = colors.gray,
			.new = colors.gray
		},
		.childCount = 2,
		.children = (struct UINode[]){
			{
				.type = UI_TYPE_ICON,
				.scale = { .old = 255, .new = 255 },
				.extent = { 48, 48 },
				.imageView = IMAGE_VIEW_GEAR
			}, {
				.type = UI_TYPE_TOOLTIP,
				.body = UI_BODY_SQUARE,
				.position = {
					.old = { 16, 96 },
					.new = { 16, 96 }
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 128, 32 },
				.color = {
					.base = { 56, 56, 56, 124 },
					.old = { 56, 56, 56, 124 },
					.new = { 56, 56, 56, 124 }
				},
				.childCount = 1,
				.children = &(struct UINode){
					.type = UI_TYPE_TEXT,
					.position = {
						.old = { 4, 4 },
						.new = { 4, 4 }
					},
					.text = "settings"
				}
			}
		}
	};

	discordButton = (struct UINode){
		.type = UI_TYPE_BUTTON,
		.body = UI_BODY_SQUARE,
		.onClick = UI_ON_CLICK_DISCORD,
		.position = {
			.old = { 16 + 64 + 16, -64 - 2 },
			.new = { 16 + 64 + 16, 16 }
		},
		.scale = { .old = 255, .new = 255 },
		.extent = { 64, 64 },
		.color = {
			.base = colors.discordBlue,
			.border = blendColor(colors.discordBlue, colors.black, 0.25),
			.old = colors.discordBlue,
			.new = colors.discordBlue
		},
		.childCount = 1,
		.children = &(struct UINode){
			.type = UI_TYPE_ICON,
			.position = {
				.old = { 32, 32 },
				.new = { 32, 32 }
			},
			.scale = { .old = 255, .new = 255 },
			.extent = { 48, 48 },
			.imageView = IMAGE_VIEW_DISCORD
		}
	};

	exitButton = (struct UINode){
		.type = UI_TYPE_BUTTON,
		.flags = UI_FLAG_CLOSE_ICON,
		.body = UI_BODY_SQUARE,
		.onClick = UI_ON_CLICK_EXIT,
		.position = {
			.old = { (i16)windowWidth - (32 + 32), -(32 + 32) },
			.new = { (i16)windowWidth - (32 + 32), 32 }
		},
		.scale = { .old = 255, .new = 255 },
		.extent = { 32, 32 },
		.color = {
			.base = colors.red,
			.border = blendColor(colors.red, colors.black, 0.25),
			.old = colors.red,
			.new = colors.red
		}
	};

	settingsDialog = (struct UINode){
		.type = UI_TYPE_DIALOG,
		.body = UI_BODY_SQUARE,
		.extent = { 512, 512 },
		.color = {
			.base = colors.gray,
			.border = blendColor(colors.gray, colors.black, 0.25),
			.old = colors.gray,
			.new = colors.gray
		}
	};

	nameDiv = (struct UINode){
		.type = UI_TYPE_NONE,
		.position = {
			.old = { (i16)windowWidth / 2, (i16)windowHeight / 2},
			.new = { ((i16)windowWidth / 2) - 400, ((i16)windowHeight / 2) - 32 }
		},
		.scale = { .old = 0, .new = 255 },
		.extent = { 128 + 16 + 512 + 16 + 128, 64 },
		.childCount = 2,
		.children = (struct UINode[]){
			{
				.type = UI_TYPE_TEXTFIELD,
				.body = UI_BODY_SQUARE,
				.position = {
					.old = { 128 + 16, 0 },
					.new = { 128 + 16, 0 },
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 512, 64 },
				.color = {
					.base = colors.white,
					.border = colors.black,
					.old = colors.white,
					.new = colors.white
				},
				.text = (char[32]){ }
			}, {
				.type = UI_TYPE_BUTTON,
				.body = UI_BODY_SQUARE,
				.onClick = UI_ON_CLICK_ENTER_LOBBY,
				.position = {
					.old = { 128 + 16 + 512 + 16, 0 },
					.new = { 128 + 16 + 512 + 16, 0 }
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 128, 64 },
				.color = {
					.base = colors.green,
					.border = blendColor(colors.green, colors.black, 0.25),
					.old = colors.green,
					.new = colors.green
				}
			}
		}
	};

	roomList = (struct UINode){
		.type = UI_TYPE_LIST,
		.body = UI_BODY_SQUARE,
		.position = {
			.old = { (i16)windowWidth / 2, (i16)windowHeight / 2 },
			.new = { (i16)windowWidth / 2, (i16)windowHeight / 2 }
		},
		.scale = { .old = 0, .new = 0 },
		.extent = {
			(u16)(windowWidth - 256),
			(u16)(windowHeight - 256)
		},
		.color = {
			.base = colors.yellow,
			.border = blendColor(colors.yellow, colors.black, 0.25),
			.old = colors.yellow,
			.new = colors.yellow
		},
		.childCount = 1,
		.children = (struct UINode[UINT8_MAX + 1]){ }
	};

	struct UINode joinButtons[UINT8_MAX];
	for (u8 i = 0; i < 255; i++) {
		joinButtons[i] = (struct UINode){
			.type = UI_TYPE_BUTTON,
			.body = UI_BODY_SQUARE,
			.flags = UI_FLAG_ARROW_ICON,
			.position = {
				.old = { 512 - (16 + 32), 16 },
				.new = { 512 - (16 + 32), 16 }
			},
			.scale = { .old = 255, .new = 255 },
			.extent = { 32, 32},
			.color = {
				.base = colors.green,
				.border = blendColor(colors.green, colors.black, 0.25),
				.old = colors.green,
				.new = colors.green
			}
		};

		roomList.children[1 + i] = (struct UINode){
			.type = UI_TYPE_LIST_ITEM,
			.body = UI_BODY_SQUARE,
			.position = {
				.old = { 8, (i16)(8 + i * (64 + 8)) },
				.new = { 8, (i16)(8 + i * (64 + 8)) }
			},
			.scale = { .old = 255, .new = 255 },
			.extent = {
				512,
				64
			},
			.color = {
				.base = colors.red,
				.border = blendColor(colors.red, colors.black, 0.25),
				.old = colors.red,
				.new = colors.red
			},
			.childCount = 1,
			.children = &joinButtons[i]
		};
	}

	roomList.children[0] = (struct UINode){ // host game
		.type = UI_TYPE_BUTTON,
		.body = UI_BODY_SQUARE,
		.onClick = UI_ON_CLICK_HOST_ROOM,
		.position = {
			.old = { (i16)roomList.extent.x - (256 + 32), (i16)roomList.extent.y - (64 + 32) },
			.new = { (i16)roomList.extent.x - (256 + 32), (i16)roomList.extent.y - (64 + 32) }
		},
		.scale = { .old = 255, .new = 255 },
		.extent = { 256, 64 },
		.color = {
			.base = colors.green,
			.border = blendColor(colors.green, colors.black, 0.25),
			.old = colors.green,
			.new = colors.green
		}
	};

	altF4Dialog = (struct UINode){
		.type = UI_TYPE_DIALOG,
		.body = UI_BODY_SQUARE,
		.position = {
			.old = { (i16)windowWidth / 2, (i16)windowHeight / 2 },
			.new = { (i16)windowWidth / 2, (i16)windowHeight / 2 }
		},
		.extent = { 512, 256 },
		.color = {
			.base = colors.gray,
			.border = blendColor(colors.gray, colors.black, 0.25f),
			.old = colors.gray,
			.new = colors.gray
		},
		.childCount = 2,
		.children = (struct UINode[]){
			{
				.type = UI_TYPE_BUTTON,
				.body = UI_BODY_SQUARE,
				.onClick = UI_ON_CLICK_EXIT,
				.position = {
					.old = { 64, 256 - (64 + 32) },
					.new = { 64, 256 - (64 + 32) }
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 256, 64 },
				.color = {
					.base = colors.red,
					.border = blendColor(colors.red, colors.black, 0.25),
					.old = colors.red,
					.new = colors.red
				}
			}, {
				.type = UI_TYPE_BUTTON,
				.body = UI_BODY_SQUARE,
				.onClick = UI_ON_CLICK_NO_EXIT,
				.position = {
					.old = { 512 - 128, 256 - (64 + 32) },
					.new = { 512 - 128, 256 - (64 + 32) }
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 256, 64 },
				.color = {
					.base = colors.green,
					.border = blendColor(colors.green, colors.black, 0.25),
					.old = colors.green,
					.new = colors.green
				}
			}
		}
	};

	createRoomDialog = (struct UINode){
		.type = UI_TYPE_DIALOG,
		.body = UI_BODY_SQUARE,
		.position = {
			.old = { (i16)windowWidth / 2, (i16)windowHeight / 2 },
			.new = { (i16)windowWidth / 2, (i16)windowHeight / 2 }
		},
		.extent = { 1024, 512 },
		.color = {
			.base = colors.gray,
			.border = blendColor(colors.gray, colors.black, 0.25f),
			.old = colors.gray,
			.new = colors.gray
		},
		.childCount = 2,
		.children = (struct UINode[]){
			{
				.type = UI_TYPE_BUTTON,
				.flags = UI_FLAG_CLOSE_ICON,
				.body = UI_BODY_SQUARE,
				.onClick = UI_ON_CLICK_CLOSE_CREATE_ROOM_DIALOG,
				.position = {
					.old = { 1024 - (32 + 32), 32 },
					.new = { 1024 - (32 + 32), 32 }
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 32, 32 },
				.color = {
					.base = colors.red,
					.border = blendColor(colors.red, colors.black, 0.25),
					.old = colors.red,
					.new = colors.red
				}
			}, {
				.type = UI_TYPE_BUTTON,
				.body = UI_BODY_SQUARE,
				.onClick = UI_ON_CLICK_CREATE_ROOM,
				.position = {
					.old = { 512 - 128, 512 - (64 + 32) },
					.new = { 512 - 128, 512 - (64 + 32) }
				},
				.scale = { .old = 255, .new = 255 },
				.extent = { 256, 64 },
				.color = {
					.base = colors.green,
					.border = blendColor(colors.green, colors.black, 0.25),
					.old = colors.green,
					.new = colors.green
				}
			}
		}
	};

	static const GUID CLSID_MMDeviceEnumerator = { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
	static const GUID IID_IMMDeviceEnumerator = { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
	static const GUID IID_IAudioClient = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
	static const GUID IID_IAudioRenderClient = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };

	HRESULT hr;
	if (FAILED(hr = CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY)))
		win32Fatal("CoInitializeEx", (DWORD)hr);

	IMMDeviceEnumerator* audioDeviceEnumerator;
	if (FAILED(hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&audioDeviceEnumerator)))
		win32Fatal("CoCreateInstance", (DWORD)hr);

	IMMDevice* audioDevice;
	if (FAILED(hr = audioDeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(audioDeviceEnumerator, eRender, eConsole, &audioDevice)))
		win32Fatal("GetDefaultAudioEndpoint", (DWORD)hr);

	IAudioClient* audioClient;
	if (FAILED(hr = audioDevice->lpVtbl->Activate(audioDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audioClient)))
		win32Fatal("Activate", (DWORD)hr);

	WAVEFORMATEX waveFormat = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = 2,
		.nSamplesPerSec = 43200,
		.wBitsPerSample = sizeof(u16) * 8,
	};
	waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

	if (FAILED(hr = audioClient->lpVtbl->Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 10000000, 0, &waveFormat, NULL)))
		win32Fatal("Initialize", (DWORD)hr);

	IAudioRenderClient* audioRenderClient;
	if (FAILED(hr = audioClient->lpVtbl->GetService(audioClient, &IID_IAudioRenderClient, (void**)&audioRenderClient)))
		win32Fatal("GetService", (DWORD)hr);

	UINT32 audioBufferSize;
	if (FAILED(hr = audioClient->lpVtbl->GetBufferSize(audioClient, &audioBufferSize)))
		win32Fatal("GetBufferSize", (DWORD)hr);

	if (FAILED(hr = audioClient->lpVtbl->Start(audioClient)))
		win32Fatal("Start", (DWORD)hr);

	serverAddress = (struct sockaddr_in){
		.sin_family = AF_INET,
		.sin_port = htons(9000),
		.sin_addr.s_addr = inet_addr("172.23.52.49")
	};

	WSADATA wsaData;
	int wr = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wr != NO_ERROR)
		win32Fatal("WSAStartup", (DWORD)wr);

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
		win32Fatal("socket", (DWORD)WSAGetLastError());

	if (ioctlsocket(sock, FIONBIO, &(u_long){ 1 }) == SOCKET_ERROR)
		win32Fatal("ioctlsocket", (DWORD)WSAGetLastError());

	if (bind(sock, (struct sockaddr*)&(struct sockaddr_in){
		.sin_family = AF_INET
	}, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
		win32Fatal("bind", (DWORD)WSAGetLastError());

	if (!(saveFile = CreateFileW(L"Saves", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)))
		win32Fatal("CreateFileW", GetLastError());

	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(saveFile, &fileSize))
			win32Fatal("GetFileSizeEx", GetLastError());

		DWORD bytesRead;
		if (!ReadFile(saveFile, &saves, (DWORD)fileSize.QuadPart, &bytesRead, NULL))
			win32Fatal("ReadFile", GetLastError());

		if (SetFilePointer(saveFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
			win32Fatal("SetFilePointer", GetLastError());
	} else {
		saves = (struct Saves){
			.keyBindings = {
				[INPUT_FORWARD] = {
					.primary = 'W',
					.secondary = VK_UP
				}, [INPUT_LEFT] = {
					.primary = 'A',
					.secondary = VK_LEFT
				}, [INPUT_DOWN] = {
					.primary = 'S',
					.secondary = VK_DOWN
				}, [INPUT_RIGHT] = {
					.primary = 'D',
					.secondary = VK_RIGHT
				}
			},
			.mouseSensitivity = 0.0005f
		};
	}

	// u16 idx;
	// if (entities.firstFree == (u16)-1) {
	// 	idx = entities.count++;
	// } else {
	// 	idx = entities.firstFree;
	// 	entities.firstFree = entities.data[entities.firstFree].next;
	// }

	// entities.data[idx] = (struct Entity){
	// 	.type = ENTITY_TYPE_PLAYER,
	// 	.flags = ENTITY_IS_PLAYER_CONTROLLED,
	// 	.speed = 1 << 15,
	// 	.next = entities.head
	// };
	// entities.head = idx;

	// u16 playerID = idx;

	LARGE_INTEGER frequency;
	if (!QueryPerformanceFrequency(&frequency))
		win32Fatal("QueryPerformanceFrequency", GetLastError());

	LARGE_INTEGER start;
	if (!QueryPerformanceCounter(&start))
		win32Fatal("QueryPerformanceCounter", GetLastError());

	for (;;) {
		u32 swapchainImageIndex;
		if ((r = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAcquireSemaphores[frame], VK_NULL_HANDLE, &swapchainImageIndex)) != VK_SUCCESS)
			vkFatal("vkAcquireNextImageKHR", r);

		if ((r = vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX)) != VK_SUCCESS)
			vkFatal("vkWaitForFences", r);

		if ((r = vkResetFences(device, 1, &fences[frame])) != VK_SUCCESS)
			vkFatal("vkResetFences", r);

		if ((r = vkResetCommandPool(device, commandPools[frame], 0)) != VK_SUCCESS)
			vkFatal("vkResetCommandPool", r);

		commandBuffer = commandBuffers[frame];
		if ((r = vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		})) != VK_SUCCESS)
			vkFatal("vkBeginCommandBuffer", r);

		static bool copied;
		if (!copied) {
			copied = true;

			u16* quadIndices = buffers[BUFFER_STAGING].data + BUFFER_OFFSET_QUAD_INDICES;
			u16 j = 0;
			for (u32 i = 0; i < BUFFER_RANGE_QUAD_INDICES / sizeof(u16); i += 6, j += 4) {
				quadIndices[i + 0] = j + 0;
				quadIndices[i + 1] = j + 1;
				quadIndices[i + 2] = j + 2;
				quadIndices[i + 3] = j + 2;
				quadIndices[i + 4] = j + 1;
				quadIndices[i + 5] = j + 3;
			}

			VkDeviceSize stagingDataOffset = BUFFER_RANGE_QUAD_INDICES;

			__builtin_memcpy(buffers[BUFFER_STAGING].data + stagingDataOffset, &incbin_geometry_start, (char*)&incbin_geometry_end - (char*)&incbin_geometry_start);

			stagingDataOffset += BUFFER_RANGE_VERTEX_INDICES + BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_ATTRIBUTES;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, _countof(imageMemoryBarriers), imageMemoryBarriers);

			vkCmdCopyBuffer(commandBuffer, buffers[BUFFER_STAGING].handle, buffers[BUFFER_DEVICE].handle, 4, (VkBufferCopy[]){
				{
					.srcOffset = BUFFER_OFFSET_QUAD_INDICES,
					.dstOffset = BUFFER_OFFSET_QUAD_INDICES,
					.size = BUFFER_RANGE_QUAD_INDICES
				}, {
					.srcOffset = BUFFER_OFFSET_VERTEX_INDICES,
					.dstOffset = BUFFER_OFFSET_VERTEX_INDICES,
					.size = BUFFER_RANGE_VERTEX_INDICES
				}, {
					.srcOffset = BUFFER_OFFSET_VERTEX_POSITIONS,
					.dstOffset = BUFFER_OFFSET_VERTEX_POSITIONS,
					.size = BUFFER_RANGE_VERTEX_POSITIONS
				}, {
					.srcOffset = BUFFER_OFFSET_VERTEX_ATTRIBUTES,
					.dstOffset = BUFFER_OFFSET_VERTEX_ATTRIBUTES,
					.size = BUFFER_RANGE_VERTEX_ATTRIBUTES
				}
			});

			createFontBitmap(buffers[BUFFER_STAGING].data + stagingDataOffset);
			vkCmdCopyBufferToImage(commandBuffer, buffers[BUFFER_STAGING].handle, images[IMAGE_FONT].handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy){
				.bufferOffset = stagingDataOffset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = images[IMAGE_FONT].arrayLayers
				},
				.imageExtent = {
					.width = images[IMAGE_FONT].width,
					.height = images[IMAGE_FONT].height,
					.depth = 1
				}
			});

			stagingDataOffset += images[IMAGE_FONT].width * images[IMAGE_FONT].height;

			vkCmdCopyBufferToImage(commandBuffer, buffers[BUFFER_STAGING].handle, images[IMAGE_DEFAULT].handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy){
				.bufferOffset = stagingDataOffset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = images[IMAGE_DEFAULT].arrayLayers
				},
				.imageExtent = {
					.width = images[IMAGE_DEFAULT].width,
					.height = images[IMAGE_DEFAULT].height,
					.depth = 1
				}
			});

			__builtin_memcpy(buffers[BUFFER_STAGING].data + stagingDataOffset, &(float){ 0.5 }, sizeof(float));
			stagingDataOffset = ALIGN_FORWARD(stagingDataOffset + sizeof(float), 8u);

			vkCmdCopyBufferToImage(commandBuffer, buffers[BUFFER_STAGING].handle, images[IMAGE_ICONS].handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy){
				.bufferOffset = stagingDataOffset,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = images[IMAGE_ICONS].arrayLayers
				},
				.imageExtent = {
					.width = images[IMAGE_ICONS].width,
					.height = images[IMAGE_ICONS].height,
					.depth = 1
				}
			});

			__builtin_memcpy(buffers[BUFFER_STAGING].data + stagingDataOffset, incbin_icons_start, (images[IMAGE_ICONS].width * images[IMAGE_ICONS].height * images[IMAGE_ICONS].arrayLayers) / 2);
			stagingDataOffset += (images[IMAGE_ICONS].width * images[IMAGE_ICONS].height * images[IMAGE_ICONS].arrayLayers) / 2;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, NULL, 2, (VkBufferMemoryBarrier[]){
				{
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
					.buffer = buffers[BUFFER_DEVICE].handle,
					.offset = BUFFER_OFFSET_QUAD_INDICES,
					.size = BUFFER_RANGE_QUAD_INDICES + BUFFER_RANGE_VERTEX_INDICES
				}, {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
					.buffer = buffers[BUFFER_DEVICE].handle,
					.offset = BUFFER_OFFSET_VERTEX_POSITIONS,
					.size = BUFFER_RANGE_VERTEX_POSITIONS + BUFFER_RANGE_VERTEX_ATTRIBUTES
				}
			}, 0, NULL);

			for (u32 i = 0; i < _countof(imageMemoryBarriers); i++) {
				imageMemoryBarriers[i].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarriers[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, _countof(imageMemoryBarriers), imageMemoryBarriers);
		}

		MSG msg;
		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				ExitProcess(EXIT_SUCCESS);
			else {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		for (;;) {
			char buff[8192];
			struct sockaddr_storage address;
			struct sockaddr_in* v4 = (struct sockaddr_in*)&address;
			struct sockaddr_in6* v6 = (struct sockaddr_in6*)&address;

			int bytesReceived;
			if ((bytesReceived = recvfrom(sock, buff, sizeof(buff), 0, (struct sockaddr*)&address, &(int){ sizeof(struct sockaddr_storage) })) == SOCKET_ERROR) {
				if ((wr = WSAGetLastError()) == WSAEWOULDBLOCK)
					break;

				win32Fatal("recvfrom", (DWORD)wr);
			}

			if (address.ss_family != AF_INET && address.ss_family != AF_INET6)
				continue;

			bool isServer = (address.ss_family == serverAddress.sin_family &&
				__builtin_memcmp(&v4->sin_addr, &serverAddress.sin_addr, sizeof(v4->sin_addr)) == 0 &&
				v4->sin_port == serverAddress.sin_port);

			bool isGameHost = false;
			if (address.ss_family == gameHostAddress.ss_family) {

				if (gameHostAddress.ss_family == AF_INET) {
					struct sockaddr_in* in = (struct sockaddr_in*)&gameHostAddress;
					isGameHost = __builtin_memcmp(&v4->sin_addr, &in->sin_addr, sizeof(v4->sin_addr)) == 0 && v4->sin_port == in->sin_port;
				} else {
					struct sockaddr_in6* in6 = (struct sockaddr_in6*)&gameHostAddress;
					isGameHost = __builtin_memcmp(&v6->sin6_addr, &in6->sin6_addr, sizeof(v6->sin6_addr)) == 0 && v4->sin_port == in6->sin6_port;
				}
			}

			struct Client* client;

			u16 idx = clients.head;
			while (idx != (u16)-1) {
				client = &clients.data[idx];

				if (address.ss_family == client->family) {
					if (address.ss_family == AF_INET) {
						if (__builtin_memcmp(&client->ip.v4, &v4->sin_addr, sizeof(v4->sin_addr)) == 0 && client->port == v4->sin_port)
							goto found;
					} else {
						if (__builtin_memcmp(&client->ip.v6, &v6->sin6_addr, sizeof(v6->sin6_addr)) == 0 && client->port == v6->sin6_port)
							goto found;
					}
				}

				idx = client->next;
			}

			if (clients.firstFree == (u16)-1)
				idx = clients.count++;
			else {
				idx = clients.firstFree;
				clients.firstFree = clients.data[clients.firstFree].next;
			}

			client = &clients.data[idx];
			client->next = clients.head;
			clients.head = idx;

			client->family = address.ss_family;
			if (address.ss_family == AF_INET) {
				__builtin_memcpy(&client->ip.v4, &v4->sin_addr, sizeof(v4->sin_addr));
				client->port = v4->sin_port;
			} else {
				__builtin_memcpy(&client->ip.v6, &v6->sin6_addr, sizeof(v6->sin6_addr));
				client->port = v6->sin6_port;
			}

		found:
			switch (buff[0]) {
				case 0x00: // receive room list
					if (isServer) {
						roomList.childCount = 1 + (u8)buff[1];

						for (u16 i = 0; i < roomList.childCount; i++) {
							__builtin_memcpy(&rooms[i], &buff[2 + i * sizeof(struct Room)], sizeof(struct Room));

							roomList.children[1 + i].text = rooms[i].description;
						}
					}
					break;
				case 0x01:
					// if (isGameHost)
					// 	__builtin_memcpy(&playerID, &buff[1], sizeof(u16));
					break;
				case 0x02:
					if (isGameHost) {
						u16 entityCount;
	            		__builtin_memcpy(&entityCount, &buff[1], sizeof(u16));

						for (u16 i = 0, j = 3; i < entityCount; i++, j += 14) {
							u16 id = (u16)(buff[j] << 8 | buff[j + 1]);

							// if (id != playerID)
							// 	__builtin_memcpy(&entities.data[id].position, &buff[2], 3 * sizeof(u32));

							entities.data[id].next = entities.head;
							entities.head = id;
						}
					}
					break;
			}
		}

		LARGE_INTEGER counter;
		if (!QueryPerformanceCounter(&counter))
			win32Fatal("QueryPerformanceCounter", GetLastError());

		i64 countsElapsed = counter.QuadPart - start.QuadPart;

		u32 now = (u32)((countsElapsed * 1000) / frequency.QuadPart);
		deltaTime = (float)(now - msElapsed) / 1000;
		msElapsed = now;

		i64 countsPerTick = frequency.QuadPart / TICKS_PER_SECOND;
		i64 targetTicks = countsElapsed / countsPerTick;
		float alpha = (float)(countsElapsed % countsPerTick) / (float)countsPerTick;

		while (ticksElapsed < targetTicks) {
			// grid.elementCount = 0;
			// for (u32 i = 0; i < 256 * 256; i++)
			// 	grid.cells[i].head = (u16)-1;

			u16 idx = entities.head;
			while (idx != (u16)-1) {
				struct Entity* entity = &entities.data[idx];

				if (entity->flags & ENTITY_IS_PLAYER_CONTROLLED && cursorLocked) {
					bool forward = keys[settings.keyBindings[INPUT_FORWARD].primary] || keys[settings.keyBindings[INPUT_FORWARD].secondary];
					bool left = keys[settings.keyBindings[INPUT_LEFT].primary] || keys[settings.keyBindings[INPUT_LEFT].secondary];
					bool down = keys[settings.keyBindings[INPUT_DOWN].primary] || keys[settings.keyBindings[INPUT_DOWN].secondary];
					bool right = keys[settings.keyBindings[INPUT_RIGHT].primary] || keys[settings.keyBindings[INPUT_RIGHT].secondary];

					entity->yaw = camera.yaw;

					vec3 acceleration = { 0, 0, 0 };

					if (forward && !down)
						acceleration -= (vec3){ camera.forward.x, 0.f, camera.forward.z };
					else if (down && !forward)
						acceleration += (vec3){ camera.forward.x, 0.f, camera.forward.z };

					if (left && !right)
						acceleration -= (vec3){ camera.right.x, 0.f, camera.right.z };
					else if (right && !left)
						acceleration += (vec3){ camera.right.x, 0.f, camera.right.z };

					entity->velocity += acceleration * entity->speed;
				}

				// static i32 drag = 0x0000F000;
				// entity->velocity *= drag;
				// entity->position += entity->velocity;

				// if (entity->position.x + entity->radius > UINT16_MAX) {
				// 	entity->position.x = UINT16_MAX - entity->radius;
				// 	entity->velocity.x = 0;
				// } else if (entity->position.x < entity->radius) {
				// 	entity->position.x = entity->radius;
				// 	entity->velocity.x = 0;
				// }

				// if (entity->position.y + entity->radius > UINT16_MAX) {
				// 	entity->position.y = UINT16_MAX - entity->radius;
				// 	entity->velocity.y = 0;
				// } else if (entity->position.y < entity->radius) {
				// 	entity->position.y = entity->radius;
				// 	entity->velocity.y = 0;
				// }

				// if (entity->position.z + entity->radius > UINT16_MAX) {
				// 	entity->position.z = UINT16_MAX - entity->radius;
				// 	entity->velocity.z = 0;
				// } else if (entity->position.z < entity->radius) {
				// 	entity->position.z = entity->radius;
				// 	entity->velocity.z = 0;
				// }

				if (entity->flags & ENTITY_IS_PLAYER_CONTROLLED) {
					char buff[17];

					buff[0] = 0x00;
					__builtin_memcpy(&buff[1], &entity->yaw, sizeof(entity->yaw));
					__builtin_memcpy(&buff[5], &entity->position, 3 * sizeof(u32));

					if (sendto(sock, buff, sizeof(buff), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
						win32Fatal("sendto", (DWORD)WSAGetLastError());
				}

				// float magnitude = vec3Length(entity->velocity);
				// if (magnitude > 0.f) {
				// 	float co = 0.1f;
				// 	float friction = co * magnitude;
				// 	entity->velocity += friction * -entity->velocity / magnitude;
				// }

				idx = entity->next;
			}

			ticksElapsed++;
		}

		UINT32 padding;
		if (FAILED(hr = audioClient->lpVtbl->GetCurrentPadding(audioClient, &padding)))
			win32Fatal("GetCurrentPadding", (DWORD)hr);

		UINT32 framesAvailable = audioBufferSize - padding;

		INT16* audioBuffer;
		if (FAILED(hr = audioRenderClient->lpVtbl->GetBuffer(audioRenderClient, framesAvailable, (BYTE**)&audioBuffer)))
			win32Fatal("GetBuffer", (DWORD)hr);

		for (UINT32 i = 0; i < framesAvailable; i++) {
			*audioBuffer++ = 0; // left
			*audioBuffer++ = 0; // right
		}

		if (FAILED(hr = audioRenderClient->lpVtbl->ReleaseBuffer(audioRenderClient, framesAvailable, 0)))
			win32Fatal("ReleaseBuffer", (DWORD)hr);

		indices2D = buffers[BUFFER_FRAME].data + BUFFER_OFFSET_INDICES_2D + frame * BUFFER_RANGE_INDICES_2D;
		vertices2D = buffers[BUFFER_FRAME].data + BUFFER_OFFSET_VERTICES_2D + frame * BUFFER_RANGE_VERTICES_2D;
		verticesText = buffers[BUFFER_FRAME].data + BUFFER_OFFSET_TEXT + frame * BUFFER_RANGE_TEXT;
		modelMatrices = buffers[BUFFER_FRAME].data + BUFFER_OFFSET_MODEL_MATRICES + frame * BUFFER_RANGE_MODEL_MATRICES;

		indices2DCount = 0;
		vertices2DCount = 0;
		verticesTextCount = 0;
		modelMatricesCount = 0;

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
		vkCmdBindVertexBuffers(commandBuffer, 0, 5, (VkBuffer[]){
			buffers[BUFFER_FRAME].handle,
			buffers[BUFFER_FRAME].handle,
			buffers[BUFFER_FRAME].handle,
			buffers[BUFFER_DEVICE].handle,
			buffers[BUFFER_DEVICE].handle,
		}, (VkDeviceSize[]){
			BUFFER_OFFSET_VERTICES_2D + frame * BUFFER_RANGE_VERTICES_2D,
			BUFFER_OFFSET_TEXT + frame * BUFFER_RANGE_TEXT,
			BUFFER_OFFSET_MODEL_MATRICES + frame * BUFFER_RANGE_MODEL_MATRICES,
			BUFFER_OFFSET_VERTEX_POSITIONS,
			BUFFER_OFFSET_VERTEX_ATTRIBUTES
		});

		vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = renderPass,
			.framebuffer = swapchainFramebuffers[swapchainImageIndex],
			.renderArea.extent = surfaceCapabilities.currentExtent,
			.clearValueCount = 2,
			.pClearValues = (VkClearValue[]){
				{
					.color.float32 = { 30.f / 255.f, 167.f / 255.f, 97.f / 255.f, 1.f },
				}, {
					.depthStencil = {
						.depth = 0.f,
						.stencil = 0
					}
				}
			}
		}, VK_SUBPASS_CONTENTS_INLINE);

		float t = (float)clamp(transitionCircle.startTime > msElapsed ? 0 : msElapsed - transitionCircle.startTime, 0, TRANSITION_ANIMATION_TIME) / (float)TRANSITION_ANIMATION_TIME;
		u16 radius = (u16)lerp((float)transitionCircle.old, (float)transitionCircle.new, t);
		u8 stencilReference = 0;

		clickedElement = NULL;
		cursor = cursorNormal;

		drawUINode(&title);
		drawUINode(&nameDiv);
		drawUINode(&roomList);
		drawUINode(&createRoomDialog);

		if (radius > 0) {
			if (t < 1.f) {
				setTransform(mat4From2DAffine(1.f, 0.f, 0.f, 1.f, (float)windowWidth / 2, (float)windowHeight / 2));
				beginPath();
				arc(0.f, 0.f, radius, 0.f, M_PI * 2.f);
				clip();

				stencilReference = 1;
			}

			// uvec3 camPos = entities.data[playerID].position >> 16; // + alpha * entities.data[playerID].velocity;
			drawScene(camera.position, camera.right, camera.up, camera.forward, stencilReference);

			vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);

			if (t < 1.f) {
				setTransform(mat4From2DAffine(1.f, 0.f, 0.f, 1.f, (float)windowWidth / 2, (float)windowHeight / 2));
				beginPath();
				arc(0.f, 0.f, radius, 0.f, M_PI * 2.f);
				stroke(colors.black, 3.f, LINE_JOIN_MITER, LINE_CAP_BUTT);
			}
		}

		drawUINode(&discordButton);
		drawUINode(&settingsButton);
		drawUINode(&exitButton);

		if (lButtonDown && !clickedElement && !cursorLocked && radius == transitionCircle.new && radius > 0) {
			cursorLocked = TRUE;
			ShowCursor(FALSE);
		} else
			SetCursor(cursor);

		vkCmdEndRenderPass(commandBuffer);
		if ((r = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS)
			vkFatal("vkEndCommandBuffer", r);

		if ((r = vkQueueSubmit(queue, 1, &(VkSubmitInfo){
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &imageAcquireSemaphores[frame],
			.pWaitDstStageMask = &(VkPipelineStageFlags){ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &semaphores[frame]
		}, fences[frame])) != VK_SUCCESS)
			vkFatal("vkQueueSubmit", r);

		if ((r = vkQueuePresentKHR(queue, &(VkPresentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &semaphores[frame],
			.swapchainCount = 1,
			.pSwapchains = &swapchain,
			.pImageIndices = &swapchainImageIndex
		})) != VK_SUCCESS)
			vkFatal("vkQueuePresentKHR", r);

		frame = (frame + 1) % FRAMES_IN_FLIGHT;
	}
}
