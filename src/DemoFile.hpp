#pragma once

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

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

/*
 * The std::string versions accept multibyte UTF-8 filenames,
 * the std::wstring versions accept wide UTF-16 filenames.
 */
class DemoFile
{
public:
	DemoFile(const std::string& filename);
	DemoFile(const std::wstring& filename);
	void ReadFrames();
	void Save(const std::string& filename);
	void Save(const std::wstring& filename);

	DemoHeader header;
	std::vector<DemoDirectoryEntry> directoryEntries;

	static bool IsValidDemoFile(const std::string& filename);
	static bool IsValidDemoFile(const std::wstring& filename);

protected:
	std::ifstream demo;
	std::streampos demoSize;

	void ConstructorInternal();
	void SaveInternal(std::ofstream o);
	static bool IsValidDemoFileInternal(std::ifstream in);

	void ReadHeader();
	void ReadDirectory();

	bool readFrames;
};
