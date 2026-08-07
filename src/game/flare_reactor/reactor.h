// Reactor: the flare_reactor experiment's interactible beacon (docs/rfc/0001-flare-reactor-
// pipeline-experiment.md, Phase 3/4). Deliberately just `bool active`, not the RFC's original
// sketch (which also had a `cooldownRemaining` float) -- Phase 4's BeaconPulseProcess will flip
// active back to false itself, right before its own Succeed(), instead of a separate per-frame
// decrement system. Sidesteps BaseGameLogic::VOnUpdate's non-virtual gap for this one piece (see
// the RFC's "Questões em aberto") -- nothing needs a per-frame tick just to count this down.
//
// No ReactorTag alongside this component (see tags.h) -- an entity either has Reactor or it
// doesn't, so the component's own presence already is this entity's identity. Matches how the
// book's own Actor::type field turned out to be barely used in practice (one real call site,
// Network.cpp, with the book's own comment admitting "FUTURE WORK: This could be in a script").
#ifndef FLARE_REACTOR_REACTOR_H
#define FLARE_REACTOR_REACTOR_H

struct Reactor {
    bool active = false;
};

#endif // FLARE_REACTOR_REACTOR_H
