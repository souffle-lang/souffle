/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserDriver.h
 *
 * Defines the parser driver.
 *
 ***********************************************************************/

#pragma once

#include "RamTypes.h"
#include "RelationTag.h"
#include "SrcLocation.h"
#include "ast/AstQualifiedName.h"
#include "ast/AstTranslationUnit.h"
#include "parser.hh"
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace souffle {

class AstClause;
class AstComponent;
class AstComponentInit;
class AstFunctorDeclaration;
class AstPragma;
class AstRelation;
class AstIO;
class AstSubsetType;
class AstType;
class DebugReport;
class ErrorReport;

using yyscan_t = void*;

struct scanner_data {
    SrcLocation yylloc;

    /* Stack of parsed files */
    std::string yyfilename;
};

class ParserDriver {
public:
    virtual ~ParserDriver() = default;

    std::unique_ptr<AstTranslationUnit> translationUnit;

    void addRelation(std::unique_ptr<AstRelation> r);
    void addFunctorDeclaration(std::unique_ptr<AstFunctorDeclaration> f);
    void addIO(std::unique_ptr<AstIO> d);
    void addType(std::unique_ptr<AstType> type);
    void addClause(std::unique_ptr<AstClause> c);
    void addComponent(std::unique_ptr<AstComponent> c);
    void addInstantiation(std::unique_ptr<AstComponentInit> ci);
    void addPragma(std::unique_ptr<AstPragma> p);

    void addIoFromDeprecatedTag(AstRelation& r);
    Own<AstSubsetType> mkDeprecatedSubType(AstQualifiedName name, AstQualifiedName attr, SrcLocation loc);

    std::set<RelationTag> addReprTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addDeprecatedTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addTag(RelationTag tag, std::vector<RelationTag> incompatible, SrcLocation tagLoc,
            std::set<RelationTag> tags);

    bool trace_scanning = false;

    std::unique_ptr<AstTranslationUnit> parse(
            const std::string& filename, FILE* in, ErrorReport& errorReport, DebugReport& debugReport);
    std::unique_ptr<AstTranslationUnit> parse(
            const std::string& code, ErrorReport& errorReport, DebugReport& debugReport);
    static std::unique_ptr<AstTranslationUnit> parseTranslationUnit(
            const std::string& filename, FILE* in, ErrorReport& errorReport, DebugReport& debugReport);
    static std::unique_ptr<AstTranslationUnit> parseTranslationUnit(
            const std::string& code, ErrorReport& errorReport, DebugReport& debugReport);

    void warning(const SrcLocation& loc, const std::string& msg);
    void error(const SrcLocation& loc, const std::string& msg);
    void error(const std::string& msg);
};

}  // end of namespace souffle

#define YY_DECL yy::parser::symbol_type yylex(souffle::ParserDriver& driver, yyscan_t yyscanner)
YY_DECL;
