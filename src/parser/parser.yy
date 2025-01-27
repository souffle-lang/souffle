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
%require "3.6"

%defines
%define api.token.constructor
%define api.value.type variant
%define parse.assert
%define api.location.type {SrcLocation}

%locations

%define parse.trace
%define parse.error verbose
%define api.value.automove

/* -- Dependencies -- */
%code requires {
    #include "AggregateOp.h"
    #include "FunctorOps.h"
    #include "ast/Annotation.h"
    #include "ast/IntrinsicAggregator.h"
    #include "ast/UserDefinedAggregator.h"
    #include "ast/AliasType.h"
    #include "ast/AlgebraicDataType.h"
    #include "ast/Argument.h"
    #include "ast/Atom.h"
    #include "ast/Attribute.h"
    #include "ast/BinaryConstraint.h"
    #include "ast/BooleanConstraint.h"
    #include "ast/BranchType.h"
    #include "ast/BranchInit.h"
    #include "ast/Clause.h"
    #include "ast/Component.h"
    #include "ast/ComponentInit.h"
    #include "ast/ComponentType.h"
    #include "ast/Constraint.h"
    #include "ast/Counter.h"
    #include "ast/Directive.h"
    #include "ast/ExecutionOrder.h"
    #include "ast/ExecutionPlan.h"
    #include "ast/FunctionalConstraint.h"
    #include "ast/FunctorDeclaration.h"
    #include "ast/IntrinsicFunctor.h"
    #include "ast/IterationCounter.h"
    #include "ast/Lattice.h"
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
    #include "ast/SubsumptiveClause.h"
    #include "ast/TokenTree.h"
    #include "ast/Type.h"
    #include "ast/TypeCast.h"
    #include "ast/UnionType.h"
    #include "ast/UnnamedVariable.h"
    #include "ast/UserDefinedFunctor.h"
    #include "ast/Variable.h"
    #include "parser/ParserUtils.h"
    #include "souffle/RamTypes.h"
    #include "souffle/BinaryConstraintOps.h"
    #include "souffle/utility/ContainerUtil.h"
    #include "souffle/utility/StringUtil.h"

    #include <ostream>
    #include <string>
    #include <vector>
    #include <list>
    #include <map>

    using namespace souffle;

    namespace souffle {
        class ParserDriver;
        namespace parser {
        }
    }

    using yyscan_t = void*;


    #define YY_NULLPTR nullptr

    /* Macro to update locations as parsing proceeds */
#define YYLLOC_DEFAULT(Cur, Rhs, N)               \
    do {                                          \
        if (N) {                                  \
            (Cur).start = YYRHSLOC(Rhs, 1).start; \
            (Cur).end = YYRHSLOC(Rhs, N).end;     \
            (Cur).file = YYRHSLOC(Rhs, N).file;   \
        } else {                                  \
            (Cur).start = YYRHSLOC(Rhs, 0).end;   \
            (Cur).end = YYRHSLOC(Rhs, 0).end;     \
            (Cur).file = YYRHSLOC(Rhs, 0).file;   \
        }                                         \
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
%token END 0                     "end of input"
%token LEAVE                     "end of included file"
%token ENTER                     "start of included file"
%token ENDFILE                   "end of file"
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
%token BTREE_DELETE_QUALIFIER    "BTREE_DELETE datastructure qualifier"
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
%token TRUELIT                   "true literal constraint"
%token FALSELIT                  "false literal constraint"
%token PLAN                      "plan keyword"
%token ITERATION                 "recursive iteration keyword"
%token CHOICEDOMAIN              "choice-domain"
%token IF                        ":-"
%token DECL                      "relation declaration"
%token FUNCTOR                   "functor declaration"
%token INPUT_DECL                "input directives declaration"
%token OUTPUT_DECL               "output directives declaration"
%token DEBUG_DELTA               "debug_delta"
%token PRINTSIZE_DECL            "printsize directives declaration"
%token LIMITSIZE_DECL            "limitsize directives declaration"
%token OVERRIDE                  "override rules of super-component"
%token TYPE                      "type declaration"
%token LATTICE                   "lattice declaration"
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
%token ATNOT                     "@!"
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
%token MAPSTO                    "->"
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
%token <std::string> OUTER_DOC_COMMENT "outer doc comment"
%token <std::string> INNER_DOC_COMMENT "inner doc comment"

/* -- Non-Terminal Types -- */
%type <RuleBody>                          aggregate_body
%type <AggregateOp>                            aggregate_func
%type <Own<ast::Argument>>                arg
%type <VecOwn<ast::Argument>>             arg_list
%type <Own<ast::Atom>>                    atom
%type <VecOwn<ast::Attribute>>            attributes_list
%type <RuleBody>                          body
%type <Own<ast::ComponentType>>           component_type
%type <Own<ast::ComponentInit>>           component_init
%type <Own<ast::Component>>               component_decl
%type <Own<ast::Component>>               component_body
%type <Own<ast::Component>>               component_head
%type <RuleBody>                          conjunction
%type <Own<ast::Constraint>>              constraint
%type <Own<ast::FunctionalConstraint>>    dependency
%type <VecOwn<ast::FunctionalConstraint>> dependency_list
%type <VecOwn<ast::FunctionalConstraint>> dependency_list_aux
%type <RuleBody>                          disjunction
%type <Own<ast::ExecutionOrder>>          plan_order
%type <Own<ast::ExecutionPlan>>           query_plan
%type <Own<ast::ExecutionPlan>>           query_plan_list
%type <Own<ast::Clause>>                  fact
%type <VecOwn<ast::Attribute>>            functor_arg_type_list
%type <std::string>                       functor_built_in
%type <Own<ast::FunctorDeclaration>>      functor_decl
%type <VecOwn<ast::Atom>>                 head
%type <ast::QualifiedName>                qualified_name
%type <VecOwn<ast::Directive>>            directive_list
%type <VecOwn<ast::Directive>>            directive_head
%type <ast::DirectiveType>                     directive_head_decl
%type <VecOwn<ast::Directive>>            relation_directive_list
%type <std::string>                       kvp_value
%type <VecOwn<ast::Argument>>             non_empty_arg_list
%type <Own<ast::Attribute>>               attribute
%type <VecOwn<ast::Attribute>>            non_empty_attributes
%type <std::vector<std::string>>          non_empty_attribute_names
%type <ast::ExecutionOrder::ExecOrder>    non_empty_plan_order_list
%type <VecOwn<ast::Attribute>>            non_empty_functor_arg_type_list
%type <Own<ast::Attribute>>               functor_attribute;
%type <std::vector<std::pair<std::string, std::string>>>      non_empty_key_value_pairs
%type <VecOwn<ast::Relation>>             relation_names
%type <Own<ast::Pragma>>                  pragma
%type <VecOwn<ast::Attribute>>            record_type_list
%type <VecOwn<ast::Relation>>             relation_decl
%type <std::set<RelationTag>>                  relation_tags
%type <VecOwn<ast::Clause>>               rule
%type <VecOwn<ast::Clause>>               rule_def
%type <RuleBody>                          term
%type <Own<ast::Type>>                    type_decl
%type <std::vector<ast::QualifiedName>>   component_type_params
%type <std::vector<ast::QualifiedName>>   component_param_list
%type <std::vector<ast::QualifiedName>>   union_type_list
%type <VecOwn<ast::BranchType>>    adt_branch_list
%type <Own<ast::BranchType>>       adt_branch
%type <Own<ast::Lattice>>                 lattice_decl
%type <std::pair<ast::LatticeOperator, Own<ast::Argument>>>                 lattice_operator
%type <std::map<ast::LatticeOperator, Own<ast::Argument>>>      lattice_operator_list
%type <ast::AnnotationList> annotations
%type <ast::Annotation> annotation
%type <ast::Annotation> inner_annotation
%type <ast::AnnotationList> inner_annotations
%type <ast::AnnotationList> non_empty_inner_annotations
%type <ast::TokenStream> annotation_input
%type <ast::TokenStream> token_stream
%type <ast::TokenTree> ident_token
%type <ast::TokenTree> token
%type <ast::TokenTree> delim
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
  | unit annotations ENTER
    { // the ENTER token helps resynchronizing current source location
      // when an included file is entered
      const auto annotations = $annotations;
      if (!annotations.empty()) {
        driver.uselessAnnotations(annotations, "before '.include'");
      }
    }
  | unit annotations LEAVE
    { // the LEAVE token helps resynchronizing current source location
      // when an included file is left
      const auto annotations = $annotations;
      if (!annotations.empty()) {
          driver.uselessAnnotations(annotations, "at end of included file");
      }
    }
  | unit annotations ENDFILE
    {
      const auto annotations = $annotations;
      if (!annotations.empty()) {
          driver.uselessAnnotations(annotations, "at end of file");
      }
    }
  | unit annotations directive_head
    {
      const auto annotations = $annotations;
      for (auto&& cur : $directive_head) {
        cur->setAnnotations(annotations);
        driver.addDirective(std::move(cur));
      }
    }
  | unit rule
    {
      for (auto&& cur : $rule) {
        driver.addClause(std::move(cur));
      }
    }
  | unit fact
    {
      auto fact = $fact;
      driver.addClause(std::move(fact));
    }
  | unit annotations component_decl
    {
      auto component_decl = $component_decl;
      component_decl->prependAnnotations($annotations);
      driver.addComponent(std::move(component_decl));
    }
  | unit annotations component_init
    {
      auto component_init = $component_init;
      component_init->setAnnotations($annotations);
      driver.addInstantiation(std::move(component_init));
    }
  | unit annotations pragma
    {
      auto pragma = $pragma;
      pragma->setAnnotations($annotations);
      driver.addPragma(std::move(pragma));
    }
  | unit annotations type_decl
    {
      auto type_decl = $type_decl;
      type_decl->setAnnotations($annotations);
      driver.addType(std::move(type_decl));
    }
  | unit annotations lattice_decl
    {
      auto lattice_decl = $lattice_decl;
      lattice_decl->setAnnotations($annotations);
      driver.addLattice(std::move(lattice_decl));
    }
  | unit annotations functor_decl
    {
      auto functor_decl = $functor_decl;
      functor_decl->setAnnotations($annotations);
      driver.addFunctorDeclaration(std::move(functor_decl));
    }
  | unit annotations relation_decl
    {
      const auto annotations = $annotations;
      for (auto&& rel : $relation_decl) {
        // Note: we duplicate annotations on every relation of the declaration.
        //
        // An alternative would be to allow distinct annotations before each
        // relation name.
        rel->setAnnotations(annotations);
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
      $$ = driver.mkQN($IDENT);
    }
  | qualified_name DOT IDENT
    {
      $$ = $1; $$.append($IDENT);
    }
  ;

/**
 * Type Declarations
 */
type_decl
  : TYPE IDENT SUBTYPE qualified_name
    {
      $$ = mk<ast::SubsetType>(driver.mkQN($IDENT), $qualified_name, @$);
    }
  | TYPE IDENT EQUALS union_type_list
    {
      auto utl = $union_type_list;
      auto id = $IDENT;
      if (utl.size() > 1) {
        $$ = mk<ast::UnionType>(driver.mkQN(id), utl, @$);
      } else {
        $$ = mk<ast::AliasType>(driver.mkQN(id), utl[0], @$);
      }
    }
  | TYPE IDENT EQUALS record_type_list
    {
      $$ = mk<ast::RecordType>(driver.mkQN($IDENT), $record_type_list, @$);
    }
  | TYPE IDENT EQUALS annotation annotations adt_branch_list
    { // special case to avoid conflict in grammar with `annotations`

      // insert outer annotations before inner annotations of the first branch
      auto branches = $adt_branch_list;
      branches.front()->prependAnnotations($annotations);
      branches.front()->prependAnnotation($annotation);
      $$ = mk<ast::AlgebraicDataType>(driver.mkQN($IDENT), std::move(branches), @$);
    }
  | TYPE IDENT EQUALS adt_branch_list
    {
      $$ = mk<ast::AlgebraicDataType>(driver.mkQN($IDENT), $adt_branch_list, @$);
    }
    /* Deprecated Type Declarations */
  | NUMBER_TYPE IDENT
    {
      $$ = driver.mkDeprecatedSubType(driver.mkQN($IDENT), driver.mkQN("number"), @$);
    }
  | SYMBOL_TYPE IDENT
    {
      $$ = driver.mkDeprecatedSubType(driver.mkQN($IDENT), driver.mkQN("symbol"), @$);
    }
  | TYPE IDENT
    {
      $$ = driver.mkDeprecatedSubType(driver.mkQN($IDENT), driver.mkQN("symbol"), @$);
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
  | adt_branch_list PIPE annotations adt_branch
    {
      $$ = $1;
      auto adt_branch = $adt_branch;
      adt_branch->addAnnotations($annotations);
      $$.emplace_back(std::move(adt_branch));
    }
  ;

adt_branch
  : IDENT[name] LBRACE inner_annotations RBRACE
    {
      $$ = mk<ast::BranchType>(driver.mkQN($name), VecOwn<ast::Attribute>{}, @$);
      $$->addAnnotations($inner_annotations);
    }
  | IDENT[name] LBRACE inner_annotations non_empty_attributes[attributes] RBRACE
    {
      $$ = mk<ast::BranchType>(driver.mkQN($name), $attributes, @$);
      $$->addAnnotations($inner_annotations);
    }
  ;

/**
 * Lattice Declarations
 */

lattice_decl
  : LATTICE IDENT[name] LT GT LBRACE lattice_operator_list RBRACE
    {
      $$ = mk<ast::Lattice>(driver.mkQN($name), std::move($lattice_operator_list), @$);
    }

lattice_operator_list
  :  lattice_operator COMMA lattice_operator_list
    {
      $$ = $3;
      $$.emplace($lattice_operator);
    }
  | lattice_operator
    {
      $$.emplace($lattice_operator);
    }

lattice_operator
  : IDENT MAPSTO arg
    {
      auto op = ast::latticeOperatorFromString($IDENT);
      if (!op.has_value()) {
        driver.error(@$, "Lattice operator not recognized");
      }
      $$ = std::make_pair(op.value(), std::move($arg));
    }

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
  | DECL IDENT[delta] EQUALS DEBUG_DELTA LPAREN IDENT[name] RPAREN relation_tags
    {
      auto tags = $relation_tags;
      $$.push_back(mk<ast::Relation>(driver.mkQN($delta), @2));
      for (auto&& rel : $$) {
        rel->setIsDeltaDebug(driver.mkQN($name));
        for (auto tag : tags) {
          if (isRelationQualifierTag(tag)) {
            rel->addQualifier(getRelationQualifierFromTag(tag));
          } else if (isRelationRepresentationTag(tag)) {
            rel->setRepresentation(getRelationRepresentationFromTag(tag));
          } else {
            assert(false && "unhandled tag");
          }
        }
      }
    }
  ;

/**
 * Relation Names
 */
relation_names
  : IDENT
    {
      $$.push_back(mk<ast::Relation>(driver.mkQN($1), @1));
    }
  | relation_names COMMA IDENT
    {
      $$ = $1;
      $$.push_back(mk<ast::Relation>(driver.mkQN($3), @3));
    }
  ;

/**
 * Attributes
 */
attributes_list
  : LPAREN RPAREN
    {
    }
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
  : annotations IDENT[name] COLON qualified_name[type]
    {
      @$ = @$.from(@name);
      $$ = mk<ast::Attribute>($name, $type, @type);
      $$->setAnnotations($annotations);
    }
  | annotations IDENT[name] COLON qualified_name[type] LT GT
    {
      @$ = @$.from(@name);
      $$ = mk<ast::Attribute>($name, $type, true, @type);
      $$->setAnnotations($annotations);
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
  | relation_tags BTREE_DELETE_QUALIFIER
    {
      $$ = driver.addReprTag(RelationTag::BTREE_DELETE, @2, $1);
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
  : annotations atom DOT
    {
      @$ = @$.from(@2);
      $$ = mk<ast::Clause>($atom, VecOwn<ast::Literal> {}, nullptr, @$);
      $$->setAnnotations($annotations);
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
   | annotations atom[less] LE atom[greater] IF inner_annotations body DOT
    {
      @$ = @$.from(@less);
      auto bodies = $body.toClauseBodies();
      const auto annotations = $annotations;
      const auto inner_annotations = $inner_annotations;
      Own<ast::Atom> lt = nameUnnamedVariables(std::move($less));
      Own<ast::Atom> gt = std::move($greater);
      for (auto&& body : bodies) {
        auto cur = mk<ast::SubsumptiveClause>(clone(lt));
        cur->setBodyLiterals(clone(body->getBodyLiterals()));
        auto literals = cur->getBodyLiterals();
        cur->setHead(clone(lt));
        cur->addToBodyFront(clone(gt));
        cur->addToBodyFront(clone(lt));
        cur->setAnnotations(annotations);
        cur->addAnnotations(inner_annotations);
        cur->setSrcLoc(@$);
        $$.push_back(std::move(cur));
      }
    }
   | annotations atom[less] LE atom[greater] IF inner_annotations body DOT query_plan
    {
      @$ = @$.from(@less);
      auto bodies = $body.toClauseBodies();
      const auto annotations = $annotations;
      const auto inner_annotations = $inner_annotations;
      Own<ast::Atom> lt = nameUnnamedVariables(std::move($less));
      Own<ast::Atom> gt = std::move($greater);
      for (auto&& body : bodies) {
        auto cur = mk<ast::SubsumptiveClause>(clone(lt));
        cur->setBodyLiterals(clone(body->getBodyLiterals()));
        auto literals = cur->getBodyLiterals();
        cur->setHead(clone(lt));
        cur->addToBodyFront(clone(gt));
        cur->addToBodyFront(clone(lt));
        cur->setAnnotations(annotations);
        cur->addAnnotations(inner_annotations);
        cur->setSrcLoc(@$);
        cur->setExecutionPlan(clone($query_plan));
        $$.push_back(std::move(cur));
      }
    }
  ;

/**
 * Rule Definition
 */
rule_def
  : head[heads] IF inner_annotations body DOT
    {
      const auto inner_annotations = $inner_annotations;
      auto bodies = $body.toClauseBodies();
      for (auto&& head : $heads) {
        for (auto&& body : bodies) {
          auto cur = clone(body);
          std::unique_ptr<ast::Atom> curhead = clone(head);
          // move annotations from head to clause
          cur->stealAnnotationsFrom(*curhead);
          cur->addAnnotations(inner_annotations);
          cur->setHead(std::move(curhead));
          cur->setSrcLoc(@$);
          $$.emplace_back(std::move(cur));
        }
      }
    }
  ;

/**
 * Rule Head
 */
head
  : annotations atom
    {
      @$ = @$.from(@atom);
      auto atom = $atom;
      atom->setAnnotations($annotations);
      $$.emplace_back(std::move(atom));
    }
  | head COMMA annotations atom
    {
      $$ = $1;
      auto atom = $atom;
      atom->setAnnotations($annotations);
      $$.emplace_back(std::move(atom));
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
      $$.disjunct($conjunction);
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
      $$.conjunct($term);
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
      $$ = $2.negated();
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
  | TRUELIT
    {
      $$ = mk<ast::BooleanConstraint>(true , @$);
    }
  | FALSELIT
    {
      $$ = mk<ast::BooleanConstraint>(false, @$);
    }
  ;

/**
 * Argument List
 */
arg_list
  : %empty
    {
    }
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
  | ITERATION LPAREN RPAREN
    {
      $$ = mk<ast::IterationCounter>(@$);
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
  | DOLLAR qualified_name[branch] LPAREN arg_list RPAREN
    {
      $$ = mk<ast::BranchInit>($branch, $arg_list, @$);
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
      $$ = mk<ast::UserDefinedFunctor>($IDENT, $arg_list, @$);
    }
  | functor_built_in LPAREN arg_list RPAREN
    {
      $$ = mk<ast::IntrinsicFunctor>($functor_built_in, $arg_list, @$);
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
      auto nested_arg = $nested_arg;
      const auto* asNumeric = as<ast::NumericConstant>(nested_arg);
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
    /* -- User-defined aggregators -- */
  | AT AT IDENT arg_list[rest] COLON arg[first] COMMA aggregate_body
    {
      auto bodies = $aggregate_body.toClauseBodies();
      if (bodies.size() != 1) {
        driver.error("ERROR: disjunctions in aggregation clauses are currently not supported");
      }
      auto rest = $rest;
      auto expr = rest.empty() ? nullptr : std::move(rest[0]);
      auto body = (bodies.size() == 1) ? clone(bodies[0]->getBodyLiterals()) : VecOwn<ast::Literal> {};
      $$ = mk<ast::UserDefinedAggregator>($IDENT, std::move($first), std::move(expr), std::move(body), @$);
    }
    /* -- aggregators -- */
  | aggregate_func arg_list COLON aggregate_body
    {
      auto aggregate_func = $aggregate_func;
      auto arg_list = $arg_list;
      auto bodies = $aggregate_body.toClauseBodies();
      if (bodies.size() != 1) {
        driver.error("ERROR: disjunctions in aggregation clauses are currently not supported");
      }
      // TODO: move this to a semantic check when aggs are extended to multiple exprs
      auto given    = arg_list.size();
      auto required = aggregateArity(aggregate_func);
      if (given < required.first || required.second < given) {
        driver.error("ERROR: incorrect expression arity for given aggregate mode");
      }
      auto expr = arg_list.empty() ? nullptr : std::move(arg_list[0]);
      auto body = (bodies.size() == 1) ? clone(bodies[0]->getBodyLiterals()) : VecOwn<ast::Literal> {};
      $$ = mk<ast::IntrinsicAggregator>(aggregate_func, std::move(expr), std::move(body), @$);
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
      $$ = mk<ast::ExecutionPlan>(@$);
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
  : component_head LBRACE component_body annotations RBRACE
    {
      auto head = $component_head;
      $$ = $component_body;
      $$->setComponentType(clone(head->getComponentType()));
      $$->copyBaseComponents(*head);
      $$->setSrcLoc(@$);
      auto annotations = $annotations;
      if (!annotations.empty()) {
          driver.uselessAnnotations(annotations, "at end of the component");
      }
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
      $$.push_back(driver.mkQN($IDENT));
    }
  | component_param_list COMMA IDENT
    {
      $$ = $1;
      $$.push_back(driver.mkQN($IDENT));
    }
  ;

/**
 * Component body
 */
component_body
  : inner_annotations
    {
      $$ = mk<ast::Component>();
      $$->addAnnotations($inner_annotations);
    }
  | component_body annotations ENTER
    {
      const auto annotations = $annotations;
      if (!annotations.empty()) {
          driver.uselessAnnotations(annotations, "before '.include'");
      }
      $$ = $1;
    }
  | component_body annotations LEAVE
    {
      const auto annotations = $annotations;
      if (!annotations.empty()) {
          driver.uselessAnnotations(annotations, "at end of included file");
      }
      $$ = $1;
    }
  | component_body annotations ENDFILE
    {
      const auto annotations = $annotations;
      if (!annotations.empty()) {
          driver.uselessAnnotations(annotations, "at end of file");
      }
      // unterminated component
      error(@1, "unterminated component, missing '}'");
      $$ = $1;
    }
  | component_body annotations directive_head
    {
      $$ = $1;
      const auto annotations = $annotations;
      for (auto&& x : $3) {
        x->setAnnotations(annotations);
        $$->addDirective(std::move(x));
      }
    }
  | component_body rule
    {
      $$ = $1;
      for (auto&& rule : $rule) {
        $$->addClause(std::move(rule));
      }
    }
  | component_body fact
    {
      $$ = $1;
      $$->addClause($fact);
    }
  | component_body annotations OVERRIDE IDENT
    {
      $$ = $1;
      $$->addOverride($4);
    }
  | component_body annotations component_init
    {
      $$ = $1;
      auto component_init = $component_init;
      component_init->setAnnotations($annotations);
      $$->addInstantiation(std::move(component_init));
    }
  | component_body annotations component_decl
    {
      $$ = $1;
      auto component_decl = $component_decl;
      component_decl->prependAnnotations($annotations);
      $$->addComponent(std::move(component_decl));
    }
  | component_body annotations type_decl
    {
      $$ = $1;
      auto type_decl = $type_decl;
      type_decl->setAnnotations($annotations);
      $$->addType(std::move(type_decl));
    }
  | component_body annotations lattice_decl
    {
      $$ = $1;
      auto lattice_decl = $lattice_decl;
      lattice_decl->setAnnotations($annotations);
      $$->addLattice(std::move(lattice_decl));
    }
  | component_body annotations relation_decl
    {
      $$ = $1;
      const auto annotations = $annotations;
      for (auto&& rel : $relation_decl) {
        driver.addIoFromDeprecatedTag(*rel);
        // Note: we duplicate annotations on every relation of the declaration.
        //
        // An alternative would be to allow distinct annotations before each
        // relation name.
        rel->setAnnotations(annotations);
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
  : annotations qualified_name[type]
    {
      $$ = mk<ast::Attribute>("", $type, @type);
      $$->setAnnotations($annotations);
    }
  | annotations IDENT[name] COLON qualified_name[type]
    {
      $$ = mk<ast::Attribute>($name, $type, @type);
      $$->setAnnotations($annotations);
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
  | TRUELIT
    {
      $$ = "true";
    }
  | FALSELIT
    {
      $$ = "false";
    }
  ;

/**
 * List of annotations
 */

annotations
  : %empty
    {
    }
  | annotations annotation
    {
      auto annotations = $1;
      if (annotations.empty()) {
        @$ = @2;
      }
      annotations.emplace_back($annotation);
      $$ = std::move(annotations);
    }
  ;

annotation
  : AT LBRACKET ident_token annotation_input RBRACKET
    {
      ast::QualifiedName key = driver.mkQN(std::get<ast::Single>($ident_token).token.text);
      $$ = ast::Annotation(
              ast::Annotation::Kind::Normal, ast::Annotation::Style::Outer, key, $annotation_input, @$);
    }
  | OUTER_DOC_COMMENT
    { // doc comment is a syntactic sugar for annotation `@[doc = "some doc"]`
      ast::TokenStream ts{makeTokenTree(ast::TokenKind::Eq, "="),
              ast::Single{ast::TokenKind::Symbol, $OUTER_DOC_COMMENT}};
      $$ = ast::Annotation(ast::Annotation::Kind::DocComment, ast::Annotation::Style::Outer,
              ast::QualifiedName::fromString("doc"), std::move(ts), @$);
    }
  ;

annotation_input
  : %empty
    {
    }
  | EQUALS token token_stream
    {
      $$ = $token_stream;
      $$.insert($$.begin(), $token);
      $$.insert($$.begin(), makeTokenTree(ast::TokenKind::Eq, "="));
    }
  | EQUALS ident_token token_stream
    {
      $$ = $token_stream;
      $$.insert($$.begin(), $ident_token);
      $$.insert($$.begin(), makeTokenTree(ast::TokenKind::Eq, "="));
    }
  | EQUALS delim
    {
      $$ = {$delim};
      $$.insert($$.begin(), makeTokenTree(ast::TokenKind::Eq, "="));
    }
  | delim
    {
      $$ = ast::TokenStream{$1};
    }
  ;

inner_annotations
  : %empty
    {
    }
  | non_empty_inner_annotations
    {
      $$ = $1;
    }
  ;

non_empty_inner_annotations
  :inner_annotation
    {
      $$.emplace_back($1);
    }
  | non_empty_inner_annotations inner_annotation
    {
      $$ = $1;
      $$.emplace_back($2);
    }
  ;

inner_annotation
  : INNER_DOC_COMMENT
    { // doc comment is a syntactic sugar for annotation `@[doc = "some doc"]`
      ast::TokenStream ts{makeTokenTree(ast::TokenKind::Eq, "="),
              ast::Single{ast::TokenKind::Symbol, $INNER_DOC_COMMENT}};
      $$ = ast::Annotation(ast::Annotation::Kind::DocComment, ast::Annotation::Style::Inner,
              ast::QualifiedName::fromString("doc"), std::move(ts), @1);
    }
  | ATNOT LBRACKET ident_token annotation_input RBRACKET
    { // we introduce ATNOT (`@!`) token because using just `@` followed by `!`
      // cause shift/reduce conflicts in the grammar when inner annotations are followed
      // by outter annotations of the next item:
      //
      // ```
      // .comp C {
      //   @![inner_for_C()]
      //   @[outter_for_D()]
      //   .decl D()
      // }
      // ```
      //
      // The parser generator (bison) is not able to lookahead for `!` after `@`. When it
      // sees the first `@` it has two choices:
      // - either consider empty inner_annotations and start outter_annotations
      //   (although it would fail because of the following `!`).
      // - or consider the start of an inner annotation.
      //
      // For these reason, we make the scanner detect `@!` as a single token.
      // The scanner does not detect any variant of `@` followed by `!`
      // if there is a whitespace or a comment in-between.
      // I believe it is a good enough tradeoff to keep the lexer simple.

      ast::QualifiedName key = driver.mkQN(std::get<ast::Single>($ident_token).token.text);
      $$ = ast::Annotation(
              ast::Annotation::Kind::Normal, ast::Annotation::Style::Inner, key, $annotation_input, @$);
    }

token_stream
  : %empty { }
  | token_stream token
  {
    $$ = $1;
    $$.emplace_back($token);
  }
  | token_stream ident_token
  {
    $$ = $1;
    $$.emplace_back($ident_token);
  }
  | token_stream delim
  {
    $$ = $1;
    $$.emplace_back($delim);
  }
  ;

delim
  : LPAREN token_stream RPAREN
  {
    $$ = makeTokenTree(ast::Delimiter::Paren, std::move($token_stream));
  }
  | LBRACE token_stream RBRACE
  {
    $$ = makeTokenTree(ast::Delimiter::Brace, std::move($token_stream));
  }
  | LBRACKET token_stream RBRACKET
  {
    $$ = makeTokenTree(ast::Delimiter::Bracket, std::move($token_stream));
  }
  ;

  // all tokens that look like an identifier
ident_token
  : IDENT     { $$ = makeTokenTree(ast::TokenKind::Ident, $IDENT); }
  | AS                        { $$ = makeTokenTree(ast::TokenKind::Ident, "as"); }
  | AUTOINC                   { $$ = makeTokenTree(ast::TokenKind::Ident, "autoinc"); }
  | BRIE_QUALIFIER            { $$ = makeTokenTree(ast::TokenKind::Ident, "brie"); }
  | BTREE_DELETE_QUALIFIER    { $$ = makeTokenTree(ast::TokenKind::Ident, "btree_delete"); }
  | BTREE_QUALIFIER           { $$ = makeTokenTree(ast::TokenKind::Ident, "btree"); }
  | BW_AND                    { $$ = makeTokenTree(ast::TokenKind::Ident, "band"); }
  | BW_NOT                    { $$ = makeTokenTree(ast::TokenKind::Ident, "bnot"); }
  | BW_OR                     { $$ = makeTokenTree(ast::TokenKind::Ident, "bor"); }
  | BW_SHIFT_L                { $$ = makeTokenTree(ast::TokenKind::Ident, "bshl"); }
  | BW_SHIFT_R                { $$ = makeTokenTree(ast::TokenKind::Ident, "bshr"); }
  | BW_SHIFT_R_UNSIGNED       { $$ = makeTokenTree(ast::TokenKind::Ident, "bshru"); }
  | BW_XOR                    { $$ = makeTokenTree(ast::TokenKind::Ident, "bxor"); }
  | CAT                       { $$ = makeTokenTree(ast::TokenKind::Ident, "cat"); }
  | CHOICEDOMAIN              { $$ = makeTokenTree(ast::TokenKind::Ident, "choice-domain"); }
  | COUNT                     { $$ = makeTokenTree(ast::TokenKind::Ident, "count"); }
  | EQREL_QUALIFIER           { $$ = makeTokenTree(ast::TokenKind::Ident, "eqrel"); }
  | FALSELIT                  { $$ = makeTokenTree(ast::TokenKind::Ident, "false"); }
  | INLINE_QUALIFIER          { $$ = makeTokenTree(ast::TokenKind::Ident, "inline"); }
  | INPUT_QUALIFIER           { $$ = makeTokenTree(ast::TokenKind::Ident, "input"); }
  | L_AND                     { $$ = makeTokenTree(ast::TokenKind::Ident, "land"); }
  | L_NOT                     { $$ = makeTokenTree(ast::TokenKind::Ident, "lnot"); }
  | L_OR                      { $$ = makeTokenTree(ast::TokenKind::Ident, "lor"); }
  | L_XOR                     { $$ = makeTokenTree(ast::TokenKind::Ident, "lxor"); }
  | MAGIC_QUALIFIER           { $$ = makeTokenTree(ast::TokenKind::Ident, "magic"); }
  | MAX                       { $$ = makeTokenTree(ast::TokenKind::Ident, "max"); }
  | MEAN                      { $$ = makeTokenTree(ast::TokenKind::Ident, "mean"); }
  | MIN                       { $$ = makeTokenTree(ast::TokenKind::Ident, "min"); }
  | NIL                       { $$ = makeTokenTree(ast::TokenKind::Ident, "nil"); }
  | NO_INLINE_QUALIFIER       { $$ = makeTokenTree(ast::TokenKind::Ident, "no_inline"); }
  | NO_MAGIC_QUALIFIER        { $$ = makeTokenTree(ast::TokenKind::Ident, "no_magic"); }
  | ORD                       { $$ = makeTokenTree(ast::TokenKind::Ident, "ord"); }
  | OUTPUT_QUALIFIER          { $$ = makeTokenTree(ast::TokenKind::Ident, "output"); }
  | OVERRIDABLE_QUALIFIER     { $$ = makeTokenTree(ast::TokenKind::Ident, "overridable"); }
  | PRINTSIZE_QUALIFIER       { $$ = makeTokenTree(ast::TokenKind::Ident, "printsize"); }
  | RANGE                     { $$ = makeTokenTree(ast::TokenKind::Ident, "range"); }
  | STATEFUL                  { $$ = makeTokenTree(ast::TokenKind::Ident, "stateful"); }
  | STRLEN                    { $$ = makeTokenTree(ast::TokenKind::Ident, "strlen"); }
  | SUBSTR                    { $$ = makeTokenTree(ast::TokenKind::Ident, "substr"); }
  | SUM                       { $$ = makeTokenTree(ast::TokenKind::Ident, "sum"); }
  | TCONTAINS                 { $$ = makeTokenTree(ast::TokenKind::Ident, "contains"); }
  | TMATCH                    { $$ = makeTokenTree(ast::TokenKind::Ident, "match"); }
  | TOFLOAT                   { $$ = makeTokenTree(ast::TokenKind::Ident, "to_float"); }
  | TONUMBER                  { $$ = makeTokenTree(ast::TokenKind::Ident, "to_number"); }
  | TOSTRING                  { $$ = makeTokenTree(ast::TokenKind::Ident, "to_string"); }
  | TOUNSIGNED                { $$ = makeTokenTree(ast::TokenKind::Ident, "to_unsigned"); }
  | TRUELIT                   { $$ = makeTokenTree(ast::TokenKind::Ident, "true"); }

  // all tokens from the lexer except delimiters
token
  : FLOAT     { $$ = makeTokenTree(ast::TokenKind::Float, $FLOAT); }
  | NUMBER    { $$ = makeTokenTree(ast::TokenKind::Number, $NUMBER); }
  | UNSIGNED  { $$ = makeTokenTree(ast::TokenKind::Unsigned, $UNSIGNED); }
  | STRING    { $$ = makeTokenTree(ast::TokenKind::Symbol, $STRING); }
  // punctuations
  | AT                        { $$ = makeTokenTree(ast::TokenKind::At, "@"); }
  | ATNOT                     { $$ = makeTokenTree(ast::TokenKind::AtNot, "@!"); }
  | CARET                     { $$ = makeTokenTree(ast::TokenKind::Caret, "^"); }
  | COLON                     { $$ = makeTokenTree(ast::TokenKind::Colon, ":"); }
  | COMMA                     { $$ = makeTokenTree(ast::TokenKind::Comma, ","); }
  | DOLLAR                    { $$ = makeTokenTree(ast::TokenKind::Dollar, "$"); }
  | DOT                       { $$ = makeTokenTree(ast::TokenKind::Dot, "."); }
  | DOUBLECOLON               { $$ = makeTokenTree(ast::TokenKind::DoubleColon, "::"); }
  | EQUALS                    { $$ = makeTokenTree(ast::TokenKind::Eq, "="); }
  | EXCLAMATION               { $$ = makeTokenTree(ast::TokenKind::Exclamation, "!"); }
  | GE                        { $$ = makeTokenTree(ast::TokenKind::Ge, ">="); }
  | GT                        { $$ = makeTokenTree(ast::TokenKind::Gt, ">"); }
  | INNER_DOC_COMMENT         { /* ignore doc comments */ }
  | IF                        { $$ = makeTokenTree(ast::TokenKind::If, ":-"); }
  | LE                        { $$ = makeTokenTree(ast::TokenKind::Le, "<="); }
  | LT                        { $$ = makeTokenTree(ast::TokenKind::Lt, "<"); }
  | MAPSTO                    { $$ = makeTokenTree(ast::TokenKind::MapsTo, "->"); }
  | MINUS                     { $$ = makeTokenTree(ast::TokenKind::Minus, "-"); }
  | NE                        { $$ = makeTokenTree(ast::TokenKind::Ne, "!="); }
  | OUTER_DOC_COMMENT         { /* ignore doc comments */ }
  | PERCENT                   { $$ = makeTokenTree(ast::TokenKind::Percent, "%"); }
  | PIPE                      { $$ = makeTokenTree(ast::TokenKind::Pipe, "|"); }
  | PLUS                      { $$ = makeTokenTree(ast::TokenKind::Plus, "+"); }
  | SEMICOLON                 { $$ = makeTokenTree(ast::TokenKind::Semicolon, ";"); }
  | SLASH                     { $$ = makeTokenTree(ast::TokenKind::Slash, "/"); }
  | STAR                      { $$ = makeTokenTree(ast::TokenKind::Star, "*"); }
  | SUBTYPE                   { $$ = makeTokenTree(ast::TokenKind::Subtype, "<:"); }
  | UNDERSCORE                { $$ = makeTokenTree(ast::TokenKind::Underscore, "_"); }
  // commands
  | COMPONENT                 { $$ = makeTokenTree(ast::TokenKind::Ident, ".comp"); }
  | DECL                      { $$ = makeTokenTree(ast::TokenKind::Ident, ".decl"); }
  | FUNCTOR                   { $$ = makeTokenTree(ast::TokenKind::Ident, ".functor"); }
  | INPUT_DECL                { $$ = makeTokenTree(ast::TokenKind::Ident, ".input"); }
  | INSTANTIATE               { $$ = makeTokenTree(ast::TokenKind::Ident, ".init"); }
  | LATTICE                   { $$ = makeTokenTree(ast::TokenKind::Ident, ".lattice"); }
  | LIMITSIZE_DECL            { $$ = makeTokenTree(ast::TokenKind::Ident, ".limitsize"); }
  | NUMBER_TYPE               { $$ = makeTokenTree(ast::TokenKind::Ident, ".number_type"); }
  | OUTPUT_DECL               { $$ = makeTokenTree(ast::TokenKind::Ident, ".output"); }
  | OVERRIDE                  { $$ = makeTokenTree(ast::TokenKind::Ident, ".override"); }
  | PLAN                      { $$ = makeTokenTree(ast::TokenKind::Ident, ".plan"); }
  | PRAGMA                    { $$ = makeTokenTree(ast::TokenKind::Ident, ".pragma"); }
  | PRINTSIZE_DECL            { $$ = makeTokenTree(ast::TokenKind::Ident, ".printsize"); }
  | SYMBOL_TYPE               { $$ = makeTokenTree(ast::TokenKind::Ident, ".symbol_type"); }
  | TYPE                      { $$ = makeTokenTree(ast::TokenKind::Ident, ".type"); }
  ;


%%

void yy::parser::error(const location_type &l, const std::string &m)
{
  driver.error(l, m);
}
