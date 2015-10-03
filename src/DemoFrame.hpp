#pragma once
#include <cstddef>
#include <string>
#include <vector>

enum class DemoFrameType : uint8_t {
	DEMO_START = 2,
	CONSOLE_COMMAND = 3,
	CLIENT_DATA = 4,
	NEXT_SECTION = 5,
	EVENT = 6,
	WEAPON_ANIM = 7,
	SOUND = 8,
	DEMO_BUFFER = 9
};

struct DemoFrame {
	DemoFrameType type;
	float time;
	int32_t frame;
};

// DEMO_START: no extra data.

struct ConsoleCommandFrame : DemoFrame {
	std::string command;
};

struct ClientDataFrame : DemoFrame {
	float origin[3];
	float viewangles[3];
	int32_t weaponBits;
	float fov;
};

// NEXT_SECTION: no extra data.

struct EventFrame : DemoFrame {
	int32_t flags;
	int32_t index;
	float delay;
	struct {
		int32_t flags;
		int32_t entityIndex;
		float origin[3];
		float angles[3];
		float velocity[3];
		int32_t ducking;
		float fparam1;
		float fparam2;
		int32_t iparam1;
		int32_t iparam2;
		int32_t bparam1;
		int32_t bparam2;
	} EventArgs;
};

struct WeaponAnimFrame : DemoFrame {
	int32_t anim;
	int32_t body;
};


struct SoundFrame : DemoFrame {
	int32_t channel;
	std::vector<char> sample;
	float attenuation;
	float volume;
	int32_t flags;
	int32_t pitch;
};

struct DemoBufferFrame : DemoFrame {
	std::vector<unsigned char> buffer;
};

// Otherwise, netmsg.
struct NetMsgFrame : DemoFrame {
	struct {
		float timestamp;

		struct {
			float vieworg[3];
			float viewangles[3];
			float forward[3];
			float right[3];
			float up[3];
			float frametime;
			float time;
			int32_t intermission;
			int32_t paused;
			int32_t spectator;
			int32_t onground;
			int32_t waterlevel;
			float simorg[3];
			float simvel[3];
			float viewheight[3];
			float idealpitch;
			float cl_viewangles[3];
			int32_t health;
			float crosshairangle[3];
			float viewsize;
			float punchangle[3];
			int32_t maxclients;
			int32_t viewentity;
			int32_t playernum;
			int32_t max_entities;
			int32_t demoplayback;
			int32_t hardware;
			int32_t smoothing;
			int32_t ptr_cmd;
			int32_t ptr_movevars;
			int32_t viewport[4];
			int32_t nextView;
			int32_t onlyClientDraw;
		} RefParams;

		struct {
			int16_t lerp_msec;
			int8_t msec;
			uint8_t align_1;
			float viewangles[3];
			float forwardmove;
			float sidemove;
			float upmove;
			int8_t lightlevel;
			uint8_t align_2;
			uint16_t buttons;
			int8_t impulse;
			int8_t weaponselect;
			uint8_t align_3;
			uint8_t align_4;
			int32_t impact_index;
			float impact_position[3];
		} UserCmd;

		struct {
			float gravity;
			float stopspeed;
			float maxspeed;
			float spectatormaxspeed;
			float accelerate;
			float airaccelerate;
			float wateraccelerate;
			float friction;
			float edgefriction;
			float waterfriction;
			float entgravity;
			float bounce;
			float stepsize;
			float maxvelocity;
			float zmax;
			float waveHeight;
			int32_t footsteps;
			std::string skyName;
			float rollangle;
			float rollspeed;
			float skycolor_r;
			float skycolor_g;
			float skycolor_b;
			float skyvec_x;
			float skyvec_y;
			float skyvec_z;
		} MoveVars;

		float view[3];
		int32_t viewmodel;
	} DemoInfo;

	int32_t incoming_sequence;
	int32_t incoming_acknowledged;
	int32_t incoming_reliable_acknowledged;
	int32_t incoming_reliable_sequence;
	int32_t outgoing_sequence;
	int32_t reliable_sequence;
	int32_t last_reliable_sequence;

	std::vector<unsigned char> msg;
};
