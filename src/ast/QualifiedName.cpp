/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/QualifiedName.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <map>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace souffle::ast {

/// Container of qualified names, provides interning by associating a unique
/// numerical index to each qualified name.
struct QNInterner {
public:
    explicit QNInterner() {
        qualifiedNames.emplace_back(QualifiedNameData{{}, ""});
        qualifiedNameToIndex.emplace("", 0);
    }

    /// Return the qualified name object for the given string.
    ///
    /// Each `.` character is treated as a separator.
    QualifiedName intern(std::string_view qn) {
        const auto It = qualifiedNameToIndex.find(qn);
        if (It != qualifiedNameToIndex.end()) {
            return QualifiedName{It->second};
        }

        const uint32_t index = static_cast<uint32_t>(qualifiedNames.size());

        QualifiedNameData qndata{splitString(qn, '.'), std::string{qn}};
        qualifiedNames.emplace_back(std::move(qndata));
        qualifiedNameToIndex.emplace(qualifiedNames.back().qualified, index);

        return QualifiedName{index};
    }

    /// Return the qualified name data object from the given index.
    const QualifiedNameData& at(uint32_t index) {
        return qualifiedNames.at(index);
    }

private:
    /// Store the qualified name data of interned qualified names.
    std::deque<QualifiedNameData> qualifiedNames;

    /// Mapping from a qualified name string representation to its index in
    /// `qualifiedNames`.
    std::unordered_map<std::string_view, uint32_t> qualifiedNameToIndex;
};

namespace {
/// The default qualified name interner instance.
QNInterner Interner;
}  // namespace

QualifiedName::QualifiedName() : index(0) {}
QualifiedName::QualifiedName(uint32_t idx) : index(idx) {}

const QualifiedNameData& QualifiedName::data() const {
    return Interner.at(index);
}

bool QualifiedName::operator==(const QualifiedName& other) const {
    return index == other.index;
}

bool QualifiedName::operator!=(const QualifiedName& other) const {
    return index != other.index;
}

void QualifiedName::append(const std::string& segment) {
    assert(segment.find('.') == std::string::npos);
    *this = Interner.intern(data().qualified + "." + segment);
}

void QualifiedName::prepend(const std::string& segment) {
    assert(segment.find('.') == std::string::npos);
    *this = Interner.intern(segment + "." + data().qualified);
}

void QualifiedName::append(const QualifiedName& rhs) {
    if (rhs.empty()) {
        return;
    }
    if (empty()) {
        index = rhs.index;
        return;
    }
    *this = Interner.intern(toString() + "." + rhs.toString());
}

/** convert to a string separated by fullstop */
const std::string& QualifiedName::toString() const {
    return data().qualified;
}

QualifiedName QualifiedName::fromString(std::string_view qname) {
    return Interner.intern(qname);
}

bool QualifiedName::lexicalLess(const QualifiedName& other) const {
    if (index == other.index) {
        return false;
    }
    return data().lexicalLess(other.data());
}

void QualifiedName::print(std::ostream& out) const {
    out << toString();
}

std::ostream& operator<<(std::ostream& out, const QualifiedName& qn) {
    out << qn.toString();
    return out;
}

const std::vector<std::string>& QualifiedName::getQualifiers() const {
    return data().segments;
}

uint32_t QualifiedName::getIndex() const {
    return index;
}

bool QualifiedName::empty() const {
    return index == 0;
}

QualifiedName QualifiedName::head() const {
    if (empty()) {
        return QualifiedName();
    }
    return fromString(data().segments.front());
}

QualifiedName QualifiedName::tail() const {
    const QualifiedNameData& qdata = data();
    if (qdata.segments.size() < 2) {
        return QualifiedName();
    } else {
        std::stringstream ss;
        ss << join(qdata.segments.begin() + 1, qdata.segments.end(), "");
        return fromString(ss.str());
    }
}

bool QualifiedNameData::lexicalLess(const QualifiedNameData& other) const {
    return std::lexicographical_compare(
            segments.begin(), segments.end(), other.segments.begin(), other.segments.end());
}

QualifiedName operator+(const std::string& head, const QualifiedName& tail) {
    QualifiedName res = tail;
    res.prepend(head);
    return res;
}

}  // namespace souffle::ast
