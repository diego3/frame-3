// Adapted IScreenElement (docs/adr/0016): the seam that lets a view (IGameView, docs/adr/0010)
// compose multiple independently updated/rendered/shown/hidden pieces -- a 3D scene, a HUD, a
// future pause menu -- instead of one monolithic VOnRender body. Dropped from the book's
// IScreenElement (GameCode4/interfaces.h), same category of cut docs/adr/0010 already made for
// IGameView: VOnRestore()/VOnLostDevice() (DirectX device-loss recovery, no raylib/OpenGL
// analogue) and VOnMsgProc(AppMsg) (raylib has no Win32 message queue -- input is already polled
// state, read directly by whichever VOnUpdate needs it). The book's own operator< comparator on
// the interface is replaced by whatever owns the elements sorting on VGetZOrder() itself (see
// HumanView::VOnRender, game/sandbox/human_view.cpp), not a method every element must carry.
#ifndef SCREEN_ELEMENT_H
#define SCREEN_ELEMENT_H

#include <cstdint>

class IScreenElement {
public:
    virtual ~IScreenElement() = default;

    virtual void VOnUpdate(float dt) = 0;
    virtual void VOnRender(float dt) = 0;

    virtual int VGetZOrder() const = 0;
    virtual void VSetZOrder(int zOrder) = 0;
    virtual bool VIsVisible() const = 0;
    virtual void VSetVisible(bool visible) = 0;
};

// Elements don't need to know their own id (unlike IGameView's GameViewId, which VOnAttach's own
// contract requires) -- whichever owner pushes them (HumanView) tracks id -> element pairs
// itself, so this is just a plain handle type, not something IScreenElement exposes.
using ScreenElementId = std::uint32_t;

#endif // SCREEN_ELEMENT_H
