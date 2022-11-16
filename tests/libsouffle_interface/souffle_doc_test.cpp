/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2023, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#include "MainDriver.h"
#include "ast/utility/Visitor.h"
#include "parser/ParserDriver.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

///
/// Parse an input file and verify:
/// - sequences of doc comments are merged into a single comment.
/// - expected annotations are present.
///

using namespace souffle;
using namespace souffle::ast;

namespace {
const std::string Input =
        R"datalog(
/// Hello
/// World
/// 😀
@[doc = "more doc"]
.decl query(
  /// v's doc
  v:number)

.comp Foo {
  //! Foo inner
  //! inner continued
  @![doc = "more inner doc"]
  /// f outer
  .decl f()
}
)datalog";

// clang-format off
const std::set<std::tuple<CommentKind, std::string>> ExpectedComments = {
  {CommentKind::DocOuter, " Hello\n World\n \xf0\x9f\x98\x80"},
  {CommentKind::DocOuter, " v's doc"},
  {CommentKind::DocInner, " Foo inner\n inner continued"},
  {CommentKind::DocOuter, " f outer"}
};
// clang-format on

// clang-format off
enum AnnotationTarget {
  OnRelation,
  OnComponent,
  OnAttribute
};
// clang-format on

const char* annotationTargetString(AnnotationTarget t) {
    switch (t) {
        case OnRelation: return "relation";
        case OnComponent: return "component";
        case OnAttribute: return "attribute";
        default: throw "missing case";
    }
}

using CollectedAnnotation =
        std::tuple<AnnotationTarget, std::string, Annotation::Kind, Annotation::Style, std::string>;

// clang-format off
const std::set<CollectedAnnotation> ExpectedAnnotations = {
  {OnRelation, "query", Annotation::Kind::DocComment, Annotation::Style::Outer,
      "@[doc = \" Hello\n World\n \xf0\x9f\x98\x80\"]"},
  {OnRelation, "query", Annotation::Kind::Normal, Annotation::Style::Outer,
      "@[doc = \"more doc\"]"},
  {OnAttribute, "v", Annotation::Kind::DocComment, Annotation::Style::Outer,
      "@[doc = \" v's doc\"]"},
  {OnComponent, "Foo", Annotation::Kind::DocComment, Annotation::Style::Inner,
      "@![doc = \" Foo inner\n inner continued\"]"},
  {OnComponent, "Foo", Annotation::Kind::Normal, Annotation::Style::Inner,
      "@![doc = \"more inner doc\"]"},
  {OnRelation, "f", Annotation::Kind::DocComment, Annotation::Style::Outer,
      "@[doc = \" f outer\"]"}
};
// clang-format on

struct AnnotationCollector : public ast::Visitor<void> {
    std::set<CollectedAnnotation> collected;

    void collect(AnnotationTarget t, std::string name, const ast::Node& n) {
        const auto* annotations = n.getAnnotations();
        if (!annotations) return;
        n.eachAnnotation([&](const ast::Annotation& ann) {
            std::stringstream ss;
            ann.print(ss);
            collected.emplace(t, name, ann.getKind(), ann.getStyle(), ss.str());
        });
    }

#define VISIT(T, Name)                                             \
    void visit_(type_identity<ast::T>, const ast::T& n) override { \
        collect(On##T, Name, n);                                   \
    }

    VISIT(Relation, n.getQualifiedName().toString());
    VISIT(Attribute, n.getName());
    VISIT(Component, n.getComponentType()->getName());
#undef VISIT
};

struct SingleFileFS : public FileSystem {
    explicit SingleFileFS(std::filesystem::path FilePath, std::string FileContent)
            : FilePath(FilePath), FileContent(FileContent) {}

    bool exists(const std::filesystem::path& Path) override {
        return (Path == FilePath);
    }

    std::filesystem::path canonical(const std::filesystem::path& Path, std::error_code& EC) override {
        if (exists(Path)) {
            EC = std::error_code{};
            return Path;
        } else {
            EC = std::make_error_code(std::errc::no_such_file_or_directory);
            return "";
        }
    }

    std::string readFile(const std::filesystem::path& Path, std::error_code& EC) override {
        if (canonical(Path, EC) == Path && EC == std::error_code{}) {
            return FileContent;
        } else {
            return std::string{};
        }
    }

private:
    const std::filesystem::path FilePath;
    const std::string FileContent;
};

}  // namespace

int main(int, char**) {
    auto InputDl = std::make_shared<SingleFileFS>("input.dl", Input);

    Global glb;
    glb.config().set("", "input.dl");

    ErrorReport errReport;
    DebugReport dbgReport(glb);
    auto VFS = std::make_shared<OverlayFileSystem>(InputDl);
    ParserDriver driver{glb, VFS};

    auto tu = driver.parseFromFS(std::filesystem::path{"input.dl"}, errReport, dbgReport);

    if (errReport.getNumErrors() > 0) {
        errReport.print(std::cerr);
        return static_cast<int>(errReport.getNumErrors());
    }

    std::set<std::tuple<CommentKind, std::string>> actual;
    for (const ParserDriver::ScannedComment& comment : driver.ScannedComments) {
        actual.emplace(std::get<1>(comment), std::get<2>(comment));
    }

    bool ok = true;

    if (ExpectedComments != actual) {
        ok = false;

        for (const auto& missing : (ExpectedComments - actual)) {
            std::cerr << "Missing expected comment or with wrong kind: " << std::get<1>(missing) << "\n";
        }

        for (const auto& unexpected : (actual - ExpectedComments)) {
            std::cerr << "Unexpected comment or with wrong kind: " << std::get<1>(unexpected) << "\n";
        }
    }

    auto& prg = tu->getProgram();
    AnnotationCollector collector;
    visit(prg, collector);

    if (ExpectedAnnotations != collector.collected) {
        ok = false;

        for (const auto& missing : (ExpectedAnnotations - collector.collected)) {
            std::cerr << "Missing expected annotation: '" << std::get<4>(missing) << "' on "
                      << annotationTargetString(std::get<0>(missing)) << " '" << std::get<1>(missing)
                      << "'\n";
        }

        for (const auto& unexpected : (collector.collected - ExpectedAnnotations)) {
            std::cerr << "Unexpected annotation: '" << std::get<4>(unexpected) << "' on "
                      << annotationTargetString(std::get<0>(unexpected)) << " '" << std::get<1>(unexpected)
                      << "'\n";
        }
    }

    return (ok ? 0 : 1);
}
