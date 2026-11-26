/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecordTable.h
 *
 * Data container implementing a map between records and their references.
 * Records are separated by arity, i.e., stored in different RecordMaps.
 *
 ***********************************************************************/

#pragma once

#include "souffle/RamTypes.h"
#include "souffle/utility/span.h"

#include <functional>
#include <initializer_list>

namespace souffle {

/** The interface of any Record Table. */
class RecordTable {
public:
    virtual ~RecordTable() {}

    virtual void setNumLanes(const std::size_t NumLanes) = 0;

    virtual RamDomain pack(const RamDomain* Tuple, const std::size_t Arity) = 0;

    virtual RamDomain pack(const std::initializer_list<RamDomain>& List) = 0;

    virtual const RamDomain* unpack(const RamDomain Ref, const std::size_t Arity) const = 0;

    /// Enumerate each record.
    virtual void enumerate(const std::function<void(const RamDomain* /*tuple*/, std::size_t /* arity*/,
                    RamDomain /* key */)>& Callback) const = 0;
};

/** @brief helper to convert tuple to record reference for the synthesiser */
template <class RecordTableT, std::size_t Arity>
RamDomain pack(RecordTableT&& recordTab, Tuple<RamDomain, Arity> const& tuple) {
    return recordTab.pack(tuple.data(), Arity);
}

/** @brief helper to convert tuple to record reference for the synthesiser */
template <class RecordTableT, std::size_t Arity>
RamDomain pack(RecordTableT&& recordTab, span<const RamDomain, Arity> tuple) {
    return recordTab.pack(tuple.data(), Arity);
}

/** @brief helper to pack using an initialization-list of RamDomain values. */
template <class RecordTableT>
RamDomain pack(RecordTableT&& recordTab, const std::initializer_list<RamDomain>&& initlist) {
    return recordTab.pack(std::data(initlist), initlist.size());
}

/**
 * @brief Construct a value belonging to an enumerated algebraic data type.
 *
 * An ADT is enumerated when none of its branches has fields.
 *
 * An ADT branch ID is the branch's zero-based position in the lexicographical
 * ordering of the ADT's qualified branch names.
 */
inline RamDomain packADTEnum(const RamDomain branch) {
    return branch;
}

/**
 * @brief Construct a value belonging to a non-enumerated algebraic data type.
 *
 * An ADT is non-enumerated when at least one of its branches has a field. This
 * function must also be used for an empty branch of such an ADT.
 *
 * The arguments must already be represented as RamDomain values, using the
 * same symbol and record tables as the program that will consume the ADT. This
 * helper hides the special record encodings used for empty, unary, and
 * multi-argument branches.
 *
 * @param branch Zero-based position in the lexicographical ordering of the
 * ADT's qualified branch names.
 * @param arguments Branch field values in declaration order, encoded as
 * RamDomain values.
 */
inline RamDomain packADT(
        RecordTable& recordTab, const RamDomain branch, const RamDomain* arguments, const std::size_t arity) {
    const RamDomain branchValue = arity == 1 ? arguments[0] : recordTab.pack(arguments, arity);
    return recordTab.pack({branch, branchValue});
}

/**
 * @brief Construct a non-enumerated ADT value from an initialization list.
 * @param branch Zero-based position in lexicographical branch-name order.
 * @param arguments Branch field values in declaration order, encoded as
 * RamDomain values.
 */
inline RamDomain packADT(
        RecordTable& recordTab, const RamDomain branch, const std::initializer_list<RamDomain>& arguments) {
    return packADT(recordTab, branch, std::data(arguments), arguments.size());
}

/**
 * @brief Construct a non-enumerated ADT value from a fixed-size tuple.
 * @param branch Zero-based position in lexicographical branch-name order.
 * @param arguments Branch field values in declaration order, encoded as
 * RamDomain values.
 */
template <std::size_t Arity>
RamDomain packADT(RecordTable& recordTab, const RamDomain branch, const Tuple<RamDomain, Arity>& arguments) {
    return packADT(recordTab, branch, arguments.data(), Arity);
}

/**
 * @brief Construct a non-enumerated ADT value from a span.
 * @param branch Zero-based position in lexicographical branch-name order.
 * @param arguments Branch field values in declaration order, encoded as
 * RamDomain values.
 */
template <std::size_t Arity>
RamDomain packADT(
        RecordTable& recordTab, const RamDomain branch, const span<const RamDomain, Arity> arguments) {
    return packADT(recordTab, branch, arguments.data(), arguments.size());
}

}  // namespace souffle
