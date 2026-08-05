// Small, shared whole-file read/write helpers -- not a resource cache (ADR-0004 exists for that;
// these files are read/written once, not GPU/audio content), not a general filesystem
// abstraction, just enough for the two real callers: LevelLoader (docs/adr/0009, reads entity/
// level definition files) and EngineConfig/GameConfig (docs/adr/0011, reads and -- unlike
// anything ADR-0008 needed -- writes a config file on first run).
#ifndef FILE_IO_H
#define FILE_IO_H

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

// Returns false (does not throw) if path can't be opened -- the common, expected case being
// "first run, config file doesn't exist yet" (ADR-0011), not an error to report.
inline bool TryReadWholeFile(const std::string &path, std::string &out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    std::ostringstream contents;
    contents << file.rdbuf();
    out = contents.str();
    return true;
}

// Throws std::runtime_error if path can't be opened -- for callers where the file is expected to
// exist (a level/entity definition file referenced by name should exist; a missing one is a real
// error, not a "use defaults" case the way a missing config file is).
inline std::string ReadWholeFile(const std::string &path) {
    std::string contents;
    if (!TryReadWholeFile(path, contents)) throw std::runtime_error("Cannot open file: " + path);
    return contents;
}

// Creates path's parent directory if needed (config/ may not exist yet on first run) and
// overwrites path with contents. Throws std::runtime_error if the file can't be opened for
// writing.
inline void WriteWholeFile(const std::string &path, const std::string &contents) {
    std::filesystem::path fsPath(path);
    if (fsPath.has_parent_path()) std::filesystem::create_directories(fsPath.parent_path());

    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot write file: " + path);
    file << contents;
}

#endif // FILE_IO_H
