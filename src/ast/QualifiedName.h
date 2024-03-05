/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file QualifiedName.h
 *
 * Defines the qualified name class
 *
 ***********************************************************************/

#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace souffle::ast {

struct QualifiedNameData {
    using Segment = std::string;
    std::vector<Segment> segments;

    /// the whole qualified name with segments glued with dot
    std::string qualified;

    bool lexicalLess(const QualifiedNameData& other) const;
};

struct QNInterner;

/**
 * @brief Qualified Name class defines fully/partially qualified names
 * to identify objects in components.
 */
class QualifiedName {
private:
    friend struct QNInterner;
    explicit QualifiedName(uint32_t);

public:
    /** Build a QualifiedName from a dot-separated qualified name */
    static QualifiedName fromString(std::string_view qualname);

    /// The empty qualified name
    QualifiedName();

    QualifiedName(const QualifiedName&) = default;
    QualifiedName(QualifiedName&&) = default;
    QualifiedName& operator=(const QualifiedName&) = default;
    QualifiedName& operator=(QualifiedName&&) = default;

    const QualifiedNameData& data() const;

    /** append one qualifier */
    void append(const std::string& name);

    /** append another qualified name */
    void append(const QualifiedName& name);

    /** prepend one qualifier */
    void prepend(const std::string& name);

    /** check for emptiness */
    bool empty() const;

    /** get qualifiers */
    const std::vector<std::string>& getQualifiers() const;

    /** convert to a string separated by fullstop */
    const std::string& toString() const;

    QualifiedName head() const;

    QualifiedName tail() const;

    bool operator==(const QualifiedName& other) const;

    bool operator!=(const QualifiedName& other) const;

    /// Lexicographic less comparison.
    ///
    /// We don't offer `operator<` because it's a costly operation
    /// that should only be used when ordering is required.
    ///
    /// See type definitions of containers below.
    bool lexicalLess(const QualifiedName& other) const;

    /** print qualified name */
    void print(std::ostream& out) const;

    friend std::ostream& operator<<(std::ostream& out, const QualifiedName& id);

    /// Return the unique identifier of the interned qualified name.
    uint32_t getIndex() const;

private:
    /// index of this qualified name in the qualified-name interner
    uint32_t index;
};

/// Return the qualified name by the adding prefix segment in head of the qualified name.
QualifiedName operator+(const std::string& head, const QualifiedName& tail);

struct OrderedQualifiedNameLess {
    bool operator()(const QualifiedName& lhs, const QualifiedName& rhs) const {
        return lhs.lexicalLess(rhs);
    }
};

struct UnorderedQualifiedNameLess {
    bool operator()(const QualifiedName& lhs, const QualifiedName& rhs) const {
        return lhs.getIndex() < rhs.getIndex();
    }
};

struct QualifiedNameHash {
    std::size_t operator()(const QualifiedName& qn) const {
        return static_cast<std::size_t>(qn.getIndex());
    }
};

/// a map from qualified name to T where qualified name keys are ordered in
/// lexicographic order.
template <typename T>
using OrderedQualifiedNameMap = std::map<QualifiedName, T, OrderedQualifiedNameLess>;

/// a map from qualified name to T where qualified name keys are not ordered in
/// any deterministic order.
template <typename T>
using UnorderedQualifiedNameMap = std::unordered_map<QualifiedName, T, QualifiedNameHash>;

/// a multi-map from qualified name to T where qualified name keys are not ordered in
/// any deterministic order.
template <typename T>
using UnorderedQualifiedNameMultimap = std::unordered_multimap<QualifiedName, T, QualifiedNameHash>;

/// an ordered set of qualified name ordered in lexicographic order.
using OrderedQualifiedNameSet = std::set<QualifiedName, OrderedQualifiedNameLess>;

/// an unordered set of qualified name.
using UnorderedQualifiedNameSet = std::unordered_set<QualifiedName, QualifiedNameHash>;

template <typename Container>
OrderedQualifiedNameSet orderedQualifiedNameSet(const Container& cont) {
    return OrderedQualifiedNameSet(cont.cbegin(), cont.cend());
}

}  // namespace souffle::ast

template <>
struct std::hash<souffle::ast::QualifiedName> {
    std::size_t operator()(const souffle::ast::QualifiedName& qn) const noexcept {
        return static_cast<std::size_t>(qn.getIndex());
    }
};
