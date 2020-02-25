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

namespace souffle {

/** Space of user-chosen tags that a relation can have */
enum class RelationTag {
    INPUT,        // relation read from csv
    OUTPUT,       // relation written to csv
    PRINTSIZE,    // number of tuples written to stdout
    OVERRIDABLE,  // rules defined in component can be overwritten by sub-component
    INLINE,       // inlined
    SUPPRESSED,   // warnings suppressed
    BRIE,         // use brie data-structure
    BTREE,        // use btree data-structure
    EQREL,        // use union data-structure
};

/** Space of qualifiers that a relation can have */
enum class RelationQualifier {
    INPUT,        // relation read from csv
    OUTPUT,       // relation written to csv
    PRINTSIZE,    // number of tuples written to stdout
    OVERRIDABLE,  // rules defined in component can be overwritten by sub-component
    INLINE,       // inlined
    SUPPRESSED,   // warnings suppressed
};

/** Space of internal representations that a relation can have */
enum class RelationRepresentation {
    DEFAULT,  // use default data-structure
    BRIE,     // use brie data-structure
    BTREE,    // use btree data-structure
    EQREL,    // use union data-structure
    INFO,     // info relation for provenance
};

/**
 * Check if a given relation tag sets a relation representation.
 */
inline bool isRelationRepresentationTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::BRIE:
        case RelationTag::BTREE:
        case RelationTag::EQREL:
            return true;
        default:
            return false;
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
        case RelationTag::SUPPRESSED:
            return true;
        default:
            return false;
    }
}

/**
 * Get the corresponding RelationQualifier for a valid RelationTag.
 */
inline RelationQualifier getRelationQualifierFromTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::INPUT:
            return RelationQualifier::INPUT;
        case RelationTag::OUTPUT:
            return RelationQualifier::OUTPUT;
        case RelationTag::PRINTSIZE:
            return RelationQualifier::PRINTSIZE;
        case RelationTag::OVERRIDABLE:
            return RelationQualifier::OVERRIDABLE;
        case RelationTag::INLINE:
            return RelationQualifier::INLINE;
        case RelationTag::SUPPRESSED:
            return RelationQualifier::SUPPRESSED;
        default:
            assert(false && "invalid relation tag");
    }
}

/**
 * Get the corresponding RelationRepresentation for a valid RelationTag.
 */
inline RelationRepresentation getRelationRepresentationFromTag(const RelationTag& tag) {
    switch (tag) {
        case RelationTag::BRIE:
            return RelationRepresentation::BRIE;
        case RelationTag::BTREE:
            return RelationRepresentation::BTREE;
        case RelationTag::EQREL:
            return RelationRepresentation::EQREL;
        default:
            assert(false && "invalid relation tag");
    }
}

inline std::ostream& operator<<(std::ostream& os, RelationQualifier qualifier) {
    switch (qualifier) {
        case RelationQualifier::INPUT:
            os << "input";
            break;
        case RelationQualifier::OUTPUT:
            os << "output";
            break;
        case RelationQualifier::PRINTSIZE:
            os << "printsize";
            break;
        case RelationQualifier::OVERRIDABLE:
            os << "overridable";
            break;
        case RelationQualifier::INLINE:
            os << "inline";
            break;
        case RelationQualifier::SUPPRESSED:
            os << "suppressed";
            break;
        default:
            assert(false && "unhandled relation qualifier");
    }

    return os;
}

inline std::ostream& operator<<(std::ostream& os, RelationRepresentation representation) {
    switch (representation) {
        case RelationRepresentation::BTREE:
            os << "btree";
            break;
        case RelationRepresentation::BRIE:
            os << "brie";
            break;
        case RelationRepresentation::EQREL:
            os << "eqrel";
            break;
        case RelationRepresentation::INFO:
            os << "info";
            break;
        case RelationRepresentation::DEFAULT:
        default:
            break;
    }

    return os;
}

}  // namespace souffle
