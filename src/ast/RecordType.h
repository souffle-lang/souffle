/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RecordType.h
 *
 * Defines the record type class
 *
 ***********************************************************************/

#pragma once

#include "ast/Attribute.h"
#include "ast/QualifiedName.h"
#include "ast/Type.h"
#include "parser/SrcLocation.h"
#include <iosfwd>
#include <vector>

namespace souffle::ast {

/**
 * @class RecordType
 * @brief Record type class for record type declarations
 *
 * A record type aggregates a list of fields (i.e. name & type) into a new type.
 * Each record type has a name making the record type unique.
 */
class RecordType : public Type {
public:
    RecordType(QualifiedName name, VecOwn<Attribute> fields, SrcLocation loc = {});

    /** Add field to record type */
    void add(std::string name, QualifiedName type);

    /** Get fields of record */
    std::vector<Attribute*> getFields() const;

    /** Set field type */
    void setFieldType(std::size_t idx, QualifiedName type);

    static bool classof(const Node*);

protected:
    void print(std::ostream& os) const override;

private:
    bool equal(const Node& node) const override;

    RecordType* cloning() const override;

private:
    /** record fields */
    VecOwn<Attribute> fields;
};

}  // namespace souffle::ast
