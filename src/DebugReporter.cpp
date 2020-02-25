/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugReporter.cpp
 *
 * Defines class for adapting other transformers to produce debug output
 *
 ***********************************************************************/

#include "DebugReporter.h"
#include "AstTranslationUnit.h"
#include "AstTypeAnalysis.h"
#include "AstTypeEnvironmentAnalysis.h"
#include "DebugReport.h"
#include "PrecedenceGraph.h"
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>

namespace souffle {

static std::string toBase64(const std::string& data) {
    static const std::vector<char> table = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
            'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
    std::string result;
    std::string tmp = data;
    unsigned int padding = 0;
    if (data.size() % 3 == 2) {
        padding = 1;
    } else if (data.size() % 3 == 1) {
        padding = 2;
    }

    for (unsigned int i = 0; i < padding; i++) {
        tmp.push_back(0);
    }
    for (unsigned int i = 0; i < tmp.size(); i += 3) {
        auto c1 = static_cast<unsigned char>(tmp[i]);
        auto c2 = static_cast<unsigned char>(tmp[i + 1]);
        auto c3 = static_cast<unsigned char>(tmp[i + 2]);
        unsigned char index1 = c1 >> 2;
        unsigned char index2 = ((c1 & 0x03) << 4) | (c2 >> 4);
        unsigned char index3 = ((c2 & 0x0F) << 2) | (c3 >> 6);
        unsigned char index4 = c3 & 0x3F;

        result.push_back(table[index1]);
        result.push_back(table[index2]);
        result.push_back(table[index3]);
        result.push_back(table[index4]);
    }
    if (padding == 1) {
        result[result.size() - 1] = '=';
    } else if (padding == 2) {
        result[result.size() - 1] = '=';
        result[result.size() - 2] = '=';
    }
    return result;
}

DebugReportSection DebugReporter::getDotGraphSection(
        const std::string& id, std::string title, const std::string& dotSpec) {
    std::string tempFileName = tempFile();
    {
        std::ofstream dotFile(tempFileName);
        dotFile << dotSpec;
    }

    std::string cmd = "dot -Tsvg < " + tempFileName;
    FILE* in = popen(cmd.c_str(), "r");
    std::stringstream data;
    while (in != nullptr) {
        char c = fgetc(in);
        if (feof(in) != 0) {
            break;
        }
        data << c;
    }
    pclose(in);
    remove(tempFileName.c_str());

    std::stringstream graphHTML;
    if (data.str().find("<svg") != std::string::npos) {
        graphHTML << "<img alt='graph image' src='data:image/svg+xml;base64," << toBase64(data.str())
                  << "'><br/>\n";
    } else {
        graphHTML << "<p>(error: unable to generate dot graph image)</p>";
    }
    graphHTML << "<a href=\"javascript:toggleVisibility('" << id << "-source"
              << "')\">(show dot source)</a>\n";
    graphHTML << "<div id='" << id << "-source"
              << "' style='display:none'>\n";
    graphHTML << "<pre>" << dotSpec << "</pre>\n";
    graphHTML << "</div>\n";
    return DebugReportSection(id, std::move(title), {}, graphHTML.str());
}

bool DebugReporter::transform(AstTranslationUnit& translationUnit) {
    auto start = std::chrono::high_resolution_clock::now();
    bool changed = applySubtransformer(translationUnit, wrappedTransformer.get());
    auto end = std::chrono::high_resolution_clock::now();
    std::string runtimeStr = "(" + std::to_string(std::chrono::duration<double>(end - start).count()) + "s)";
    if (changed) {
        generateDebugReport(translationUnit, wrappedTransformer->getName(),
                "After " + wrappedTransformer->getName() + " " + runtimeStr);
    } else {
        translationUnit.getDebugReport().addSection(DebugReportSection(wrappedTransformer->getName(),
                "After " + wrappedTransformer->getName() + " " + runtimeStr + " (unchanged)", {}, ""));
    }
    return changed;
}

DebugReportSection formatCodeSection(const std::string& id, const std::string& title, std::string code) {
    std::stringstream codeHTML;
    std::string escapedCode = std::move(code);
    while (true) {
        size_t i = escapedCode.find("<");
        if (i == std::string::npos) {
            break;
        }
        escapedCode.replace(i, 1, "&lt;");
    }
    codeHTML << "<pre>" << escapedCode << "</pre>\n";
    return DebugReportSection(id, title, codeHTML.str());
}

DebugReportSection formatDotGraphSection(
        const std::string& id, const std::string& title, const std::string& dotSpec) {
    std::string tempFileName = tempFile();
    {
        std::ofstream dotFile(tempFileName);
        dotFile << dotSpec;
    }

    std::string cmd = "dot -Tsvg < " + tempFileName;
    FILE* in = popen(cmd.c_str(), "r");
    std::stringstream data;
    while (in != nullptr) {
        char c = fgetc(in);
        if (feof(in) != 0) {
            break;
        }
        data << c;
    }
    pclose(in);
    remove(tempFileName.c_str());

    std::stringstream graphHTML;
    if (data.str().find("<svg") != std::string::npos) {
        graphHTML << "<img alt='graph image' src='data:image/svg+xml;base64," << toBase64(data.str())
                  << "'><br/>\n";
    } else {
        graphHTML << "<p>(error: unable to generate dot graph image)</p>";
    }
    graphHTML << "<a href=\"javascript:toggleVisibility('" << id << "-source"
              << "')\">(show dot source)</a>\n";
    graphHTML << "<div id='" << id << "-source"
              << "' style='display:none'>\n";
    graphHTML << "<pre>" << dotSpec << "</pre>\n";
    graphHTML << "</div>\n";
    return DebugReportSection(id, title, graphHTML.str());
}
void DebugReporter::generateDebugReport(
        AstTranslationUnit& translationUnit, const std::string& id, std::string title) {
    std::stringstream datalogSpec;
    translationUnit.getProgram()->print(datalogSpec);

    DebugReportSection datalogSection = formatCodeSection(id + "-dl", "Datalog", datalogSpec.str());

    std::stringstream typeAnalysis;
    translationUnit.getAnalysis<TypeAnalysis>()->print(typeAnalysis);
    DebugReportSection typeAnalysisSection =
            formatCodeSection(id + "-ta", "Type Analysis", typeAnalysis.str());

    std::stringstream typeEnvironmentAnalysis;
    translationUnit.getAnalysis<TypeEnvironmentAnalysis>()->print(typeEnvironmentAnalysis);
    DebugReportSection typeEnvironmentAnalysisSection =
            formatCodeSection(id + "-tea", "Type Environment Analysis", typeEnvironmentAnalysis.str());

    std::stringstream precGraphDot;
    translationUnit.getAnalysis<PrecedenceGraph>()->print(precGraphDot);
    DebugReportSection precedenceGraphSection =
            formatDotGraphSection(id + "-prec-graph", "Precedence Graph", precGraphDot.str());

    std::stringstream sccGraphDot;
    translationUnit.getAnalysis<SCCGraph>()->print(sccGraphDot);
    DebugReportSection sccGraphSection =
            formatDotGraphSection(id + "-scc-graph", "SCC Graph", sccGraphDot.str());

    std::stringstream topsortSCCGraph;
    translationUnit.getAnalysis<TopologicallySortedSCCGraph>()->print(topsortSCCGraph);
    DebugReportSection topsortSCCGraphSection =
            formatCodeSection(id + "-topsort-scc-graph", "SCC Topological Sort Order", topsortSCCGraph.str());

    translationUnit.getDebugReport().addSection(DebugReportSection(id, std::move(title),
            {datalogSection, typeAnalysisSection, typeEnvironmentAnalysisSection, precedenceGraphSection,
                    sccGraphSection, topsortSCCGraphSection},
            ""));
}

}  // end of namespace souffle
