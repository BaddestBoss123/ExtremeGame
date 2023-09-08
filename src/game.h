#include "font.h"

enum Games : u8 {
	GAME_WAVES,
	GAME_KOTH,
	GAME_PARKOUR
};

static const char* gameNames[] = {
	[GAME_WAVES] = "Waves",
	[GAME_KOTH] = "King of the Hill",
	[GAME_PARKOUR] = "Parkour"
};

static enum Games activeGame;

enum RoomFlags : u8 {
	ROOM_PASSWORD = 1 << 0,
	ROOM_JOIN_WHILE_PLAYING = 1 << 1
};

struct Room {
	struct {
		char name[32];

		u16 family;
		union {
			char v4[4];
			char v6[16];
		} ip;
		u16 port;
	} host;

	char description[64];

	u8 game;
	u8 flags;

	u16 playerCount;
	u16 capacity;
};

static struct Room rooms[256];

enum BarrelType : u8 {
	BARREL_TYPE_REGULAR,
	BARREL_TYPE_DRONE,
	BARREL_TYPE_TRAP
};

struct Barrel {
	enum BarrelType type;

	u8 rotation;
	u8 reloadTime;
	u8 reloadTimer;

	u16 width;
	u16 length;

	u16 next;
};

static struct {
	struct Barrel* data;
	u16 firstFree;
	u16 count;
} barrels = {
	.data = (struct Barrel[UINT16_MAX]){ },
	.firstFree = (u16)-1
};

enum Input {
	INPUT_FORWARD,
	INPUT_LEFT,
	INPUT_DOWN,
	INPUT_RIGHT,
	INPUT_MAX_ENUM
};

struct KeyBinding {
	u8 primary;
	u8 secondary;
};

struct Character {
	char name[32];
	u64 xp;
};

static u16 characterCount;
static struct Character characters[512];

struct Saves {
	struct KeyBinding keyBindings[INPUT_MAX_ENUM];
	float mouseSensitivity;
	enum Games lastHostedGame;
	char playerName[32];
};

static struct Saves saves;
#define settings saves

enum EntityType : u8 {
	ENTITY_TYPE_PLAYER
};

enum EntityFlags : u16 {
	ENTITY_IS_PLAYER_CONTROLLED = 1 << 0
};

struct Entity {
	uvec3 position;
	ivec3 velocity;

	float pitch;
	float yaw;

	u16 speed;
	u16 radius;
	u16 mass;

	u16 flags;
	u16 next;

	enum EntityType type;
};

static struct {
	struct Entity* data;
	u16 firstFree;
	u16 head;
	u16 count;
} entities = {
	.data = (struct Entity[UINT16_MAX]){ },
	.firstFree = (u16)-1,
	.head = (u16)-1
};

enum Directions : u8 {
	DIRECTION_NORTH,
	DIRECTION_EAST,
	DIRECTION_SOUTH,
	DIRECTION_WEST
};

struct TerrainDoor {
	u16 node;
	enum Directions direction;
};

struct TerrainNode {
	enum Meshes mesh;

	u8 doorCount;
	struct TerrainDoor* doors;
};

struct Region {
	struct TerrainNode* nodes;
	u16 root;
};

static struct Region region = {
	.nodes = (struct TerrainNode[]){
		{

		}
	}
};

// struct GridElement {
// 	u16 entity;
// 	u16 next;
// };

// struct GridCell {
// 	u16 head;
// };

// static struct {
// 	struct GridCell cells[256 * 256];
// 	struct GridElement elements[UINT16_MAX];
// 	u16 elementCount;
// } grid;

// static inline void gridInsert(u16 idx) {
// 	struct Entity* entity = &entities.data[idx];

// 	u16 firstX = (entity->position.x >> 16) - entity->radius;
// 	u16 lastX = (entity->position.x >> 16) + entity->radius;
// 	u16 firstZ = (entity->position.z >> 16) - entity->radius;
// 	u16 lastZ = (entity->position.z >> 16) + entity->radius;

// 	for (u16 z = firstZ; z < lastZ; z++) {
// 		for (u16 x = firstX; x < lastX; x++) {
// 			grid.elements[grid.elementCount] = (struct GridElement){
// 				.entity = idx,
// 				.next = grid.cells[x + z * 256].head
// 			};

// 			grid.cells[x + z * 256].head = grid.elementCount++;
// 		}
// 	}
// }
