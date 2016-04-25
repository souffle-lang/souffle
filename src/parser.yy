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
 * @file parser.yy
 *
 * @brief Parser for Datalog
 *
 ***********************************************************************/
%skeleton "lalr1.cc"
%require "3.0.2"
%defines
%define parser_class_name {parser}
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define api.location.type {AstSrcLocation}

%code requires {
    #include <config.h>
    #include <stdlib.h>
    #include <stdio.h>
    #include <assert.h>
    #include <unistd.h>
    #include <stdarg.h>
    #include <string>
    #include <stack>
    #include <unistd.h>

    #include "Util.h"
    #include "AstProgram.h"
    #include "AstClause.h"
    #include "AstComponent.h"
    #include "AstRelation.h"
    #include "AstArgument.h"
    #include "AstNode.h"
    #include "BinaryOperator.h"
    #include "AstParserUtils.h"
    
    #include "AstSrcLocation.h"

    using namespace souffle;

    namespace souffle {
        class ParserDriver;
    }

    typedef void* yyscan_t;
    
    #define YY_NULLPTR nullptr
    
    /* Macro to update locations as parsing proceeds */
    # define YYLLOC_DEFAULT(Cur, Rhs, N)                       \
    do {                                                       \
       if (N) {                                                \
           (Cur).start        = YYRHSLOC(Rhs, 1).start;        \
           (Cur).end          = YYRHSLOC(Rhs, N).end;          \
           (Cur).filename     = YYRHSLOC(Rhs, N).filename;     \
       } else {                                                \
           (Cur).start    = YYRHSLOC(Rhs, 0).end;              \
           (Cur).filename = YYRHSLOC(Rhs, 0).filename;         \
       }                                                       \
     } while (0)
}

%param { ParserDriver &driver }
%param { yyscan_t yyscanner }

%locations

%define parse.trace
%define parse.error verbose

%code {
    #include "ParserDriver.h"
}

%token END 0                     "end of file"
%token <std::string> STRING      "symbol"
%token <std::string> IDENT       "identifier"
%token <int> NUMBER              "number"
%token <std::string> RELOP       "relational operator"
%token OUTPUT_QUALIFIER          "relation qualifier output"
%token INPUT_QUALIFIER           "relation qualifier input"
%token PRINTSIZE_QUALIFIER       "relation qualifier printsize"
%token OVERRIDABLE_QUALIFIER     "relation qualifier overidable"
%token TMATCH                    "match predicate"
%token TCONTAINS                 "checks whether substring is contained in a string"
%token CAT                       "concatenation of two strings"
%token ORD                       "ordinal number of a string"
%token MIN                       "min aggregator"
%token MAX                       "max aggregator"
%token COUNT                     "count aggregator"
%token STRICT                    "strict marker"
%token PLAN                      "plan keyword"
%token IF                        ":-"
%token DECL                      "relation declaration"
%token OVERRIDE                  "override rules of super-component"
%token TYPE                      "type declaration"
%token COMPONENT                 "component declaration"
%token INSTANTIATE               "component instantiation"
%token NUMBER_TYPE               "numeric type declaration"
%token SYMBOL_TYPE               "symbolic type declaration"
%token AS                        "type cast"
%token NIL                       "nil reference"
%token PIPE                      "|"
%token LBRACKET                  "["
%token RBRACKET                  "]"
%token UNDERSCORE                "_"
%token DOLLAR                    "$"
%token PLUS                      "+"
%token MINUS                     "-"
%token EXCLAMATION               "!"
%token LPAREN                    "("
%token RPAREN                    ")"
%token COMMA                     ","
%token COLON                     ":"
%token SEMICOLON                 ";"
%token DOT                       "."
%token EQUALS                    "="
%token STAR                      "*"
%token SLASH                     "/"
%token CARET                     "^"
%token PERCENT                   "%"
%token LBRACE                    "{"
%token RBRACE                    "}"
%token LT                        "<"
%token GT                        ">"
%token BW_AND                    "band"
%token BW_OR                     "bor"
%token BW_XOR                    "bxor"
%token BW_NOT                    "bnot"
%token L_AND                     "land"
%token L_OR                      "lor"
%token L_NOT                     "lnot"

%type <int>                      qualifiers 
%type <AstRelationIdentifier *>  rel_id
%type <AstProgram *>             unit
%type <AstType *>                type
%type <AstComponent *>           component component_head component_body
%type <AstComponentType *>       comp_type
%type <AstComponentInit *>       comp_init
%type <AstRelation *>            attributes relation
%type <AstArgument *>            arg
%type <AstAtom *>                arg_list atom
%type <std::vector<AstAtom*>>    head
%type <RuleBody *>               literal conjunction body
%type <AstClause *>              fact
%type <std::vector<AstClause*>>  rule rule_def
%type <AstExecutionOrder *>      exec_order exec_order_list
%type <AstExecutionPlan *>       exec_plan exec_plan_list
%type <AstRecordInit *>          recordlist 
%type <AstRecordType *>          recordtype 
%type <AstUnionType *>             uniontype
%type <std::vector<std::string>> type_params type_param_list
%type <std::string>              comp_override

%printer { yyoutput << $$; } <*>;

%left DOT COLON
%right AS
%left L_OR
%left L_AND
%left BW_OR 
%left BW_XOR
%left BW_AND
%left PLUS MINUS
%left STAR SLASH PERCENT
%left BW_NOT L_NOT
%left NEG 
%left CARET

%%
%start program;

/* Program */
program: unit

/* Top-level statement */
unit: unit type { $$ = $1; driver.addType($2); }
    | unit relation { $$ = $1; driver.addRelation($2); }
    | unit fact { $$ = $1; driver.addClause($2); }
    | unit rule { $$ = $1; for(const auto& cur : $2) driver.addClause(cur); }
    | unit component { $$ = $1; driver.addComponent($2); }
    | unit comp_init { $$ = $1; driver.addInstantiation($2); }
    | {
      }
    ;

/* Type Declaration */
type: NUMBER_TYPE IDENT {
          $$ = new AstPrimitiveType($2, true);
          $$->setSrcLoc(@$);
      }
    | SYMBOL_TYPE IDENT {
          $$ = new AstPrimitiveType($2, false);
          $$->setSrcLoc(@$);
      }
    | TYPE IDENT {
          $$ = new AstPrimitiveType($2);
          $$->setSrcLoc(@$);
      }
    | TYPE IDENT EQUALS uniontype {
          $$ = $4; 
          $$->setName($2);
          $$->setSrcLoc(@$);
      }
    | TYPE IDENT EQUALS LBRACKET recordtype RBRACKET {
          $$ = $5; 
          $$->setName($2);
          $$->setSrcLoc(@$); 
      }
    | TYPE IDENT EQUALS LBRACKET RBRACKET {
          $$ = new AstRecordType(); 
          $$->setName($2); 
          $$->setSrcLoc(@$); 
      }
    ;

recordtype: IDENT COLON IDENT  { $$ = new AstRecordType(); $$->add($1, $3); } 
          | recordtype COMMA IDENT COLON IDENT  {  $$ = $1; $1->add($3, $5); } 
          ;

uniontype: IDENT  { $$ = new AstUnionType(); $$->add($1); } 
         | uniontype PIPE IDENT { $$ = $1; $1->add($3); }
         ;

/* Relation Identifier */

rel_id: 
      IDENT { $$ = new AstRelationIdentifier($1); }
    | rel_id DOT IDENT { $$ = $1; $$->append($3); }
    ;
         
         
/* Relations */
attributes: IDENT COLON IDENT {
           $$ = new AstRelation();
           AstAttribute *a = new AstAttribute($1, $3);
           a->setSrcLoc(@3);
           $$->addAttribute(std::unique_ptr<AstAttribute>(a));
          }
        | attributes COMMA IDENT COLON IDENT {
            $$ = $1;
            AstAttribute *a = new AstAttribute($3, $5);
            a->setSrcLoc(@5);
            $$->addAttribute(std::unique_ptr<AstAttribute>(a));
          }
        ;

qualifiers: qualifiers OUTPUT_QUALIFIER { if($1 & OUTPUT_RELATION) driver.error(@2, "output qualifier already set"); $$ = $1 | OUTPUT_RELATION; }
          | qualifiers INPUT_QUALIFIER { if($1 & INPUT_RELATION) driver.error(@2, "input qualifier already set"); $$ = $1 | INPUT_RELATION; }
          | qualifiers PRINTSIZE_QUALIFIER { if($1 & PRINTSIZE_RELATION) driver.error(@2, "printsize qualifier already set"); $$ = $1 | PRINTSIZE_RELATION; }
          | qualifiers OVERRIDABLE_QUALIFIER { if($1 & OVERRIDABLE_RELATION) driver.error(@2, "overridable qualifier already set"); $$ = $1 | OVERRIDABLE_RELATION; }
          | {$$ = 0; }
          ;

relation: DECL IDENT LPAREN attributes RPAREN qualifiers {
           $$ = $4;
           $4->setName($2);
           $4->setQualifier($6);
           $$->setSrcLoc(@$);
          }
        ;

/* Atom */
arg: STRING {
       $$ = new AstStringConstant(driver.getSymbolTable(), $1.c_str());
       $$->setSrcLoc(@$);
     }
   | UNDERSCORE {
       $$ = new AstUnnamedVariable();
       $$->setSrcLoc(@$);
     }
   | DOLLAR {
       $$ = new AstCounter();
       $$->setSrcLoc(@$);
     }
   | IDENT {
       $$ = new AstVariable($1);
       $$->setSrcLoc(@$);
     }
   | NUMBER {
       $$ = new AstNumberConstant($1);
       $$->setSrcLoc(@$);
     }
   | LPAREN arg RPAREN {
       $$ = $2;
     }
   | arg BW_OR arg {
       $$ = new AstBinaryFunctor(BinaryOp::BOR, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg BW_XOR arg {
       $$ = new AstBinaryFunctor(BinaryOp::BXOR, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg BW_AND arg {
       $$ = new AstBinaryFunctor(BinaryOp::BAND, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg L_OR arg {
       $$ = new AstBinaryFunctor(BinaryOp::LOR, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg L_AND arg {
       $$ = new AstBinaryFunctor(BinaryOp::LAND, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg PLUS arg {
       $$ = new AstBinaryFunctor(BinaryOp::ADD, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg MINUS arg {
       $$ = new AstBinaryFunctor(BinaryOp::SUB, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg STAR arg {
       $$ = new AstBinaryFunctor(BinaryOp::MUL, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg SLASH arg {
       $$ = new AstBinaryFunctor(BinaryOp::DIV, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg PERCENT arg {
       $$ = new AstBinaryFunctor(BinaryOp::MOD, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | arg CARET arg {
       $$ = new AstBinaryFunctor(BinaryOp::EXP, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   | CAT LPAREN arg COMMA arg RPAREN {
       $$ = new AstBinaryFunctor(BinaryOp::CAT, std::unique_ptr<AstArgument>($3), std::unique_ptr<AstArgument>($5));
       $$->setSrcLoc(@$);
     }
   | ORD LPAREN arg RPAREN {
       $$ = new AstUnaryFunctor(AstUnaryFunctor::ORDINAL, std::unique_ptr<AstArgument>($3));
       $$->setSrcLoc(@$);
     }
   |  arg AS IDENT { 
       $$ = new AstTypeCast(std::unique_ptr<AstArgument>($1), $3); 
       $$->setSrcLoc(@$);
     }
   |  MINUS arg %prec NEG { 
       $$ = new AstUnaryFunctor(AstUnaryFunctor::NEGATION, std::unique_ptr<AstArgument>($2));
       $$->setSrcLoc(@$); 
     }
   |  BW_NOT arg { 
       $$ = new AstUnaryFunctor(AstUnaryFunctor::BNOT, std::unique_ptr<AstArgument>($2));
       $$->setSrcLoc(@$); 
     }
   |  L_NOT arg { 
       $$ = new AstUnaryFunctor(AstUnaryFunctor::LNOT, std::unique_ptr<AstArgument>($2));
       $$->setSrcLoc(@$); 
     }
   | LBRACKET RBRACKET  {
       $$ = new AstRecordInit(); 
       $$->setSrcLoc(@$);
     } 
   | LBRACKET recordlist RBRACKET {
       $$ = $2; 
       $$->setSrcLoc(@$);
     }
   | NIL {
       $$ = new AstNullConstant();
       $$->setSrcLoc(@$);
     }
   | COUNT COLON atom {
       auto res = new AstAggregator(AstAggregator::count);
       res->addBodyLiteral(std::unique_ptr<AstLiteral>($3));
       $$ = res;
       $$->setSrcLoc(@$);
     }
   | COUNT COLON LBRACE body RBRACE {
       auto res = new AstAggregator(AstAggregator::count);
       auto bodies = $4->toClauseBodies();
       if (bodies.size() != 1) {
    	   std::cerr << "ERROR: currently not supporting non-conjunctive aggreation clauses!";
    	   exit(1);
       }
       for(const auto& cur : bodies[0]->getBodyLiterals()) 
           res->addBodyLiteral(std::unique_ptr<AstLiteral>(cur->clone()));
       delete bodies[0];
       delete $4;
       $$ = res;
       $$->setSrcLoc(@$);
     }
   | MIN arg COLON atom {
       auto res = new AstAggregator(AstAggregator::min);
       res->setTargetExpression(std::unique_ptr<AstArgument>($2));
       res->addBodyLiteral(std::unique_ptr<AstLiteral>($4));
       $$ = res;
       $$->setSrcLoc(@$);
     }
   | MIN arg COLON LBRACE body RBRACE {
       auto res = new AstAggregator(AstAggregator::min);
       res->setTargetExpression(std::unique_ptr<AstArgument>($2));
       auto bodies = $5->toClauseBodies();
	   if (bodies.size() != 1) {
		   std::cerr << "ERROR: currently not supporting non-conjunctive aggreation clauses!";
		   exit(1);
	   }
       for(const auto& cur : bodies[0]->getBodyLiterals()) 
    	   res->addBodyLiteral(std::unique_ptr<AstLiteral>(cur->clone()));
       delete bodies[0];
       delete $5;
       $$ = res;
       $$->setSrcLoc(@$);
     }
   | MAX arg COLON atom {
       auto res = new AstAggregator(AstAggregator::max);
       res->setTargetExpression(std::unique_ptr<AstArgument>($2));
       res->addBodyLiteral(std::unique_ptr<AstLiteral>($4));
       $$ = res;
       $$->setSrcLoc(@$);
     }
   | MAX arg COLON LBRACE body RBRACE {
       auto res = new AstAggregator(AstAggregator::max);
       res->setTargetExpression(std::unique_ptr<AstArgument>($2));
       auto bodies = $5->toClauseBodies();
	   if (bodies.size() != 1) {
		   std::cerr << "ERROR: currently not supporting non-conjunctive aggreation clauses!";
		   exit(1);
	   }
       for(const auto& cur : bodies[0]->getBodyLiterals()) 
          res->addBodyLiteral(std::unique_ptr<AstLiteral>(cur->clone()));
       delete bodies[0];
       delete $5;
       $$ = res;
       $$->setSrcLoc(@$);
     }
   ;

recordlist: arg {
             $$ = new AstRecordInit(); 
             $$->add(std::unique_ptr<AstArgument>($1));
            }
          | recordlist COMMA arg {
             $$ = $1; 
             $$->add(std::unique_ptr<AstArgument>($3)); 
            }
          ; 

arg_list: arg {
            $$ = new AstAtom();
            $$->addArgument(std::unique_ptr<AstArgument>($1));
          }
        | arg_list COMMA arg {
            $$ = $1;
            $$->addArgument(std::unique_ptr<AstArgument>($3));
          }
        ;

atom: rel_id LPAREN arg_list RPAREN {
          $$ = $3; $3->setName(*$1);
          delete $1;
          $$->setSrcLoc(@$);
        }
    ;

/* Literal */
literal: arg RELOP arg {
            auto* res = new AstConstraint($2, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
            res->setSrcLoc(@$);
            $$ = new RuleBody(std::move(RuleBody::constraint(res)));
          }
       | arg LT arg {
           auto* res = new AstConstraint(BinaryRelOp::LT, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
           res->setSrcLoc(@$);
           $$ = new RuleBody(std::move(RuleBody::constraint(res)));
         }
       | arg GT arg {
    	   auto* res = new AstConstraint(BinaryRelOp::GT, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
           res->setSrcLoc(@$);
           $$ = new RuleBody(std::move(RuleBody::constraint(res)));
         }
       | arg EQUALS arg {
    	   auto* res = new AstConstraint(BinaryRelOp::EQ, std::unique_ptr<AstArgument>($1), std::unique_ptr<AstArgument>($3));
           res->setSrcLoc(@$);
           $$ = new RuleBody(std::move(RuleBody::constraint(res)));
         }
       | atom {
            $1->setSrcLoc(@$);
            $$ = new RuleBody(std::move(RuleBody::atom($1)));
          }
       | EXCLAMATION atom {
		    $$ = new RuleBody(std::move(RuleBody::atom($2)));
		    $$->negate();
          }
       | EXCLAMATION LPAREN body RPAREN {
			$$ = $3;
			$$->negate();
		  }
       | TMATCH LPAREN arg COMMA arg RPAREN {
            auto* res = new AstConstraint(BinaryRelOp::MATCH, std::unique_ptr<AstArgument>($3), std::unique_ptr<AstArgument>($5));
            res->setSrcLoc(@$);
		    $$ = new RuleBody(std::move(RuleBody::constraint(res)));
          }
       | TCONTAINS LPAREN arg COMMA arg RPAREN {
            auto* res = new AstConstraint(BinaryRelOp::CONTAINS, std::unique_ptr<AstArgument>($3), std::unique_ptr<AstArgument>($5));
            res->setSrcLoc(@$);
            $$ = new RuleBody(std::move(RuleBody::constraint(res)));
          }
       ;

/* Fact */
fact: atom DOT {
          $$ = new AstClause();
          $$->setHead(std::unique_ptr<AstAtom>($1));
          $$->setSrcLoc(@$);
      }
    ;

/* Head */
head : atom					{ $$.push_back($1); }
     | head COMMA atom		{ $$.swap($1); $$.push_back($3); }
	 ;

/* Body */
conjunction: literal 		{ $$ = $1; }
    | conjunction COMMA literal {
							  $$ = $1;
							  $$->conjunct(std::move(*$3));
      }
    ;

/* Body */
body : conjunction						{ $$ = $1; }
     | body SEMICOLON conjunction		{ $$ = $1; $$->disjunct(std::move(*$3)); }
    
    
/* execution order list */
exec_order_list: NUMBER {
          $$ = new AstExecutionOrder();
          $$->appendAtomIndex($1);
      }
    | exec_order_list COMMA NUMBER {
          $$ = $1;
          $$->appendAtomIndex($3);
      }
    ;
    
/* execution order */
exec_order: LPAREN exec_order_list RPAREN {
           $$ = $2;
           $$->setSrcLoc(@$);
      }
    ;
    
/* execution plan list */
exec_plan_list : NUMBER COLON exec_order {
           $$ = new AstExecutionPlan();
           $$->setOrderFor($1, std::unique_ptr<AstExecutionOrder>($3));
      }
    | exec_plan_list COMMA NUMBER COLON exec_order {
           $$ = $1;
           $$->setOrderFor($3, std::unique_ptr<AstExecutionOrder>($5));
    }
    ;
    
/* execution plan */
exec_plan: PLAN exec_plan_list {
          $$ = $2;
          $$->setSrcLoc(@$);
      }
    ;

/* Rule Definition */
rule_def: head IF body DOT  {
		  auto bodies = $3->toClauseBodies();
          for(const auto& head : $1) {
        	  for(AstClause* body : bodies) {
				  AstClause* cur = body->clone();
				  cur->setHead(std::unique_ptr<AstAtom>(head->clone()));
				  cur->setSrcLoc(@$);
				  cur->setGenerated($1.size() != 1 || bodies.size() != 1);
				  $$.push_back(cur);
        	  }
          }
          for(auto& head : $1) delete head;
          for(AstClause* body : bodies) delete body;
          delete $3;
      }
;

/* Rule */
rule: rule_def {
         $$ = $1;
      }
    |
      rule STRICT {
         $$ = $1;
         for(const auto& cur : $$) cur->setFixedExecutionPlan();
      }
    | rule exec_plan {
         $$ = $1;
         for(const auto& cur : $$) cur->setExecutionPlan(std::unique_ptr<AstExecutionPlan>($2));
      }
    ;
    
/* Type Parameters */

type_param_list: 
      IDENT {
          $$.push_back($1);
      }
    | type_param_list COMMA IDENT {
          $$ = $1;
          $$.push_back($3);
      }
    ;
    
type_params: {
      }
    | LT type_param_list GT {
        $$ = $2;
      }
    ;

/* Component type */
    
comp_type: IDENT type_params {
        $$ = new AstComponentType($1,$2);
      }
    ;

/* Component */
    
component_head: COMPONENT comp_type {
        $$ = new AstComponent();
        $$->setComponentType(*$2);
        delete $2;
      }
    | component_head COLON comp_type {
        $$ = $1;
        $$->addBaseComponent(*$3);
        delete $3;
      }
    | component_head COMMA comp_type {
        $$ = $1;
        $$->addBaseComponent(*$3);
        delete $3;
      }
    
component_body:
      component_body relation      { $$ = $1; $$->addRelation(std::unique_ptr<AstRelation>($2)); }
    | component_body fact          { $$ = $1; $$->addClause(std::unique_ptr<AstClause>($2)); }
    | component_body rule          { $$ = $1; for(const auto& cur : $2) $$->addClause(std::unique_ptr<AstClause>(cur)); }
    | component_body comp_override { $$ = $1; $$->addOverride($2); }
    | component_body component     { $$ = $1; $$->addComponent(std::unique_ptr<AstComponent>($2)); }
    | component_body comp_init     { $$ = $1; $$->addInstantiation(std::unique_ptr<AstComponentInit>($2)); }
    | {
        $$ = new AstComponent();
    }
    
component: component_head LBRACE component_body RBRACE {
        $$ = $3;
        $$->setComponentType($1->getComponentType());
        $$->setBaseComponents($1->getBaseComponents());
        delete $1;
        $$->setSrcLoc(@$);
      }
  
/* Component Instantition */

comp_init: INSTANTIATE IDENT EQUALS comp_type {
        $$ = new AstComponentInit();
        $$->setInstanceName($2);
        $$->setComponentType(*$4);
        $$->setSrcLoc(@$);
        delete $4;
    }

/* Override rules of a relation */

comp_override: OVERRIDE IDENT {
         $$ = $2; 
}

%%
void yy::parser::error(const location_type &l, const std::string &m) {
   driver.error(l, m);
}
