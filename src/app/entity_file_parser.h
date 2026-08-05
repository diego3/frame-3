// Format-swappable seam (docs/adr/0008): the interface concern (turning file bytes into an
// EntityDefNode) stays separate from the format-specific concern (understanding YAML/JSON/
// whatever syntax). Swapping formats later means writing a new IEntityFileParser implementation
// and changing which one gets constructed -- nothing in EntityDefNode, EntityFactory, or any
// ComponentLoader changes.
#ifndef ENTITY_FILE_PARSER_H
#define ENTITY_FILE_PARSER_H

#include <string>

#include "entity_def.h"

class IEntityFileParser {
public:
    virtual ~IEntityFileParser() = default;

    // Parse takes already-read file contents, not a path -- reading bytes off disk isn't a format
    // concern, and entity definition files are loose text files read once per load, not GPU/audio
    // resources, so they don't belong in ResourceCache<T> (Ch. 8, ADR-0004) either.
    virtual EntityDefNode Parse(const std::string &fileContents) = 0;
};

#endif // ENTITY_FILE_PARSER_H
