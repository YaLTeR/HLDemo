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

	std::vector<std::unique_ptr<DemoFrame>> frames;

	DemoDirectoryEntry() = default;
	~DemoDirectoryEntry() = default;
	DemoDirectoryEntry(const DemoDirectoryEntry& e);
	DemoDirectoryEntry(DemoDirectoryEntry&&) = default;
	DemoDirectoryEntry& operator= (DemoDirectoryEntry e);

private:
	void swap(DemoDirectoryEntry& e);
};

/*
 * The std::string versions accept multibyte UTF-8 filenames,
 * the std::wstring versions accept wide UTF-16 filenames.
 */
class DemoFile
{
public:
	DemoFile(std::string filename, bool read_frames);
	DemoFile(std::wstring filename, bool read_frames);
	void ReadFrames();

	inline bool DidReadFrames() const { return readFrames; }
	inline const std::string& GetFilename() const { return filename; }

	void Save() const;
	void Save(const std::string& filename) const;
	void Save(const std::wstring& filename) const;

	DemoHeader header;
	std::vector<DemoDirectoryEntry> directoryEntries;

	static bool IsValidDemoFile(const std::string& filename);
	static bool IsValidDemoFile(const std::wstring& filename);

protected:
	std::string filename;
	bool readFrames;

	void ConstructorInternal(std::ifstream demo, bool read_frames);
	void ReadFramesInternal(std::ifstream& demo, size_t demoSize);
	void SaveInternal(std::ofstream o) const;
	static bool IsValidDemoFileInternal(std::ifstream in);

	void ReadHeader(std::ifstream& demo);
	void ReadDirectory(std::ifstream& demo, size_t demoSize);

private:
	void swap(DemoFile& f);
};
