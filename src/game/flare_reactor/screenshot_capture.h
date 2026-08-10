// ScreenshotCapture: dev/debug tool that turns EvtData_ScreenshotRequested (events.h) into a PNG
// on disk under screenshots/, numbered image-0001.png, image-0002.png, etc. Event-driven -- not a
// raw TakeScreenshot() call inline in FlareReactorView -- on purpose: KEY_P (human_view.cpp) is
// only today's one source of the event; nothing stops a second source (see events.h's header
// comment on the "AI loop taking its own screenshots" idea) from Queue<EvtData_ScreenshotRequested>
// -ing the same event with zero changes needed here.
//
// Numbering survives process restarts: the constructor scans outputDir once for the highest
// existing image-NNNN.png index and continues from there, instead of always restarting at 0001 and
// silently overwriting a previous session's captures.
#ifndef FLARE_REACTOR_SCREENSHOT_CAPTURE_H
#define FLARE_REACTOR_SCREENSHOT_CAPTURE_H

#include <string>

#include "app/events/event_manager.h"
#include "events.h"

class ScreenshotCapture {
public:
    // outputDir: relative to the working directory the binary is run from -- same convention as
    // every other resources/config path in this project (see .claude/skills/run/SKILL.md). Created
    // (MakeDirectory) if it doesn't exist yet.
    explicit ScreenshotCapture(EventManager &events, std::string outputDir = "screenshots");

private:
    void OnScreenshotRequested(const EvtData_ScreenshotRequested &event);

    std::string outputDir_;
    int nextIndex_;
};

#endif // FLARE_REACTOR_SCREENSHOT_CAPTURE_H
