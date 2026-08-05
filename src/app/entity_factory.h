// Component-loader registry over EntityDefNode (docs/adr/0008) -- the same registration shape
// EventManager::Subscribe and ResourceCache<T>'s constructor-supplied Loader already use elsewhere
// in this codebase, applied here to "component name -> data" instead of "event type -> handler" or
// "path -> resource."
//
// Deviates from ADR-0008's literal sketch in one way: the "unknown component" case takes a plain
// UnknownComponentHandler callback (default: no-op) instead of a hardcoded TraceLog call. TraceLog
// is a real raylib symbol needing libraylib.a linked -- calling it directly here would drag every
// consumer of this header (including entity_factory_test.cpp) into linking raylib, breaking this
// project's established "pure-logic systems need no window/GL context to test" property (see
// src/Makefile's tests section). A caller that wants raylib's TraceLog (or any other logging) just
// passes it as the handler; this class itself stays raylib-free.
#ifndef ENTITY_FACTORY_H
#define ENTITY_FACTORY_H

#include <functional>
#include <string>
#include <unordered_map>

#include <entt/entt.hpp>

#include "entity_def.h"

class EntityFactory {
public:
    using ComponentLoader = std::function<void(entt::registry &, entt::entity, const EntityDefNode &)>;
    using UnknownComponentHandler = std::function<void(const std::string &componentName)>;

    explicit EntityFactory(UnknownComponentHandler onUnknownComponent = [](const std::string &) {})
        : onUnknownComponent_(std::move(onUnknownComponent)) {}

    void RegisterComponentLoader(const std::string &componentName, ComponentLoader loader) {
        loaders_[componentName] = std::move(loader);
    }

    // `def` is one entity's definition -- a map from component name to that component's own data
    // (e.g. the "components:" node of an entity definition file). A component name with no
    // registered loader is skipped (onUnknownComponent_ is still called) rather than a hard
    // failure -- forward-compatible with a definition file written for a newer build than this
    // one, at the cost of silently ignoring a real typo in a component name.
    entt::entity Create(entt::registry &registry, const EntityDefNode &def) const {
        entt::entity entity = registry.create();

        for (const auto &[componentName, componentData] : def.AsMap()) {
            auto it = loaders_.find(componentName);
            if (it == loaders_.end()) {
                onUnknownComponent_(componentName);
                continue;
            }
            it->second(registry, entity, componentData);
        }

        return entity;
    }

private:
    UnknownComponentHandler onUnknownComponent_;
    std::unordered_map<std::string, ComponentLoader> loaders_;
};

#endif // ENTITY_FACTORY_H
