#include "app/entity/level_loader.h"

LevelLoader::LevelLoader(EntityFactory &entityFactory, IEntityFileParser &parser, FileReader readFile)
    : entityFactory_(entityFactory), parser_(parser), readFile_(std::move(readFile)) {}

std::vector<entt::entity> LevelLoader::Load(entt::registry &registry, EventManager &events,
                                             const std::string &levelPath) {
    EntityDefNode level = parser_.Parse(readFile_(levelPath));

    std::vector<entt::entity> spawned;
    for (const EntityDefNode &placement : level.Get("actors").AsList()) {
        std::string resourcePath = placement.Get("resource").AsString();
        EntityDefNode entityDef = parser_.Parse(readFile_(resourcePath));

        // "position" is sugar for an override targeting the Position component specifically (the
        // overwhelmingly common per-instance override) -- folded into the same overridesMap
        // MergeOverrides() below already handles, not a separate code path.
        EntityDefNode::Map overridesMap;
        if (const EntityDefNode *overrides = placement.TryGet("overrides")) {
            overridesMap = overrides->AsMap();
        }
        if (const EntityDefNode *position = placement.TryGet("position")) {
            overridesMap["Position"] = *position;
        }

        EntityDefNode merged = MergeOverrides(entityDef.Get("components"), EntityDefNode(overridesMap));
        entt::entity entity = entityFactory_.Create(registry, merged);
        spawned.push_back(entity);

        events.Queue(EvtData_EntitySpawned{entity});
    }

    return spawned;
}
