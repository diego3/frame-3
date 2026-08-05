// Format-agnostic value tree for entity/component definition files (docs/adr/0008). Whatever
// concrete format a definition file is written in (YAML today; JSON or something else later, per
// IEntityFileParser -- entity_file_parser.h), a parser eagerly materializes the whole document
// into this one concrete type. Nothing downstream (EntityFactory, a ComponentLoader) ever touches
// a format-specific type, and this type itself needs no format library to construct directly (see
// entity_def_test.cpp) -- the same "concrete value type over virtual node interface" reasoning
// ADR-0004 used for ResourceCache<T>'s own key design.
//
// Deviates from ADR-0008's literal sketch in one way: scalars are stored as a single std::string
// alternative, not separate double/bool slots in the variant. Mirrors how mini-yaml (and most text
// formats) hand back scalar values -- as raw text, converted to a target type only on demand
// (AsFloat()/AsInt()) -- rather than eagerly guessing a scalar's "real" type while parsing, which
// is exactly how mini-yaml's own Node::As<T>() already works.
#ifndef ENTITY_DEF_H
#define ENTITY_DEF_H

#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class EntityDefNode {
public:
    using List = std::vector<EntityDefNode>;
    using Map = std::unordered_map<std::string, EntityDefNode>;

    EntityDefNode() = default;
    explicit EntityDefNode(std::string value) : value_(std::move(value)) {}
    explicit EntityDefNode(List value) : value_(std::move(value)) {}
    explicit EntityDefNode(Map value) : value_(std::move(value)) {}

    bool HasKey(const std::string &key) const {
        const Map *map = std::get_if<Map>(&value_);
        return map != nullptr && map->find(key) != map->end();
    }

    // Throws if this node isn't a map, or key isn't present -- for fields a caller genuinely
    // requires. Use TryGet() for fields that may legitimately be absent.
    const EntityDefNode &Get(const std::string &key) const {
        const Map *map = std::get_if<Map>(&value_);
        if (map == nullptr) throw std::runtime_error("EntityDefNode::Get: not a map");

        auto it = map->find(key);
        if (it == map->end()) throw std::runtime_error("EntityDefNode::Get: missing key '" + key + "'");

        return it->second;
    }

    // nullptr if this node isn't a map, or key isn't present -- never throws, for fields that may
    // legitimately be absent.
    const EntityDefNode *TryGet(const std::string &key) const {
        const Map *map = std::get_if<Map>(&value_);
        if (map == nullptr) return nullptr;

        auto it = map->find(key);
        return it != map->end() ? &it->second : nullptr;
    }

    std::string AsString(const std::string &fallback = "") const {
        const std::string *value = std::get_if<std::string>(&value_);
        return value != nullptr ? *value : fallback;
    }

    float AsFloat(float fallback = 0.0f) const { return AsNumber(fallback); }
    int AsInt(int fallback = 0) const { return AsNumber(fallback); }

    // Empty (not a throw) if this node isn't actually a list/map -- consistent with AsString/
    // AsFloat/AsInt's fallback-on-mismatch behavior, and with EntityFactory's own forward-
    // compatible "skip what doesn't match" handling of a malformed/unexpected definition file.
    const List &AsList() const {
        static const List kEmpty;
        const List *value = std::get_if<List>(&value_);
        return value != nullptr ? *value : kEmpty;
    }

    const Map &AsMap() const {
        static const Map kEmpty;
        const Map *value = std::get_if<Map>(&value_);
        return value != nullptr ? *value : kEmpty;
    }

private:
    template <typename T>
    T AsNumber(T fallback) const {
        const std::string *value = std::get_if<std::string>(&value_);
        if (value == nullptr) return fallback;

        std::istringstream iss(*value);
        T result;
        iss >> result;
        return iss.fail() ? fallback : result;
    }

    std::variant<std::monostate, std::string, List, Map> value_;
};

#endif // ENTITY_DEF_H
