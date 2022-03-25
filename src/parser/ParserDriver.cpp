/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ParserDriver.cpp
 *
 * Defines the parser driver.
 *
 ***********************************************************************/

#include "parser/ParserDriver.h"
#include "Global.h"
#include "ast/Clause.h"
#include "ast/Component.h"
#include "ast/ComponentInit.h"
#include "ast/Directive.h"
#include "ast/FunctorDeclaration.h"
#include "ast/Pragma.h"
#include "ast/Program.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/SubsetType.h"
#include "ast/TranslationUnit.h"
#include "ast/Type.h"
#include "ast/utility/Utils.h"
#include "parser/parser.hh"
#include "reports/ErrorReport.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/StreamUtil.h"
#include "souffle/utility/StringUtil.h"
#include "souffle/utility/tinyformat.h"
#include <memory>
#include <utility>
#include <vector>

using YY_BUFFER_STATE = struct yy_buffer_state*;
extern YY_BUFFER_STATE yy_scan_string(const char*, yyscan_t scanner);
extern int yylex_destroy(yyscan_t scanner);
extern int yylex_init_extra(ScannerInfo* data, yyscan_t* scanner);
extern void yyset_in(FILE* in_str, yyscan_t scanner);
extern void yyset_debug(int, yyscan_t scanner);

namespace souffle {

Own<ast::TranslationUnit> ParserDriver::parse(
        const std::string& filename, FILE* in, ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = mk<ast::TranslationUnit>(mk<ast::Program>(), errorReport, debugReport);
    yyscan_t scanner;
    ScannerInfo data;
    data.yyfilename = filename;
    yylex_init_extra(&data, &scanner);
    yyset_debug(0, scanner);
    yyset_in(in, scanner);

    yy::parser parser(*this, scanner);
    parser.parse();

    yylex_destroy(scanner);

    return std::move(translationUnit);
}

Own<ast::TranslationUnit> ParserDriver::parse(
        const std::string& code, ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = mk<ast::TranslationUnit>(mk<ast::Program>(), errorReport, debugReport);

    ScannerInfo data;
    data.yyfilename = "<in-memory>";
    yyscan_t scanner;
    yylex_init_extra(&data, &scanner);
    yyset_debug(0, scanner);
    yy_scan_string(code.c_str(), scanner);
    yy::parser parser(*this, scanner);
    parser.parse();

    yylex_destroy(scanner);

    return std::move(translationUnit);
}

Own<ast::TranslationUnit> ParserDriver::parseTranslationUnit(
        const std::string& filename, FILE* in, ErrorReport& errorReport, DebugReport& debugReport) {
    ParserDriver parser;
    return parser.parse(filename, in, errorReport, debugReport);
}

Own<ast::TranslationUnit> ParserDriver::parseTranslationUnit(
        const std::string& code, ErrorReport& errorReport, DebugReport& debugReport) {
    ParserDriver parser;
    return parser.parse(code, errorReport, debugReport);
}

void ParserDriver::addPragma(Own<ast::Pragma> p) {
    ast::Program& program = translationUnit->getProgram();
    program.addPragma(std::move(p));
}

void ParserDriver::addFunctorDeclaration(Own<ast::FunctorDeclaration> f) {
    const std::string& name = f->getName();
    ast::Program& program = translationUnit->getProgram();
    const ast::FunctorDeclaration* existingFunctorDecl = ast::getFunctorDeclaration(program, f->getName());
    if (existingFunctorDecl != nullptr) {
        Diagnostic err(Diagnostic::Type::ERROR,
                DiagnosticMessage("Redefinition of functor " + toString(name), f->getSrcLoc()),
                {DiagnosticMessage("Previous definition", existingFunctorDecl->getSrcLoc())});
        translationUnit->getErrorReport().addDiagnostic(err);
    } else {
        program.addFunctorDeclaration(std::move(f));
    }
}

void ParserDriver::addRelation(Own<ast::Relation> r) {
    const auto& name = r->getQualifiedName();
    ast::Program& program = translationUnit->getProgram();
    if (ast::Relation* prev = program.getRelation(name)) {
        Diagnostic err(Diagnostic::Type::ERROR,
                DiagnosticMessage("Redefinition of relation " + toString(name), r->getSrcLoc()),
                {DiagnosticMessage("Previous definition", prev->getSrcLoc())});
        translationUnit->getErrorReport().addDiagnostic(err);
    } else {
        program.addRelation(std::move(r));
    }
}

void ParserDriver::addDirective(Own<ast::Directive> directive) {
    ast::Program& program = translationUnit->getProgram();
    if (directive->getType() == ast::DirectiveType::printsize) {
        for (const auto& cur : program.getDirectives()) {
            if (cur->getQualifiedName() == directive->getQualifiedName() &&
                    cur->getType() == ast::DirectiveType::printsize) {
                Diagnostic err(Diagnostic::Type::ERROR,
                        DiagnosticMessage("Redefinition of printsize directives for relation " +
                                                  toString(directive->getQualifiedName()),
                                directive->getSrcLoc()),
                        {DiagnosticMessage("Previous definition", cur->getSrcLoc())});
                translationUnit->getErrorReport().addDiagnostic(err);
                return;
            }
        }
    } else if (directive->getType() == ast::DirectiveType::limitsize) {
        for (const auto& cur : program.getDirectives()) {
            if (cur->getQualifiedName() == directive->getQualifiedName() &&
                    cur->getType() == ast::DirectiveType::limitsize) {
                Diagnostic err(Diagnostic::Type::ERROR,
                        DiagnosticMessage("Redefinition of limitsize directives for relation " +
                                                  toString(directive->getQualifiedName()),
                                directive->getSrcLoc()),
                        {DiagnosticMessage("Previous definition", cur->getSrcLoc())});
                translationUnit->getErrorReport().addDiagnostic(err);
                return;
            }
        }
    }
    program.addDirective(std::move(directive));
}

void ParserDriver::addType(Own<ast::Type> type) {
    ast::Program& program = translationUnit->getProgram();
    const auto& name = type->getQualifiedName();
    auto* existingType = getIf(program.getTypes(),
            [&](const ast::Type* current) { return current->getQualifiedName() == name; });
    if (existingType != nullptr) {
        Diagnostic err(Diagnostic::Type::ERROR,
                DiagnosticMessage("Redefinition of type " + toString(name), type->getSrcLoc()),
                {DiagnosticMessage("Previous definition", existingType->getSrcLoc())});
        translationUnit->getErrorReport().addDiagnostic(err);
    } else {
        program.addType(std::move(type));
    }
}

void ParserDriver::addClause(Own<ast::Clause> c) {
    ast::Program& program = translationUnit->getProgram();
    program.addClause(std::move(c));
}
void ParserDriver::addComponent(Own<ast::Component> c) {
    ast::Program& program = translationUnit->getProgram();
    program.addComponent(std::move(c));
}
void ParserDriver::addInstantiation(Own<ast::ComponentInit> ci) {
    ast::Program& program = translationUnit->getProgram();
    program.addInstantiation(std::move(ci));
}

void ParserDriver::addIoFromDeprecatedTag(ast::Relation& rel) {
    if (rel.hasQualifier(RelationQualifier::INPUT)) {
        addDirective(mk<ast::Directive>(ast::DirectiveType::input, rel.getQualifiedName(), rel.getSrcLoc()));
    }

    if (rel.hasQualifier(RelationQualifier::OUTPUT)) {
        addDirective(mk<ast::Directive>(ast::DirectiveType::output, rel.getQualifiedName(), rel.getSrcLoc()));
    }

    if (rel.hasQualifier(RelationQualifier::PRINTSIZE)) {
        addDirective(
                mk<ast::Directive>(ast::DirectiveType::printsize, rel.getQualifiedName(), rel.getSrcLoc()));
    }
}

std::set<RelationTag> ParserDriver::addDeprecatedTag(
        RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags) {
    if (!Global::config().has("legacy")) {
        warning(tagLoc, tfm::format("Deprecated %s qualifier was used", tag));
    }
    return addTag(tag, std::move(tagLoc), std::move(tags));
}

Own<ast::Counter> ParserDriver::addDeprecatedCounter(SrcLocation tagLoc) {
    if (!Global::config().has("legacy")) {
        warning(tagLoc, "Deprecated $ symbol was used. Use functor 'autoinc()' instead.");
    }
    return mk<ast::Counter>();
}

std::set<RelationTag> ParserDriver::addReprTag(
        RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags) {
    return addTag(tag, {RelationTag::BTREE, RelationTag::BRIE, RelationTag::EQREL}, std::move(tagLoc),
            std::move(tags));
}

std::set<RelationTag> ParserDriver::addTag(RelationTag tag, SrcLocation tagLoc, std::set<RelationTag> tags) {
    return addTag(tag, {tag}, std::move(tagLoc), std::move(tags));
}

std::set<RelationTag> ParserDriver::addTag(RelationTag tag, std::vector<RelationTag> incompatible,
        SrcLocation tagLoc, std::set<RelationTag> tags) {
    if (any_of(incompatible, [&](auto&& x) { return contains(tags, x); })) {
        error(tagLoc, tfm::format("%s qualifier already set", join(incompatible, "/")));
    }

    tags.insert(tag);
    return tags;
}

Own<ast::SubsetType> ParserDriver::mkDeprecatedSubType(
        ast::QualifiedName name, ast::QualifiedName baseTypeName, SrcLocation loc) {
    if (!Global::config().has("legacy")) {
        warning(loc, "Deprecated type declaration used");
    }
    return mk<ast::SubsetType>(std::move(name), std::move(baseTypeName), std::move(loc));
}

void ParserDriver::warning(const SrcLocation& loc, const std::string& msg) {
    translationUnit->getErrorReport().addWarning(msg, loc);
}
void ParserDriver::error(const SrcLocation& loc, const std::string& msg) {
    translationUnit->getErrorReport().addError(msg, loc);
}
void ParserDriver::error(const std::string& msg) {
    translationUnit->getErrorReport().addDiagnostic(
            Diagnostic(Diagnostic::Type::ERROR, DiagnosticMessage(msg)));
}

}  // end of namespace souffle
