// Level loading: actor placement over a level file (docs/adr/0009). Composes ADR-0008's pieces
// (EntityFactory, IEntityFileParser, EntityDefNode) with placement + per-instance overrides --
// doesn't need a BaseGameLogic host object to exist in (see the ADR's "Is it time for a
// BaseGameLogic?" section: not yet).
#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include <functional>
#include <string>
#include <vector>

#include <entt/entt.hpp>

#include "entity_def.h"
#include "entity_factory.h"
#include "entity_file_parser.h"
#include "event_manager.h"
#include "file_io.h"

// Fired for every entity LevelLoader::Load spawns (docs/adr/0009's "View-plurality seam, kept on
// purpose" -- fired unconditionally even though no RemoteView/AIView-equivalent subscriber exists
// yet, since the whole value of this event is that LevelLoader never has to change once one does).
// This project's first real (non-test-fake) event type.
struct EvtData_EntitySpawned {
    entt::entity entity;
};

class LevelLoader {
public:
    using FileReader = std::function<std::string(const std::string &path)>;

    // FileReader is injectable (deviates from ADR-0009's sketch, which didn't show file I/O
    // explicitly) so this can be unit-tested against in-memory fake files, the same fake-first
    // pattern EntityFactory/ResourceCache<T>'s own tests already use, instead of needing real
    // files on disk. Defaults to ReadWholeFile (file_io.h) for real use.
    explicit LevelLoader(EntityFactory &entityFactory, IEntityFileParser &parser,
                          FileReader readFile = ReadWholeFile);

    // Parses levelPath; for each actors[] entry, parses the referenced entity resource file
    // (ADR-0008), merges position/overrides on top via MergeOverrides() (entity_def.h), creates
    // the entity through entityFactory_, and Queue()s EvtData_EntitySpawned for it (ADR-0005's
    // Queue/DispatchQueued, not Emit -- per ADR-0009's own follow-up note, now that it exists).
    // Returns every entity created, in file order.
    std::vector<entt::entity> Load(entt::registry &registry, EventManager &events,
                                    const std::string &levelPath);

private:
    EntityFactory &entityFactory_;
    IEntityFileParser &parser_;
    FileReader readFile_;
};

#endif // LEVEL_LOADER_H
