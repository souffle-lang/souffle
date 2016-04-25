/*
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All Rights reserved
 * 
 * The Universal Permissive License (UPL), Version 1.0
 * 
 * Subject to the condition set forth below, permission is hereby granted to any person obtaining a copy of this software,
 * associated documentation and/or data (collectively the "Software"), free of charge and under any and all copyright rights in the 
 * Software, and any and all patent rights owned or freely licensable by each licensor hereunder covering either (i) the unmodified 
 * Software as contributed to or provided by such licensor, or (ii) the Larger Works (as defined below), to deal in both
 * 
 * (a) the Software, and
 * (b) any piece of software and/or hardware listed in the lrgrwrks.txt file if one is included with the Software (each a “Larger
 * Work” to which the Software is contributed by such licensors),
 * 
 * without restriction, including without limitation the rights to copy, create derivative works of, display, perform, and 
 * distribute the Software and make, use, sell, offer for sale, import, export, have made, and have sold the Software and the 
 * Larger Work(s), and to sublicense the foregoing rights on either these or other terms.
 * 
 * This license is subject to the following condition:
 * The above copyright notice and either this complete permission notice or at a minimum a reference to the UPL must be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
    #include <stdio.h>
    #include <libgen.h>
    #include <ctype.h>
    #include <sys/stat.h>
    #include <stack>
    #include <string>
    #include <sstream>
    #include <assert.h>
    #include <unistd.h>

    #include "AstProgram.h"
    #include "StringPool.h"

    #include "AstSrcLocation.h"
    #define YYLTYPE AstSrcLocation

    #include "ParserDriver.h"    
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

/* Add line number tracking */
%option yylineno noyywrap nounput

%x IN_COMMENT

%%

"//".*$                          {  }
<INITIAL>{
"/*"              BEGIN(IN_COMMENT);
}
<IN_COMMENT>{
"*/"      BEGIN(INITIAL);
[^*\n]+   // eat comment in chunks
"*"       // eat the lone star
\n        yylineno++;
}
[ \t\r\v\f]*                     {  }
\n                               { yycolumn = 1; }
"#".*$                           { // processing line directive from cpp
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
".decl"                          { return yy::parser::make_DECL(yylloc); }
".type"                          { return yy::parser::make_TYPE(yylloc); }
".comp"                          { return yy::parser::make_COMPONENT(yylloc); }
".init"                          { return yy::parser::make_INSTANTIATE(yylloc); }
".number_type"                   { return yy::parser::make_NUMBER_TYPE(yylloc); }
".symbol_type"                   { return yy::parser::make_SYMBOL_TYPE(yylloc); }
".override"                      { return yy::parser::make_OVERRIDE(yylloc); }
"band"                           { return yy::parser::make_BW_AND(yylloc); }
"bor"                            { return yy::parser::make_BW_OR(yylloc); }
"bxor"                           { return yy::parser::make_BW_XOR(yylloc); }
"bnot"                           { return yy::parser::make_BW_NOT(yylloc); }
"land"                           { return yy::parser::make_L_AND(yylloc); }
"lor"                            { return yy::parser::make_L_OR(yylloc); }
"lnot"                           { return yy::parser::make_L_NOT(yylloc); }
"match"                          { return yy::parser::make_TMATCH(yylloc); }
"cat"                            { return yy::parser::make_CAT(yylloc); }
"ord"                            { return yy::parser::make_ORD(yylloc); }
"contains"                       { return yy::parser::make_TCONTAINS(yylloc); }
"output"                         { return yy::parser::make_OUTPUT_QUALIFIER(yylloc); }
"input"                          { return yy::parser::make_INPUT_QUALIFIER(yylloc); }
"overridable"                    { return yy::parser::make_OVERRIDABLE_QUALIFIER(yylloc); }
"printsize"                      { return yy::parser::make_PRINTSIZE_QUALIFIER(yylloc); }
"min"                            { return yy::parser::make_MIN(yylloc); }
"max"                            { return yy::parser::make_MAX(yylloc); }
"nil"                            { return yy::parser::make_NIL(yylloc); }
"count"                          { return yy::parser::make_COUNT(yylloc); }
".strict"                        { return yy::parser::make_STRICT(yylloc); }
".plan"                          { return yy::parser::make_PLAN(yylloc); }
"|"                              { return yy::parser::make_PIPE(yylloc); }
"["                              { return yy::parser::make_LBRACKET(yylloc); }
"]"                              { return yy::parser::make_RBRACKET(yylloc); }
"_"                              { return yy::parser::make_UNDERSCORE(yylloc); }
"$"                              { return yy::parser::make_DOLLAR(yylloc); }
"+"                              { return yy::parser::make_PLUS(yylloc); }
"-"                              { return yy::parser::make_MINUS(yylloc); }
"!"                              { return yy::parser::make_EXCLAMATION(yylloc); }
"("                              { return yy::parser::make_LPAREN(yylloc); }
")"                              { return yy::parser::make_RPAREN(yylloc); }
","                              { return yy::parser::make_COMMA(yylloc); }
":"                              { return yy::parser::make_COLON(yylloc); }
";"                              { return yy::parser::make_SEMICOLON(yylloc); }
"."                              { return yy::parser::make_DOT(yylloc); }
"="                              { return yy::parser::make_EQUALS(yylloc); }
"*"                              { return yy::parser::make_STAR(yylloc); }
"/"                              { return yy::parser::make_SLASH(yylloc); }
"^"                              { return yy::parser::make_CARET(yylloc); }
"%"                              { return yy::parser::make_PERCENT(yylloc); }
"{"                              { return yy::parser::make_LBRACE(yylloc); }
"}"                              { return yy::parser::make_RBRACE(yylloc); }
"<"                              { return yy::parser::make_LT(yylloc); }
">"                              { return yy::parser::make_GT(yylloc); }
[\?[:alpha:]][_\?[:alnum:]]*     { return yy::parser::make_IDENT(SLOOKUP(yytext), yylloc); }
":-"                             { return yy::parser::make_IF(yylloc); }
(!=|>=|<=)                       { return yy::parser::make_RELOP(SLOOKUP(yytext), yylloc); }
[0-9]+                           { try {
                                      return yy::parser::make_NUMBER(std::stoi(yytext), yylloc); 
                                   } catch (...) { 
                                      driver.error(yylloc, "integer constant must be in range [0, 2147483647]");
                                      return yy::parser::make_NUMBER(0, yylloc);
                                   }
                                 }
\"[^\"]*\"                       { yytext[strlen(yytext)-1]=0; 
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
<<EOF>>                          { return yy::parser::make_END(yylloc); }                                 
.                                { driver.error(yylloc, std::string("unexpected ") + yytext); } 

%%
