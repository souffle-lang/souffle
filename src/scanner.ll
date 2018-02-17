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
%option extra-type="struct scanner_data *"
%{

#if defined(__clang__)
# pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#endif

    #include <stdio.h>
    #include <libgen.h>
    #include <ctype.h>
    #include <sys/stat.h>
    #include <stack>
    #include <string>
    #include <sstream>
    #include <assert.h>
    #include <unistd.h>
    #include <cstring>

    #include "AstProgram.h"
    #include "StringPool.h"

    #include "AstSrcLocation.h"
    #define YYLTYPE AstSrcLocation

    #include "ParserDriver.h"
    #include "RamTypes.h"
    #include "parser.hh"

    #define register

    /* String Pool declarations */
    StringPool::hashentry *StringPool::hashtab[HASH_SIZE];

#define yylloc yyget_extra(yyscanner)->yylloc

#define yyfilename yyget_extra(yyscanner)->yyfilename

    /* Execute when matching */
#define YY_USER_ACTION  { \
    yylloc.start = AstSrcLocation::Point({ yylineno, yycolumn }); \
    yycolumn += yyleng;             \
    yylloc.end   = AstSrcLocation::Point({ yylineno, yycolumn }); \
    yylloc.filename = yyfilename;   \
}

%}

%x COMMENT

/* Add line number tracking */
%option yylineno noyywrap nounput

%%
".decl"                               { return yy::parser::make_DECL(yylloc); }
".input"                              { return yy::parser::make_INPUT_DECL(yylloc); }
".output"                             { return yy::parser::make_OUTPUT_DECL(yylloc); }
".printsize"                          { return yy::parser::make_PRINTSIZE_DECL(yylloc); }
".type"                               { return yy::parser::make_TYPE(yylloc); }
".comp"                               { return yy::parser::make_COMPONENT(yylloc); }
".init"                               { return yy::parser::make_INSTANTIATE(yylloc); }
".number_type"                        { return yy::parser::make_NUMBER_TYPE(yylloc); }
".symbol_type"                        { return yy::parser::make_SYMBOL_TYPE(yylloc); }
".override"                           { return yy::parser::make_OVERRIDE(yylloc); }
".pragma"                             { return yy::parser::make_PRAGMA(yylloc); }
"band"                                { return yy::parser::make_BW_AND(yylloc); }
"bor"                                 { return yy::parser::make_BW_OR(yylloc); }
"bxor"                                { return yy::parser::make_BW_XOR(yylloc); }
"bnot"                                { return yy::parser::make_BW_NOT(yylloc); }
"land"                                { return yy::parser::make_L_AND(yylloc); }
"lor"                                 { return yy::parser::make_L_OR(yylloc); }
"lnot"                                { return yy::parser::make_L_NOT(yylloc); }
"sin"                                 { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"cos"                                 { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"tan"                                 { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"asin"                                { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"acos"                                { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"atan"                                { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"sinh"                                { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"cosh"                                { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"tanh"                                { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"asinh"                               { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"acosh"                               { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"atanh"                               { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"log"                                 { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"exp"                                 { return yy::parser::make_RESERVED(yytext, yylloc); } // TODO (see issue #298)
"match"                               { return yy::parser::make_TMATCH(yylloc); }
"cat"                                 { return yy::parser::make_CAT(yylloc); }
"ord"                                 { return yy::parser::make_ORD(yylloc); }
"strlen"                              { return yy::parser::make_STRLEN(yylloc); }
"substr"                              { return yy::parser::make_SUBSTR(yylloc); }
"contains"                            { return yy::parser::make_TCONTAINS(yylloc); }
"output"                              { return yy::parser::make_OUTPUT_QUALIFIER(yylloc); }
"input"                               { return yy::parser::make_INPUT_QUALIFIER(yylloc); }
"data"                                { return yy::parser::make_DATA_QUALIFIER(yylloc); }
"overridable"                         { return yy::parser::make_OVERRIDABLE_QUALIFIER(yylloc); }
"printsize"                           { return yy::parser::make_PRINTSIZE_QUALIFIER(yylloc); }
"eqrel"                               { return yy::parser::make_EQREL_QUALIFIER(yylloc); }
"hashmap"                               { return yy::parser::make_HASHMAP_QUALIFIER(yylloc); }
"inline"                              { return yy::parser::make_INLINE_QUALIFIER(yylloc); }
"brie"                                { return yy::parser::make_BRIE_QUALIFIER(yylloc); }
"btree"                               { return yy::parser::make_BTREE_QUALIFIER(yylloc); }
"min"                                 { return yy::parser::make_MIN(yylloc); }
"max"                                 { return yy::parser::make_MAX(yylloc); }
"nil"                                 { return yy::parser::make_NIL(yylloc); }
"_"                                   { return yy::parser::make_UNDERSCORE(yylloc); }
"count"                               { return yy::parser::make_COUNT(yylloc); }
"sum"                                 { return yy::parser::make_SUM(yylloc); }
".strict"                             { return yy::parser::make_STRICT(yylloc); }
".plan"                               { return yy::parser::make_PLAN(yylloc); }
"|"                                   { return yy::parser::make_PIPE(yylloc); }
"["                                   { return yy::parser::make_LBRACKET(yylloc); }
"]"                                   { return yy::parser::make_RBRACKET(yylloc); }
"$"                                   { return yy::parser::make_DOLLAR(yylloc); }
"+"                                   { return yy::parser::make_PLUS(yylloc); }
"-"                                   { return yy::parser::make_MINUS(yylloc); }
"!"                                   { return yy::parser::make_EXCLAMATION(yylloc); }
"("                                   { return yy::parser::make_LPAREN(yylloc); }
")"                                   { return yy::parser::make_RPAREN(yylloc); }
","                                   { return yy::parser::make_COMMA(yylloc); }
":"                                   { return yy::parser::make_COLON(yylloc); }
";"                                   { return yy::parser::make_SEMICOLON(yylloc); }
"."                                   { return yy::parser::make_DOT(yylloc); }
"="                                   { return yy::parser::make_EQUALS(yylloc); }
"*"                                   { return yy::parser::make_STAR(yylloc); }
"/"                                   { return yy::parser::make_SLASH(yylloc); }
"^"                                   { return yy::parser::make_CARET(yylloc); }
"%"                                   { return yy::parser::make_PERCENT(yylloc); }
"{"                                   { return yy::parser::make_LBRACE(yylloc); }
"}"                                   { return yy::parser::make_RBRACE(yylloc); }
"<"                                   { return yy::parser::make_LT(yylloc); }
">"                                   { return yy::parser::make_GT(yylloc); }
":-"                                  { return yy::parser::make_IF(yylloc); }
(!=|>=|<=)                            { return yy::parser::make_RELOP(SLOOKUP(yytext), yylloc); }
[0-9]+"."[0-9]+"."[0-9]+"."[0-9]+     {
                                        try {
                                        char *token = std::strtok(yytext, ".");
                                        int i = 0;
                                        int vals[4];
                                        while (token != NULL) {
                                          vals[i] = std::stoi(token);
                                          if(vals[i] > 255) {
                                            driver.error(yylloc, "IP out of range");
                                            return yy::parser::make_NUMBER(0, yylloc);
                                          }
                                          token = std::strtok(NULL, ".");
                                          ++i;
                                        }
                                        int ipnumber = (vals[0]<<24) + (vals[1]<<16) + (vals[2]<<8) + vals[3];
                                        return yy::parser::make_NUMBER(ipnumber, yylloc);
                                        } catch(...) {
                                          driver.error(yylloc, "IP out of range");
                                          return yy::parser::make_NUMBER(0, yylloc);
                                        }
                                      }
0b[0-1][0-1]*                         {
                                        try {
                                          return yy::parser::make_NUMBER((RamDomain)std::stoull(yytext+2, NULL, 2), yylloc);
                                        } catch(...) {
                                          driver.error(yylloc, "bool out of range");
                                          return yy::parser::make_NUMBER(0, yylloc);
                                        }
                                      }
0x[a-fA-F0-9]+                        {
                                        try {
                                          return yy::parser::make_NUMBER((RamDomain)std::stoull(yytext+2, NULL, 16), yylloc);
                                        } catch(...) {
                                          driver.error(yylloc, "hex out of range");
                                          return yy::parser::make_NUMBER(0, yylloc);
                                        }
                                      }
0|([1-9][0-9]*)                       {
                                        try {
                                          return yy::parser::make_NUMBER(std::stoull(yytext, NULL, 10), yylloc);
                                        } catch (...) {
                                          driver.error(yylloc, "int out of range");
                                          return yy::parser::make_NUMBER(0, yylloc);
                                        }
                                      }
[\?a-zA-Z]|[_\?a-zA-Z][_\?a-zA-Z0-9]+ {
                                        return yy::parser::make_IDENT(SLOOKUP(yytext), yylloc);
                                      }
\"([^\"]*|\\[^n])*\"                  {
                                        yytext[strlen(yytext)-1]=0;
                                        if(strlen(&yytext[1]) == 0) {
                                          driver.error(yylloc, "string literal is empty");
                                        }
                                        for(size_t i=1;i<=strlen(&yytext[1]); i++) {
                                          if(yytext[i] == '\t' || yytext[i] == '\n') {
                                            driver.error(yylloc, "no tabs/newlines in string literals");
                                            break;
                                          }
                                        }
                                        for(size_t i=1;i<=strlen(&yytext[1]); i++) {
                                          if(!isascii(yytext[i])) {
                                            driver.error(yylloc, "only ascii characters in string literals");
                                            break;
                                          }
                                        }
                                        return yy::parser::make_STRING(SLOOKUP(&yytext[1]), yylloc);
                                      }
\#.*$                                 {
                                        char fname[yyleng+1];
                                        int lineno;
                                        if(sscanf(yytext,"# %d \"%s",&lineno,fname)>=2) {
                                          assert(strlen(fname) > 0 && "failed conversion");
                                          fname[strlen(fname)-1]='\0';
                                          yycolumn = 1; yylineno = lineno-1;
                                          yyfilename = SLOOKUP(fname);
                                        } else if(sscanf(yytext,"#line %d \"%s",&lineno,fname)>=2) {
                                          assert(strlen(fname) > 0 && "failed conversion");
                                          fname[strlen(fname)-1]='\0';
                                          yycolumn = 1; yylineno = lineno-1;
                                          yyfilename = SLOOKUP(fname);
                                        }
                                      }
"//".*$                               { }
"/*"                                  { BEGIN(COMMENT); }
<COMMENT>{
"*/"                                  { BEGIN(INITIAL); }
[^*\n]+                               { }
"*"                                   { }
\n                                    { }
}
\n                                    { yycolumn = 1; }
[ \t\r\v\f]*                          { }
<<EOF>>                               { return yy::parser::make_END(yylloc); }
.                                     { driver.error(yylloc, std::string("unexpected ") + yytext); }
%%
