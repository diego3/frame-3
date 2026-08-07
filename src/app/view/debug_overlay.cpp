#include "app/view/debug_overlay.h"

#include "raylib.h"

#if defined(__linux__) && !defined(PLATFORM_WEB)

#include <dirent.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    constexpr float kSampleIntervalSec = 0.5f;

    struct ProcStats {
        long rssKb = 0;
        long vmKb = 0;
        int threads = 0;
        int openFds = 0;
        float cpuPercent = 0.0f;
    };

    bool g_visible = false;
    float g_sampleAccum = 0.0f;
    ProcStats g_lastStats;
    // -1 means "no previous sample yet" -- CPU% needs a delta between two samples, so the first
    // sample after toggling the overlay on always reports 0%.
    long g_prevCpuTicks = -1;

    // Every /proc/self/status line worth reading here has the shape "Label:   value [unit]", so
    // one parse covers VmRSS, VmSize, and Threads: skip the label token, read the number after it.
    long ParseFirstNumber(const std::string &line) {
        std::istringstream iss(line);
        std::string label;
        long value = 0;
        iss >> label >> value;
        return value;
    }

    void ReadStatus(ProcStats &stats) {
        std::ifstream file("/proc/self/status");
        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("VmRSS:", 0) == 0) stats.rssKb = ParseFirstNumber(line);
            else if (line.rfind("VmSize:", 0) == 0) stats.vmKb = ParseFirstNumber(line);
            else if (line.rfind("Threads:", 0) == 0) stats.threads = (int)ParseFirstNumber(line);
        }
    }

    // /proc/self/fd is a directory with one entry per open file descriptor (each a symlink to
    // what it points at) plus the usual "." and ".." -- so the FD count is just dirent count - 2.
    int CountOpenFds() {
        DIR *dir = opendir("/proc/self/fd");
        if (dir == nullptr) return 0;

        int count = 0;
        while (readdir(dir) != nullptr) count++;
        closedir(dir);

        return count - 2;
    }

    // /proc/self/stat is a single machine-parsable line, not key:value like status. Field 2 is
    // "(comm)" -- the executable name in parens, which can itself contain spaces or parens -- so
    // fields are located from the last ')' rather than by naive whitespace splitting. Everything
    // after that is fixed-position; utime/stime (fields 14/15 overall) end up at indices 11/12
    // once field 3 (state) starts the split.
    float SampleCpuPercent(float wallDtSec) {
        std::ifstream file("/proc/self/stat");
        std::string content;
        std::getline(file, content);

        size_t closeParen = content.rfind(')');
        if (closeParen == std::string::npos) return 0.0f;

        std::istringstream rest(content.substr(closeParen + 2));
        std::vector<std::string> fields;
        std::string field;
        while (rest >> field) fields.push_back(field);
        if (fields.size() < 13) return 0.0f;

        long utime = std::stol(fields[11]);
        long stime = std::stol(fields[12]);
        long totalTicks = utime + stime;

        float cpuPercent = 0.0f;
        if (g_prevCpuTicks >= 0 && wallDtSec > 0.0f) {
            long deltaTicks = totalTicks - g_prevCpuTicks;
            long ticksPerSec = sysconf(_SC_CLK_TCK);
            float deltaCpuSec = (float)deltaTicks / (float)ticksPerSec;
            cpuPercent = (deltaCpuSec / wallDtSec) * 100.0f;
        }
        g_prevCpuTicks = totalTicks;

        return cpuPercent;
    }

    void Sample(float wallDtSec) {
        ProcStats stats;
        ReadStatus(stats);
        stats.openFds = CountOpenFds();
        stats.cpuPercent = SampleCpuPercent(wallDtSec);
        g_lastStats = stats;
    }
}

void UpdateDebugOverlay(float dt) {
    if (IsKeyPressed(KEY_F3)) g_visible = !g_visible;
    if (!g_visible) return;

    g_sampleAccum += dt;
    if (g_sampleAccum >= kSampleIntervalSec) {
        Sample(g_sampleAccum);
        g_sampleAccum = 0.0f;
    }
}

void DrawDebugOverlay() {
    if (!g_visible) return;

    const int x = 10;
    const int y = 10;
    const int lineHeight = 18;
    const int lineCount = 6;
    const int panelWidth = 220;

    DrawRectangle(x - 5, y - 5, panelWidth, lineCount * lineHeight + 10, Fade(BLACK, 0.6f));

    DrawText(TextFormat("FPS: %d", GetFPS()), x, y + 0 * lineHeight, 16, LIME);
    DrawText(TextFormat("CPU: %.1f%%", g_lastStats.cpuPercent), x, y + 1 * lineHeight, 16, RAYWHITE);
    DrawText(TextFormat("RSS: %.1f MB", g_lastStats.rssKb / 1024.0f), x, y + 2 * lineHeight, 16, RAYWHITE);
    DrawText(TextFormat("VMem: %.1f MB", g_lastStats.vmKb / 1024.0f), x, y + 3 * lineHeight, 16, RAYWHITE);
    DrawText(TextFormat("Threads: %d", g_lastStats.threads), x, y + 4 * lineHeight, 16, RAYWHITE);
    DrawText(TextFormat("Open FDs: %d", g_lastStats.openFds), x, y + 5 * lineHeight, 16, RAYWHITE);
}

#else // !(__linux__ && !PLATFORM_WEB) -- /proc doesn't exist here (PLATFORM_WEB, non-Linux)

void UpdateDebugOverlay(float) {}
void DrawDebugOverlay() {}

#endif
