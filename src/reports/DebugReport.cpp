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

#include "reports/DebugReport.h"
#include "Global.h"
#include "souffle/utility/FileUtil.h"
#include "souffle/utility/tinyformat.h"
#include <fstream>
#include <ostream>
#include <sstream>
#include <vector>

namespace souffle {

namespace {
/**
 * Generate a full-content diff between two sources.
 * Both arguments are passed into a `std::ostream` so you may exploit stream implementations.
 */
template <typename A, typename B>
std::string generateDiff(const A& prev, const B& curr) {
    TempFileStream in_prev;
    TempFileStream in_curr;
    in_prev << prev;
    in_curr << curr;
    in_prev.flush();
    in_curr.flush();
    std::string diff_cmd =
            "diff --new-line-format='+%L' "
            "     --old-line-format='-%L' "
            "     --unchanged-line-format=' %L' ";
    return execStdOut(diff_cmd + in_prev.getFileName() + " " + in_curr.getFileName()).str();
}

std::string replaceAll(std::string_view text, std::string_view key, std::string_view replacement) {
    std::ostringstream ss;
    for (auto tail = text; !tail.empty();) {
        auto i = tail.find(key);
        ss << tail.substr(0, i);
        if (i == std::string::npos) break;

        ss << replacement;
        tail = tail.substr(i + key.length());
    }
    return ss.str();
}
}  // namespace

DebugReport::DebugReport() : enabled(false), reportPath(), programName() {}

DebugReport::DebugReport(Global& glb) {
    enabled = !glb.config().get("debug-report").empty();
    if (enabled) {
        reportPath = glb.config().get("debug-report");
        programName = glb.config().get("");
    }
}

DebugReport::DebugReport(const std::string& report_path, const std::string& program_name)
        : enabled(true), reportPath(report_path), programName(program_name) {}

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

DebugReport::~DebugReport() {
    while (!currentSubsections.empty()) {
        endSection("forced-closed", "Forcing end of unknown section");
    }

    flush();
}

void DebugReport::flush() {
    if (!enabled || empty()) return;

    std::ofstream(*reportPath) << *this;
}

bool DebugReport::empty() const {
    return sections.empty();
}

void DebugReport::addSection(DebugReportSection section) {
    auto& buf = currentSubsections.empty() ? sections : currentSubsections.top();
    buf.emplace_back(std::move(section));
}

static std::string codeToHtml(const std::string_view code) {
    return replaceAll(
            replaceAll(replaceAll(replaceAll(replaceAll(code, "&", "&nbsp;"), "<", "&lt;"), ">", "&gt;"),
                    "\"", "&quot;"),
            "'", "&lsquo");
}

void DebugReport::addSection(std::string id, std::string title, const std::string_view code) {
    addSection(DebugReportSection(
            std::move(id), std::move(title), tfm::format("<pre>%s</pre>", codeToHtml(code))));
}

void DebugReport::addCodeSection(std::string id, std::string title, std::string_view language,
        std::string_view prev, std::string_view curr) {
    const std::string diff = (prev.empty() ? std::string(curr) : generateDiff(prev, curr));
    auto divId = nextUniqueId++;
    auto html = R"(
        <div id="code-id-%d" class="diff-%s">%s</div>
    )";
    addSection(DebugReportSection(
            std::move(id), std::move(title), tfm::format(html, divId, language, codeToHtml(diff))));
}

void DebugReport::endSection(std::string currentSectionName, std::string currentSectionTitle) {
    auto subsections = std::move(currentSubsections.top());
    currentSubsections.pop();
    addSection(DebugReportSection(
            std::move(currentSectionName), std::move(currentSectionTitle), std::move(subsections), ""));
    flush();
}

void DebugReport::print(std::ostream& out) const {
    out << R"--html--(
<!DOCTYPE html>
<html lang="en">
<meta charset="utf8">
<head>
<title>Souffle Debug Report ()--html--";
    out << *programName << R"--html--()</title>
<style>
    ul { list-style-type: none; }
    ul > li.leaf { display: inline-block; padding: 0em 1em; }
    ul > li.nonleaf { padding: 0em 1em; }
    * { font-family: sans-serif; }
    pre { white-space: pre-wrap; font-family: monospace; }
    a:link { text-decoration: none; color: blue; }
    a:visited { text-decoration: none; color: blue; }
    div.headerdiv { background-color:lightgrey; margin:10px; padding-left:10px; padding-right:10px;
        padding-top:3px; padding-bottom:3px; border-radius:5px }
    .headerdiv h1 { display:inline; }
    .headerdiv a { float:right; }
</style>

<link rel="stylesheet" href=
  "https://cdn.jsdelivr.net/gh/highlightjs/cdn-release@11.7.0/build/styles/default.min.css">
<script src=
  "https://cdn.jsdelivr.net/gh/highlightjs/cdn-release@11.7.0/build/highlight.min.js"></script>

<link rel="stylesheet" type="text/css" href=
    "https://cdn.jsdelivr.net/npm/diff2html@3.4.22/bundles/css/diff2html.min.css" />
<script type="text/javascript" src=
    "https://cdn.jsdelivr.net/npm/diff2html@3.4.22/bundles/js/diff2html-ui-base.min.js"></script>

<script>
  function toggleVisibility(id) {
    var element = document.getElementById(id);
    if (element.style.display == 'none') {
      element.style.display = 'block';
    } else {
      element.style.display = 'none';
    }
  }

  if (typeof hljs !== 'undefined') {
    hljs.registerLanguage('souffle', function (hljs) {
      let COMMENT_MODES = [
        hljs.C_LINE_COMMENT_MODE,
        hljs.C_BLOCK_COMMENT_MODE,
      ]

      let KEYWORDS = {
        $pattern: /\.?\w+/,
        literal: 'true false',
        keyword: '.pragma .functor .comp .init .override .decl .input .output .type .plan .include .once ' +
          'ord strlen strsub range matches land lor lxor lnot bwand bwor bwxor bwnot bshl bshr bshru inline btree btree_delete override unsigned number float symbol',
      }

      let STRING = hljs.QUOTE_STRING_MODE
      let NUMBERS = {
        scope: 'number', relevance: 0, variants: [
          { begin: /\b0b[01]+/ },
          { begin: /\b\d+\.\d+/ }, // float
          { begin: /\b\d+\.\d+.\d+.\d+/ }, // IPv4 literal
          { begin: /\b\d+u?/ },
          { begin: /\b0x[a-fA-F0-9]+u?/ }
        ]
      }

      let PREPROCESSOR = {
        scope: 'meta',
        begin: /#\s*[a-z]+\b/,
        end: /$/,
        keyword: {
          'meta-keyword': 'if else elif endif define undef warning error line pragma ifdef ifndef include'
        },
        contains: [
          { begin: /\\\n/, relevance: 0 },
          hljs.inherit(STRING, { scope: 'meta-string' }),
        ].concat(COMMENT_MODES)
      };

      let PRED_OP = { begin: /:\-/, scope: 'keyword' } // relevance booster

      let RELATION_DISPATCH = {
          scope: 'title.function',
          relevance: 0,
          begin: hljs.regex.concat(hljs.IDENT_RE, hljs.regex.lookahead(/\s*\(/))
      };

      let INNER = [
        RELATION_DISPATCH,
        PRED_OP,
        STRING,
        NUMBERS,
      ].concat(COMMENT_MODES)

      return {
        name: 'souffle',
        keywords: KEYWORDS,
        contains: INNER.concat([{ begin: /\.$/ }]) // relevance booster
      };
    });

    hljs.registerLanguage('ram', function (hljs) {
      const COMMENT_MODES = [
        hljs.C_LINE_COMMENT_MODE,
        hljs.C_BLOCK_COMMENT_MODE,
      ];

      const KEYWORDS = {
        keyword: [
          'ALL',
          'AND',
          'BEGIN',
          'CALL',
          'CLEAR',
          'DEBUG',
          'DECLARATION',
          'END',
          'EXIT',
          'FLOAT',
          'FOR',
          'IF',
          'IN',
          'INDEX',
          'INSERT',
          'INTO',
          'IO',
          'ISEMPTY',
          'LOOP',
          'MAIN',
          'NOT',
          'NUMBER',
          'ON',
          'PROGRAM',
          'QUERY',
          'SEARCH',
          'STRING',
          'SUBROUTINE',
          'SWAP',
          'UNSIGNED',
          'count',
          'max',
          'mean',
          'min',
          'sum',
        ],
      };

      const STRING = hljs.QUOTE_STRING_MODE;

      const INDEX = {
        scope: 'variable',
        begin: /\bt\d+(\.\d+)?/
      }

      const INNER = [
        INDEX,
        hljs.QUOTE_STRING_MODE,
        hljs.C_NUMBER_MODE,
      ].concat(COMMENT_MODES)

      return {
        name: 'ram',
        keywords: KEYWORDS,
        contains: INNER
      };
    });

    hljs.configure({ languages: ['souffle', 'ram'] })
  }

  if (typeof Diff2HtmlUI !== 'undefined' && typeof hljs !== 'undefined') {
      // file extension determines the language used for highlighting
      let souffle_file = `Datalog.souffle`
      let souffle_prefix = `diff ${souffle_file} ${souffle_file}
--- ${souffle_file}
+++ ${souffle_file}
@@ -1 +1 @@
`
      let ram_file = `Datalog.ram`
      let ram_prefix = `diff ${ram_file} ${ram_file}
--- ${ram_file}
+++ ${ram_file}
@@ -1 +1 @@
`
      let extLang = new Map();
      extLang.set('souffle', 'souffle');
      extLang.set('ram', 'ram');

      document.addEventListener('DOMContentLoaded', function() {
          var els = document.getElementsByClassName("diff-souffle");
          Array.prototype.forEach.call(els, function(el) {
              diff2htmlUi = new Diff2HtmlUI(el, souffle_prefix + el.textContent, {
                  drawFileList: false,
                  highlight: true,
                  matching: 'none',
                  outputFormat: 'side-by-side',
                  synchronisedScroll: true,
                  highlightLanguages: extLang
              }, hljs);
              diff2htmlUi.draw();
          });
          var els = document.getElementsByClassName("diff-ram");
          Array.prototype.forEach.call(els, function(el) {
              diff2htmlUi = new Diff2HtmlUI(el, ram_prefix + el.textContent, {
                  drawFileList: false,
                  highlight: true,
                  matching: 'none',
                  outputFormat: 'side-by-side',
                  synchronisedScroll: true,
                  highlightLanguages: extLang
              }, hljs);
              diff2htmlUi.draw();
          });
      });
  }
</script>
</head>
<body>
<div class='headerdiv'><h1>Souffle Debug Report ()--html--";
    out << *programName << ")</h1></div>\n";
    for (const DebugReportSection& section : sections) {
        section.printIndex(out);
    }
    for (const DebugReportSection& section : sections) {
        section.printContent(out);
    }
    out << R"(<a href='#'>(return to top)</a>
</body>
</html>)";
}

}  // end of namespace souffle
