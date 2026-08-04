# 8. Data-driven entity/component loading, starting with YAML behind a swappable format abstraction

- Status: Proposed
- Date: 2026-08-04

## Context

*Game Coding Complete* Ch. 6-7 defines Actors from XML: an `ActorFactory` reads a `<Actor>`
element, walks its child elements (`<PhysicsComponent>`, `<RenderComponent>`, ...), looks up a
registered creation function per tag name, and calls `VInit(TiXmlElement*)` on the resulting
`ActorComponent` to parse its own subtree. The payoff is real: designers/level authors add or tune
an enemy by editing a text file, not by recompiling C++.

[ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) already answered the Actor-vs-ECS half of
this (ECS via EnTT, entities are `entt::entity` handles, components are plain data), and flagged
"a future `SpawnEnemy(registry, position)`" as the shape for spawning — but every example since has
been a hardcoded, in-code factory. `.claude/skills/engine-architecture` §3 goes one step further,
noting that *if* enemy variants are needed later, "a small data table (enemy type → component
values) read by one generic factory" would be the move — but that's still an in-code table, and
the skill explicitly defers it ("not a reason to reach for EnTT's snapshot/prototype utilities
prematurely"). Neither addresses the book's actual idea: definitions living in a file on disk,
editable without a rebuild. No ADR has evaluated that until now.

This ADR was prompted by a request to design exactly that, with one explicit added constraint: the
file format itself should sit behind an abstraction, starting with YAML but swappable for JSON,
a flat text format, or something else later without rewriting the rest of the system.

## Decision — a component-loader registry over a format-agnostic node tree, YAML as the first parser

The book's design splits cleanly into two concerns already, and that split is exactly where the
format-swappable boundary belongs: **(1) turning file bytes into a tree of values**, and **(2)
turning a tree of values into component data on an entity**. Concern (2) should never need to know
which concrete format concern (1) came from — that's the whole point of asking for this to be
swappable.

### The format-agnostic value tree — not a virtual node interface

Rather than a polymorphic `DataNode` interface (which would have to be implemented once per
format, and would force every backend down to whatever the thinnest common denominator can
express), the parse step eagerly materializes the *entire* parsed document into one small, concrete
value type:

```cpp
// entity_def.h (sketch) -- one concrete type, not a per-format virtual interface. A YAML parser,
// a JSON parser, or a hand-rolled flat-text parser all produce this same type; nothing downstream
// (ComponentLoaders, EntityFactory) ever touches a format-specific type.
class EntityDefNode {
public:
    using Map = std::unordered_map<std::string, EntityDefNode>;
    using List = std::vector<EntityDefNode>;

    bool HasKey(const std::string &key) const;
    const EntityDefNode &Get(const std::string &key) const;   // throws/asserts if absent
    const EntityDefNode *TryGet(const std::string &key) const; // nullptr if absent -- optional fields

    std::string AsString(const std::string &fallback = "") const;
    float AsFloat(float fallback = 0.0f) const;
    int AsInt(int fallback = 0) const;
    const List &AsList() const;
    const Map &AsMap() const;

private:
    std::variant<std::monostate, std::string, double, bool, List, Map> value_;
};
```

Chosen over a virtual `IDataNode` per the same reasoning
[ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) used for `ResourceCache<T>`'s own
key design: the thing that actually varies (file syntax) is fully consumed during parsing, so
nothing downstream needs runtime polymorphism to stay format-agnostic — it just needs one shared
value type, which is simpler to use, easier to unit-test (buildable directly in a test with no
YAML/JSON library at all, the same way `ResourceCache<T>`'s tests use a fake loader instead of
real raylib types), and has no virtual-call overhead walking a config file's tree.

### The swappable half — `IEntityFileParser`, one implementation to start

```cpp
// entity_file_parser.h (sketch)
class IEntityFileParser {
public:
    virtual ~IEntityFileParser() = default;
    virtual EntityDefNode Parse(const std::string &fileContents) = 0;
};

// entity_file_parser_yaml.h/.cpp (sketch) -- the only implementation landing now.
class YamlEntityFileParser : public IEntityFileParser {
public:
    EntityDefNode Parse(const std::string &fileContents) override;
};
```

This *is* the abstraction the proposal asked for: swapping to JSON later means writing
`JsonEntityFileParser : IEntityFileParser` and changing which one gets constructed — nothing in
`EntityDefNode`, `EntityFactory`, or any `ComponentLoader` changes. `Parse` takes already-read file
contents rather than a path, deliberately: reading bytes off disk isn't a format concern, and entity
definition files are loose text files read once per load, not GPU/audio resources — they don't
belong in `ResourceCache<T>` (Ch. 8, ADR-0004), whose caching behavior (path-keyed, `shared_ptr`
lifetime, weak_ptr-driven unload) solves a problem (avoid re-uploading the same GPU/audio resource)
that doesn't apply here. A plain `std::ifstream` read is enough, matching ADR-0004's "loose files
under `assets/`, no ZIP bundling" decision.

### Component loaders — the same registry shape as `ResourceCache<T>`'s `Loader`, by name instead of by extension

```cpp
// entity_factory.h/.cpp (sketch)
class EntityFactory {
public:
    using ComponentLoader = std::function<void(entt::registry &, entt::entity, const EntityDefNode &)>;

    void RegisterComponentLoader(const std::string &componentName, ComponentLoader loader) {
        loaders_[componentName] = std::move(loader);
    }

    // `def` is one entity's definition -- a map from component name to that component's own data.
    entt::entity Create(entt::registry &registry, const EntityDefNode &def) {
        entt::entity entity = registry.create();
        for (const auto &[componentName, componentData] : def.AsMap()) {
            auto it = loaders_.find(componentName);
            if (it == loaders_.end()) {
                TraceLog(LOG_WARNING, "Unknown component '%s' in entity definition, skipping", componentName.c_str());
                continue;   // forward-compatible: a newer definition file referencing a component
                            // this build doesn't have yet shouldn't crash the whole entity's load.
            }
            it->second(registry, entity, componentData);
        }
        return entity;
    }

private:
    std::unordered_map<std::string, ComponentLoader> loaders_;
};
```

Registered once per real component type, the same way `Subscribe`/`RegisterLoader`-style calls
already work elsewhere in this codebase:

```cpp
factory.RegisterComponentLoader("Position", [](entt::registry &r, entt::entity e, const EntityDefNode &n) {
    r.emplace<Position>(e, Vector3{n.Get("x").AsFloat(), n.Get("y").AsFloat(), n.Get("z").AsFloat()});
});
factory.RegisterComponentLoader("Health", [](entt::registry &r, entt::entity e, const EntityDefNode &n) {
    r.emplace<Health>(e, n.Get("max").AsFloat());
});
factory.RegisterComponentLoader("EnemyTag", [](entt::registry &, entt::entity e, const EntityDefNode &) {
    // marker component -- no data to parse
});
```

An example definition file (`assets/entities/enemy_slime.yaml`):

```yaml
components:
  Position: { x: 0, y: 0, z: 0 }
  Health: { max: 50 }
  EnemyTag: {}
```

And loading it:

```cpp
YamlEntityFileParser parser;
EntityDefNode def = parser.Parse(ReadWholeFile("assets/entities/enemy_slime.yaml"));
entt::entity enemy = factory.Create(registry, def.Get("components"));
```

This directly fulfills the exact future extension the engine-architecture skill's §3 already
named ("a small data table read by one generic factory") — except the table now lives in an
editable file instead of an in-code array, which was the actual point of the book's idea.

**Not a replacement for hardcoded factories.** `SpawnEnemy(registry, position)`-style functions
remain the right tool for anything computed at runtime with no meaningful authored data (a
projectile spawned along the player's aim vector, for instance). `EntityFactory` is for content a
human authors once and tunes without recompiling — the two coexist; this ADR doesn't retire the
other.

## Options considered for the YAML parser

Vendoring a YAML library needs the same lens ADR-0006 (test framework) already applied: does it
fit this repo's Makefile-only build (no CMake, per ADR-0001 Decision 3)?

| | **mini-yaml** (recommended) | **rapidyaml (ryml)** | **yaml-cpp** | **Hand-roll our own "YAML-ish" parser** |
|---|---|---|---|---|
| Vendoring shape | Small: one header + one small `.cpp` (MIT). Compiles via one more Makefile rule, the same pattern `app/process_manager.cpp` already uses. | Single-header distribution exists (an amalgamated release asset), but the source project itself is large/template-heavy; getting the single-header form is an extra step beyond "clone at a tag." | Builds as its own library, **via CMake only** — no alternative build system provided upstream. | None — no new dependency. |
| Fits the Makefile-only constraint (ADR-0001 Decision 3, reaffirmed by ADR-0006) | Yes. | Yes, once the amalgamated header is obtained. | **No** — would reopen the CMake question both of those ADRs already closed, for a dependency this time instead of a test framework. | Trivially yes. |
| YAML spec coverage | Partial by design (maps/sequences/scalars; author's own docs note it skips anchors/aliases/tags/multi-document) — but that's *all* an entity definition file in this proposal actually uses. | Full YAML 1.2, actively maintained, fast. | Full YAML 1.2, the most mature/widely-used option. | Whatever we implement — not real YAML, just YAML-*shaped* syntax; would not be validated by standard YAML tooling/linters. |
| Maintenance / community | Smaller, less active than the other two real libraries. | Active, growing. | Very mature, the de facto standard C++ YAML library. | None — we own every bug. |
| Matches this project's stated appetite | Smallest footprint that's still genuinely YAML (not a bespoke dialect) — same size-conscious call as picking doctest over Catch2/GoogleTest in ADR-0006. | Reasonable, more integration friction than mini-yaml for no capability this proposal needs yet. | Rejected outright on the CMake constraint alone, independent of its (real) quality. | Rejected because the request was specifically for YAML — a hand-rolled subset wouldn't be YAML, undermining the reason to name a standard format at all (editor support, existing tooling, contributors already knowing the syntax). |

### Recommendation: **mini-yaml**

Covers exactly what an entity/component definition file needs (nested maps, sequences, scalars),
vendors the same way EnTT/doctest already do (clone at a pinned tag, wire into the Makefile), and
doesn't reopen the CMake question ADR-0001/ADR-0006 already closed. Its known gaps (anchors,
aliases, multi-document, YAML tags) are exactly the parts of the YAML spec this use case has no
reason to touch — a level/entity definition file is a flat, human-authored data shape, not a
config format needing YAML's more exotic features. If that changes later (e.g. wanting anchors to
de-duplicate shared stat blocks across many enemy definitions), the `IEntityFileParser` boundary
is exactly what makes swapping to rapidyaml or yaml-cpp at that point a contained change — a new
`*EntityFileParser` implementation, not a rewrite of `EntityDefNode`, `EntityFactory`, or any
`ComponentLoader`.

## Tradeoffs accepted

- `EntityDefNode` materializes the whole parsed document up front, rather than lazily wrapping the
  backend library's own tree — the right call for small, human-authored config files (a handful of
  entities' worth of component data), would be the wrong call for large data files; revisit only
  if entity definition files grow far beyond that.
- mini-yaml's partial spec coverage (no anchors/aliases/tags/multi-document) is a real functional
  limit, not just a vendoring convenience — accepted because nothing in this proposal's actual use
  case needs those features today; the parser abstraction exists specifically so this isn't a
  one-way door.
- Unknown component names in a definition file are skipped with a warning, not a hard failure —
  chosen for forward-compatibility (an older build reading a definition file written for a newer
  one), at the cost of silently ignoring a real typo in a component name. No validation/schema
  layer is proposed here to catch that class of mistake; revisit if that turns out to matter in
  practice.
- No hot-reload (re-`Parse`ing a definition file when it changes on disk while the game is
  running) — not proposed here; entity definitions load once, same as any other asset today.
- No support yet for one entity definition referencing/extending another (the book's XML actors
  can do inheritance-like composition) — not proposed here; revisit only once duplicated
  definitions across many similar entity files becomes a real, felt problem.

## Consequences / follow-ups

- `.claude/skills/engine-architecture` §3 should gain a note pointing at this ADR once
  `EntityFactory`/`EntityDefNode`/`YamlEntityFileParser` land in code, per the skill's own "update
  once it lands" convention.
- Entity definition files live under `assets/entities/` (matching `CONVENTIONS.md`'s existing
  per-context asset organization), loaded as loose files — no new asset-bundling concern beyond
  what ADR-0004 already decided.
- mini-yaml needs vendoring (pinned tag, a Makefile rule compiling its `.cpp`) the first time this
  actually lands in code — this ADR sketches the shape but doesn't wire the build yet.
- First real `ComponentLoader` registrations should also be the first test of whether
  `EntityDefNode::Get`'s throw-on-missing-key behavior (vs. a `TryGet`-only API) is the right
  default ergonomics for component loaders that have genuinely optional fields.
- If/when a second format (JSON, or otherwise) actually gets requested, that implementation should
  be the first real test of whether `IEntityFileParser`'s single `Parse(fileContents)` method is
  a wide-enough seam, or whether some format needs something the interface doesn't yet expose.

## Open Questions

- **Should `EntityFactory::Create` also handle nested/child entities** (e.g. a turret entity
  definition that also spawns an attached projectile-spawn-point entity)? Not addressed here;
  likely relates to the still-open scene-graph/hierarchy question
  ([ADR-0002](0002-scene-graph-hierarchy-options.md), status Proposed) rather than something to
  solve inside `EntityFactory` itself.
- **Validation/schema** for definition files (catching a typo'd component name or a
  wrong-shaped value at load time with a clear error, vs. today's silent-skip-with-warning) — not
  designed here; revisit if bad definition files become a recurring authoring problem.

## References

- *Game Coding Complete, 4th Edition* (McShaffry & Graham), Ch. 6-7 — `ActorFactory`,
  `ActorComponent::VInit(TiXmlElement*)`, XML-defined Actors.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) — ECS via EnTT; the `SpawnEnemy(registry,
  position)` hardcoded-factory shape this ADR extends to file-driven data.
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) — `ResourceCache<T>`'s
  loose-files-no-ZIP decision (reused here) and its "concrete value type over virtual node
  interface" reasoning (reapplied to `EntityDefNode`).
- [ADR-0006](0006-doctest-for-unit-tests.md) — the Makefile-only vendoring constraint (no CMake)
  this ADR's YAML-library comparison reapplies.
- `.claude/skills/engine-architecture/SKILL.md` §3 — the "small data table read by one generic
  factory" future extension this ADR designs for real.
- [mini-yaml](https://github.com/jimmiebergmann/mini-yaml), [rapidyaml](https://github.com/biojppm/rapidyaml),
  [yaml-cpp](https://github.com/jbeder/yaml-cpp) — the three real YAML libraries compared above.
