#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <boost/nowide/fstream.hpp>

#include "DemoFrame.hpp"

struct DemoHeader {
	int32_t netProtocol;
	int32_t demoProtocol;
	std::string mapName;
	std::string gameDir;
	int32_t mapCRC;
	int32_t directoryOffset;
};

struct DemoDirectoryEntry {
	int32_t type;
	std::string description;
	int32_t flags;
	int32_t CDTrack;
	float trackTime;
	int32_t frameCount;
	int32_t offset;
	int32_t fileLength;

	std::vector<std::shared_ptr<DemoFrame>> frames;
};

class DemoFile
{
public:
	DemoFile(const std::string& filename);
	void ReadFrames();
	void Save(const std::string& filename);

	DemoHeader header;
	std::vector<DemoDirectoryEntry> directoryEntries;

	static bool IsValidDemoFile(const std::string& filename);

protected:
	boost::nowide::ifstream demo;
	std::streampos demoSize;

	void ReadHeader();
	void ReadDirectory();

	bool readFrames;
};
