/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
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
%define api.parser.class {parser}
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define api.location.type {SrcLocation}

// Defined in version 3.2. This would solve a lot of the verbose `std::move`s.
// NOTE:  Turns out becaue of another unspeakable hack (see below),
//        we dont need as many of these `std::move`s anymore.
// %define api.value.automove

%locations

%define parse.trace
%define parse.error verbose

/* -- Dependencies -- */
%code requires {
    #include "AggregateOp.h"
    #include "FunctorOps.h"
    #include "ast/Aggregator.h"
    #include "ast/Argument.h"
    #include "ast/Atom.h"
    #include "ast/Attribute.h"
    #include "ast/BinaryConstraint.h"
    #include "ast/BooleanConstraint.h"
    #include "ast/BranchInit.h"
    #include "ast/Clause.h"
    #include "ast/Component.h"
    #include "ast/ComponentInit.h"
    #include "ast/ComponentType.h"
    #include "ast/Constraint.h"
    #include "ast/Counter.h"
    #include "ast/ExecutionOrder.h"
    #include "ast/ExecutionPlan.h"
    #include "ast/FunctionalConstraint.h"
    #include "ast/FunctorDeclaration.h"
    #include "ast/Directive.h"
    #include "ast/IntrinsicFunctor.h"
    #include "ast/Literal.h"
    #include "ast/NilConstant.h"
    #include "ast/NumericConstant.h"
    #include "ast/Pragma.h"
    #include "ast/QualifiedName.h"
    #include "ast/RecordInit.h"
    #include "ast/RecordType.h"
    #include "ast/Relation.h"
    #include "ast/StringConstant.h"
    #include "ast/SubsetType.h"
    #include "ast/AlgebraicDataType.h"
    #include "ast/BranchType.h"
    #include "ast/Type.h"
    #include "ast/TypeCast.h"
    #include "ast/UnionType.h"
    #include "ast/UnnamedVariable.h"
    #include "ast/UserDefinedFunctor.h"
    #include "ast/Variable.h"
    #include "parser/Helper.h"
    #include "parser/ParserUtils.h"
    #include "souffle/RamTypes.h"
    #include "souffle/BinaryConstraintOps.h"
    #include "souffle/utility/ContainerUtil.h"
    #include "souffle/utility/StringUtil.h"

    #include <ostream>
    #include <string>
    #include <vector>

    using namespace souffle;

    using namespace souffle::parser;

    using yyscan_t = void*;

    #define YY_NULLPTR nullptr

    /* Macro to update locations as parsing proceeds */
    #define YYLLOC_DEFAULT(Cur, Rhs, N)                         \
    do {                                                        \
        if (N) {                                                \
            (Cur).start         = YYRHSLOC(Rhs, 1).start;       \
            (Cur).end           = YYRHSLOC(Rhs, N).end;         \
            (Cur).filenames     = YYRHSLOC(Rhs, N).filenames;   \
        } else {                                                \
            (Cur).start         = YYRHSLOC(Rhs, 0).end;         \
            (Cur).end           = YYRHSLOC(Rhs, 0).end;         \
            (Cur).filenames     = YYRHSLOC(Rhs, 0).filenames;   \
        }                                                       \
    } while (0)
}

%code {
   #include "parser/ParserDriver.h"
   #define YY_DECL yy::parser::symbol_type yylex(souffle::ParserDriver& driver, yyscan_t yyscanner)
   YY_DECL;
}

%param { ParserDriver &driver }
%param { yyscan_t yyscanner }

/* -- Tokens -- */
%token END 0                     "end of file"
%token <std::string> STRING      "symbol"
%token <std::string> IDENT       "identifier"
%token <std::string> NUMBER      "number"
%token <std::string> UNSIGNED    "unsigned number"
%token <std::string> FLOAT       "float"
%token AUTOINC                   "auto-increment functor"
%token PRAGMA                    "pragma directive"
%token OUTPUT_QUALIFIER          "relation qualifier output"
%token INPUT_QUALIFIER           "relation qualifier input"
%token PRINTSIZE_QUALIFIER       "relation qualifier printsize"
%token BRIE_QUALIFIER            "BRIE datastructure qualifier"
%token BTREE_QUALIFIER           "BTREE datastructure qualifier"
%token EQREL_QUALIFIER           "equivalence relation qualifier"
%token OVERRIDABLE_QUALIFIER     "relation qualifier overidable"
%token INLINE_QUALIFIER          "relation qualifier inline"
%token NO_INLINE_QUALIFIER       "relation qualifier no_inline"
%token MAGIC_QUALIFIER           "relation qualifier magic"
%token NO_MAGIC_QUALIFIER        "relation qualifier no_magic"
%token TMATCH                    "match predicate"
%token TCONTAINS                 "checks whether substring is contained in a string"
%token STATEFUL                  "stateful functor"
%token CAT                       "concatenation of strings"
%token ORD                       "ordinal number of a string"
%token RANGE                     "range"
%token STRLEN                    "length of a string"
%token SUBSTR                    "sub-string of a string"
%token MEAN                      "mean aggregator"
%token MIN                       "min aggregator"
%token MAX                       "max aggregator"
%token COUNT                     "count aggregator"
%token SUM                       "sum aggregator"
%token TRUE                      "true literal constraint"
%token FALSE                     "false literal constraint"
%token PLAN                      "plan keyword"
%token CHOICEDOMAIN              "choice-domain"
%token IF                        ":-"
%token DECL                      "relation declaration"
%token FUNCTOR                   "functor declaration"
%token INPUT_DECL                "input directives declaration"
%token OUTPUT_DECL               "output directives declaration"
%token PRINTSIZE_DECL            "printsize directives declaration"
%token LIMITSIZE_DECL            "limitsize directives declaration"
%token OVERRIDE                  "override rules of super-component"
%token TYPE                      "type declaration"
%token COMPONENT                 "component declaration"
%token INSTANTIATE               "component instantiation"
%token NUMBER_TYPE               "numeric type declaration"
%token SYMBOL_TYPE               "symbolic type declaration"
%token TOFLOAT                   "convert to float"
%token TONUMBER                  "convert to signed integer"
%token TOSTRING                  "convert to string"
%token TOUNSIGNED                "convert to unsigned integer"
%token ITOU                      "convert int to unsigned"
%token ITOF                      "convert int to float"
%token UTOI                      "convert unsigned to int"
%token UTOF                      "convert unsigned to float"
%token FTOI                      "convert float to int"
%token FTOU                      "convert float to unsigned"
%token AS                        "type cast"
%token AT                        "@"
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
%token DOUBLECOLON               "::"
%token SEMICOLON                 ";"
%token DOT                       "."
%token EQUALS                    "="
%token STAR                      "*"
%token SLASH                     "/"
%token CARET                     "^"
%token PERCENT                   "%"
%token LBRACE                    "{"
%token RBRACE                    "}"
%token SUBTYPE                   "<:"
%token LT                        "<"
%token GT                        ">"
%token LE                        "<="
%token GE                        ">="
%token NE                        "!="
%token BW_AND                    "band"
%token BW_OR                     "bor"
%token BW_XOR                    "bxor"
%token BW_SHIFT_L                "bshl"
%token BW_SHIFT_R                "bshr"
%token BW_SHIFT_R_UNSIGNED       "bshru"
%token BW_NOT                    "bnot"
%token L_AND                     "land"
%token L_OR                      "lor"
%token L_XOR                     "lxor"
%token L_NOT                     "lnot"

/* -- Non-Terminal Types -- */
%type <Mov<RuleBody>>                          aggregate_body
%type <AggregateOp>                            aggregate_func
%type <Mov<Own<ast::Argument>>>                arg
%type <Mov<VecOwn<ast::Argument>>>             arg_list
%type <Mov<Own<ast::Atom>>>                    atom
%type <Mov<VecOwn<ast::Attribute>>>            attributes_list
%type <Mov<RuleBody>>                          body
%type <Mov<Own<ast::ComponentType>>>           component_type
%type <Mov<Own<ast::ComponentInit>>>           component_init
%type <Mov<Own<ast::Component>>>               component_decl
%type <Mov<Own<ast::Component>>>               component_body
%type <Mov<Own<ast::Component>>>               component_head
%type <Mov<RuleBody>>                          conjunction
%type <Mov<Own<ast::Constraint>>>              constraint
%type <Mov<Own<ast::FunctionalConstraint>>>    dependency
%type <Mov<VecOwn<ast::FunctionalConstraint>>> dependency_list
%type <Mov<VecOwn<ast::FunctionalConstraint>>> dependency_list_aux
%type <Mov<RuleBody>>                          disjunction
%type <Mov<Own<ast::ExecutionOrder>>>          plan_order
%type <Mov<Own<ast::ExecutionPlan>>>           query_plan
%type <Mov<Own<ast::ExecutionPlan>>>           query_plan_list
%type <Mov<Own<ast::Clause>>>                  fact
%type <Mov<VecOwn<ast::Attribute>>>            functor_arg_type_list
%type <Mov<std::string>>                       functor_built_in
%type <Mov<Own<ast::FunctorDeclaration>>>      functor_decl
%type <Mov<VecOwn<ast::Atom>>>                 head
%type <Mov<ast::QualifiedName>>                qualified_name
%type <Mov<VecOwn<ast::Directive>>>            directive_list
%type <Mov<VecOwn<ast::Directive>>>            directive_head
%type <ast::DirectiveType>                     directive_head_decl
%type <Mov<VecOwn<ast::Directive>>>            relation_directive_list
%type <Mov<std::string>>                       kvp_value
%type <Mov<VecOwn<ast::Argument>>>             non_empty_arg_list
%type <Mov<Own<ast::Attribute>>>               attribute
%type <Mov<VecOwn<ast::Attribute>>>            non_empty_attributes
%type <Mov<std::vector<std::string>>>          non_empty_attribute_names
%type <Mov<ast::ExecutionOrder::ExecOrder>>    non_empty_plan_order_list
%type <Mov<VecOwn<ast::Attribute>>>            non_empty_functor_arg_type_list
%type <Mov<Own<ast::Attribute>>>               functor_attribute;
%type <Mov<std::vector<std::pair
            <std::string, std::string>>>>      non_empty_key_value_pairs
%type <Mov<VecOwn<ast::Relation>>>             relation_names
%type <Mov<Own<ast::Pragma>>>                  pragma
%type <Mov<VecOwn<ast::Attribute>>>            record_type_list
%type <Mov<VecOwn<ast::Relation>>>             relation_decl
%type <std::set<RelationTag>>                  relation_tags
%type <Mov<VecOwn<ast::Clause>>>               rule
%type <Mov<VecOwn<ast::Clause>>>               rule_def
%type <Mov<RuleBody>>                          term
%type <Mov<Own<ast::Type>>>                    type
%type <Mov<std::vector<ast::QualifiedName>>>   component_type_params
%type <Mov<std::vector<ast::QualifiedName>>>   component_param_list
%type <Mov<std::vector<ast::QualifiedName>>>   union_type_list
%type <Mov<VecOwn<ast::BranchType>>>    adt_branch_list
%type <Mov<Own<ast::BranchType>>>       adt_branch

/* -- Operator precedence -- */
%left L_OR
%left L_XOR
%left L_AND
%left BW_OR
%left BW_XOR
%left BW_AND
%left BW_SHIFT_L BW_SHIFT_R BW_SHIFT_R_UNSIGNED
%left PLUS MINUS
%left STAR SLASH PERCENT
%precedence NEG BW_NOT L_NOT
%right CARET

/* -- Grammar -- */
%%

%start program;

/**
 * Program
 */
program
  : unit
  ;

/**
 * Top-level Program Elements 
 */
unit
  : %empty
    { }
  | unit directive_head
    {
      for (auto&& cur : $directive_head)
        driver.addDirective(std::move(cur));
    }
  | unit rule
    {
      for (auto&& cur : $rule   )
        driver.addClause(std::move(cur));
    }
  | unit fact
    {
      driver.addClause($fact);
    }
  | unit component_decl
    {
      driver.addComponent($component_decl);
    }
  | unit component_init
    {
      driver.addInstantiation($component_init);
    }
  | unit pragma
    {
      driver.addPragma($pragma);
    }
  | unit type
    {
      driver.addType($type);
    }
  | unit functor_decl
    {
      driver.addFunctorDeclaration($functor_decl);
    }
  | unit relation_decl
    {
      for (auto&& rel : $relation_decl) {
        driver.addIoFromDeprecatedTag(*rel);
        driver.addRelation(std::move(rel));
      }
    }
  ;

/**
 * A Qualified Name
 */

qualified_name
  : IDENT
    {
      $$ = $IDENT;
    }
  | qualified_name DOT  IDENT
    {
      $$ = $1; $$->append($IDENT);
    }
  ;

/**
 * Types
 */

/**
 * Type Declarations
 */
type
  : TYPE IDENT SUBTYPE qualified_name
    {
      $$ = mk<ast::SubsetType>($2, $4, @$);
    }
  | TYPE IDENT EQUALS union_type_list
    {
      $$ = mk<ast::UnionType>($2, $4, @$);
    }
  | TYPE IDENT EQUALS record_type_list
    {
      $$ = mk<ast::RecordType>($2, $4, @$);
    }
  | TYPE IDENT EQUALS adt_branch_list
    {
      $$ = mk<ast::AlgebraicDataType>($2, $4, @$);
    }
    /* Deprecated Types */
  | NUMBER_TYPE IDENT
    {
      $$ = driver.mkDeprecatedSubType($IDENT, "number", @$);
    }
  | SYMBOL_TYPE IDENT
    {
      $$ = driver.mkDeprecatedSubType($IDENT, "symbol", @$);
    }
  | TYPE IDENT
    {
      $$ = driver.mkDeprecatedSubType($IDENT, "symbol", @$);
    }
  ;

/* Attribute definition of a relation */
/* specific wrapper to ensure the err msg says "expected ',' or ')'" */
record_type_list
  : LBRACKET RBRACKET
    { }
  | LBRACKET non_empty_attributes RBRACKET
    {
      $$ = $2;
    }
  ;

/* Union type argument declarations */
union_type_list
  : qualified_name
    {
      $$.push_back($qualified_name);
    }
  | union_type_list PIPE qualified_name
    {
      $$ = $1;
      $$.push_back($qualified_name);
    }
  ;

adt_branch_list
  : adt_branch
    {
      $$.push_back($adt_branch);
    }
  | adt_branch_list PIPE adt_branch
    {
      $$ = $1;
      $$.push_back($adt_branch);
    }
  ;

adt_branch
  : IDENT[name] LBRACE RBRACE
    {
      $$ = mk<ast::BranchType>($name, VecOwn<ast::Attribute>{}, @$);
    }
  | IDENT[name] LBRACE non_empty_attributes[attributes] RBRACE
    {
      $$ = mk<ast::BranchType>($name, $attributes, @$);
    }
  ;

/**
 * Relations
 */

/**
 * Relation Declaration
 */
relation_decl
  : DECL relation_names attributes_list relation_tags dependency_list
    {
      auto tags = $relation_tags;
      auto attributes_list = $attributes_list;
      $$ = $relation_names;
      for (auto&& rel : $$) {
        for (auto tag : tags) {
          if (isRelationQualifierTag(tag)) {
            rel->addQualifier(getRelationQualifierFromTag(tag));
          } else if (isRelationRepresentationTag(tag)) {
            rel->setRepresentation(getRelationRepresentationFromTag(tag));
          } else {
            assert(false && "unhandled tag");
          }
        }
        for (auto&& fd : $dependency_list) {
          rel->addDependency(souffle::clone(fd));
        }
        rel->setAttributes(clone(attributes_list));
      }
    }
  ;

/**
 * Relation Names
 */
relation_names
  : IDENT
    {
      $$.push_back(mk<ast::Relation>($1, @1));
    }
  | relation_names COMMA IDENT
    {
      $$ = $1;
      $$.push_back(mk<ast::Relation>($3, @3));
    }
  ;

/**
 * Attributes
 */
attributes_list
  : LPAREN RPAREN
    { }
  | LPAREN non_empty_attributes RPAREN
    {
      $$ = $2;
    }
  ;

non_empty_attributes
  : attribute
    {
      $$.push_back($attribute);
    }
  | non_empty_attributes COMMA attribute
    {
      $$ = $1;
      $$.push_back($attribute);
    }
  ;

attribute
  : IDENT[name] COLON qualified_name[type]
    {
      $$ = mk<ast::Attribute>($name, $type, @type);
    }
  ;

/**
 * Relation Tags
 */
relation_tags
  : %empty
    { }
  | relation_tags OVERRIDABLE_QUALIFIER
    {
      $$ = driver.addTag(RelationTag::OVERRIDABLE, @2, $1);
    }
  | relation_tags INLINE_QUALIFIER
    {
      $$ = driver.addTag(RelationTag::INLINE, @2, $1);
    }
  | relation_tags NO_INLINE_QUALIFIER
    {
      $$ = driver.addTag(RelationTag::NO_INLINE, @2, $1);
    }
  | relation_tags MAGIC_QUALIFIER
    {
      $$ = driver.addTag(RelationTag::MAGIC, @2, $1);
    }
  | relation_tags NO_MAGIC_QUALIFIER
    {
      $$ = driver.addTag(RelationTag::NO_MAGIC, @2, $1);
    }
  | relation_tags BRIE_QUALIFIER
    {
      $$ = driver.addReprTag(RelationTag::BRIE, @2, $1);
    }
  | relation_tags BTREE_QUALIFIER
    {
      $$ = driver.addReprTag(RelationTag::BTREE, @2, $1);
    }
  | relation_tags EQREL_QUALIFIER
    {
      $$ = driver.addReprTag(RelationTag::EQREL, @2, $1);
    }
  /* Deprecated Qualifiers */
  | relation_tags OUTPUT_QUALIFIER
    {
      $$ = driver.addDeprecatedTag(RelationTag::OUTPUT, @2, $1);
    }
  | relation_tags INPUT_QUALIFIER
    {
      $$ = driver.addDeprecatedTag(RelationTag::INPUT, @2, $1);
    }
  | relation_tags PRINTSIZE_QUALIFIER
    {
      $$ = driver.addDeprecatedTag(RelationTag::PRINTSIZE, @2, $1);
    }
  ;

/**
 * Attribute Name List
 */
non_empty_attribute_names
  : IDENT
    {
      $$.push_back($IDENT);
    }

  | non_empty_attribute_names[curr_var_list] COMMA IDENT
    {
      $$ = $curr_var_list;
      $$.push_back($IDENT);
    }
  ;

/**
 * Functional Dependency Constraint
 */
dependency
  : IDENT[key]
    {
        $$ = mk<ast::FunctionalConstraint>(mk<ast::Variable>($key, @$), @$);
    }
  | LPAREN non_empty_attribute_names RPAREN
    {
      VecOwn<ast::Variable> keys;
      for (std::string s : $non_empty_attribute_names) {
        keys.push_back(mk<ast::Variable>(s, @$));
      }
      $$ = mk<ast::FunctionalConstraint>(std::move(keys), @$);
    }
  ;

dependency_list_aux
  : dependency
    {
      $$.push_back($dependency);
    }
  | dependency_list_aux[list] COMMA dependency[next]
    {
      $$ = std::move($list);
      $$.push_back(std::move($next));
    }
  ;

dependency_list
  : %empty
    { }
  | CHOICEDOMAIN dependency_list_aux[list]
    {
      $$ = std::move($list);
    }
  ;

/**
 * Datalog Rule Structure
 */

/**
 * Fact
 */
fact
  : atom DOT
    {
      $$ = mk<ast::Clause>($atom, Mov<VecOwn<ast::Literal>> {}, nullptr, @$);
    }
  ;

/**
 * Rule
 */
rule
  : rule_def
    {
      $$ = $rule_def;
    }
  | rule_def query_plan
    {
      $$ = $rule_def;
      auto query_plan = $query_plan;
      for (auto&& rule : $$) {
        rule->setExecutionPlan(clone(query_plan));
      }
    }
  ;

/**
 * Rule Definition
 */
rule_def
  : head[heads] IF body DOT
    {
      auto bodies = $body->toClauseBodies();
      for (auto&& head : $heads) {
        for (auto&& body : bodies) {
          auto cur = clone(body);
          cur->setHead(clone(head));
          cur->setSrcLoc(@$);
          $$.push_back(std::move(cur));
        }
      }
    }
  ;

/**
 * Rule Head
 */
head
  : atom
    {
      $$.push_back($atom);
    }
  | head COMMA atom
    {
      $$ = $1; $$.push_back($atom);
    }
  ;

/**
 * Rule Body
 */
body
  : disjunction
    {
      $$ = $disjunction;
    }
  ;

disjunction
  : conjunction
    {
      $$ = $conjunction;
    }
  | disjunction SEMICOLON conjunction
    {
      $$ = $1;
      $$->disjunct($conjunction);
    }
  ;

conjunction
  : term
    {
      $$ = $term;
    }
  | conjunction COMMA term
    {
      $$ = $1;
      $$->conjunct($term);
    }
  ;

/**
 * Terms in Rule Bodies
 */
term
  : atom
    {
      $$ = RuleBody::atom($atom);
    }
  | constraint
    {
      $$ = RuleBody::constraint($constraint);
    }
  | LPAREN disjunction RPAREN
    {
      $$ = $disjunction;
    }
  | EXCLAMATION term
    {
      $$ = $2->negated();
    }
  ;

/**
 * Rule body atom
 */
atom
  : qualified_name LPAREN arg_list RPAREN
    {
      $$ = mk<ast::Atom>($qualified_name, $arg_list, @$);
    }
  ;

/**
 * Literal Constraints
 */
constraint
    /* binary infix constraints */
  : arg LT arg
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::LT, $1, $3, @$);
    }
  | arg GT arg
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::GT, $1, $3, @$);
    }
  | arg LE arg
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::LE, $1, $3, @$);
    }
  | arg GE arg
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::GE, $1, $3, @$);
    }
  | arg EQUALS arg
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::EQ, $1, $3, @$);
    }
  | arg NE arg
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::NE, $1, $3, @$);
    }

    /* binary prefix constraints */
  | TMATCH LPAREN arg[a0] COMMA arg[a1] RPAREN
    {
      $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::MATCH, $a0, $a1, @$);
    }
  | TCONTAINS LPAREN arg[a0] COMMA arg[a1] RPAREN
    {
       $$ = mk<ast::BinaryConstraint>(BinaryConstraintOp::CONTAINS, $a0, $a1, @$);
    }

    /* zero-arity constraints */
  | TRUE
    {
      $$ = mk<ast::BooleanConstraint>(true , @$);
    }
  | FALSE
    {
      $$ = mk<ast::BooleanConstraint>(false, @$);
    }
  ;

/**
 * Argument List
 */
arg_list
  : %empty
    { }
  | non_empty_arg_list
    {
      $$ = $1;
    } ;

non_empty_arg_list
  : arg
    {
      $$.push_back($arg);
    }
  | non_empty_arg_list COMMA arg
    {
      $$ = $1; $$.push_back($arg);
    }
  ;

/**
 * Atom argument
 */
arg
  : STRING
    {
      $$ = mk<ast::StringConstant>($STRING, @$);
    }
  | FLOAT
    {
      $$ = mk<ast::NumericConstant>($FLOAT, ast::NumericConstant::Type::Float, @$);
    }
  | UNSIGNED
    {
      auto&& n = $UNSIGNED; // drop the last character (`u`)
      $$ = mk<ast::NumericConstant>(n.substr(0, n.size() - 1), ast::NumericConstant::Type::Uint, @$);
    }
  | NUMBER
    {
      $$ = mk<ast::NumericConstant>($NUMBER, @$);
    }
  | UNDERSCORE
    {
      $$ = mk<ast::UnnamedVariable>(@$);
    }
  | DOLLAR
    {
      $$ = driver.addDeprecatedCounter(@$);
    }
  | AUTOINC LPAREN RPAREN
    {
      $$ = mk<ast::Counter>(@$);
    }
  | IDENT
    {
      $$ = mk<ast::Variable>($IDENT, @$);
    }
  | NIL
    {
      $$ = mk<ast::NilConstant>(@$);
    }
  | LBRACKET arg_list RBRACKET
    {
      $$ = mk<ast::RecordInit>($arg_list, @$);
    }
  | DOLLAR IDENT[branch] LPAREN arg_list RPAREN
    {
      $$ = mk<ast::BranchInit>($branch, $arg_list, @$);
    }
  | DOLLAR IDENT[branch]
    {
      $$ = mk<ast::BranchInit>($branch, VecOwn<ast::Argument>{}, @$);
    }
  | LPAREN arg RPAREN
    {
      $$ = $2;
    }
  | AS LPAREN arg COMMA qualified_name RPAREN
    {
      $$ = mk<ast::TypeCast>($3, $qualified_name, @$);
    }
  | AT IDENT LPAREN arg_list RPAREN
    {
      $$ = mk<ast::UserDefinedFunctor>($IDENT, *$arg_list, @$);
    }
  | functor_built_in LPAREN arg_list RPAREN
    {
      $$ = mk<ast::IntrinsicFunctor>($functor_built_in, *$arg_list, @$);
    }

    /* some aggregates have the same name as functors */
  | aggregate_func LPAREN arg[first] COMMA non_empty_arg_list[rest] RPAREN
    {
      VecOwn<ast::Argument> arg_list = $rest;
      arg_list.insert(arg_list.begin(), $first);
      auto agg_2_func = [](AggregateOp op) -> char const* {
        switch (op) {
          case AggregateOp::COUNT : return {};
          case AggregateOp::MAX   : return "max";
          case AggregateOp::MEAN  : return {};
          case AggregateOp::MIN   : return "min";
          case AggregateOp::SUM   : return {};
          default                 :
            fatal("missing base op handler, or got an overload op?");
        }
      };
      if (auto* func_op = agg_2_func($aggregate_func)) {
        $$ = mk<ast::IntrinsicFunctor>(func_op, std::move(arg_list), @$);
      } else {
        driver.error(@$, "aggregate operation has no functor equivalent");
        $$ = mk<ast::UnnamedVariable>(@$);
      }
    }

    /* -- intrinsic functor -- */
    /* unary functors */
  | MINUS arg[nested_arg] %prec NEG
    {
      // If we have a constant that is not already negated we just negate the constant value.
      auto nested_arg = *$nested_arg;
      const auto* asNumeric = as<ast::NumericConstant>(*nested_arg);
      if (asNumeric && !isPrefix("-", asNumeric->getConstant())) {
        $$ = mk<ast::NumericConstant>("-" + asNumeric->getConstant(), asNumeric->getFixedType(), @nested_arg);
      } else { // Otherwise, create a functor.
        $$ = mk<ast::IntrinsicFunctor>(@$, FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME, std::move(nested_arg));
      }
    }
  | BW_NOT  arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "~", $2);
    }
  | L_NOT arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "!", $2);
    }

    /* binary infix functors */
  | arg PLUS arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "+"  , $1, $3);
    }
  | arg MINUS arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "-"  , $1, $3);
    }
  | arg STAR arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "*"  , $1, $3);
    }
  | arg SLASH arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "/"  , $1, $3);
    }
  | arg PERCENT arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "%"  , $1, $3);
    }
  | arg CARET arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "**" , $1, $3);
    }
  | arg L_AND arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "&&" , $1, $3);
    }
  | arg L_OR arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "||" , $1, $3);
    }
  | arg L_XOR arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "^^" , $1, $3);
    }
  | arg BW_AND arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "&"  , $1, $3);
    }
  | arg BW_OR arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "|"  , $1, $3);
    }
  | arg BW_XOR arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "^"  , $1, $3);
    }
  | arg BW_SHIFT_L arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, "<<" , $1, $3);
    }
  | arg BW_SHIFT_R arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, ">>" , $1, $3);
    }
  | arg BW_SHIFT_R_UNSIGNED arg
    {
      $$ = mk<ast::IntrinsicFunctor>(@$, ">>>", $1, $3);
    }

    /* -- aggregators -- */
  | aggregate_func arg_list COLON aggregate_body
    {
      auto bodies = $aggregate_body->toClauseBodies();
      if (bodies.size() != 1) {
        driver.error("ERROR: disjunctions in aggregation clauses are currently not supported");
      }
      // TODO: move this to a semantic check when aggs are extended to multiple exprs
      auto given    = $arg_list.size();
      auto required = aggregateArity($aggregate_func);
      if (given < required.first || required.second < given) {
        driver.error("ERROR: incorrect expression arity for given aggregate mode");
      }
      auto expr = $arg_list.empty() ? nullptr : std::move($arg_list[0]);
      auto body = (bodies.size() == 1) ? clone(bodies[0]->getBodyLiterals()) : VecOwn<ast::Literal> {};
      $$ = mk<ast::Aggregator>($aggregate_func, std::move(expr), std::move(body), @$);
    }
  ;

functor_built_in
  : CAT
    {
      $$ = "cat";
    }
  | ORD
    {
      $$ = "ord";
    }
  | RANGE
    {
      $$ = "range";
    }
  | STRLEN
    {
      $$ = "strlen";
    }
  | SUBSTR
    {
      $$ = "substr";
    }
  | TOFLOAT
    {
      $$ = "to_float";
    }
  | TONUMBER
    {
      $$ = "to_number";
    }
  | TOSTRING
    {
      $$ = "to_string";
    }
  | TOUNSIGNED
    {
      $$ = "to_unsigned";
    }
  ;

aggregate_func
  : COUNT
    {
      $$ = AggregateOp::COUNT;
    }
  | MAX
    {
      $$ = AggregateOp::MAX;
    }
  | MEAN
    {
      $$ = AggregateOp::MEAN;
    }
  | MIN
    {
      $$ = AggregateOp::MIN;
    }
  | SUM
    {
      $$ = AggregateOp::SUM;
    }
  ;

aggregate_body
  : LBRACE body RBRACE
    {
      $$ = $body;
    }
  | atom
    {
      $$ = RuleBody::atom($atom);
    }
  ;

/**
 * Query Plan
 */
query_plan
  : PLAN query_plan_list
    {
      $$ = $query_plan_list;
    };

query_plan_list
  : NUMBER COLON plan_order
    {
      $$ = mk<ast::ExecutionPlan>();
      $$->setOrderFor(RamSignedFromString($NUMBER), Own<ast::ExecutionOrder>($plan_order));
    }
  | query_plan_list[curr_list] COMMA NUMBER COLON plan_order
    {
      $$ = $curr_list;
      $$->setOrderFor(RamSignedFromString($NUMBER), $plan_order);
    }
  ;

plan_order
  : LPAREN RPAREN
    {
      $$ = mk<ast::ExecutionOrder>(ast::ExecutionOrder::ExecOrder(), @$);
    }
  | LPAREN non_empty_plan_order_list RPAREN
    {
      $$ = mk<ast::ExecutionOrder>($2, @$);
    }
  ;

non_empty_plan_order_list
  : NUMBER
    {
      $$.push_back(RamUnsignedFromString($NUMBER));
    }
  | non_empty_plan_order_list COMMA NUMBER
    {
      $$ = $1; $$.push_back(RamUnsignedFromString($NUMBER));
    }
  ;

/**
 * Components
 */

/**
 * Component Declaration
 */
component_decl
  : component_head LBRACE component_body RBRACE
    {
      auto head = $component_head;
      $$ = $component_body;
      $$->setComponentType(clone(head->getComponentType()));
      $$->copyBaseComponents(**head);
      $$->setSrcLoc(@$);
    }
  ;

/**
 * Component Head
 */
component_head
  : COMPONENT component_type
    {
      $$ = mk<ast::Component>();
      $$->setComponentType($component_type);
    }
  | component_head COLON component_type
    {
      $$ = $1;
      $$->addBaseComponent($component_type);
    }
  | component_head COMMA component_type
    {
      $$ = $1;
      $$->addBaseComponent($component_type);
    }
  ;

/**
 * Component Type
 */
component_type
  : IDENT component_type_params
    {
      $$ = mk<ast::ComponentType>($IDENT, $component_type_params, @$);
    };

/**
 * Component Parameters
 */
component_type_params
  : %empty
    { }
  | LT component_param_list GT
    {
      $$ = $component_param_list;
    }
  ;

/**
 * Component Parameter List
 */
component_param_list
  : IDENT
    {
      $$.push_back($IDENT);
    }
  | component_param_list COMMA IDENT
    {
      $$ = $1;
      $$.push_back($IDENT);
    }
  ;

/**
 * Component body 
 */
component_body
  : %empty
    {
      $$ = mk<ast::Component>();
    }
  | component_body directive_head
    {
      $$ = $1;
      for (auto&& x : $2) {
        $$->addDirective(std::move(x));
      }
    }
  | component_body rule
    {
      $$ = $1;
      for (auto&& x : $2) {
        $$->addClause(std::move(x));
      }
    }
  | component_body fact
    {
      $$ = $1;
      $$->addClause($2);
    }
  | component_body OVERRIDE IDENT
    {
      $$ = $1;
      $$->addOverride($3);
    }
  | component_body component_init
    {
      $$ = $1;
      $$->addInstantiation($2);
    }
  | component_body component_decl
    {
      $$ = $1;
      $$->addComponent($2);
    }
  | component_body type
    {
      $$ = $1;
      $$->addType($2);
    }
  | component_body relation_decl
    {
      $$ = $1;
      for (auto&& rel : $relation_decl) {
        driver.addIoFromDeprecatedTag(*rel);
        $$->addRelation(std::move(rel));
      }
    }
  ;

/**
 * Component Initialisation
 */
component_init
  : INSTANTIATE IDENT EQUALS component_type
    {
      $$ = mk<ast::ComponentInit>($IDENT, $component_type, @$);
    }
  ;

/**
 * User-Defined Functors
 */

/**
 * Functor declaration
 */
functor_decl
  : FUNCTOR IDENT LPAREN functor_arg_type_list[args] RPAREN COLON qualified_name
    {
      $$ = mk<ast::FunctorDeclaration>($IDENT, $args, mk<ast::Attribute>("return_type", $qualified_name, @qualified_name), false, @$);
    }
  | FUNCTOR IDENT LPAREN functor_arg_type_list[args] RPAREN COLON qualified_name STATEFUL
    {
      $$ = mk<ast::FunctorDeclaration>($IDENT, $args, mk<ast::Attribute>("return_type", $qualified_name, @qualified_name), true, @$);
    }
  ;

/**
 * Functor argument list type
 */
functor_arg_type_list
  : %empty { }
  | non_empty_functor_arg_type_list
    {
      $$ = $1;
    }
  ;

non_empty_functor_arg_type_list
  : functor_attribute
    {
      $$.push_back($functor_attribute);
    }
  | non_empty_functor_arg_type_list COMMA functor_attribute
    {
      $$ = $1; $$.push_back($functor_attribute);
    }
  ;

functor_attribute
  : qualified_name[type]
    {
      $$ = mk<ast::Attribute>("", $type, @type);
    }
  | IDENT[name] COLON qualified_name[type]
    {
      $$ = mk<ast::Attribute>($name, $type, @type);
    }
  ;

/**
 * Other Directives
 */

/**
 * Pragma Directives
 */
pragma
  : PRAGMA STRING[key] STRING[value]
    {
      $$ = mk<ast::Pragma>($key, $value, @$);
    }
  | PRAGMA STRING[option]
    {
      $$ = mk<ast::Pragma>($option, "", @$);
    }
  ;

/**
 * Directives
 */
directive_head
  : directive_head_decl directive_list
    {
      auto directive_head_decl = $directive_head_decl;
      for (auto&& io : $directive_list) {
        io->setType(directive_head_decl);
        $$.push_back(std::move(io));
      }
    }
  ;

directive_head_decl
  : INPUT_DECL
    {
      $$ = ast::DirectiveType::input;
    }
  | OUTPUT_DECL
    {
      $$ = ast::DirectiveType::output;
    }
  | PRINTSIZE_DECL
    {
      $$ = ast::DirectiveType::printsize;
    }
  | LIMITSIZE_DECL
    {
      $$ = ast::DirectiveType::limitsize;
    }
  ;

/**
 * Directive List
 */
directive_list
  : relation_directive_list
    {
      $$ = $relation_directive_list;
    }
  | relation_directive_list LPAREN RPAREN
    {
      $$ = $relation_directive_list;
    }
  | relation_directive_list LPAREN non_empty_key_value_pairs RPAREN
    {
      $$ = $relation_directive_list;
      for (auto&& kvp : $non_empty_key_value_pairs) {
        for (auto&& io : $$) {
          io->addParameter(kvp.first, kvp.second);
        }
      }
    }
  ;

/**
 * Directive List
 */
relation_directive_list
  : qualified_name
    {
      $$.push_back(mk<ast::Directive>(ast::DirectiveType::input, $1, @1));
    }
  | relation_directive_list COMMA qualified_name
    {
      $$ = $1;
      $$.push_back(mk<ast::Directive>(ast::DirectiveType::input, $3, @3));
    }
  ;

/**
 * Key-value Pairs
 */
non_empty_key_value_pairs
  : IDENT EQUALS kvp_value
    {
      $$.push_back({$1, $3});
    }
  | non_empty_key_value_pairs COMMA IDENT EQUALS kvp_value
    {
      $$ = $1;
      $$.push_back({$3, $5});
    }
  ;

kvp_value
  : STRING
    {
      $$ = $STRING;
    }
  | IDENT
    {
      $$ = $IDENT;
    }
  | NUMBER
    {
      $$ = $NUMBER;
    }
  | TRUE
    {
      $$ = "true";
    }
  | FALSE
    {
      $$ = "false";
    }
  ;

%%

void yy::parser::error(const location_type &l, const std::string &m)
{
  driver.error(l, m);
}
