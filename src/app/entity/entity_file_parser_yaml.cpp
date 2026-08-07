#include "app/entity/entity_file_parser_yaml.h"

#include <yaml/Yaml.hpp>

// NOTE: this specific mini-yaml checkout (vendor/mini-yaml, pinned commit -- see build.sh/test.sh)
// does not parse flow-style maps/sequences ("{ x: 1, y: 2 }", "[a, b, c]") -- confirmed empirically
// while implementing this: a flow-style map parses as an empty scalar, not a Map. ADR-0008's own
// example YAML used flow style for compactness; entity definition files must use block style
// (newline + indentation) instead. Not previously known when ADR-0008 was written; flagged in its
// Implementation status note. mini-yaml's own gaps list (anchors/aliases/tags/multi-document, per
// ADR-0008's comparison table) already excluded flow style implicitly -- this makes it explicit.
namespace {
    EntityDefNode ConvertNode(const Yaml::Node &node) {
        if (node.IsMap()) {
            EntityDefNode::Map map;
            for (auto it = node.Begin(); it != node.End(); it++) {
                map.emplace((*it).first, ConvertNode((*it).second));
            }
            return EntityDefNode(std::move(map));
        }

        if (node.IsSequence()) {
            EntityDefNode::List list;
            for (auto it = node.Begin(); it != node.End(); it++) {
                list.push_back(ConvertNode((*it).second));
            }
            return EntityDefNode(std::move(list));
        }

        if (node.IsScalar()) return EntityDefNode(node.As<std::string>());

        return EntityDefNode();   // Yaml::Node::None -- an empty/missing value.
    }
}

EntityDefNode YamlEntityFileParser::Parse(const std::string &fileContents) {
    Yaml::Node root;
    Yaml::Parse(root, fileContents);
    return ConvertNode(root);
}
