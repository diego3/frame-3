// Wraps DebugOverlay (debug_overlay.h, the F3 HUD) as an IScreenElement, for any view that wants
// to fold it into its own composed stack (docs/adr/0016) instead of driving it as bespoke
// free-function calls in main.cpp's own frame loop. Header-only (a handful of one-line overrides)
// same as game_view.h/screen_element.h, not a .cpp of its own.
//
// Only safe for a view that's alive for its game's *entire* run -- game/camera_fps/human_view.cpp
// (docs/adr/0017) is its only user so far, for exactly that reason: game/sandbox's HumanView is
// only alive during the GAMEPLAY screen, so folding DebugOverlay into its stack would lose
// coverage during LOGO/TITLE/OPTIONS/ENDING -- ADR-0016's own reasoning for keeping DebugOverlay
// outside any one view's stack there still applies unchanged; game/sandbox/main.cpp keeps calling
// UpdateDebugOverlay/DrawDebugOverlay directly.
#ifndef DEBUG_OVERLAY_SCREEN_ELEMENT_H
#define DEBUG_OVERLAY_SCREEN_ELEMENT_H

#include "debug_overlay.h"
#include "screen_element.h"

class DebugOverlayScreenElement : public IScreenElement {
public:
    void VOnUpdate(float dt) override { UpdateDebugOverlay(dt); }
    void VOnRender(float dt) override { (void)dt; DrawDebugOverlay(); }

    int VGetZOrder() const override { return zOrder_; }
    void VSetZOrder(int zOrder) override { zOrder_ = zOrder; }
    bool VIsVisible() const override { return visible_; }
    void VSetVisible(bool visible) override { visible_ = visible; }

private:
    int zOrder_ = 200;
    bool visible_ = true;
};

#endif // DEBUG_OVERLAY_SCREEN_ELEMENT_H
