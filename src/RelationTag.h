/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RelationTag.h
 *
 * Identifies the available relation tags, including qualifiers and
 * representations.
 ***********************************************************************/

#pragma once

#include "souffle/utility/MiscUtil.h"
#include <ostream>

namespace souffle {

/** Space of user-chosen tags that a relation can have */
enum class RelationTag {
    INPUT,         // relation read from csv
    OUTPUT,        // relation written to csv
    PRINTSIZE,     // number of tuples written to stdout
    OVERRIDABLE,   // rules defined in component can be overwritten by sub-component
    INLINE,        // inlined
    NO_INLINE,     // never inline
    MAGIC,         // enable magic-set on this relation
    NO_MAGIC,      // never magic-set on this relation
    SUPPRESSED,    // warnings suppressed
    BRIE,          // use brie data-structure
    BTREE,         // use btree data-structure
    BTREE_DELETE,  // use btree_delete data-structure
    EQREL,         // use union data-structure
};

/** Space of qualifiers that a relation can have */
enum class RelationQualifier {
    INPUT,        // relation read from csv
    OUTPUT,       // relation written to csv
    PRINTSIZE,    // number of tuples written to stdout
    OVERRIDABLE,  // rules defined in component can be overwritten by sub-component
    INLINE,       // inlined
    NO_INLINE,    // never inline
    MAGIC,        // enable magic-set on this relation
    NO_MAGIC,     // never magic-set on this relation
    SUPPRESSED,   // warnings suppressed
};

/** Space of internal representations that a relation can have */
enum class RelationRepresentation {
    DEFAULT,       // use default data-structure
    BRIE,          // use brie data-structure
    BTREE,         // use btree data-structure
    BTREE_DELETE,  // use btree_delete data-structure
    EQREL,         // use union data-structure
    PROVENANCE,    // use custom btree data-structure with provenance extras
    INFO,          // info relation for provenance
};

/**
 * Check if a given relation tag sets a relation representation.
 */
inline bool isRelationRepresentationTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::BRIE:
        case RelationTag::BTREE:
        case RelationTag::BTREE_DELETE:
        case RelationTag::EQREL: return true;
        default: return false;
    }
}

/**
 * Check if a given relation tag is a relation qualifier.
 */
inline bool isRelationQualifierTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::INPUT:
        case RelationTag::OUTPUT:
        case RelationTag::PRINTSIZE:
        case RelationTag::OVERRIDABLE:
        case RelationTag::INLINE:
        case RelationTag::NO_INLINE:
        case RelationTag::MAGIC:
        case RelationTag::NO_MAGIC:
        case RelationTag::SUPPRESSED: return true;
        default: return false;
    }
}

/**
 * Get the corresponding RelationQualifier for a valid RelationTag.
 */
inline RelationQualifier getRelationQualifierFromTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::INPUT: return RelationQualifier::INPUT;
        case RelationTag::OUTPUT: return RelationQualifier::OUTPUT;
        case RelationTag::PRINTSIZE: return RelationQualifier::PRINTSIZE;
        case RelationTag::OVERRIDABLE: return RelationQualifier::OVERRIDABLE;
        case RelationTag::INLINE: return RelationQualifier::INLINE;
        case RelationTag::NO_INLINE: return RelationQualifier::NO_INLINE;
        case RelationTag::MAGIC: return RelationQualifier::MAGIC;
        case RelationTag::NO_MAGIC: return RelationQualifier::NO_MAGIC;
        case RelationTag::SUPPRESSED: return RelationQualifier::SUPPRESSED;
        default: fatal("invalid relation tag");
    }
}

/**
 * Get the corresponding RelationRepresentation for a valid RelationTag.
 */
inline RelationRepresentation getRelationRepresentationFromTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::BRIE: return RelationRepresentation::BRIE;
        case RelationTag::BTREE: return RelationRepresentation::BTREE;
        case RelationTag::BTREE_DELETE: return RelationRepresentation::BTREE_DELETE;
        case RelationTag::EQREL: return RelationRepresentation::EQREL;
        default: fatal("invalid relation tag");
    }

    UNREACHABLE_BAD_CASE_ANALYSIS
}

inline std::ostream& operator<<(std::ostream& os, RelationTag qualifier) {
    switch (qualifier) {
        case RelationTag::INPUT: return os << "input";
        case RelationTag::OUTPUT: return os << "output";
        case RelationTag::PRINTSIZE: return os << "printsize";
        case RelationTag::OVERRIDABLE: return os << "overridable";
        case RelationTag::INLINE: return os << "inline";
        case RelationTag::NO_INLINE: return os << "no_inline";
        case RelationTag::MAGIC: return os << "magic";
        case RelationTag::NO_MAGIC: return os << "no_magic";
        case RelationTag::SUPPRESSED: return os << "suppressed";
        case RelationTag::BRIE: return os << "brie";
        case RelationTag::BTREE: return os << "btree";
        case RelationTag::BTREE_DELETE: return os << "btree_delete";
        case RelationTag::EQREL: return os << "eqrel";
    }

    UNREACHABLE_BAD_CASE_ANALYSIS
}

inline std::ostream& operator<<(std::ostream& os, RelationQualifier qualifier) {
    switch (qualifier) {
        case RelationQualifier::INPUT: return os << "input";
        case RelationQualifier::OUTPUT: return os << "output";
        case RelationQualifier::PRINTSIZE: return os << "printsize";
        case RelationQualifier::OVERRIDABLE: return os << "overridable";
        case RelationQualifier::INLINE: return os << "inline";
        case RelationQualifier::NO_INLINE: return os << "no_inline";
        case RelationQualifier::MAGIC: return os << "magic";
        case RelationQualifier::NO_MAGIC: return os << "no_magic";
        case RelationQualifier::SUPPRESSED: return os << "suppressed";
    }

    UNREACHABLE_BAD_CASE_ANALYSIS
}

inline std::ostream& operator<<(std::ostream& os, RelationRepresentation representation) {
    switch (representation) {
        case RelationRepresentation::BTREE: return os << "btree";
        case RelationRepresentation::BTREE_DELETE: return os << "btree_delete";
        case RelationRepresentation::BRIE: return os << "brie";
        case RelationRepresentation::EQREL: return os << "eqrel";
        case RelationRepresentation::PROVENANCE: return os << "provenance";
        case RelationRepresentation::INFO: return os << "info";
        case RelationRepresentation::DEFAULT: return os;
    }

    UNREACHABLE_BAD_CASE_ANALYSIS
}

}  // namespace souffle
