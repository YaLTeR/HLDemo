#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <vector>
#include <boost/nowide/fstream.hpp>

#include "DemoFile.hpp"
#include "DemoFrame.hpp"

enum {
	HEADER_SIZE = 544,
	HEADER_SIGNATURE_CHECK_SIZE = 6,
	HEADER_SIGNATURE_SIZE = 8,
	HEADER_MAPNAME_SIZE = 260,
	HEADER_GAMEDIR_SIZE = 260,

	MIN_DIR_ENTRY_COUNT = 1,
	MAX_DIR_ENTRY_COUNT = 1024,
	DIR_ENTRY_SIZE = 92,
	DIR_ENTRY_DESCRIPTION_SIZE = 64,

	MIN_FRAME_SIZE = 12,
	FRAME_CONSOLE_COMMAND_SIZE = 64,
	FRAME_CLIENT_DATA_SIZE = 32,
	FRAME_EVENT_SIZE = 84,
	FRAME_WEAPON_ANIM_SIZE = 8,
	FRAME_SOUND_SIZE_1 = 8,
	FRAME_SOUND_SIZE_2 = 16,
	FRAME_DEMO_BUFFER_SIZE = 4,
	FRAME_NETMSG_SIZE = 468,
	FRAME_NETMSG_DEMOINFO_SIZE = 436,
	FRAME_NETMSG_DEMOINFO_MOVEVARS_SKYNAME_SIZE = 32,
	FRAME_NETMSG_MIN_MESSAGE_LENGTH = 0,
	FRAME_NETMSG_MAX_MESSAGE_LENGTH = 65536
};

namespace nowide = boost::nowide;

template<typename T>
static void read_object(nowide::ifstream& i, T& obj)
{
	i.read(reinterpret_cast<char*>(&obj), sizeof(T));
}

template<typename T>
static void read_objects(nowide::ifstream& i, std::vector<T>& objs)
{
	i.read(reinterpret_cast<char*>(objs.data()), objs.size() * sizeof(T));
}

template<typename T>
static void write_object(nowide::ofstream& o, const T& obj)
{
	o.write(reinterpret_cast<const char*>(&obj), sizeof(T));
}

template<typename T>
static void write_objects(nowide::ofstream& o, const std::vector<T>& objs)
{
	o.write(reinterpret_cast<const char*>(objs.data()), objs.size() * sizeof(T));
}

DemoFile::DemoFile(const std::string& filename)
{
	demo.open(filename.c_str(), std::ios::binary);
	if (!demo)
		throw std::runtime_error("Error opening the demo file.");

	demo.seekg(0, std::ios::end);
	demoSize = demo.tellg();
	if (demoSize < HEADER_SIZE)
		throw std::runtime_error("Invalid demo file (the size is too small).");

	demo.seekg(0, std::ios::beg);
	char signature[HEADER_SIGNATURE_CHECK_SIZE];
	demo.read(signature, sizeof(signature));
	if (std::memcmp(signature, "HLDEMO", sizeof(signature)))
		throw std::runtime_error("Invalid demo file (signature doesn't match).");

	ReadHeader();
	ReadDirectory();

	readFrames = false;
}

void DemoFile::ReadHeader()
{
	demo.seekg(HEADER_SIGNATURE_SIZE, std::ios::beg);
	read_object(demo, header.demoProtocol);
	read_object(demo, header.netProtocol);

	std::vector<char> mapNameBuf(HEADER_MAPNAME_SIZE);
	read_objects(demo, mapNameBuf);
	mapNameBuf.push_back('\0');
	header.mapName = mapNameBuf.data();

	std::vector<char> gameDirBuf(HEADER_GAMEDIR_SIZE);
	read_objects(demo, gameDirBuf);
	gameDirBuf.push_back('\0');
	header.gameDir = gameDirBuf.data();

	read_object(demo, header.mapCRC);
	read_object(demo, header.directoryOffset);
}

void DemoFile::ReadDirectory()
{
	if (header.directoryOffset < 0 || demoSize - std::streamoff{ 4 } < header.directoryOffset)
		throw std::runtime_error("Error parsing the demo directory: invalid directory offset.");

	demo.seekg(header.directoryOffset, std::ios::beg);
	int32_t dirEntryCount;
	read_object(demo, dirEntryCount);
	if (dirEntryCount < MIN_DIR_ENTRY_COUNT || dirEntryCount > MAX_DIR_ENTRY_COUNT || (demoSize - std::streamoff{ dirEntryCount * DIR_ENTRY_SIZE }) < demo.tellg())
		throw std::runtime_error("Error parsing the demo directory: invalid directory entry count.");

	directoryEntries.clear();
	directoryEntries.reserve(dirEntryCount);
	for (auto i = 0; i < dirEntryCount; ++i) {
		DemoDirectoryEntry entry;
		read_object(demo, entry.type);

		std::vector<char> descriptionBuf(DIR_ENTRY_DESCRIPTION_SIZE);
		read_objects(demo, descriptionBuf);
		descriptionBuf.push_back('\0');
		entry.description = descriptionBuf.data();

		read_object(demo, entry.flags);
		read_object(demo, entry.CDTrack);
		read_object(demo, entry.trackTime);
		read_object(demo, entry.frameCount);
		read_object(demo, entry.offset);
		read_object(demo, entry.fileLength);

		directoryEntries.push_back(entry);
	}
}

bool DemoFile::IsValidDemoFile(const std::string& filename)
{
	nowide::ifstream in;
	in.open(filename.c_str(), std::ios::binary);
	if (!in)
		throw std::runtime_error("Error opening the file.");

	in.seekg(0, std::ios::end);
	auto size = in.tellg();
	if (size < HEADER_SIZE)
		return false;

	in.seekg(0, std::ios::beg);
	char signature[HEADER_SIGNATURE_CHECK_SIZE];
	in.read(signature, sizeof(signature));
	if (std::memcmp(signature, "HLDEMO", sizeof(signature)))
		return false;

	return true;
}

void DemoFile::ReadFrames()
{
	if (readFrames)
		return;

	if (header.demoProtocol != 5) {
		throw std::runtime_error("Only demo protocol 5 is supported.");
	}

	size_t i = 0;
	// On any error, just skip to the next entry.
	for (auto& entry : directoryEntries) {
		i++;

		auto offset = entry.offset;
		if (entry.offset < 0 || demoSize < entry.offset) {
			// Invalid offset.
			continue;
		}

		demo.seekg(offset, std::ios::beg);

		bool stop = false;
		while (!stop) {
			if (demoSize - std::streamoff{ MIN_FRAME_SIZE } < demo.tellg()) {
				// Unexpected EOF.
				break;
			}

			DemoFrame frame;
			read_object(demo, frame.type);
			read_object(demo, frame.time);
			read_object(demo, frame.frame);

			switch (frame.type) {
			case DemoFrameType::DEMO_START:
			{
				entry.frames.emplace_back(new DemoFrame(std::move(frame)));
			}
				break;

			case DemoFrameType::CONSOLE_COMMAND:
			{
				if (demoSize - std::streamoff{ FRAME_CONSOLE_COMMAND_SIZE } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				ConsoleCommandFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				std::vector<char> commandBuf(FRAME_CONSOLE_COMMAND_SIZE);
				read_objects(demo, commandBuf);
				commandBuf.push_back('\0');
				f.command = commandBuf.data();

				entry.frames.emplace_back(new ConsoleCommandFrame(std::move(f)));
			}
				break;

			case DemoFrameType::CLIENT_DATA:
			{
				if (demoSize - std::streamoff{ FRAME_CLIENT_DATA_SIZE } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				ClientDataFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				for (auto i = 0; i < 3; ++i)
					read_object(demo, f.origin[i]);
				for (auto i = 0; i < 3; ++i)
					read_object(demo, f.viewangles[i]);
				read_object(demo, f.weaponBits);
				read_object(demo, f.fov);

				entry.frames.emplace_back(new ClientDataFrame(std::move(f)));
			}
				break;

			case DemoFrameType::NEXT_SECTION:
			{
				entry.frames.emplace_back(new DemoFrame(std::move(frame)));

				stop = true;
			}
				break;

			case DemoFrameType::EVENT:
			{
				if (demoSize - std::streamoff{ FRAME_EVENT_SIZE } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				EventFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				read_object(demo, f.flags);
				read_object(demo, f.index);
				read_object(demo, f.delay);
				read_object(demo, f.EventArgs.flags);
				read_object(demo, f.EventArgs.entityIndex);
				for (auto i = 0; i < 3; ++i)
					read_object(demo, f.EventArgs.origin[i]);
				for (auto i = 0; i < 3; ++i)
					read_object(demo, f.EventArgs.angles[i]);
				for (auto i = 0; i < 3; ++i)
					read_object(demo, f.EventArgs.velocity[i]);
				read_object(demo, f.EventArgs.ducking);
				read_object(demo, f.EventArgs.fparam1);
				read_object(demo, f.EventArgs.fparam2);
				read_object(demo, f.EventArgs.iparam1);
				read_object(demo, f.EventArgs.iparam2);
				read_object(demo, f.EventArgs.bparam1);
				read_object(demo, f.EventArgs.bparam2);

				entry.frames.emplace_back(new EventFrame(std::move(f)));
			}
				break;

			case DemoFrameType::WEAPON_ANIM:
			{
				if (demoSize - std::streamoff{ FRAME_WEAPON_ANIM_SIZE } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				WeaponAnimFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				read_object(demo, f.anim);
				read_object(demo, f.body);

				entry.frames.emplace_back(new WeaponAnimFrame(std::move(f)));
			}
				break;

			case DemoFrameType::SOUND:
			{
				if (demoSize - std::streamoff{ FRAME_SOUND_SIZE_1 } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				SoundFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				read_object(demo, f.channel);

				int32_t length;
				read_object(demo, length);

				if (demoSize - std::streamoff{ length } - std::streamoff{ FRAME_SOUND_SIZE_2 } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				f.sample.resize(length);
				read_objects(demo, f.sample);
				read_object(demo, f.attenuation);
				read_object(demo, f.volume);
				read_object(demo, f.flags);
				read_object(demo, f.pitch);

				entry.frames.emplace_back(new SoundFrame(std::move(f)));
			}
				break;

			case DemoFrameType::DEMO_BUFFER:
			{
				if (demoSize - std::streamoff{ FRAME_DEMO_BUFFER_SIZE } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				DemoBufferFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				int32_t length;
				read_object(demo, length);

				if (demoSize - std::streamoff{ length } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				f.buffer.resize(length);
				read_objects(demo, f.buffer);

				entry.frames.emplace_back(new DemoBufferFrame(std::move(f)));
			}
				break;

			default:
			{
				if (demoSize - std::streamoff{ FRAME_NETMSG_SIZE } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				NetMsgFrame f;
				f.type = frame.type;
				f.time = frame.time;
				f.frame = frame.frame;

				read_object(demo, f.DemoInfo.timestamp);
				read_object(demo, f.DemoInfo.RefParams.vieworg[0]);
				read_object(demo, f.DemoInfo.RefParams.vieworg[1]);
				read_object(demo, f.DemoInfo.RefParams.vieworg[2]);
				read_object(demo, f.DemoInfo.RefParams.viewangles[0]);
				read_object(demo, f.DemoInfo.RefParams.viewangles[1]);
				read_object(demo, f.DemoInfo.RefParams.viewangles[2]);
				read_object(demo, f.DemoInfo.RefParams.forward[0]);
				read_object(demo, f.DemoInfo.RefParams.forward[1]);
				read_object(demo, f.DemoInfo.RefParams.forward[2]);
				read_object(demo, f.DemoInfo.RefParams.right[0]);
				read_object(demo, f.DemoInfo.RefParams.right[1]);
				read_object(demo, f.DemoInfo.RefParams.right[2]);
				read_object(demo, f.DemoInfo.RefParams.up[0]);
				read_object(demo, f.DemoInfo.RefParams.up[1]);
				read_object(demo, f.DemoInfo.RefParams.up[2]);
				read_object(demo, f.DemoInfo.RefParams.frametime);
				read_object(demo, f.DemoInfo.RefParams.time);
				read_object(demo, f.DemoInfo.RefParams.intermission);
				read_object(demo, f.DemoInfo.RefParams.paused);
				read_object(demo, f.DemoInfo.RefParams.spectator);
				read_object(demo, f.DemoInfo.RefParams.onground);
				read_object(demo, f.DemoInfo.RefParams.waterlevel);
				read_object(demo, f.DemoInfo.RefParams.simorg[0]);
				read_object(demo, f.DemoInfo.RefParams.simorg[1]);
				read_object(demo, f.DemoInfo.RefParams.simorg[2]);
				read_object(demo, f.DemoInfo.RefParams.simvel[0]);
				read_object(demo, f.DemoInfo.RefParams.simvel[1]);
				read_object(demo, f.DemoInfo.RefParams.simvel[2]);
				read_object(demo, f.DemoInfo.RefParams.viewheight[0]);
				read_object(demo, f.DemoInfo.RefParams.viewheight[1]);
				read_object(demo, f.DemoInfo.RefParams.viewheight[2]);
				read_object(demo, f.DemoInfo.RefParams.idealpitch);
				read_object(demo, f.DemoInfo.RefParams.cl_viewangles[0]);
				read_object(demo, f.DemoInfo.RefParams.cl_viewangles[1]);
				read_object(demo, f.DemoInfo.RefParams.cl_viewangles[2]);
				read_object(demo, f.DemoInfo.RefParams.health);
				read_object(demo, f.DemoInfo.RefParams.crosshairangle[0]);
				read_object(demo, f.DemoInfo.RefParams.crosshairangle[1]);
				read_object(demo, f.DemoInfo.RefParams.crosshairangle[2]);
				read_object(demo, f.DemoInfo.RefParams.viewsize);
				read_object(demo, f.DemoInfo.RefParams.punchangle[0]);
				read_object(demo, f.DemoInfo.RefParams.punchangle[1]);
				read_object(demo, f.DemoInfo.RefParams.punchangle[2]);
				read_object(demo, f.DemoInfo.RefParams.maxclients);
				read_object(demo, f.DemoInfo.RefParams.viewentity);
				read_object(demo, f.DemoInfo.RefParams.playernum);
				read_object(demo, f.DemoInfo.RefParams.max_entities);
				read_object(demo, f.DemoInfo.RefParams.demoplayback);
				read_object(demo, f.DemoInfo.RefParams.hardware);
				read_object(demo, f.DemoInfo.RefParams.smoothing);
				read_object(demo, f.DemoInfo.RefParams.ptr_cmd);
				read_object(demo, f.DemoInfo.RefParams.ptr_movevars);
				read_object(demo, f.DemoInfo.RefParams.viewport[0]);
				read_object(demo, f.DemoInfo.RefParams.viewport[1]);
				read_object(demo, f.DemoInfo.RefParams.viewport[2]);
				read_object(demo, f.DemoInfo.RefParams.viewport[3]);
				read_object(demo, f.DemoInfo.RefParams.nextView);
				read_object(demo, f.DemoInfo.RefParams.onlyClientDraw);
				read_object(demo, f.DemoInfo.UserCmd.lerp_msec);
				read_object(demo, f.DemoInfo.UserCmd.msec);
				read_object(demo, f.DemoInfo.UserCmd.align_1);
				read_object(demo, f.DemoInfo.UserCmd.viewangles[0]);
				read_object(demo, f.DemoInfo.UserCmd.viewangles[1]);
				read_object(demo, f.DemoInfo.UserCmd.viewangles[2]);
				read_object(demo, f.DemoInfo.UserCmd.forwardmove);
				read_object(demo, f.DemoInfo.UserCmd.sidemove);
				read_object(demo, f.DemoInfo.UserCmd.upmove);
				read_object(demo, f.DemoInfo.UserCmd.lightlevel);
				read_object(demo, f.DemoInfo.UserCmd.align_2);
				read_object(demo, f.DemoInfo.UserCmd.buttons);
				read_object(demo, f.DemoInfo.UserCmd.impulse);
				read_object(demo, f.DemoInfo.UserCmd.weaponselect);
				read_object(demo, f.DemoInfo.UserCmd.align_3);
				read_object(demo, f.DemoInfo.UserCmd.align_4);
				read_object(demo, f.DemoInfo.UserCmd.impact_index);
				read_object(demo, f.DemoInfo.UserCmd.impact_position[0]);
				read_object(demo, f.DemoInfo.UserCmd.impact_position[1]);
				read_object(demo, f.DemoInfo.UserCmd.impact_position[2]);
				read_object(demo, f.DemoInfo.MoveVars.gravity);
				read_object(demo, f.DemoInfo.MoveVars.stopspeed);
				read_object(demo, f.DemoInfo.MoveVars.maxspeed);
				read_object(demo, f.DemoInfo.MoveVars.spectatormaxspeed);
				read_object(demo, f.DemoInfo.MoveVars.accelerate);
				read_object(demo, f.DemoInfo.MoveVars.airaccelerate);
				read_object(demo, f.DemoInfo.MoveVars.wateraccelerate);
				read_object(demo, f.DemoInfo.MoveVars.friction);
				read_object(demo, f.DemoInfo.MoveVars.edgefriction);
				read_object(demo, f.DemoInfo.MoveVars.waterfriction);
				read_object(demo, f.DemoInfo.MoveVars.entgravity);
				read_object(demo, f.DemoInfo.MoveVars.bounce);
				read_object(demo, f.DemoInfo.MoveVars.stepsize);
				read_object(demo, f.DemoInfo.MoveVars.maxvelocity);
				read_object(demo, f.DemoInfo.MoveVars.zmax);
				read_object(demo, f.DemoInfo.MoveVars.waveHeight);
				read_object(demo, f.DemoInfo.MoveVars.footsteps);
				std::vector<char> skyNameBuf(FRAME_NETMSG_DEMOINFO_MOVEVARS_SKYNAME_SIZE);
				read_objects(demo, skyNameBuf);
				skyNameBuf.push_back('\0');
				f.DemoInfo.MoveVars.skyName = skyNameBuf.data();
				read_object(demo, f.DemoInfo.MoveVars.rollangle);
				read_object(demo, f.DemoInfo.MoveVars.rollspeed);
				read_object(demo, f.DemoInfo.MoveVars.skycolor_r);
				read_object(demo, f.DemoInfo.MoveVars.skycolor_g);
				read_object(demo, f.DemoInfo.MoveVars.skycolor_b);
				read_object(demo, f.DemoInfo.MoveVars.skyvec_x);
				read_object(demo, f.DemoInfo.MoveVars.skyvec_y);
				read_object(demo, f.DemoInfo.MoveVars.skyvec_z);
				read_object(demo, f.DemoInfo.view[0]);
				read_object(demo, f.DemoInfo.view[1]);
				read_object(demo, f.DemoInfo.view[2]);
				read_object(demo, f.DemoInfo.viewmodel);

				read_object(demo, f.incoming_sequence);
				read_object(demo, f.incoming_acknowledged);
				read_object(demo, f.incoming_reliable_acknowledged);
				read_object(demo, f.incoming_reliable_sequence);
				read_object(demo, f.outgoing_sequence);
				read_object(demo, f.reliable_sequence);
				read_object(demo, f.last_reliable_sequence);

				int32_t length;
				read_object(demo, length);
				if (length < FRAME_NETMSG_MIN_MESSAGE_LENGTH
					|| length > FRAME_NETMSG_MAX_MESSAGE_LENGTH) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				if (demoSize - std::streamoff{ length } < demo.tellg()) {
					// Unexpected EOF.
					stop = true;
					break;
				}

				f.msg.resize(length);
				read_objects(demo, f.msg);

				entry.frames.emplace_back(new NetMsgFrame(std::move(f)));
			}
				break;
			}
		}
	}

	readFrames = true;
	// Now that we read the frames we can close the demo
	// as there isn't anything else we can read.
	demo.close();
}

void DemoFile::Save(const std::string& filename)
{
	nowide::ofstream o(filename.c_str(), std::ios::trunc | std::ios::binary);
	if (!o)
		throw std::runtime_error("Error opening the output file.");

	char signature[] = { 'H', 'L', 'D', 'E', 'M', 'O', '\0', '\0' };
	o.write(signature, sizeof(signature));
	write_object(o, header.demoProtocol);
	write_object(o, header.netProtocol);

	std::vector<char> mapNameBuf;
	std::copy(std::begin(header.mapName), std::end(header.mapName), std::back_inserter(mapNameBuf));
	mapNameBuf.push_back('\0');
	mapNameBuf.resize(HEADER_MAPNAME_SIZE);
	write_objects(o, mapNameBuf);

	std::vector<char> gameDirBuf;
	std::copy(std::begin(header.gameDir), std::end(header.gameDir), std::back_inserter(gameDirBuf));
	gameDirBuf.push_back('\0');
	gameDirBuf.resize(HEADER_GAMEDIR_SIZE);
	write_objects(o, gameDirBuf);

	write_object(o, header.mapCRC);

	// Directory offset goes here.
	auto dirOffsetPos = o.tellp();
	o.seekp(4, std::ios::cur);

	for (auto& entry : directoryEntries) {
		entry.offset = static_cast<int32_t>(o.tellp());

		// We need to write at least one NextSectionFrame, otherwise
		// the engine might break trying to play back the demo.
		bool wroteNextSection = false;
		for (const auto& frame : entry.frames) {
			write_object(o, frame->type);
			write_object(o, frame->time);
			write_object(o, frame->frame);

			switch (frame->type) {
			case DemoFrameType::DEMO_START:
				// No extra info.
				break;

			case DemoFrameType::CONSOLE_COMMAND:
			{
				auto f = reinterpret_cast<ConsoleCommandFrame*>(frame.get());

				std::vector<char> commandBuf;
				std::copy(std::begin(f->command), std::end(f->command), std::back_inserter(commandBuf));
				commandBuf.push_back('\0');
				commandBuf.resize(FRAME_CONSOLE_COMMAND_SIZE);
				write_objects(o, commandBuf);
			}
				break;

			case DemoFrameType::CLIENT_DATA:
			{
				auto f = reinterpret_cast<ClientDataFrame*>(frame.get());

				for (auto i = 0; i < 3; ++i)
					write_object(o, f->origin[i]);
				for (auto i = 0; i < 3; ++i)
					write_object(o, f->viewangles[i]);
				write_object(o, f->weaponBits);
				write_object(o, f->fov);
			}
				break;

			case DemoFrameType::NEXT_SECTION:
				// No extra info.
				wroteNextSection = true;
				break;

			case DemoFrameType::EVENT:
			{
				auto f = reinterpret_cast<EventFrame*>(frame.get());

				write_object(o, f->flags);
				write_object(o, f->index);
				write_object(o, f->delay);
				write_object(o, f->EventArgs.flags);
				write_object(o, f->EventArgs.entityIndex);
				for (auto i = 0; i < 3; ++i)
					write_object(o, f->EventArgs.origin[i]);
				for (auto i = 0; i < 3; ++i)
					write_object(o, f->EventArgs.angles[i]);
				for (auto i = 0; i < 3; ++i)
					write_object(o, f->EventArgs.velocity[i]);
				write_object(o, f->EventArgs.ducking);
				write_object(o, f->EventArgs.fparam1);
				write_object(o, f->EventArgs.fparam2);
				write_object(o, f->EventArgs.iparam1);
				write_object(o, f->EventArgs.iparam2);
				write_object(o, f->EventArgs.bparam1);
				write_object(o, f->EventArgs.bparam2);
			}
				break;

			case DemoFrameType::WEAPON_ANIM:
			{
				auto f = reinterpret_cast<WeaponAnimFrame*>(frame.get());

				write_object(o, f->anim);
				write_object(o, f->body);
			}
				break;

			case DemoFrameType::SOUND:
			{
				auto f = reinterpret_cast<SoundFrame*>(frame.get());

				write_object(o, f->channel);
				write_object(o, static_cast<int32_t>(f->sample.size()));
				write_objects(o, f->sample);
				write_object(o, f->attenuation);
				write_object(o, f->volume);
				write_object(o, f->flags);
				write_object(o, f->pitch);
			}
				break;

			case DemoFrameType::DEMO_BUFFER:
			{
				auto f = reinterpret_cast<DemoBufferFrame*>(frame.get());

				write_object(o, static_cast<int32_t>(f->buffer.size()));
				write_objects(o, f->buffer);
			}
				break;

			default:
			{
				auto f = reinterpret_cast<NetMsgFrame*>(frame.get());

				write_object(o, f->DemoInfo.timestamp);
				write_object(o, f->DemoInfo.RefParams.vieworg[0]);
				write_object(o, f->DemoInfo.RefParams.vieworg[1]);
				write_object(o, f->DemoInfo.RefParams.vieworg[2]);
				write_object(o, f->DemoInfo.RefParams.viewangles[0]);
				write_object(o, f->DemoInfo.RefParams.viewangles[1]);
				write_object(o, f->DemoInfo.RefParams.viewangles[2]);
				write_object(o, f->DemoInfo.RefParams.forward[0]);
				write_object(o, f->DemoInfo.RefParams.forward[1]);
				write_object(o, f->DemoInfo.RefParams.forward[2]);
				write_object(o, f->DemoInfo.RefParams.right[0]);
				write_object(o, f->DemoInfo.RefParams.right[1]);
				write_object(o, f->DemoInfo.RefParams.right[2]);
				write_object(o, f->DemoInfo.RefParams.up[0]);
				write_object(o, f->DemoInfo.RefParams.up[1]);
				write_object(o, f->DemoInfo.RefParams.up[2]);
				write_object(o, f->DemoInfo.RefParams.frametime);
				write_object(o, f->DemoInfo.RefParams.time);
				write_object(o, f->DemoInfo.RefParams.intermission);
				write_object(o, f->DemoInfo.RefParams.paused);
				write_object(o, f->DemoInfo.RefParams.spectator);
				write_object(o, f->DemoInfo.RefParams.onground);
				write_object(o, f->DemoInfo.RefParams.waterlevel);
				write_object(o, f->DemoInfo.RefParams.simorg[0]);
				write_object(o, f->DemoInfo.RefParams.simorg[1]);
				write_object(o, f->DemoInfo.RefParams.simorg[2]);
				write_object(o, f->DemoInfo.RefParams.simvel[0]);
				write_object(o, f->DemoInfo.RefParams.simvel[1]);
				write_object(o, f->DemoInfo.RefParams.simvel[2]);
				write_object(o, f->DemoInfo.RefParams.viewheight[0]);
				write_object(o, f->DemoInfo.RefParams.viewheight[1]);
				write_object(o, f->DemoInfo.RefParams.viewheight[2]);
				write_object(o, f->DemoInfo.RefParams.idealpitch);
				write_object(o, f->DemoInfo.RefParams.cl_viewangles[0]);
				write_object(o, f->DemoInfo.RefParams.cl_viewangles[1]);
				write_object(o, f->DemoInfo.RefParams.cl_viewangles[2]);
				write_object(o, f->DemoInfo.RefParams.health);
				write_object(o, f->DemoInfo.RefParams.crosshairangle[0]);
				write_object(o, f->DemoInfo.RefParams.crosshairangle[1]);
				write_object(o, f->DemoInfo.RefParams.crosshairangle[2]);
				write_object(o, f->DemoInfo.RefParams.viewsize);
				write_object(o, f->DemoInfo.RefParams.punchangle[0]);
				write_object(o, f->DemoInfo.RefParams.punchangle[1]);
				write_object(o, f->DemoInfo.RefParams.punchangle[2]);
				write_object(o, f->DemoInfo.RefParams.maxclients);
				write_object(o, f->DemoInfo.RefParams.viewentity);
				write_object(o, f->DemoInfo.RefParams.playernum);
				write_object(o, f->DemoInfo.RefParams.max_entities);
				write_object(o, f->DemoInfo.RefParams.demoplayback);
				write_object(o, f->DemoInfo.RefParams.hardware);
				write_object(o, f->DemoInfo.RefParams.smoothing);
				write_object(o, f->DemoInfo.RefParams.ptr_cmd);
				write_object(o, f->DemoInfo.RefParams.ptr_movevars);
				write_object(o, f->DemoInfo.RefParams.viewport[0]);
				write_object(o, f->DemoInfo.RefParams.viewport[1]);
				write_object(o, f->DemoInfo.RefParams.viewport[2]);
				write_object(o, f->DemoInfo.RefParams.viewport[3]);
				write_object(o, f->DemoInfo.RefParams.nextView);
				write_object(o, f->DemoInfo.RefParams.onlyClientDraw);
				write_object(o, f->DemoInfo.UserCmd.lerp_msec);
				write_object(o, f->DemoInfo.UserCmd.msec);
				write_object(o, f->DemoInfo.UserCmd.align_1);
				write_object(o, f->DemoInfo.UserCmd.viewangles[0]);
				write_object(o, f->DemoInfo.UserCmd.viewangles[1]);
				write_object(o, f->DemoInfo.UserCmd.viewangles[2]);
				write_object(o, f->DemoInfo.UserCmd.forwardmove);
				write_object(o, f->DemoInfo.UserCmd.sidemove);
				write_object(o, f->DemoInfo.UserCmd.upmove);
				write_object(o, f->DemoInfo.UserCmd.lightlevel);
				write_object(o, f->DemoInfo.UserCmd.align_2);
				write_object(o, f->DemoInfo.UserCmd.buttons);
				write_object(o, f->DemoInfo.UserCmd.impulse);
				write_object(o, f->DemoInfo.UserCmd.weaponselect);
				write_object(o, f->DemoInfo.UserCmd.align_3);
				write_object(o, f->DemoInfo.UserCmd.align_4);
				write_object(o, f->DemoInfo.UserCmd.impact_index);
				write_object(o, f->DemoInfo.UserCmd.impact_position[0]);
				write_object(o, f->DemoInfo.UserCmd.impact_position[1]);
				write_object(o, f->DemoInfo.UserCmd.impact_position[2]);
				write_object(o, f->DemoInfo.MoveVars.gravity);
				write_object(o, f->DemoInfo.MoveVars.stopspeed);
				write_object(o, f->DemoInfo.MoveVars.maxspeed);
				write_object(o, f->DemoInfo.MoveVars.spectatormaxspeed);
				write_object(o, f->DemoInfo.MoveVars.accelerate);
				write_object(o, f->DemoInfo.MoveVars.airaccelerate);
				write_object(o, f->DemoInfo.MoveVars.wateraccelerate);
				write_object(o, f->DemoInfo.MoveVars.friction);
				write_object(o, f->DemoInfo.MoveVars.edgefriction);
				write_object(o, f->DemoInfo.MoveVars.waterfriction);
				write_object(o, f->DemoInfo.MoveVars.entgravity);
				write_object(o, f->DemoInfo.MoveVars.bounce);
				write_object(o, f->DemoInfo.MoveVars.stepsize);
				write_object(o, f->DemoInfo.MoveVars.maxvelocity);
				write_object(o, f->DemoInfo.MoveVars.zmax);
				write_object(o, f->DemoInfo.MoveVars.waveHeight);
				write_object(o, f->DemoInfo.MoveVars.footsteps);
				std::vector<char> skyNameBuf;
				std::copy(std::begin(f->DemoInfo.MoveVars.skyName), std::end(f->DemoInfo.MoveVars.skyName), std::back_inserter(skyNameBuf));
				skyNameBuf.push_back('\0');
				skyNameBuf.resize(FRAME_NETMSG_DEMOINFO_MOVEVARS_SKYNAME_SIZE);
				write_objects(o, skyNameBuf);
				write_object(o, f->DemoInfo.MoveVars.rollangle);
				write_object(o, f->DemoInfo.MoveVars.rollspeed);
				write_object(o, f->DemoInfo.MoveVars.skycolor_r);
				write_object(o, f->DemoInfo.MoveVars.skycolor_g);
				write_object(o, f->DemoInfo.MoveVars.skycolor_b);
				write_object(o, f->DemoInfo.MoveVars.skyvec_x);
				write_object(o, f->DemoInfo.MoveVars.skyvec_y);
				write_object(o, f->DemoInfo.MoveVars.skyvec_z);
				write_object(o, f->DemoInfo.view[0]);
				write_object(o, f->DemoInfo.view[1]);
				write_object(o, f->DemoInfo.view[2]);
				write_object(o, f->DemoInfo.viewmodel);

				write_object(o, f->incoming_sequence);
				write_object(o, f->incoming_acknowledged);
				write_object(o, f->incoming_reliable_acknowledged);
				write_object(o, f->incoming_reliable_sequence);
				write_object(o, f->outgoing_sequence);
				write_object(o, f->reliable_sequence);
				write_object(o, f->last_reliable_sequence);

				write_object(o, static_cast<int32_t>(f->msg.size()));
				write_objects(o, f->msg);
			}
				break;
			}
		}

		if (!wroteNextSection) {
			DemoFrame f;
			f.type = DemoFrameType::NEXT_SECTION;
			f.time = 0;
			f.frame = 0;

			write_object(o, f.type);
			write_object(o, f.time);
			write_object(o, f.frame);
		}
	}

	auto dirOffset = o.tellp();
	write_object(o, static_cast<int32_t>(directoryEntries.size()));
	for (const auto& entry : directoryEntries) {
		write_object(o, entry.type);

		std::vector<char> descriptionBuf;
		std::copy(std::begin(entry.description), std::end(entry.description), std::back_inserter(descriptionBuf));
		descriptionBuf.push_back('\0');
		descriptionBuf.resize(DIR_ENTRY_DESCRIPTION_SIZE);
		write_objects(o, descriptionBuf);

		write_object(o, entry.flags);
		write_object(o, entry.CDTrack);
		write_object(o, entry.trackTime);
		write_object(o, entry.frameCount);
		write_object(o, entry.offset);
		write_object(o, entry.fileLength);
	}

	o.seekp(dirOffsetPos, std::ios::beg);
	write_object(o, static_cast<int32_t>(dirOffset));
	header.directoryOffset = static_cast<int32_t>(dirOffset);

	o.close();
}
