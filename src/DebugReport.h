/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugReport.h
 *
 * Defines classes for creating HTML reports of debugging information.
 *
 ***********************************************************************/
#pragma once

#include "AstTransformer.h"

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace souffle {

class AstTranslationUnit;

/**
 * Class representing a section of a HTML report.
 * Consists of a unique identifier, a title, a number of subsections,
 * and the HTML code for the body of the section.
 */
class DebugReportSection {
private:
    std::string id;
    std::string title;
    std::vector<DebugReportSection> subsections;
    std::string body;

public:
    DebugReportSection(
            std::string id, std::string title, std::vector<DebugReportSection> subsections, std::string body)
            : id(std::move(id)), title(std::move(title)), subsections(std::move(subsections)),
              body(std::move(body)) {}

    /**
     * Outputs the HTML code for the index to the given stream,
     * consisting of a link to the section body followed by a list of
     * the indices for each subsection.
     */
    void printIndex(std::ostream& out) const;

    /**
     * Outputs the HTML code for the title header to the given stream.
     */
    void printTitle(std::ostream& out) const;

    /**
     * Outputs the HTML code for the content of the section to the given
     * stream, consisting of the title header, the body text, followed
     * by the content for each subsection.
     */
    void printContent(std::ostream& out) const;

    bool hasSubsections() const {
        return !subsections.empty();
    }
};

/**
 * Class representing a HTML report, consisting of a list of sections.
 */
class DebugReport {
private:
    std::vector<DebugReportSection> sections;

public:
    bool empty() const {
        return sections.empty();
    }

    void addSection(const DebugReportSection& section) {
        sections.push_back(section);
    }

    /**
     * Outputs a complete HTML document to the given stream,
     * consisting of an index of all of the sections of the report,
     * followed by the content of each section.
     */
    void print(std::ostream& out) const;

    friend std::ostream& operator<<(std::ostream& out, const DebugReport& report) {
        report.print(out);
        return out;
    }
};

/**
 * Transformation pass which wraps another transformation pass and generates
 * a debug report section for the stage after applying the wrapped transformer,
 * and adds it to the translation unit's debug report.
 */
class DebugReporter : public AstTransformer {
private:
    std::unique_ptr<AstTransformer> wrappedTransformer;

    bool transform(AstTranslationUnit& translationUnit) override;

public:
    DebugReporter(std::unique_ptr<AstTransformer> wrappedTransformer)
            : wrappedTransformer(std::move(wrappedTransformer)) {}

    std::string getName() const override {
        return "DebugReporter";
    }

    /**
     * Generate a debug report section for the current state of the given translation unit
     * with the given id and title, and add the section to the translation unit's debug report.
     * @param translationUnit translation unit to generate and add debug report section
     * @param id the unique id of the generated section
     * @param title the text to display as the heading of the section
     */
    static void generateDebugReport(AstTranslationUnit& translationUnit, std::string id, std::string title);

    /**
     * Generate a debug report section for code (preserving formatting), with the given id and title.
     */
    static DebugReportSection getCodeSection(std::string id, std::string title, std::string code);

    /**
     * Generated a debug report section for a dot graph specification, with the given id and title.
     */
    static DebugReportSection getDotGraphSection(std::string id, std::string title, std::string dotSpec);
};

}  // end of namespace souffle
