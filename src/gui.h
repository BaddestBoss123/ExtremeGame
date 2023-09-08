#include "game.h"

enum UIType : u8 {
	UI_TYPE_NONE,
	UI_TYPE_BUTTON,
	UI_TYPE_CHECKBOX,
	UI_TYPE_TEXTFIELD,
	UI_TYPE_DIALOG,
	UI_TYPE_LIST,
	UI_TYPE_LIST_ITEM,
	UI_TYPE_TOOLTIP,
	UI_TYPE_ICON,
	UI_TYPE_TEXT
};

enum UIBody : u8 {
	UI_BODY_NONE,
	UI_BODY_SQUARE,
	UI_BODY_CIRCLE
};

enum UIState : u8 {
	UI_STATE_NEUTRAL,
	UI_STATE_HOVERED,
	UI_STATE_CLICKED,
	UI_STATE_RELEASED
};

enum UIFlags : u8 {
	UI_FLAG_ON_HOVER_ROTATE_ICON = 1 << 0,
	UI_FLAG_INVISIBLE = 1 << 1,
	UI_FLAG_CLOSE_ICON = 1 << 2,
	UI_FLAG_ARROW_ICON = 1 << 3,
	UI_FLAG_CAN_CLICK = 1 << 4,
	UI_FLAG_DIALOG_OPEN = 1 << 5,
	UI_FLAG_CHECKBOX_CHECKED = 1 << 6
};

enum UIOnClick : u8 {
	UI_ON_CLICK_NONE,
	UI_ON_CLICK_NO_EXIT,
	UI_ON_CLICK_EXIT,
	UI_ON_CLICK_EXIT_GAME,
	UI_ON_CLICK_SETTINGS,
	UI_ON_CLICK_DISCORD,
	UI_ON_CLICK_ENTER_LOBBY,
	UI_ON_CLICK_LEAVE_LOBBY,
	UI_ON_CLICK_HOST_ROOM,
	UI_ON_CLICK_CLOSE_CREATE_ROOM_DIALOG,
	UI_ON_CLICK_CREATE_ROOM
};

struct UINode {
	enum UIType type;
	u8 flags;
	enum UIBody body;
	enum UIState state;
	enum UIOnClick onClick;
	enum ImageViews imageView;
	u8 childCount;

	struct {
		i16vec2 old;
		i16vec2 new;
		u32 startTime;
	} position;

	struct {
		float old; // todo: u8 or u16
		float new;
		u32 startTime;
	} rotation;

	struct {
		u8 old;
		u8 new;
		u32 startTime;
	} scale;

	union {
		u16vec2 extent;
		u16 radius;
	};

	struct {
		u8vec4 base;
		u8vec4 border;
		u8vec4 old;
		u8vec4 new;
		u32 startTime;
	} color;

	char* text;
	struct UINode* children;
};

#define ANIMATE(property, newValue, delay) \
	property.old = property.new; \
	property.new = newValue; \
	property.startTime = msElapsed + delay

static struct UINode* focusedTextfield;
static struct UINode* clickedElement;

static struct UINode title;
static struct UINode exitButton;

static struct UINode settingsButton;
static struct UINode settingsDialog;

static struct UINode discordButton;

static struct UINode leaderboardButton;
static struct UINode leaderboardDialog;

static struct UINode nameDiv;

static struct UINode roomList;

static struct UINode createGameButton;
static struct UINode createRoomDialog;

static struct UINode chatTextfield;

static struct UINode altF4Dialog;

static struct {
	u16 old;
	u16 new;
	u32 startTime;
} transitionCircle;

#define ANIMATION_TIME 256
#define TRANSITION_ANIMATION_TIME 512

static inline bool isAnimating(struct UINode* node) {
	return (i64)msElapsed - (i64)node->position.startTime < ANIMATION_TIME ||
		(i64)msElapsed - (i64)node->scale.startTime < ANIMATION_TIME;
}

static inline void drawUINode(struct UINode* node) {
	struct UINode* tree[64];
	u32 childOffsets[512];
	mat4 transforms[64];
	u32 depth = 0;

	for (;;) {
		if (node->flags & UI_FLAG_INVISIBLE)
			goto bubble;

		vec2 localPosition = vec2Lerp(
			(vec2){ node->position.old.x, node->position.old.y },
			(vec2){ node->position.new.x, node->position.new.y },
			easeOutQuadratic((float)clamp(node->position.startTime > msElapsed ? 0 : msElapsed - node->position.startTime, 0, ANIMATION_TIME) / (float)ANIMATION_TIME));

		float rotation = lerp(
			node->rotation.old,
			node->rotation.new,
			easeOutQuadratic((float)clamp(msElapsed - node->rotation.startTime, 0, ANIMATION_TIME) / (float)ANIMATION_TIME));

		float scale = lerp(
			(float)node->scale.old / 255.f,
			(float)node->scale.new / 255.f,
			easeOutQuadratic((float)clamp(node->scale.startTime > msElapsed ? 0 : msElapsed - node->scale.startTime, 0, ANIMATION_TIME) / (float)ANIMATION_TIME));

		if (scale == 0.f)
			goto bubble;

		float c = __builtin_cosf(rotation);
		float s = __builtin_sinf(rotation);

		transforms[depth] = mat4From2DAffine(c * scale, s * scale, -s * scale, c * scale, localPosition.x, localPosition.y);
		if (depth)
			transforms[depth] = transforms[depth - 1] * transforms[depth];

		setTransform(transforms[depth]);

		vec2 position = { transforms[depth][0][3], transforms[depth][1][3] };

		u8vec4 color = blendColor(
			node->color.old,
			node->color.new,
			easeOutQuadratic((float)clamp(msElapsed - node->color.startTime, 0, ANIMATION_TIME) / (float)ANIMATION_TIME));

		bool animating = isAnimating(node);

		bool inBounds;
		switch (node->body) {
			case UI_BODY_NONE:
				inBounds = false;
				break;
			case UI_BODY_SQUARE:
				inBounds = (float)mouse.x > position.x && (float)mouse.x < position.x + node->extent.x && (float)mouse.y > position.y && (float)mouse.y < position.y + node->extent.y;
				beginPath();
				rect(0.f, 0.f, node->extent.x, node->extent.y);
				fill(color);
				stroke(node->color.border, 3.f, LINE_JOIN_MITER, LINE_CAP_BUTT);
				break;
			case UI_BODY_CIRCLE:
				i16 dx = (i16)(mouse.x - (i16)position.x);
				i16 dy = (i16)(mouse.y - (i16)position.y);
				inBounds = dx * dx + dy * dy <= node->radius * node->radius;
				beginPath();
				arc(0.f, 0.f, node->radius, 0.f, M_PI * 2.f);
				fill(color);
				stroke(node->color.border, 3.f, LINE_JOIN_MITER, LINE_CAP_BUTT);
				break;
		}

		if (animating)
			inBounds = false;

		if (!inBounds) {
			// if (node->type == UI_TYPE_DIALOG && node->state != UI_STATE_CLICKED && lButtonDown && !animating) {
			// 	if (!clickedElement || (clickedElement->type != UI_TYPE_BUTTON)) {
			// 		i16vec2 temp = node->position.old;
			// 		node->position.old = node->position.new;
			// 		node->position.new = temp;
			// 		node->position.startTime = msElapsed;
			// 	}
			// }

			if (node->state != UI_STATE_NEUTRAL)
				for (u16 i = 0; i < node->childCount; i++)
					if (node->flags & UI_FLAG_ON_HOVER_ROTATE_ICON && node->children[i].type == UI_TYPE_ICON) {
						node->children[i].rotation.old = node->children[i].rotation.new;
						node->children[i].rotation.new = 0;
						node->children[i].rotation.startTime = msElapsed;
					}

			for (u16 i = 0; i < node->childCount; i++)
				if (node->children[i].type == UI_TYPE_TOOLTIP)
					node->children[i].flags |= UI_FLAG_INVISIBLE;

			node->state = UI_STATE_NEUTRAL;
		} else {
			if (lButtonDown)
				clickedElement = node;

			if (node->type == UI_TYPE_TEXTFIELD)
				cursor = cursorBeam;

			if (node->flags & UI_FLAG_ON_HOVER_ROTATE_ICON)
				for (u16 i = 0; i < node->childCount; i++)
					if (node->children[i].type == UI_TYPE_ICON)
						node->children[i].rotation.new -= deltaTime;
		}

		switch (node->state) {
			case UI_STATE_NEUTRAL:
				if (!inBounds) {
					node->color.old = color;
					node->color.new = node->color.base;
					node->color.startTime = msElapsed;
					break;
				}

				for (u16 i = 0; i < node->childCount; i++) {
					if (node->flags & UI_FLAG_ON_HOVER_ROTATE_ICON && node->children[i].type == UI_TYPE_ICON) {
						node->children[i].rotation.old = 0;
						node->children[i].rotation.new = 0;
					} else if (node->children[i].type == UI_TYPE_TOOLTIP)
						node->children[i].flags &= ~UI_FLAG_INVISIBLE;
				}

				if (lButtonDown)
					node->flags &= ~UI_FLAG_CAN_CLICK;

				node->state = UI_STATE_HOVERED;
				__attribute__((fallthrough));
			case UI_STATE_HOVERED:
				if (node->type == UI_TYPE_BUTTON) {
					cursor = cursorHand;

					node->color.old = color;
					node->color.new = blendColor(node->color.base, colors.white, 0.25f);
					node->color.startTime = msElapsed;
				}

				if (!lButtonDown) {
					node->flags |= UI_FLAG_CAN_CLICK;
					break;
				}

				if (!(node->flags & UI_FLAG_CAN_CLICK))
					break;

				if (lButtonDown)
					node->flags &= ~UI_FLAG_CAN_CLICK;

				node->state = UI_STATE_CLICKED;
				__attribute__((fallthrough));
			case UI_STATE_CLICKED:
				if (node->type == UI_TYPE_BUTTON) {
					node->color.old = color;
					node->color.new = blendColor(node->color.base, colors.black, 0.15f);
					node->color.startTime = msElapsed;
				} else if (node->type == UI_TYPE_TEXTFIELD)
					focusedTextfield = node;

				if (lButtonDown)
					break;

				node->state = UI_STATE_RELEASED;
				__attribute__((fallthrough));
			case UI_STATE_RELEASED:
				switch (node->onClick) {
					case UI_ON_CLICK_NONE: break;
					case UI_ON_CLICK_NO_EXIT:
						ANIMATE(altF4Dialog.position, ((i16vec2){ center.x, center.y }), msElapsed);
						ANIMATE(altF4Dialog.scale, 0, msElapsed);
						break;
					case UI_ON_CLICK_EXIT:
						// if (transitionCircle.new > 0.f) {
						// 	transitionCircle.old = transitionCircle.new;
						// 	transitionCircle.new = 0.f;
						// 	transitionCircle.startTime = msElapsed;

						// 	exitButton.position.old = exitButton.position.new;
						// 	exitButton.position.startTime = msElapsed + ANIMATION_TIME;

						// 	nameTextfield.position.old = nameTextfield.position.new;
						// 	nameTextfield.position.new.y = ((i16)surfaceCapabilities.currentExtent.height / 2) - 32;
						// 	nameTextfield.position.startTime = msElapsed + 1000;

						// 	readyButton.position.old = readyButton.position.new;
						// 	readyButton.position.new.y = ((i16)surfaceCapabilities.currentExtent.height / 2) - 32;
						// 	readyButton.position.startTime = msElapsed + 1000;
						// } else
						if (!PostMessageW((HWND)hwnd, WM_CLOSE, 0, 0))
							win32Fatal("PostMessageW", GetLastError());
						break;
					case UI_ON_CLICK_EXIT_GAME:
						ANIMATE(nameDiv.position, ((i16vec2){ center.x - 400, center.y - 32 }), TRANSITION_ANIMATION_TIME);
						ANIMATE(nameDiv.scale, 255, TRANSITION_ANIMATION_TIME);

						exitButton.position.old = exitButton.position.new;
						exitButton.position.startTime = msElapsed + TRANSITION_ANIMATION_TIME;
						exitButton.onClick = UI_ON_CLICK_EXIT;

						ANIMATE(transitionCircle, 0, 0);
						break;
					case UI_ON_CLICK_SETTINGS:

						break;
					case UI_ON_CLICK_DISCORD:
						ShellExecuteW(NULL, L"open", L"https://discord.gg/E3pKkpqHPE", NULL, NULL, SW_SHOWNORMAL);
						break;
					case UI_ON_CLICK_ENTER_LOBBY:
						if (sendto(sock, &(char){ 0 }, 1, 0, (struct sockaddr*)&serverAddress, (int){ sizeof(serverAddress) }) == SOCKET_ERROR)
							win32Fatal("sendto", (DWORD)WSAGetLastError());

						ANIMATE(nameDiv.position, ((i16vec2){ center.x, center.y }), 0);
						ANIMATE(nameDiv.scale, 0, 0);

						for (u8 i = 0; i < nameDiv.childCount; i++)
							nameDiv.children[i].position.startTime = msElapsed;

						ANIMATE(roomList.position, ((i16vec2){ 128, 128 }), ANIMATION_TIME);
						ANIMATE(roomList.scale, 255, ANIMATION_TIME);

						ANIMATE(exitButton.position, ((i16vec2){
							(i16)(roomList.position.new.x + (roomList.extent.x - (32 + 32))),
							roomList.position.new.y + 32
						}), ANIMATION_TIME);
						exitButton.onClick = UI_ON_CLICK_LEAVE_LOBBY;
						break;
					case UI_ON_CLICK_LEAVE_LOBBY:
						u32 delay = 0;
						if (createRoomDialog.scale.new == 255) {
							ANIMATE(createRoomDialog.position, ((i16vec2){ center.x, center.y }), 0);
							ANIMATE(createRoomDialog.scale, 0, 0);
							delay = ANIMATION_TIME;
						}

						ANIMATE(roomList.position, ((i16vec2){ center.x, center.y }), delay);
						ANIMATE(roomList.scale, 0, delay);

						ANIMATE(nameDiv.position, ((i16vec2){ center.x - 400, center.y - 32 }), delay + ANIMATION_TIME);
						ANIMATE(nameDiv.scale, 255, delay + ANIMATION_TIME);

						for (u8 i = 0; i < nameDiv.childCount; i++)
							nameDiv.children[i].position.startTime = delay + msElapsed;

						ANIMATE(exitButton.position, ((i16vec2){ (i16)windowWidth - 64, 32 }), delay);
						exitButton.onClick = UI_ON_CLICK_EXIT;
						break;
					case UI_ON_CLICK_HOST_ROOM:
						ANIMATE(createRoomDialog.position, ((i16vec2){ center.x - 512, center.y - 256 }), 0);
						ANIMATE(createRoomDialog.scale, 255, 0);

						node->position.startTime = msElapsed;
						exitButton.scale.startTime = msElapsed;
						break;
					case UI_ON_CLICK_CLOSE_CREATE_ROOM_DIALOG:
						ANIMATE(createRoomDialog.position, ((i16vec2){ center.x, center.y }), 0);
						ANIMATE(createRoomDialog.scale, 0, 0);

						node->position.startTime = msElapsed;
						exitButton.scale.startTime = msElapsed;
						break;
					case UI_ON_CLICK_CREATE_ROOM:
						char description[64] = "room description";

						ANIMATE(createRoomDialog.position, ((i16vec2){ center.x, center.y }), 0);
						ANIMATE(createRoomDialog.scale, 0, 0);

						ANIMATE(roomList.position, ((i16vec2){ center.x, center.y }), ANIMATION_TIME);
						ANIMATE(roomList.scale, 0, ANIMATION_TIME);

						ANIMATE(exitButton.position, ((i16vec2){ (i16)windowWidth - 64, 32 }), TRANSITION_ANIMATION_TIME);
						exitButton.onClick = UI_ON_CLICK_EXIT_GAME;

						ANIMATE(transitionCircle, (u16)(__builtin_sqrtf((float)(windowWidth * windowWidth + windowHeight * windowHeight)) / 2.f), TRANSITION_ANIMATION_TIME);

						if (!cursorLocked) {
							cursorLocked = TRUE;
							ShowCursor(FALSE);
						}

						// u8 msg[1 + 32 + 64 + 1 + 1 + 2 + 2];
						// msg[0] = 0x01;
						// __builtin_memcpy(&msg[1], nameTextfield.text, 32);
						// __builtin_memcpy(&msg[1 + 32], description, 64);
						// msg[1 + 32 + 64] = GAME_KOTH;
						// msg[1 + 32 + 64 + 1] = ROOM_JOIN_WHILE_PLAYING;
						// __builtin_memcpy(&msg[1 + 32 + 64 + 1 + 1], &(u16){ 1 }, sizeof(u16));
						// __builtin_memcpy(&msg[1 + 32 + 64 + 1 + 1 + 2], &(u16){ 16 }, sizeof(u16));

						// if (sendto(sock, (char*)msg, sizeof(msg), 0, (struct sockaddr*)&serverAddress, (int){ sizeof(serverAddress) }) == SOCKET_ERROR)
						// 	win32Fatal("sendto", (DWORD)WSAGetLastError());

						break;
				}

				node->state = UI_STATE_HOVERED;
		}

		if (node->flags & UI_FLAG_CLOSE_ICON) {
			beginPath();
			moveTo(8.f, 8.f);
			lineTo(24.f, 24.f);
			moveTo(24.f, 8.f);
			lineTo(8.f, 24.f);
			stroke((u8vec4){ 204, 204, 204, color.a }, 4.f, LINE_JOIN_MITER, LINE_CAP_BUTT);
		} else if (node->flags & UI_FLAG_ARROW_ICON) {
			beginPath();
			moveTo(8.f, 8.f);
			lineTo(24.f, 16.f);
			lineTo(8.f, 24.f);
			closePath();
			u8vec4 col = (u8vec4){ 204, 204, 204, color.a };
			fill(col);
			stroke(blendColor(col, colors.black, 0.25), 2.f, LINE_JOIN_MITER, LINE_CAP_BUTT);
		}

		if (node->type == UI_TYPE_ICON)
			drawImage(node->imageView, -node->extent.x * 0.5f, -node->extent.y * 0.5f, node->extent.x, node->extent.y);

		if (node->text && __builtin_strlen(node->text))
			drawText(node->text, 0, 0, 32.f, color);

		if (node->childCount) {
			tree[depth] = node;
			childOffsets[depth++] = 0;
			node = node->children;
		} else {
		bubble:
			while (depth) {
				node = tree[--depth];
				if (++childOffsets[depth] < node->childCount) {
					node = &node->children[childOffsets[depth++]];
					break;
				}
			}

			if (!depth)
				return;
		}
	}
}
