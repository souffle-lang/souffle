/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file scanner.ll
 *
 * @brief Scanner for the datalog parser
 *
 ***********************************************************************/
%option reentrant
%option extra-type="struct ScannerInfo *"
%{

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#elif defined(__GNUG__)
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

    #include <cstdio>
    #ifndef _MSC_VER
    #include <libgen.h>
    #endif
    #include <cctype>
    #include <sys/stat.h>
    #include <stack>
    #include <string>
    #include <sstream>
    #include <cassert>
    #ifndef _MSC_VER
    #include <unistd.h>
    #endif
    #include <cstring>

    #include "ast/Program.h"

    #ifdef _MSC_VER
    #define YY_NO_UNISTD_H
    #endif

    #include "parser/parser.hh"
    #include "parser/SrcLocation.h"
    #include "parser/ParserDriver.h"

    #define YYLTYPE SrcLocation

    #define YY_DECL yy::parser::symbol_type yylex(souffle::ParserDriver& driver, yyscan_t yyscanner)
    YY_DECL;

    #include "souffle/RamTypes.h"

    #include "souffle/utility/StringUtil.h"
    #include "souffle/utility/FileUtil.h"
    #include "souffle/utility/StreamUtil.h"
    #include "souffle/utility/MiscUtil.h"
    #include "souffle/utility/FunctionalUtil.h"
    #include "souffle/utility/ContainerUtil.h"
    #include "souffle/utility/CacheUtil.h"
    #include "souffle/utility/ParallelUtil.h"

    #define register

#define yylloc yyget_extra(yyscanner)->yylloc

#define yyinfo (*yyget_extra(yyscanner))

    void update_location(YYLTYPE& loc, int leng, char* text) {
        loc.start = loc.end;
        for (int i = 0; i < leng; ++i) {
          if (text[i] == '\n') {
            loc.end.line += 1;
            loc.end.column = 1;
          } else {
            loc.end.column += 1;
          }
        }
    }

    /* Execute when matching */
#define YY_USER_ACTION update_location(yylloc, yyleng, yytext);

    // scan a string with escape sequences, skipping surrounding double-quotes if any.
    std::string lexString(souffle::ParserDriver& driver, const SrcLocation& loc, const char* text) {
      std::string result;
      const size_t start = (text[0] == '"' ? 1 : 0);
      const size_t end = strlen(text) - (text[0] == '"' ? 1 : 0);
      bool error = false;
      char error_char;
      for (size_t i = start; i < end; i++) {
        if (text[i] == '\\' && i + 1 < end) {
          switch (text[i+1]) {
            case '"':  result += '"'; break;
            case '\'': result += '\''; break;
            case '\\': result += '\\'; break;
            case 'a':  result += '\a'; break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'v':  result += '\v'; break;
            default:
              error_char = text[i+1];
              error = true;
              break;
          }
          i++;
        } else {
          result += text[i];
        }
        if (error) {
          break;
        }
      }
      if (error) driver.error(loc, std::string("Unknown escape sequence \\") + error_char);
      return result;
    }

%}

%x COMMENT
%x INCLUDE

WS [ \t\r\v\f]

/* Add line number tracking */
%option noyywrap nounput

%%
".decl"/{WS}                          { return yy::parser::make_DECL(yylloc); }
".functor"/{WS}                       { return yy::parser::make_FUNCTOR(yylloc); }
".input"/{WS}                         { return yy::parser::make_INPUT_DECL(yylloc); }
".output"/{WS}                        { return yy::parser::make_OUTPUT_DECL(yylloc); }
".printsize"/{WS}                     { return yy::parser::make_PRINTSIZE_DECL(yylloc); }
".limitsize"/{WS}                     { return yy::parser::make_LIMITSIZE_DECL(yylloc); }
".type"/{WS}                          { return yy::parser::make_TYPE(yylloc); }
".comp"/{WS}                          { return yy::parser::make_COMPONENT(yylloc); }
".init"/{WS}                          { return yy::parser::make_INSTANTIATE(yylloc); }
".number_type"/{WS}                   { return yy::parser::make_NUMBER_TYPE(yylloc); }
".symbol_type"/{WS}                   { return yy::parser::make_SYMBOL_TYPE(yylloc); }
".override"/{WS}                      { return yy::parser::make_OVERRIDE(yylloc); }
".pragma"/{WS}                        { return yy::parser::make_PRAGMA(yylloc); }
".plan"/{WS}                          { return yy::parser::make_PLAN(yylloc); }
".include"                            {
                                        yyinfo.LastIncludeDirectiveLoc = yylloc;
                                        BEGIN(INCLUDE);
                                      }
".once"                               {
                                        if (!driver.canEnterOnce(yylloc)) {
                                          yypop_buffer_state(yyscanner);
                                          yyinfo.pop();
                                          if (!YY_CURRENT_BUFFER) {
                                            return yy::parser::make_END(yylloc);
                                          }
                                        }
                                      }
"autoinc"                             { return yy::parser::make_AUTOINC(yylloc); }
"band"                                { return yy::parser::make_BW_AND(yylloc); }
"bor"                                 { return yy::parser::make_BW_OR(yylloc); }
"bxor"                                { return yy::parser::make_BW_XOR(yylloc); }
"bnot"                                { return yy::parser::make_BW_NOT(yylloc); }
"bshl"                                { return yy::parser::make_BW_SHIFT_L(yylloc); }
"bshr"                                { return yy::parser::make_BW_SHIFT_R(yylloc); }
"bshru"                               { return yy::parser::make_BW_SHIFT_R_UNSIGNED(yylloc); }
"land"                                { return yy::parser::make_L_AND(yylloc); }
"lor"                                 { return yy::parser::make_L_OR(yylloc); }
"lxor"                                { return yy::parser::make_L_XOR(yylloc); }
"lnot"                                { return yy::parser::make_L_NOT(yylloc); }
"match"                               { return yy::parser::make_TMATCH(yylloc); }
"mean"                                { return yy::parser::make_MEAN(yylloc); }
"cat"                                 { return yy::parser::make_CAT(yylloc); }
"ord"                                 { return yy::parser::make_ORD(yylloc); }
"fold"                                { return yy::parser::make_FOLD(yylloc); }
"range"                               { return yy::parser::make_RANGE(yylloc); }
"strlen"                              { return yy::parser::make_STRLEN(yylloc); }
"substr"                              { return yy::parser::make_SUBSTR(yylloc); }
"stateful"                            { return yy::parser::make_STATEFUL(yylloc); }
"contains"                            { return yy::parser::make_TCONTAINS(yylloc); }
"output"                              { return yy::parser::make_OUTPUT_QUALIFIER(yylloc); }
"input"                               { return yy::parser::make_INPUT_QUALIFIER(yylloc); }
"overridable"                         { return yy::parser::make_OVERRIDABLE_QUALIFIER(yylloc); }
"printsize"                           { return yy::parser::make_PRINTSIZE_QUALIFIER(yylloc); }
"eqrel"                               { return yy::parser::make_EQREL_QUALIFIER(yylloc); }
"inline"                              { return yy::parser::make_INLINE_QUALIFIER(yylloc); }
"no_inline"                           { return yy::parser::make_NO_INLINE_QUALIFIER(yylloc); }
"magic"                               { return yy::parser::make_MAGIC_QUALIFIER(yylloc); }
"no_magic"                            { return yy::parser::make_NO_MAGIC_QUALIFIER(yylloc); }
"brie"                                { return yy::parser::make_BRIE_QUALIFIER(yylloc); }
"btree_delete"                        { return yy::parser::make_BTREE_DELETE_QUALIFIER(yylloc); }
"btree"                               { return yy::parser::make_BTREE_QUALIFIER(yylloc); }
"min"                                 { return yy::parser::make_MIN(yylloc); }
"max"                                 { return yy::parser::make_MAX(yylloc); }
"as"                                  { return yy::parser::make_AS(yylloc); }
"nil"                                 { return yy::parser::make_NIL(yylloc); }
"_"                                   { return yy::parser::make_UNDERSCORE(yylloc); }
"count"                               { return yy::parser::make_COUNT(yylloc); }
"sum"                                 { return yy::parser::make_SUM(yylloc); }
"true"                                { return yy::parser::make_TRUELIT(yylloc); }
"false"                               { return yy::parser::make_FALSELIT(yylloc); }
"to_float"                            { return yy::parser::make_TOFLOAT(yylloc); }
"to_number"                           { return yy::parser::make_TONUMBER(yylloc); }
"to_string"                           { return yy::parser::make_TOSTRING(yylloc); }
"to_unsigned"                         { return yy::parser::make_TOUNSIGNED(yylloc); }
"choice-domain"                       { return yy::parser::make_CHOICEDOMAIN(yylloc); }
"__FILE__"                            {
                                        return yy::parser::make_STRING(yylloc.file->Reported, yylloc);
                                      }
"__LINE__"                            { return yy::parser::make_NUMBER(std::to_string(yylloc.start.line), yylloc); }
"__INCL__"                            {
                                          std::string result;
                                          const IncludeStack* incl = yylloc.file.get();
                                          const Point* pos = &incl->IncludePos;
                                          // skip top
                                          if (incl) incl = incl->ParentStack.get();

                                          bool first = true;
                                          while(incl) {
                                            std::stringstream concat;
                                            concat << incl->Reported << ":" << *pos;
                                            if (!first) concat << ';';
                                            concat << result;
                                            result = concat.str();
                                            first = false;
                                            pos = &incl->IncludePos;
                                            incl = incl->ParentStack.get();
                                          }
                                          return yy::parser::make_STRING(result, yylloc);
                                      }
"|"                                   { return yy::parser::make_PIPE(yylloc); }
"["                                   { return yy::parser::make_LBRACKET(yylloc); }
"]"                                   { return yy::parser::make_RBRACKET(yylloc); }
"$"                                   { return yy::parser::make_DOLLAR(yylloc); }
"+"                                   { return yy::parser::make_PLUS(yylloc); }
"-"                                   { return yy::parser::make_MINUS(yylloc); }
"("                                   { return yy::parser::make_LPAREN(yylloc); }
")"                                   { return yy::parser::make_RPAREN(yylloc); }
","                                   { return yy::parser::make_COMMA(yylloc); }
":"                                   { return yy::parser::make_COLON(yylloc); }
";"                                   { return yy::parser::make_SEMICOLON(yylloc); }
"."                                   { return yy::parser::make_DOT(yylloc); }
"<:"                                  { return yy::parser::make_SUBTYPE(yylloc); }
"<="                                  { return yy::parser::make_LE(yylloc); }
">="                                  { return yy::parser::make_GE(yylloc); }
"!="                                  { return yy::parser::make_NE(yylloc); }
"="                                   { return yy::parser::make_EQUALS(yylloc); }
"!"                                   { return yy::parser::make_EXCLAMATION(yylloc); }
"*"                                   { return yy::parser::make_STAR(yylloc); }
"@"                                   { return yy::parser::make_AT(yylloc); }
"/"                                   { return yy::parser::make_SLASH(yylloc); }
"^"                                   { return yy::parser::make_CARET(yylloc); }
"%"                                   { return yy::parser::make_PERCENT(yylloc); }
"{"                                   { return yy::parser::make_LBRACE(yylloc); }
"}"                                   { return yy::parser::make_RBRACE(yylloc); }
"<"                                   { return yy::parser::make_LT(yylloc); }
">"                                   { return yy::parser::make_GT(yylloc); }
":-"                                  { return yy::parser::make_IF(yylloc); }
[0-9]+"."[0-9]+"."[0-9]+"."[0-9]+     {
                                        try {
                                        char *token = std::strtok(yytext, ".");
                                        int i = 0;
                                        int vals[4];
                                        while (token != NULL) {
                                          vals[i] = std::stoi(token);
                                          if(vals[i] > 255) {
                                            driver.error(yylloc, "IP out of range");
                                            return yy::parser::make_NUMBER("0", yylloc);
                                          }
                                          token = std::strtok(NULL, ".");
                                          ++i;
                                        }
                                        int ipnumber = (vals[0]<<24) + (vals[1]<<16) + (vals[2]<<8) + vals[3];
                                        return yy::parser::make_NUMBER(std::to_string(ipnumber), yylloc);
                                        } catch(...) {
                                          driver.error(yylloc, "IP out of range");
                                          return yy::parser::make_NUMBER("0", yylloc);
                                        }
                                      }
[0-9]+[.][0-9]+                       { return yy::parser::make_FLOAT(yytext, yylloc); }
[0-9]+                                { return yy::parser::make_NUMBER(yytext, yylloc); }
0b[0-1]+                              { return yy::parser::make_NUMBER(yytext, yylloc); }
0x[a-fA-F0-9]+                        { return yy::parser::make_NUMBER(yytext, yylloc); }
[0-9]+u                               { return yy::parser::make_UNSIGNED(yytext, yylloc); }
0b[0-1]+u                             { return yy::parser::make_UNSIGNED(yytext, yylloc); }
0x[a-fA-F0-9]+u                       { return yy::parser::make_UNSIGNED(yytext, yylloc); }
[\?a-zA-Z]|[_\?a-zA-Z][_\?a-zA-Z0-9]+ {
                                        return yy::parser::make_IDENT(yytext, yylloc);
                                      }
\"(\\.|[^"\\])*\"                     {
                                        return yy::parser::make_STRING(lexString(driver, yylloc, yytext), yylloc);
                                      }
\#.*$                                 {
                                        /* formats:
                                          "#" linenum filename
                                          "#" linenum filename 1
                                          "#" linenum filename 2
                                          "#line" linenum
                                          "#line" linenum filename
                                        */
                                        std::unique_ptr<char[]> fname_ptr = std::make_unique<char[]>(yyleng+1);
                                        char* fname = fname_ptr.get();
                                        fname[0] = 0;
                                        int lineno = 0;
                                        int flag = 0;

                                        if ((sscanf(yytext,"# %d \"%[^\"]\" %d",&lineno,fname,&flag)>=2) ||
                                            (sscanf(yytext,"#line %d \"%[^\"]\" %d",&lineno,fname,&flag)>=1)) {

                                          if (fname[0] != 0) {
                                            std::string filename = lexString(driver, yylloc, fname);
                                            /* recognized C preprocessor flags:
                                             * 0 (or no flag) => update location
                                             * 1 => enter file (include push)
                                             * 2 => return to file (include pop)
                                             */

                                            if (flag == 0) {
                                              // update
                                              yyinfo.setReported(filename);
                                              yylloc.start = yylloc.end = {lineno-1, 1};
                                            } else if (flag == 1) {
                                              yyinfo.push(filename, yylloc,
                                                      yylloc.file->ReducedConsecutiveNonLeadingWhitespaces);
                                              yylloc.start = yylloc.end = {lineno-1, 1};
                                            } else if (flag == 2) {
                                              yyinfo.pop();
                                              yyinfo.setReported(filename);
                                              yylloc.start = yylloc.end = {lineno-1, 1};
                                            }
                                          } else {
                                              yylloc.start = yylloc.end = {lineno-1, 1};
                                          }
                                        }
                                      }
"//".*$                               {
                                        yyinfo.CommentExtent = yylloc;
                                        yyinfo.CommentContent.str(yytext);
                                        driver.addComment(yyinfo.CommentExtent, yyinfo.CommentContent);
                                        yyinfo.CommentContent.str("");
                                      }
"/*"                                  {
                                        yyinfo.CommentContent.str("");
                                        yyinfo.CommentExtent = yylloc;
                                        yyinfo.CommentContent << yytext;
                                        BEGIN(COMMENT);
                                      }
<COMMENT>{
"*/"                                  {
                                        yyinfo.CommentExtent += yylloc;
                                        std::string X(yytext);
                                        yyinfo.CommentContent << X;
                                        driver.addComment(yyinfo.CommentExtent, yyinfo.CommentContent);
                                        yyinfo.CommentContent.str("");
                                        BEGIN(INITIAL);
                                      }
[^*\n]+                               { yyinfo.CommentExtent += yylloc; yyinfo.CommentContent << yytext; }
"*"                                   { yyinfo.CommentExtent += yylloc; yyinfo.CommentContent << yytext; }
\n                                    { yyinfo.CommentExtent += yylloc; yyinfo.CommentContent << yytext; }
<<EOF>>                               { /* unterminated comment */
                                          yyinfo.CommentExtent += yylloc;
                                          std::string X(yytext);
                                          yyinfo.CommentContent << X;
                                          driver.addComment(yyinfo.CommentExtent, yyinfo.CommentContent);
                                          yyinfo.CommentContent.str("");

                                          driver.error(yyinfo.CommentExtent, std::string("unterminated comment block"));
                                          BEGIN(INITIAL);
                                      }
}
<INCLUDE>{
{WS}+                                 { }
\"(\\.|[^"\\])*\"                     { /* include file name */
                                        std::string path = lexString(driver, yylloc, yytext);
                                        std::optional<std::filesystem::path> maybePath = driver.searchIncludePath(path, yylloc);
                                        yyin = nullptr;

                                        if (!maybePath) {
                                          driver.error(yylloc, std::string("cannot find include file ") + yytext);
                                          return yy::parser::make_END(yylloc);
                                        } else {
                                          std::error_code ec;
                                          auto code = driver.readFile(*maybePath, ec);

                                          if (ec) {
                                            driver.error(yylloc, std::string("cannot read file ") + maybePath->u8string());
                                            return yy::parser::make_END(yylloc);
                                          }

                                          auto state = yy_create_buffer(nullptr, 32768, yyscanner);
                                          yypush_buffer_state(state, yyscanner);
                                          yy_scan_string(code->c_str(), yyscanner);
                                          yy_delete_buffer(state, yyscanner);

                                          yyinfo.holdInputBuffer(std::move(code));
                                          yyinfo.push(*maybePath, yyinfo.LastIncludeDirectiveLoc,
                                                  /* not using a C preprocessor, all whitespaces are
                                                     maintained */
                                                  false);
                                        }
                                        BEGIN(INITIAL);
                                      }
.                                     { driver.error(yylloc, std::string("unexpected ") + yytext); }
}
\n                                    { }
{WS}+                                 { }
<<EOF>>                               {
                                        yypop_buffer_state(yyscanner);
                                        yyinfo.pop();
                                        if (!YY_CURRENT_BUFFER) {
                                          return yy::parser::make_END(yylloc);
                                        }
                                      }
.                                     { driver.error(yylloc, std::string("unexpected ") + yytext); }
%%
// vim: filetype=lex
