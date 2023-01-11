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
#ifndef _MSC_VER
#include <unistd.h>
#endif

extern YY_BUFFER_STATE yy_scan_string(const char*, yyscan_t scanner);
extern int yylex_destroy(yyscan_t scanner);
extern int yylex_init_extra(ScannerInfo* data, yyscan_t* scanner);
extern void yyset_debug(int, yyscan_t scanner);

namespace souffle {

ParserDriver::ParserDriver(Global& g) : glb(g) {
    vfs = std::make_shared<RealFileSystem>();
}

ParserDriver::ParserDriver(Global& g, std::shared_ptr<FileSystem> fs) : glb(g) {
    if (fs) {
        vfs = fs;
    } else {
        vfs = std::make_shared<RealFileSystem>();
    }
}

Own<ast::TranslationUnit> ParserDriver::parse(const std::string& filename, const std::string& code,
        bool reducedConsecutiveNonLeadingWhitespaces, ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = mk<ast::TranslationUnit>(glb, mk<ast::Program>(), errorReport, debugReport);
    yyscan_t scanner;
    ScannerInfo data(vfs);
    SrcLocation emptyLoc;

    std::filesystem::path filePath(filename);
    if (vfs->exists(filename)) {
        std::error_code ec;
        filePath = vfs->canonical(filename, ec);
    }
    data.push(filePath, emptyLoc, reducedConsecutiveNonLeadingWhitespaces);

    yylex_init_extra(&data, &scanner);
    yyset_debug(0, scanner);

    yy_scan_string(code.c_str(), scanner);
    yy::parser parser(*this, scanner);
    parser.parse();

    yylex_destroy(scanner);

    return std::move(translationUnit);
}

Own<ast::TranslationUnit> ParserDriver::parseFromFS(
        const std::filesystem::path& path, ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = mk<ast::TranslationUnit>(glb, mk<ast::Program>(), errorReport, debugReport);
    yyscan_t scanner;
    ScannerInfo data(vfs);
    SrcLocation emptyLoc;

    std::filesystem::path filePath(path);
    if (!vfs->exists(filePath)) {
        throw std::runtime_error(std::string("File does not exist: ") + filePath.string());
    }

    std::error_code ec;
    filePath = vfs->canonical(filePath, ec);

    yylex_init_extra(&data, &scanner);
    yyset_debug(0, scanner);

    auto code = readFile(filePath, ec);
    if (ec) {
        throw std::runtime_error(std::string("Cannot read file: ") + filePath.string());
    }

    data.push(filePath, emptyLoc);

    yy_scan_string(code->c_str(), scanner);

    yy::parser parser(*this, scanner);

    parser.parse();

    yylex_destroy(scanner);

    return std::move(translationUnit);
}

Own<ast::TranslationUnit> ParserDriver::parse(
        const std::string& code, ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = mk<ast::TranslationUnit>(glb, mk<ast::Program>(), errorReport, debugReport);

    ScannerInfo data(vfs);
    SrcLocation emptyLoc;
    data.push("<in-memory>", emptyLoc);
    data.setReported("<in-memory>");

    yyscan_t scanner;
    yylex_init_extra(&data, &scanner);
    yyset_debug(0, scanner);

    yy_scan_string(code.c_str(), scanner);
    yy::parser parser(*this, scanner);
    parser.parse();

    yylex_destroy(scanner);

    return std::move(translationUnit);
}

Own<ast::TranslationUnit> ParserDriver::parseTranslationUnit(Global& glb, const std::string& filename,
        const std::string& code, bool reducedConsecutiveNonLeadingWhitespaces, ErrorReport& errorReport,
        DebugReport& debugReport, std::shared_ptr<FileSystem> vfs) {
    ParserDriver parser(glb, vfs);
    return parser.parse(filename, code, reducedConsecutiveNonLeadingWhitespaces, errorReport, debugReport);
}

Own<ast::TranslationUnit> ParserDriver::parseTranslationUnitFromFS(Global& glb,
        const std::filesystem::path& path, ErrorReport& errorReport, DebugReport& debugReport,
        std::shared_ptr<FileSystem> vfs) {
    ParserDriver parser(glb, vfs);
    return parser.parseFromFS(path, errorReport, debugReport);
}

Own<ast::TranslationUnit> ParserDriver::parseTranslationUnit(Global& glb, const std::string& code,
        ErrorReport& errorReport, DebugReport& debugReport, std::shared_ptr<FileSystem> vfs) {
    ParserDriver parser(glb, vfs);
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
    if (!translationUnit->global().config().has("legacy")) {
        warning(WarnType::DeprecatedQualifier, tagLoc, tfm::format("Deprecated %s qualifier was used", tag));
    }
    return addTag(tag, std::move(tagLoc), std::move(tags));
}

Own<ast::Counter> ParserDriver::addDeprecatedCounter(SrcLocation tagLoc) {
    if (!translationUnit->global().config().has("legacy")) {
        warning(WarnType::DollarSign, tagLoc,
                "Deprecated $ symbol was used. Use functor 'autoinc()' instead.");
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
    if (!translationUnit->global().config().has("legacy")) {
        warning(WarnType::DeprecatedTypeDecl, loc, "Deprecated type declaration used");
    }
    return mk<ast::SubsetType>(std::move(name), std::move(baseTypeName), std::move(loc));
}

void ParserDriver::warning(const WarnType type, const SrcLocation& loc, const std::string& msg) {
    translationUnit->getErrorReport().addWarning(type, msg, loc);
}
void ParserDriver::error(const SrcLocation& loc, const std::string& msg) {
    translationUnit->getErrorReport().addError(msg, loc);
}
void ParserDriver::error(const std::string& msg) {
    translationUnit->getErrorReport().addDiagnostic(
            Diagnostic(Diagnostic::Type::ERROR, DiagnosticMessage(msg)));
}

std::unique_ptr<std::string> ParserDriver::readFile(const std::filesystem::path& path, std::error_code& ec) {
    return std::make_unique<std::string>(vfs->readFile(path, ec));
}

std::optional<std::filesystem::path> ParserDriver::searchIncludePath(
        const std::string& IncludeString, const SrcLocation& Loc) {
    std::error_code ec;
    std::filesystem::path Request(IncludeString);

    if (Request.is_absolute()) {
        if (vfs->exists(Request)) {
            return vfs->canonical(Request, ec);
        } else {
            return std::nullopt;
        }
    }

    // search relative from current physical input file
    std::filesystem::path Candidate = std::filesystem::path(Loc.file->Physical).parent_path() / Request;
    if (vfs->exists(Candidate)) {
        return vfs->canonical(Candidate, ec);
    }

#if defined(__APPLE__)
    // work-around a bug in libcxx version <= 12, std::filesystem::current_path
    // writes out of bound and corrupt memory.
    char* cwd = ::getcwd(nullptr, 0);
    if (cwd == nullptr) {
        std::cerr << "Error: cannot get current working directory.\n";
        return std::nullopt;
    }
    const std::filesystem::path CurrentWD = std::string(cwd);
    free(cwd);
#else
    const std::filesystem::path CurrentWD = std::filesystem::current_path();
#endif

    // search relative from include directories
    for (auto&& includeDir : glb.config().getMany("include-dir")) {
        auto dir = std::filesystem::path(includeDir);
        if (dir.is_relative()) {
            dir = (CurrentWD / dir);
        }
        Candidate = std::filesystem::path(dir) / Request;
        if (vfs->exists(Candidate)) {
            return vfs->canonical(Candidate, ec);
        }
    }

    return std::nullopt;
}

bool ParserDriver::canEnterOnce(const SrcLocation& onceLoc) {
    const auto Inserted = VisitedOnceLocations.emplace(onceLoc.file->Physical, onceLoc.start.line);
    return Inserted.second;
}

void ParserDriver::addComment(const SrcLocation& Loc, const std::stringstream& Content) {
    ScannedComments.emplace_back(Loc, Content.str());
}

}  // end of namespace souffle
