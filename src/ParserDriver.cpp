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

#include "ParserDriver.h"
#include "AstProgram.h"
#include "AstTranslationUnit.h"
#include "ErrorReport.h"

typedef struct yy_buffer_state* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char*, yyscan_t scanner);
extern int yylex_destroy(yyscan_t scanner);
extern int yylex_init_extra(scanner_data* data, yyscan_t* scanner);
extern void yyset_in(FILE* in_str, yyscan_t scanner);

namespace souffle {

ParserDriver::ParserDriver() : trace_scanning(false), trace_parsing(false) {}

ParserDriver::~ParserDriver() = default;

std::unique_ptr<AstTranslationUnit> ParserDriver::parse(const std::string& filename, FILE* in,
        SymbolTable& symbolTable, ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = std::unique_ptr<AstTranslationUnit>(new AstTranslationUnit(
            std::unique_ptr<AstProgram>(new AstProgram()), symbolTable, errorReport, debugReport));
    yyscan_t scanner;
    scanner_data data;
    data.yyfilename = filename.c_str();
    yylex_init_extra(&data, &scanner);
    yyset_in(in, scanner);

    yy::parser parser(*this, scanner);
    parser.set_debug_level(trace_parsing);
    parser.parse();

    yylex_destroy(scanner);

    translationUnit->getProgram()->finishParsing();

    return std::move(translationUnit);
}

std::unique_ptr<AstTranslationUnit> ParserDriver::parse(const std::string& code, SymbolTable& symbolTable,
        ErrorReport& errorReport, DebugReport& debugReport) {
    translationUnit = std::unique_ptr<AstTranslationUnit>(new AstTranslationUnit(
            std::unique_ptr<AstProgram>(new AstProgram()), symbolTable, errorReport, debugReport));

    scanner_data data;
    data.yyfilename = "<in-memory>";
    yyscan_t scanner;
    yylex_init_extra(&data, &scanner);
    yy_scan_string(code.c_str(), scanner);
    yy::parser parser(*this, scanner);
    parser.set_debug_level(trace_parsing);
    parser.parse();

    yylex_destroy(scanner);

    translationUnit->getProgram()->finishParsing();

    return std::move(translationUnit);
}

std::unique_ptr<AstTranslationUnit> ParserDriver::parseTranslationUnit(const std::string& filename, FILE* in,
        SymbolTable& symbolTable, ErrorReport& errorReport, DebugReport& debugReport) {
    ParserDriver parser;
    return parser.parse(filename, in, symbolTable, errorReport, debugReport);
}

std::unique_ptr<AstTranslationUnit> ParserDriver::parseTranslationUnit(const std::string& code,
        SymbolTable& symbolTable, ErrorReport& errorReport, DebugReport& debugReport) {
    ParserDriver parser;
    return parser.parse(code, symbolTable, errorReport, debugReport);
}

void ParserDriver::addPragma(std::unique_ptr<AstPragma> p) {
    translationUnit->getProgram()->addPragma(std::move(p));
}

void ParserDriver::addRelation(std::unique_ptr<AstRelation> r) {
    const auto& name = r->getName();
    if (AstRelation* prev = translationUnit->getProgram()->getRelation(name)) {
        Diagnostic err(Diagnostic::ERROR,
                DiagnosticMessage("Redefinition of relation " + toString(name), r->getSrcLoc()),
                {DiagnosticMessage("Previous definition", prev->getSrcLoc())});
        translationUnit->getErrorReport().addDiagnostic(err);
    } else {
        if (r->isInput()) {
            translationUnit->getErrorReport().addWarning(
                    "Deprecated input qualifier was used in relation " + toString(name), r->getSrcLoc());
        }
        if (r->isOutput()) {
            translationUnit->getErrorReport().addWarning(
                    "Deprecated output qualifier was used in relation " + toString(name), r->getSrcLoc());
        }
        if (r->isPrintSize()) {
            translationUnit->getErrorReport().addWarning(
                    "Deprecated printsize qualifier was used in relation " + toString(name), r->getSrcLoc());
        }
        translationUnit->getProgram()->addRelation(std::move(r));
    }
}

void ParserDriver::addIODirectiveChain(std::unique_ptr<AstIODirective> d) {
    for (auto& currentName : d->getNames()) {
        auto clone = std::unique_ptr<AstIODirective>(d->clone());
        clone->setName(currentName);
        addIODirective(std::move(clone));
    }
}

void ParserDriver::addIODirective(std::unique_ptr<AstIODirective> d) {
    if (d->isOutput()) {
        translationUnit->getProgram()->addIODirective(std::move(d));
        return;
    }

    for (const auto& cur : translationUnit->getProgram()->getIODirectives()) {
        if (((cur->isInput() && d->isInput()) || (cur->isPrintSize() && d->isPrintSize())) &&
                cur->getName() == d->getName()) {
            Diagnostic err(Diagnostic::ERROR,
                    DiagnosticMessage(
                            "Redefinition of input directives for relation " + toString(d->getName()),
                            d->getSrcLoc()),
                    {DiagnosticMessage("Previous definition", cur->getSrcLoc())});
            translationUnit->getErrorReport().addDiagnostic(err);
            return;
        }
    }
    translationUnit->getProgram()->addIODirective(std::move(d));
}

void ParserDriver::addType(std::unique_ptr<AstType> type) {
    const auto& name = type->getName();
    if (const AstType* prev = translationUnit->getProgram()->getType(name)) {
        Diagnostic err(Diagnostic::ERROR,
                DiagnosticMessage("Redefinition of type " + toString(name), type->getSrcLoc()),
                {DiagnosticMessage("Previous definition", prev->getSrcLoc())});
        translationUnit->getErrorReport().addDiagnostic(err);
    } else {
        translationUnit->getProgram()->addType(std::move(type));
    }
}

void ParserDriver::addClause(std::unique_ptr<AstClause> c) {
    translationUnit->getProgram()->addClause(std::move(c));
}
void ParserDriver::addComponent(std::unique_ptr<AstComponent> c) {
    translationUnit->getProgram()->addComponent(std::move(c));
}
void ParserDriver::addInstantiation(std::unique_ptr<AstComponentInit> ci) {
    translationUnit->getProgram()->addInstantiation(std::move(ci));
}

souffle::SymbolTable& ParserDriver::getSymbolTable() {
    return translationUnit->getSymbolTable();
}

void ParserDriver::error(const AstSrcLocation& loc, const std::string& msg) {
    translationUnit->getErrorReport().addError(msg, loc);
}
void ParserDriver::error(const std::string& msg) {
    translationUnit->getErrorReport().addDiagnostic(Diagnostic(Diagnostic::ERROR, DiagnosticMessage(msg)));
}

}  // end of namespace souffle
