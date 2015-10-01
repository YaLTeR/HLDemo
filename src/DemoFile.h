#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <boost/nowide/fstream.hpp>

#include "DemoFrame.h"

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

	auto GetHeader() const { return header; }
	auto GetDirectoryEntryCount() const { return dirEntryCount; }
	auto GetDirectoryEntries() const { return dirEntries; }
	void ReadFrames();

	void Save(const std::string& filename);

	static bool IsValidDemoFile(const std::string& filename);

protected:
	boost::nowide::ifstream demo;
	std::streampos demoSize;

	void ReadHeader();
	DemoHeader header;

	void ReadDirectory();
	int32_t dirEntryCount;
	std::vector<DemoDirectoryEntry> dirEntries;

	bool readFrames;
};
