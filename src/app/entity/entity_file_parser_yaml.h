// The only IEntityFileParser implementation landing with ADR-0008 -- backed by mini-yaml
// (vendor/mini-yaml, MIT), chosen for its minimal footprint and plain-Makefile vendoring (one
// header + one small .cpp, the same shape EnTT/doctest already use here). See ADR-0008's
// comparison table for why mini-yaml over rapidyaml/yaml-cpp/hand-rolling.
#ifndef ENTITY_FILE_PARSER_YAML_H
#define ENTITY_FILE_PARSER_YAML_H

#include "app/entity/entity_file_parser.h"

class YamlEntityFileParser : public IEntityFileParser {
public:
    // Throws Yaml::ParsingException (vendor/mini-yaml/yaml/Yaml.hpp) on invalid YAML.
    EntityDefNode Parse(const std::string &fileContents) override;
};

#endif // ENTITY_FILE_PARSER_YAML_H
