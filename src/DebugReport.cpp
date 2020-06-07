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
#include "Global.h"
#include "utility/FileUtil.h"
#include "utility/tinyformat.h"
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
    auto&& dst = Global::config().get("debug-report");
    if (dst.empty() || empty()) return;

    std::ofstream(dst) << *this;
}

void DebugReport::addSection(std::string id, std::string title, const std::string_view code) {
    addSection(DebugReportSection(
            std::move(id), std::move(title), tfm::format("<pre>%s</pre>", replaceAll(code, "<", "&lt"))));
}

void DebugReport::addCodeSection(std::string id, std::string title, std::string_view language,
        std::string_view prev, std::string_view curr) {
    auto diff =
            replaceAll(replaceAll(prev.empty() ? curr : generateDiff(prev, curr), "\\", "\\\\"), "`", "\\`");
    auto divId = nextUniqueId++;
    auto html = R"(
        <div id="code-id-%d"></div>
        <script type="text/javascript"> renderDiff('%s', 'code-id-%d', `%s`) </script>
    )";
    addSection(DebugReportSection(
            std::move(id), std::move(title), tfm::format(html, divId, language, divId, diff)));
}

void DebugReport::endSection(std::string currentSectionName, std::string currentSectionTitle) {
    auto subsections = std::move(currentSubsections.top());
    currentSubsections.pop();
    addSection(DebugReportSection(
            std::move(currentSectionName), std::move(currentSectionTitle), std::move(subsections), ""));
    flush();
}

void DebugReport::print(std::ostream& out) const {
    out << R"(
<!DOCTYPE html>
<html lang='en-AU'>
<head>
<meta charset=\"UTF-8\">
<title>Souffle Debug Report ()";
    out << Global::config().get("") << R"()</title>
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

<link rel="stylesheet" type="text/css" href=
    "https://cdn.jsdelivr.net/npm/highlight.js@10.0.0/styles/default.min.css" />
<script type="text/javascript" src=
    "https://cdn.jsdelivr.net/gh/highlightjs/cdn-release@10.0.0/build/highlight.min.js"></script>

<link rel="stylesheet" type="text/css" href=
    "https://cdn.jsdelivr.net/npm/diff2html/bundles/css/diff2html.min.css" />
<script type="text/javascript" src=
    "https://cdn.jsdelivr.net/npm/diff2html/bundles/js/diff2html-ui-base.min.js"></script>

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
        $pattern: '\\.?\\w+',
        literal: 'true false',
        keyword: '.pragma .functor .component .decl .input .output ' +
          'ord strlen strsub range matches land lor lxor lnot bwand bwor bwxor bwnot bshl bshr bshru',
      }

      let STRING = hljs.QUOTE_STRING_MODE
      let NUMBERS = {
        className: 'number', relevance: 0, variants: [
          { begin: /0b[01]+/ },
          { begin: /\d+\.\d+/ }, // float
          { begin: /\d+\.\d+.\d+.\d+/ }, // IPv4 literal
          { begin: /\d+u?/ },
          { begin: /0x[a-fA-F0-9]+u?/ }
        ]
      }

      let PREPROCESSOR = {
        className: 'meta',
        begin: /#\s*[a-z]+\b/,
        end: /$/,
        keywords: {
          'meta-keyword': 'if else elif endif define undef warning error line pragma ifdef ifndef include'
        },
        contains: [
          { begin: /\\\n/, relevance: 0 },
          hljs.inherit(STRING, { className: 'meta-string' }),
        ].concat(COMMENT_MODES)
      };

      let ATOM = { begin: /[a-z][A-Za-z0-9_]*/, relevance: 0 }
      let VAR = {
        className: 'symbol', relevance: 0, variants: [
          { begin: /[A-Z][a-zA-Z0-9_]*/ },
          { begin: /_[A-Za-z0-9_]*/ },
        ]
      }
      let PARENTED = { begin: /\(/, end: /\)/, relevance: 0 }
      let LIST = { begin: /\[/, end: /\]/ }
      let PRED_OP = { begin: /:-/ } // relevance booster

      let INNER = [
        ATOM,
        VAR,
        PARENTED,
        PRED_OP,
        LIST,
        STRING,
        NUMBERS,
      ].concat(COMMENT_MODES)

      PARENTED.contains = INNER;
      LIST.contains = INNER;

      return {
        name: 'souffle',
        keywords: KEYWORDS,
        contains: INNER.concat([{ begin: /\.$/ }]) // relevance booster
      };
    })
    // TODO: Add a highlighter for `ram`
    hljs.configure({ languages: ['souffle'] })
  }

  if (typeof Diff2HtmlUI !== 'undefined' && typeof hljs !== 'undefined') {
    function renderDiff(lang, id, diff) {
      // file extension determines the language used for highlighting
      let file   = `Datalog.${lang}`
      let prefix = `diff ${file} ${file}
--- ${file}
+++ ${file}
@@ -1 +1 @@
`
      new Diff2HtmlUI(document.getElementById(id), prefix + diff, {
        drawFileList: false,
        highlight: true,
        matching: 'none',
        outputFormat: 'side-by-side',
        synchronisedScroll: true,
      }, hljs).draw()
    }
  } else { // fallback to plain text
    function renderDiff(lang, id, diff) {
      document.getElementById(id).innerText = diff
    }
  }
</script>
</head>
<body>
<div class='headerdiv'><h1>Souffle Debug Report ()";
    out << Global::config().get("") << ")</h1></div>\n";
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
