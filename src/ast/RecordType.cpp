/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "ast/RecordType.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/tinyformat.h"
#include <ostream>
#include <string>
#include <utility>

namespace souffle::ast {
RecordType::RecordType(QualifiedName name, VecOwn<Attribute> fields, SrcLocation loc)
        : Type(NK_RecordType, std::move(name), std::move(loc)), fields(std::move(fields)) {
    assert(allValidPtrs(this->fields));
}

void RecordType::add(std::string name, QualifiedName type) {
    fields.push_back(mk<Attribute>(std::move(name), std::move(type)));
}

std::vector<Attribute*> RecordType::getFields() const {
    return toPtrVector(fields);
}

void RecordType::setFieldType(std::size_t idx, QualifiedName type) {
    fields.at(idx)->setTypeName(std::move(type));
}

void RecordType::print(std::ostream& os) const {
    printAnnotations(os);
    os << tfm::format(".type %s = [%s]", getQualifiedName(), join(fields, ", "));
}

bool RecordType::equal(const Node& node) const {
    const auto& other = asAssert<RecordType>(node);
    return getQualifiedName() == other.getQualifiedName() && equal_targets(fields, other.fields);
}

RecordType* RecordType::cloning() const {
    return new RecordType(getQualifiedName(), clone(fields), getSrcLoc());
}

bool RecordType::classof(const Node* n) {
    return n->getKind() == NK_RecordType;
}

}  // namespace souffle::ast
