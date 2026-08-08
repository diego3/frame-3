#include "screenshot_capture.h"

#include <cstdio>

#include <raylib.h>

namespace {
    // Scans outputDir for image-NNNN.png files and returns the highest NNNN found, or 0 if the
    // directory doesn't exist yet or has none -- the constructor uses this once so numbering
    // resumes correctly across restarts instead of overwriting an earlier session's captures.
    int HighestExistingIndex(const std::string &outputDir) {
        if (!DirectoryExists(outputDir.c_str())) return 0;

        FilePathList files = LoadDirectoryFilesEx(outputDir.c_str(), ".png", false);
        int highest = 0;
        for (unsigned int i = 0; i < files.count; ++i) {
            int index = 0;
            if (std::sscanf(GetFileName(files.paths[i]), "image-%d.png", &index) == 1 && index > highest) {
                highest = index;
            }
        }
        UnloadDirectoryFiles(files);
        return highest;
    }
}

ScreenshotCapture::ScreenshotCapture(EventManager &events, std::string outputDir)
    : outputDir_(std::move(outputDir)), nextIndex_(HighestExistingIndex(outputDir_) + 1) {
    if (!DirectoryExists(outputDir_.c_str())) MakeDirectory(outputDir_.c_str());

    events.Subscribe<EvtData_ScreenshotRequested>(
        [this](const EvtData_ScreenshotRequested &event) { OnScreenshotRequested(event); });
}

void ScreenshotCapture::OnScreenshotRequested(const EvtData_ScreenshotRequested &event) {
    (void)event;

    // TextFormat's buffer is raylib's own static/reused one -- copy into a real std::string right
    // away (path) before the second TextFormat call TakeScreenshot makes internally could reuse it.
    std::string path = TextFormat("%s/image-%04d.png", outputDir_.c_str(), nextIndex_);
    TakeScreenshot(path.c_str());
    TraceLog(LOG_INFO, "ScreenshotCapture: saved %s", path.c_str());
    ++nextIndex_;
}
