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

#include "RelationTag.h"
#include "VirtualFileSystem.h"
#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/Counter.h"
#include "ast/Directive.h"
#include "ast/FunctorDeclaration.h"
#include "ast/Pragma.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/SubsetType.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "parser/SrcLocation.h"
#include "reports/DebugReport.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace souffle {

class ParserDriver {
public:
    ParserDriver(Global& g);
    ParserDriver(Global& g, std::shared_ptr<FileSystem> fs);
    virtual ~ParserDriver() = default;

    void addRelation(Own<ast::Relation> r);
    void addFunctorDeclaration(Own<ast::FunctorDeclaration> f);
    void addDirective(Own<ast::Directive> d);
    void addType(Own<ast::Type> type);
    void addClause(Own<ast::Clause> c);
    void addComponent(Own<ast::Component> c);
    void addInstantiation(Own<ast::ComponentInit> ci);
    void addPragma(Own<ast::Pragma> p);

    void addIoFromDeprecatedTag(ast::Relation& r);
    Own<ast::SubsetType> mkDeprecatedSubType(
            ast::QualifiedName name, ast::QualifiedName attr, SrcLocation loc);

    std::set<RelationTag> addReprTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addDeprecatedTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags);
    std::set<RelationTag> addTag(RelationTag tag, std::vector<RelationTag> incompatible, SrcLocation tagLoc,
            std::set<RelationTag> tags);

    Own<ast::Counter> addDeprecatedCounter(SrcLocation tagLoc);

    Own<ast::TranslationUnit> parse(const std::string& filename, const std::string& codein,
            bool reducedConsecutiveNonLeadingWhitespaces, ErrorReport& errorReport, DebugReport& debugReport);
    Own<ast::TranslationUnit> parse(
            const std::string& code, ErrorReport& errorReport, DebugReport& debugReport);
    Own<ast::TranslationUnit> parseFromFS(
            const std::filesystem::path& path, ErrorReport& errorReport, DebugReport& debugReport);

    static Own<ast::TranslationUnit> parseTranslationUnit(Global& glb, const std::string& filename,
            const std::string& code, bool reducedConsecutiveNonLeadingWhitespaces, ErrorReport& errorReport,
            DebugReport& debugReport, std::shared_ptr<FileSystem> vfs = nullptr);
    static Own<ast::TranslationUnit> parseTranslationUnit(Global& glb, const std::string& code,
            ErrorReport& errorReport, DebugReport& debugReport, std::shared_ptr<FileSystem> vfs = nullptr);
    static Own<ast::TranslationUnit> parseTranslationUnitFromFS(Global& glb,
            const std::filesystem::path& path, ErrorReport& errorReport, DebugReport& debugReport,
            std::shared_ptr<FileSystem> vfs = nullptr);

    void warning(const WarnType warn, const SrcLocation& loc, const std::string& msg);
    void error(const SrcLocation& loc, const std::string& msg);
    void error(const std::string& msg);

    std::unique_ptr<std::string> readFile(const std::filesystem::path& path, std::error_code& ec);

    std::optional<std::filesystem::path> searchIncludePath(
            const std::string& IncludeString, const SrcLocation& IncludeLoc);

    // Return true if the given source location is visited for the first time by `.once`
    // and record that source location so that next calls will return false.
    //
    // The source location column number is non-significant.
    bool canEnterOnce(const SrcLocation& onceLoc);

    // Add a scanned comment.
    void addComment(const SrcLocation& Loc, const std::stringstream& Content);

    Own<ast::TranslationUnit> translationUnit;

    bool trace_scanning = false;

    // Canonical path and line number of location that have already been
    // visited by `.once`.
    std::set<std::pair<std::filesystem::path, int>> VisitedOnceLocations;

    // All the scanned comments.
    std::deque<std::pair<SrcLocation, std::string>> ScannedComments;

private:
    Global& glb;

    std::shared_ptr<FileSystem> vfs;
};

}  // end of namespace souffle
