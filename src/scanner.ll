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

%x ZERO HEX BIN IN_COMMENT

%%

0 { BEGIN(ZERO); }
"//".*$                          {  }
<INITIAL>{
"/*"              BEGIN(IN_COMMENT);
}
<IN_COMMENT>{
"*/"      BEGIN(INITIAL);
[^*\n]+   // eat comment in chunks
"*"       // eat the lone star
\n        //
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
<ZERO>[0-9]*  {  
                BEGIN(INITIAL);
                try {
                      printf("\nreturning number\n");
                      return yy::parser::make_NUMBER(std::stoll(yytext, NULL, 10), yylloc);  
                    } catch (...) { 
                      driver.error(yylloc, "integer constant must be in range [0, 2147483647]");
                      return yy::parser::make_NUMBER(0, yylloc);
                    }
              }
<ZERO>[b] { BEGIN(BIN); }
<ZERO>[x] { BEGIN(HEX); }

<BIN>[0-1]+ { BEGIN(INITIAL);
              try {
                return yy::parser::make_NUMBER(std::stoll(yytext, NULL, 2), yylloc); 
              }
              catch(...) {
                driver.error(yylloc, "bool out of range");
                return yy::parser::make_NUMBER(0, yylloc); 
              }
            }

<HEX>[A-Fa-f0-9]+ {  BEGIN(INITIAL); 
                     try {
                       return yy::parser::make_NUMBER(std::stoll(yytext, NULL, 16), yylloc);
                     }
                     catch(...) {
                       driver.error(yylloc, "hex out of range");
                       return yy::parser::make_NUMBER(0, yylloc);
                     }
                  }

":-"                             { return yy::parser::make_IF(yylloc); }
(!=|>=|<=)                       { return yy::parser::make_RELOP(SLOOKUP(yytext), yylloc); }
[_\?[:alpha:]][[:alnum:]_\?]*     { if (!strcmp(yytext, "_")) {
                                        return yy::parser::make_UNDERSCORE(yylloc);
                                    } else {
                                        return yy::parser::make_IDENT(SLOOKUP(yytext), yylloc); }
                                    }
[0-9]+                           { try {
                                      return yy::parser::make_NUMBER(std::stoll(yytext), yylloc); 
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
