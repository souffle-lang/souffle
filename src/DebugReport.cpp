/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DebugReport.cpp
 *
 * Defines classes for creating HTML reports of debugging information.
 *
 ***********************************************************************/

#include "DebugReport.h"
#include "AstTranslationUnit.h"
#include "MagicSet.h"
#include "PrecedenceGraph.h"

#include <cstdio>
#include <sstream>

namespace souffle {

static std::string toBase64(std::string data) {
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
        unsigned char c1 = static_cast<unsigned char>(tmp[i]);
        unsigned char c2 = static_cast<unsigned char>(tmp[i + 1]);
        unsigned char c3 = static_cast<unsigned char>(tmp[i + 2]);
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

void DebugReportSection::printIndex(std::ostream& out) const {
    out << "<a href=\"#" << id << "\">" << title << "</a>\n";
    out << "<ul>\n";
    bool isLeaf = true;
    for (const DebugReportSection& subsection : subsections) {
        if (subsection.hasSubsections()) {
            isLeaf = false;
            break;
        }
    }
    for (const DebugReportSection& subsection : subsections) {
        if (isLeaf) {
            out << "<li class='leaf'>";
        } else {
            out << "<li>";
        }
        subsection.printIndex(out);
        out << "</li>";
    }
    out << "</ul>\n";
}

void DebugReportSection::printTitle(std::ostream& out) const {
    out << "<a id=\"" << id << "\"></a>\n";
    out << "<div class='headerdiv'>\n";
    out << "<h1>" << title << "</h1>\n";
    out << "<a href='#'>(return to top)</a>\n";
    out << "</div><div style='clear:both'></div>\n";
}

void DebugReportSection::printContent(std::ostream& out) const {
    printTitle(out);
    out << "<div style='padding-left: 1em'>\n";
    out << body << "\n";
    for (const DebugReportSection& subsection : subsections) {
        subsection.printContent(out);
    }
    out << "</div>\n";
}

void DebugReport::print(std::ostream& out) const {
    out << "<!DOCTYPE html>\n";
    out << "<html>\n";
    out << "<head>\n";
    out << "<meta charset=\"UTF-8\">\n";
    out << "<title>Souffle Debug Report</title>\n";
    out << "<style>\n";
    out << "ul { list-style-type: none; }\n";
    out << "ul > li.leaf { display: inline-block; padding: 0em 1em; }\n";
    out << "ul > li.nonleaf { padding: 0em 1em; }\n";
    out << "* { font-family: sans-serif; }\n";
    out << "pre { white-space: pre-wrap; font-family: monospace; }\n";
    out << "a:link { text-decoration: none; color: blue; }\n";
    out << "a:visited { text-decoration: none; color: blue; }\n";
    out << "div.headerdiv { background-color:lightgrey; margin:10px; padding-left:10px; padding-right:10px; "
           "padding-top:3px; padding-bottom:3px; border-radius:5px }\n";
    out << ".headerdiv h1 { display:inline; }\n";
    out << ".headerdiv a { float:right; }\n";
    out << "</style>\n";
    out << "<script type=\"text/javascript\">\n";
    out << "function toggleVisibility(id) {\n";
    out << "  var element = document.getElementById(id);\n";
    out << "  if (element.style.display == 'none') {\n";
    out << "    element.style.display = 'block';\n";
    out << "  } else {\n";
    out << "    element.style.display = 'none';\n";
    out << "  }\n";
    out << "}\n";
    out << "</script>\n";
    out << "</head>\n";
    out << "<body>\n";
    out << "<div class='headerdiv'><h1>Souffle Debug Report</h1></div>\n";
    for (const DebugReportSection& section : sections) {
        section.printIndex(out);
    }
    for (const DebugReportSection& section : sections) {
        section.printContent(out);
    }
    out << "<a href='#'>(return to top)</a>\n";
    out << "</body>\n";
    out << "</html>\n";
}

DebugReportSection DebugReporter::getCodeSection(std::string id, std::string title, std::string code) {
    std::stringstream codeHTML;
    std::string escapedCode = code;
    while (true) {
        size_t i = escapedCode.find("<");
        if (i == std::string::npos) {
            break;
        }
        escapedCode.replace(i, 1, "&lt;");
    }
    codeHTML << "<pre>" << escapedCode << "</pre>\n";
    return DebugReportSection(id, title, {}, codeHTML.str());
}

DebugReportSection DebugReporter::getDotGraphSection(std::string id, std::string title, std::string dotSpec) {
    std::stringstream cmd;
    cmd << "dot -Tsvg <<END_DOT_FILE\n";
    cmd << dotSpec << "\n";
    cmd << "END_DOT_FILE\n";
    FILE* in = popen(cmd.str().c_str(), "r");
    std::stringstream data;
    while (true) {
        char c = fgetc(in);
        if (feof(in)) {
            break;
        }
        data << c;
    }
    pclose(in);
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
    return DebugReportSection(id, title, {}, graphHTML.str());
}

bool DebugReporter::transform(AstTranslationUnit& translationUnit) {
    auto start = std::chrono::high_resolution_clock::now();
    bool changed = wrappedTransformer->apply(translationUnit);
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

void DebugReporter::generateDebugReport(
        AstTranslationUnit& translationUnit, std::string id, std::string title) {
    std::stringstream datalogSpec;
    translationUnit.getProgram()->print(datalogSpec);

    DebugReportSection datalogSection = getCodeSection(id + "-dl", "Datalog", datalogSpec.str());

    std::stringstream precGraphDot;
    translationUnit.getAnalysis<PrecedenceGraph>()->outputPrecedenceGraph(precGraphDot);
    DebugReportSection precedenceGraphSection =
            getDotGraphSection(id + "-prec-graph", "Precedence Graph", precGraphDot.str());

    std::stringstream sccGraphDot;
    translationUnit.getAnalysis<SCCGraph>()->outputSCCGraph(sccGraphDot);
    DebugReportSection sccGraphSection =
            getDotGraphSection(id + "-scc-graph", "SCC Graph", sccGraphDot.str());

    std::stringstream topsortSCCGraph;
    translationUnit.getAnalysis<TopologicallySortedSCCGraph>()->outputTopologicallySortedSCCGraph(
            topsortSCCGraph);
    DebugReportSection topsortSCCGraphSection =
            getCodeSection(id + "-topsort-scc-graph", "SCC Topological Sort Order", topsortSCCGraph.str());

    std::stringstream adornment;
    translationUnit.getAnalysis<Adornment>()->outputAdornment(adornment);
    DebugReportSection adornmentSection =
            getCodeSection(id + "-adornment-display", "Adornment", adornment.str());

    translationUnit.getDebugReport().addSection(
            DebugReportSection(id, title, {datalogSection, precedenceGraphSection, sccGraphSection,
                                                  topsortSCCGraphSection, adornmentSection},
                    ""));
}

}  // end of namespace souffle
