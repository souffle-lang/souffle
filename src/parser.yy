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
%define parser_class_name {parser}
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
    #include "ast/AstArgument.h"
    #include "ast/AstClause.h"
    #include "ast/AstComponent.h"
    #include "ast/AstFunctorDeclaration.h"
    #include "ast/AstIO.h"
    #include "ast/AstNode.h"
    #include "ast/AstParserUtils.h"
    #include "ast/AstPragma.h"
    #include "ast/AstProgram.h"
    #include "BinaryConstraintOps.h"
    #include "FunctorOps.h"
    #include "RamTypes.h"
    #include "utility/StringUtil.h"

    using namespace souffle;

    namespace souffle {
        class ParserDriver;

        namespace parser {
          // FIXME: (when we can finally use Bison 3.2) Expunge this abombination.
          // HACK:  Bison 3.0.2 is stupid and ugly and doesn't support move semantics
          //        with the `lalr1.cc` skeleton and that makes me very mad.
          //        Thankfully (or not) two can play stupid games:
          //          Behold! std::auto_ptr 2: The Revengening
          // NOTE:  Bison 3.2 came out in 2019. `std::unique_ptr` appeared in C++11.
          //        How timely.
          // NOTE:  There are specialisations wrappers that'll allow us to (almost)
          //        transparently remove `Mov` once we switch to Bison 3.2+.

          template<typename A>
          struct Mov {
            mutable A value;

            Mov() = default;
            Mov(Mov&&) = default;
            template<typename B>
            Mov(B value) : value(std::move(value)) {}

            // CRIMES AGAINST COMPUTING HAPPENS HERE
            // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
            Mov(const Mov& x) : value(std::move(x.value)) {}
            Mov& operator=(Mov x) { value = std::move(x.value); return *this; }
            // detach/convert implicitly.
            operator A() { return std::move(value); }

            // support ptr-like behaviour
            A* operator->() { return &value; }
            A operator*() { return std::move(value); }
          };

          template<typename A>
          A unwrap(Mov<A> x) { return *x; }

          template<typename A>
          A unwrap(A x) { return x; }

          template<typename A>
          struct Mov<Own<A>> {
            mutable Own<A> value;

            Mov() = default;
            Mov(Mov&&) = default;
            template<typename B>
            Mov(B value) : value(std::move(value)) {}

            // CRIMES AGAINST COMPUTING HAPPENS HERE
            // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
            Mov(const Mov& x) : value(std::move(x.value)) {}
            Mov& operator=(Mov x) { value = std::move(x.value); return *this; }
            // detach/convert implicitly.
            operator Own<A>() { return std::move(value); }
            Own<A> operator*() { return std::move(value); }

            // support ptr-like behaviour
            A* operator->() { return value.get(); }
          };

          template<typename A>
          struct Mov<std::vector<A>> {
            mutable std::vector<A> value;

            Mov() = default;
            Mov(Mov&&) = default;
            template<typename B>
            Mov(B value) : value(std::move(value)) {}

            // CRIMES AGAINST COMPUTING HAPPENS HERE
            // HACK: Pretend you can copy it, but actually move it. Keeps Bison 3.0.2 happy.
            Mov(const Mov& x) : value(std::move(x.value)) {}
            Mov& operator=(Mov x) { value = std::move(x.value); return *this; }
            // detach/convert implicitly.
            operator std::vector<A>() { return std::move(value); }
            auto operator*() {
              std::vector<decltype(unwrap(std::declval<A>()))> ys;
              for (auto&& x : value) ys.push_back(unwrap(std::move(x)));
              return ys;
            }

            // basic ops
            using iterator = typename std::vector<A>::iterator;
            typename std::vector<A>::value_type& operator[](size_t i) { return value[i]; }
            iterator begin() { return value.begin(); }
            iterator end() { return value.end(); }
            void push_back(A x) { value.push_back(std::move(x)); }
            size_t size() const { return value.size(); }
            bool empty() const { return value.empty(); }
          };
        }

        template<typename A>
        parser::Mov<A> clone(const parser::Mov<A>& x) { return clone(x.value); }
    }

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
    #include "ParserDriver.h"
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
%token PRAGMA                    "pragma directive"
%token OUTPUT_QUALIFIER          "relation qualifier output"
%token INPUT_QUALIFIER           "relation qualifier input"
%token PRINTSIZE_QUALIFIER       "relation qualifier printsize"
%token BRIE_QUALIFIER            "BRIE datastructure qualifier"
%token BTREE_QUALIFIER           "BTREE datastructure qualifier"
%token EQREL_QUALIFIER           "equivalence relation qualifier"
%token OVERRIDABLE_QUALIFIER     "relation qualifier overidable"
%token INLINE_QUALIFIER          "relation qualifier inline"
%token TMATCH                    "match predicate"
%token TCONTAINS                 "checks whether substring is contained in a string"
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
%token IF                        ":-"
%token DECL                      "relation declaration"
%token FUNCTOR                   "functor declaration"
%token INPUT_DECL                "input directives declaration"
%token OUTPUT_DECL               "output directives declaration"
%token PRINTSIZE_DECL            "printsize directives declaration"
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
%type <Mov<RuleBody>>                       aggregate_body
%type <AggregateOp>                         aggregate_func
%type <Mov<Own<AstArgument>>>               arg
%type <Mov<VecOwn<AstArgument>>>            arg_list
%type <Mov<Own<AstAtom>>>                   atom
%type <Mov<VecOwn<AstAttribute>>>           attributes_list
%type <Mov<RuleBody>>                       body
%type <Mov<Own<AstComponentType>>>          comp_type
%type <Mov<Own<AstComponentInit>>>          comp_init
%type <Mov<Own<AstComponent>>>              component
%type <Mov<Own<AstComponent>>>              component_body
%type <Mov<Own<AstComponent>>>              component_head
%type <Mov<RuleBody>>                       conjunction
%type <Mov<Own<AstConstraint>>>             constraint
%type <Mov<RuleBody>>                       disjunction
%type <Mov<Own<AstExecutionOrder>>>         exec_order
%type <Mov<Own<AstExecutionPlan>>>          exec_plan
%type <Mov<Own<AstExecutionPlan>>>          exec_plan_list
%type <Mov<Own<AstClause>>>                 fact
%type <Mov<std::vector<TypeAttribute>>>     functor_arg_type_list
%type <Mov<std::string>>                    functor_built_in
%type <Mov<Own<AstFunctorDeclaration>>>     functor_decl
%type <Mov<VecOwn<AstAtom>>>                head
%type <Mov<AstQualifiedName>>               identifier
%type <Mov<VecOwn<AstIO>>>                  io_directive_list
%type <Mov<VecOwn<AstIO>>>                  io_head
%type <AstIoType>                           io_head_decl
%type <Mov<VecOwn<AstIO>>>                  io_relation_list
%type <Mov<std::string>>                    kvp_value
%type <Mov<VecOwn<AstArgument>>>            non_empty_arg_list
%type <Mov<VecOwn<AstAttribute>>>           non_empty_attributes
%type <Mov<AstExecutionOrder::ExecOrder>>   non_empty_exec_order_list
%type <Mov<std::vector<TypeAttribute>>>     non_empty_functor_arg_type_list
%type <Mov<std::vector<std::pair
            <std::string, std::string>>>>   non_empty_key_value_pairs
%type <Mov<VecOwn<AstRelation>>>            non_empty_relation_list
%type <Mov<Own<AstPragma>>>                 pragma
%type <TypeAttribute>                       predefined_type
%type <Mov<VecOwn<AstAttribute>>>           record_type_list
%type <Mov<VecOwn<AstRelation>>>            relation_decl
%type <std::set<RelationTag>>               relation_tags
%type <Mov<VecOwn<AstClause>>>              rule
%type <Mov<VecOwn<AstClause>>>              rule_def
%type <Mov<RuleBody>>                       term
%type <Mov<Own<AstType>>>                   type
%type <Mov<std::vector<AstQualifiedName>>>  type_params
%type <Mov<std::vector<AstQualifiedName>>>  type_param_list
%type <Mov<std::vector<AstQualifiedName>>>  union_type_list

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

/* Program */
program
  : unit
  ;

/* Top-level statement */
unit
  : %empty              { }
  | unit io_head        { for (auto&& cur : $io_head) driver.addIO(std::move(cur)); }
  | unit rule           { for (auto&& cur : $rule   ) driver.addClause(std::move(cur)); }
  | unit fact           { driver.addClause            ($fact); }
  | unit component      { driver.addComponent         ($component); }
  | unit comp_init      { driver.addInstantiation     ($comp_init); }
  | unit pragma         { driver.addPragma            ($pragma); }
  | unit type           { driver.addType              ($type); }
  | unit functor_decl   { driver.addFunctorDeclaration($functor_decl); }
  | unit relation_decl  {
        for (auto&& rel : $relation_decl) {
            driver.addIoFromDeprecatedTag(*rel);
            driver.addRelation(std::move(rel));
        }
    }
  ;

/**
 * Identifiers
 */

identifier
  :                 IDENT { $$ = $IDENT; }
    /* TODO (azreika): in next version: DOT -> DOUBLECOLON */
  | identifier DOT  IDENT { $$ = $1; $$->append($IDENT); }
  ;

/**
 * Types
 */

/* Type declarations */
type
  : TYPE IDENT[name] SUBTYPE IDENT[base_type_name] {
        $$ = mk<AstSubsetType>($name, $base_type_name, @$);
   }
  | TYPE IDENT EQUALS    union_type_list  { $$ = mk<AstUnionType >($2, $4, @$); }
  | TYPE IDENT EQUALS   record_type_list  { $$ = mk<AstRecordType>($2, $4, @$); }
    /* deprecated subset type forms */
  | NUMBER_TYPE IDENT { $$ = driver.mkDeprecatedSubType($IDENT, "number", @$); }
  | SYMBOL_TYPE IDENT { $$ = driver.mkDeprecatedSubType($IDENT, "symbol", @$); }
  | TYPE        IDENT { $$ = driver.mkDeprecatedSubType($IDENT, "symbol", @$); }
  ;

/* Union type argument declarations */
union_type_list
  :                       identifier {          $$.push_back($identifier); }
  | union_type_list PIPE  identifier { $$ = $1; $$.push_back($identifier); }
  ;

/**
 * Relations
 */

/* Relation declaration */
relation_decl
  : DECL non_empty_relation_list attributes_list relation_tags {
        auto tags             = $relation_tags;
        auto attributes_list  = $attributes_list;

        $$ = $non_empty_relation_list;
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

            rel->setAttributes(clone(attributes_list));
        }
    }
  ;

/* List of relation names to declare */
non_empty_relation_list
  :                               IDENT {          $$.push_back(mk<AstRelation>($1, @1)); }
  | non_empty_relation_list COMMA IDENT { $$ = $1; $$.push_back(mk<AstRelation>($3, @3)); }
  ;

/* Attribute definition of a relation */
/* specific wrapper to ensure the err msg says "expected ',' or ')'" */
record_type_list
  : LBRACKET RBRACKET                       { }
  | LBRACKET non_empty_attributes RBRACKET  { $$ = $2; }
  ;
attributes_list
  : LPAREN RPAREN                       { }
  | LPAREN non_empty_attributes RPAREN  { $$ = $2; }
  ;
non_empty_attributes
  :                            IDENT COLON identifier
    {           $$.push_back(mk<AstAttribute>($IDENT, $identifier, @identifier)); }
  | non_empty_attributes COMMA IDENT COLON identifier
    { $$ = $1;  $$.push_back(mk<AstAttribute>($IDENT, $identifier, @identifier)); }
  ;

/* Relation tags */
relation_tags
  : %empty { }
  | relation_tags      OUTPUT_QUALIFIER
    { $$ = driver.addDeprecatedTag(RelationTag::OUTPUT, @2, $1); }
  | relation_tags       INPUT_QUALIFIER
    { $$ = driver.addDeprecatedTag(RelationTag::INPUT, @2, $1); }
  | relation_tags   PRINTSIZE_QUALIFIER
    { $$ = driver.addDeprecatedTag(RelationTag::PRINTSIZE, @2, $1); }
  | relation_tags OVERRIDABLE_QUALIFIER { $$ = driver.addTag(RelationTag::OVERRIDABLE , @2, $1); }
  | relation_tags      INLINE_QUALIFIER { $$ = driver.addTag(RelationTag::INLINE      , @2, $1); }
  | relation_tags        BRIE_QUALIFIER { $$ = driver.addReprTag(RelationTag::BRIE    , @2, $1); }
  | relation_tags       BTREE_QUALIFIER { $$ = driver.addReprTag(RelationTag::BTREE   , @2, $1); }
  | relation_tags       EQREL_QUALIFIER { $$ = driver.addReprTag(RelationTag::EQREL   , @2, $1); }
  ;

/**
 * Datalog Rule Structure
 */

/* Fact */
fact : atom DOT { $$ = mk<AstClause>($atom, Mov<VecOwn<AstLiteral>> {}, nullptr, @$); };

/* Rule */
rule
  : rule_def {
        $$ = $rule_def;
    }
  | rule_def exec_plan {
        $$ = $rule_def;
        auto exec_plan = $exec_plan;
        for (auto&& rule : $$) {
            rule->setExecutionPlan(clone(exec_plan));
        }
    }
  ;

/* Rule definition */
rule_def
  : head[heads] IF body DOT {
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

/* Rule head */
head
  :            atom {          $$.push_back($atom); }
  | head COMMA atom { $$ = $1; $$.push_back($atom); }
  ;

/* Rule body */
body : disjunction { $$ = $disjunction; };

disjunction
  :                       conjunction { $$ = $conjunction; }
  | disjunction SEMICOLON conjunction { $$ = $1; $$->disjunct($conjunction); }
  ;

conjunction
  :                   term { $$ = $term; }
  | conjunction COMMA term { $$ = $1; $$->conjunct($term); }
  ;

/* Rule execution plan */
exec_plan : PLAN exec_plan_list { $$ = $exec_plan_list; };

/* Rule execution plan list */
exec_plan_list
  : NUMBER COLON exec_order {
        $$ = mk<AstExecutionPlan>();
        $$->setOrderFor(RamSignedFromString($NUMBER), Own<AstExecutionOrder>($exec_order));
    }
  | exec_plan_list[curr_list] COMMA NUMBER COLON exec_order {
        $$ = $curr_list;
        $$->setOrderFor(RamSignedFromString($NUMBER), $exec_order);
    }
  ;

/* Rule execution order */
exec_order
  : LPAREN RPAREN                           { $$ = mk<AstExecutionOrder>(AstExecutionOrder::ExecOrder(), @$); }
  | LPAREN non_empty_exec_order_list RPAREN { $$ = mk<AstExecutionOrder>($2, @$); }
  ;
non_empty_exec_order_list
  :                                 NUMBER {          $$.push_back(RamUnsignedFromString($NUMBER)); }
  | non_empty_exec_order_list COMMA NUMBER { $$ = $1; $$.push_back(RamUnsignedFromString($NUMBER)); }
  ;

/**
 * Terms in Rule Bodies
 */

/* Rule body term */
term
  : atom                      { $$ = RuleBody::atom($atom); }
  | constraint                { $$ = RuleBody::constraint($constraint); }
  | LPAREN disjunction RPAREN { $$ = $disjunction; }
  | EXCLAMATION term          { $$ = $2->negated(); }
  ;

/* Rule body atom */
atom : identifier LPAREN arg_list RPAREN { $$ = mk<AstAtom>($identifier, $arg_list, @$); };

/* Rule literal constraints */
constraint
    /* binary infix constraints */
  : arg LT      arg { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::LT, $1, $3, @$); }
  | arg GT      arg { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::GT, $1, $3, @$); }
  | arg LE      arg { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::LE, $1, $3, @$); }
  | arg GE      arg { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::GE, $1, $3, @$); }
  | arg EQUALS  arg { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::EQ, $1, $3, @$); }
  | arg NE      arg { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::NE, $1, $3, @$); }

    /* binary prefix constraints */
  | TMATCH    LPAREN arg[a0] COMMA arg[a1] RPAREN
    { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::MATCH   , $a0, $a1, @$); }
  | TCONTAINS LPAREN arg[a0] COMMA arg[a1] RPAREN
    { $$ = mk<AstBinaryConstraint>(BinaryConstraintOp::CONTAINS, $a0, $a1, @$); }

    /* zero-arity constraints */
  | TRUE  { $$ = mk<AstBooleanConstraint>(true , @$); }
  | FALSE { $$ = mk<AstBooleanConstraint>(false, @$); }
  ;

/* Argument list */
arg_list : %empty { } | non_empty_arg_list { $$ = $1; } ;
non_empty_arg_list
  :                           arg {          $$.push_back($arg); }
  | non_empty_arg_list COMMA  arg { $$ = $1; $$.push_back($arg); }
  ;

/* Atom argument */
arg
  : STRING      { $$ = mk<AstStringConstant >($STRING, @$); }
  | FLOAT       { $$ = mk<AstNumericConstant>($FLOAT, AstNumericConstant::Type::Float, @$); }
  | UNSIGNED    {
      auto&& n = $UNSIGNED; // drop the last character (`u`)
      $$ = mk<AstNumericConstant>(n.substr(0, n.size() - 1), AstNumericConstant::Type::Uint, @$);
    }
  | NUMBER      { $$ = mk<AstNumericConstant>($NUMBER, @$); }
  | UNDERSCORE  { $$ = mk<AstUnnamedVariable>(@$); }
  | DOLLAR      { $$ = mk<AstCounter        >(@$); }
  | IDENT       { $$ = mk<AstVariable       >($IDENT, @$); }
  | NIL         { $$ = mk<AstNilConstant    >(@$); }

  /* TODO (azreika): in next version: prepend records with identifiers */
  | LBRACKET arg_list RBRACKET { $$ = mk<AstRecordInit>($arg_list, @$); }

  |     LPAREN arg                  RPAREN { $$ = $2; }
  | AS  LPAREN arg COMMA identifier RPAREN { $$ = mk<AstTypeCast>($3, $identifier, @$); }

  | AT IDENT         LPAREN arg_list RPAREN { $$ = mk<AstUserDefinedFunctor>($IDENT, *$arg_list, @$); }
  | functor_built_in LPAREN arg_list RPAREN { $$ = mk<AstIntrinsicFunctor>($functor_built_in, *$arg_list, @$); }

    /* some aggregates have the same name as functors */
  | aggregate_func LPAREN arg[first] COMMA non_empty_arg_list[rest] RPAREN {
        VecOwn<AstArgument> arg_list = $rest;
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
          $$ = mk<AstIntrinsicFunctor>(func_op, std::move(arg_list), @$);
        } else {
          driver.error(@$, "aggregate operation has no functor equivalent");
          $$ = mk<AstUnnamedVariable>(@$);
        }
    }

    /* -- intrinsic functor -- */
    /* unary functors */
  | MINUS arg[nested_arg] %prec NEG {
        // If we have a constant that is not already negated we just negate the constant value.
        auto nested_arg = *$nested_arg;
        const auto* asNumeric = dynamic_cast<const AstNumericConstant*>(&*nested_arg);
        if (asNumeric && !isPrefix("-", asNumeric->getConstant())) {
            $$ = mk<AstNumericConstant>("-" + asNumeric->getConstant(), asNumeric->getType(), @nested_arg);
        } else { // Otherwise, create a functor.
            $$ = mk<AstIntrinsicFunctor>(@$, FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME, std::move(nested_arg));
        }
    }
  | BW_NOT  arg { $$ = mk<AstIntrinsicFunctor>(@$, "~", $2); }
  | L_NOT   arg { $$ = mk<AstIntrinsicFunctor>(@$, "!", $2); }

    /* binary infix functors */
  | arg PLUS                arg { $$ = mk<AstIntrinsicFunctor>(@$, "+"  , $1, $3); }
  | arg MINUS               arg { $$ = mk<AstIntrinsicFunctor>(@$, "-"  , $1, $3); }
  | arg STAR                arg { $$ = mk<AstIntrinsicFunctor>(@$, "*"  , $1, $3); }
  | arg SLASH               arg { $$ = mk<AstIntrinsicFunctor>(@$, "/"  , $1, $3); }
  | arg PERCENT             arg { $$ = mk<AstIntrinsicFunctor>(@$, "%"  , $1, $3); }
  | arg CARET               arg { $$ = mk<AstIntrinsicFunctor>(@$, "**" , $1, $3); }
  | arg L_AND               arg { $$ = mk<AstIntrinsicFunctor>(@$, "&&" , $1, $3); }
  | arg L_OR                arg { $$ = mk<AstIntrinsicFunctor>(@$, "||" , $1, $3); }
  | arg L_XOR               arg { $$ = mk<AstIntrinsicFunctor>(@$, "^^" , $1, $3); }
  | arg BW_AND              arg { $$ = mk<AstIntrinsicFunctor>(@$, "&"  , $1, $3); }
  | arg BW_OR               arg { $$ = mk<AstIntrinsicFunctor>(@$, "|"  , $1, $3); }
  | arg BW_XOR              arg { $$ = mk<AstIntrinsicFunctor>(@$, "^"  , $1, $3); }
  | arg BW_SHIFT_L          arg { $$ = mk<AstIntrinsicFunctor>(@$, "<<" , $1, $3); }
  | arg BW_SHIFT_R          arg { $$ = mk<AstIntrinsicFunctor>(@$, ">>" , $1, $3); }
  | arg BW_SHIFT_R_UNSIGNED arg { $$ = mk<AstIntrinsicFunctor>(@$, ">>>", $1, $3); }

    /* -- aggregators -- */
  | aggregate_func arg_list COLON aggregate_body {
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
        auto body = (bodies.size() == 1) ? clone(bodies[0]->getBodyLiterals()) : VecOwn<AstLiteral> {};
        $$ = mk<AstAggregator>($aggregate_func, std::move(expr), std::move(body), @$);
    }
  ;

functor_built_in
  : CAT         { $$ = "cat";         }
  | ORD         { $$ = "ord";         }
  | RANGE       { $$ = "range";       }
  | STRLEN      { $$ = "strlen";      }
  | SUBSTR      { $$ = "substr";      }
  | TOFLOAT     { $$ = "to_float";    }
  | TONUMBER    { $$ = "to_number";   }
  | TOSTRING    { $$ = "to_string";   }
  | TOUNSIGNED  { $$ = "to_unsigned"; }
  ;

aggregate_func
  : COUNT { $$ = AggregateOp::COUNT;  }
  | MAX   { $$ = AggregateOp::MAX;    }
  | MEAN  { $$ = AggregateOp::MEAN;   }
  | MIN   { $$ = AggregateOp::MIN;    }
  | SUM   { $$ = AggregateOp::SUM;    }
  ;

aggregate_body
  : LBRACE body RBRACE  { $$ = $body; }
  | atom                { $$ = RuleBody::atom($atom); }
  ;

/**
 * Components
 */

/* Component */
component
  : component_head LBRACE component_body RBRACE {
        auto head = $component_head;
        $$ = $component_body;
        $$->setComponentType(clone(head->getComponentType()));
        $$->copyBaseComponents(**head);
        $$->setSrcLoc(@$);
    }
  ;

/* Component head */
component_head
  : COMPONENT             comp_type { $$ = mk<AstComponent>();  $$->setComponentType($comp_type); }
  | component_head COLON  comp_type { $$ = $1;                  $$->addBaseComponent($comp_type); }
  | component_head COMMA  comp_type { $$ = $1;                  $$->addBaseComponent($comp_type); }
  ;

/* Component type */
comp_type : IDENT type_params { $$ = mk<AstComponentType>($IDENT, $type_params, @$); };

/* Component type parameters */
type_params
  : %empty                { }
  | LT type_param_list GT { $$ = $type_param_list; }
  ;

/* Component type parameter list */
type_param_list
  :                       IDENT {          $$.push_back($IDENT); }
  | type_param_list COMMA IDENT { $$ = $1; $$.push_back($IDENT); }
  ;

/* Component body */
component_body
  : %empty                        { $$ = mk<AstComponent>(); }
  | component_body io_head        { $$ = $1; for (auto&& x : $2) $$->addIO    (std::move(x)); }
  | component_body rule           { $$ = $1; for (auto&& x : $2) $$->addClause(std::move(x)); }
  | component_body fact           { $$ = $1; $$->addClause       ($2); }
  | component_body OVERRIDE IDENT { $$ = $1; $$->addOverride     ($3); }
  | component_body comp_init      { $$ = $1; $$->addInstantiation($2); }
  | component_body component      { $$ = $1; $$->addComponent    ($2); }
  | component_body type           { $$ = $1; $$->addType         ($2); }
  | component_body relation_decl  {
        $$ = $1;
        for (auto&& rel : $relation_decl) {
            driver.addIoFromDeprecatedTag(*rel);
            $$->addRelation(std::move(rel));
        }
    }
  ;

/* Component initialisation */
comp_init : INSTANTIATE IDENT EQUALS comp_type { $$ = mk<AstComponentInit>($IDENT, $comp_type, @$); };

/**
 * User-Defined Functors
 */

/* Functor declaration */
functor_decl
  : FUNCTOR IDENT LPAREN functor_arg_type_list[args] RPAREN COLON predefined_type
    { $$ = mk<AstFunctorDeclaration>($IDENT, $args, $predefined_type, @$); }
  ;

/* Functor argument list type */
functor_arg_type_list : %empty { } | non_empty_functor_arg_type_list { $$ = $1; };
non_empty_functor_arg_type_list
  :                                        predefined_type {          $$.push_back($predefined_type); }
  | non_empty_functor_arg_type_list COMMA  predefined_type { $$ = $1; $$.push_back($predefined_type); }
  ;

/* Predefined type */
predefined_type
  : IDENT {
        if ($IDENT == "number") {
            $$ = TypeAttribute::Signed;
        } else if ($IDENT == "symbol") {
            $$ = TypeAttribute::Symbol;
        } else if ($IDENT == "float") {
            $$ = TypeAttribute::Float;
        } else if ($IDENT == "unsigned") {
            $$ = TypeAttribute::Unsigned;
        } else {
            driver.error(@IDENT, "[number | symbol | float | unsigned] identifier expected");
        }
    }
  ;

/**
 * Other Directives
 */

/* Pragma directives */
pragma
  : PRAGMA STRING[key   ] STRING[value] { $$ = mk<AstPragma>($key   , $value, @$); }
  | PRAGMA STRING[option]               { $$ = mk<AstPragma>($option, ""    , @$); }
  ;

/* io directives */
io_head
  : io_head_decl io_directive_list {
        auto io_head_decl = $io_head_decl;
        for (auto&& io : $io_directive_list) {
            io->setType(io_head_decl);
            $$.push_back(std::move(io));
        }
    }
  ;

io_head_decl
  : INPUT_DECL      { $$ = AstIoType::input;      }
  | OUTPUT_DECL     { $$ = AstIoType::output;     }
  | PRINTSIZE_DECL  { $$ = AstIoType::printsize;  }
  ;

/* IO directive list */
io_directive_list
  : io_relation_list                { $$ = $io_relation_list; }
  | io_relation_list LPAREN RPAREN  { $$ = $io_relation_list; }
  | io_relation_list LPAREN non_empty_key_value_pairs RPAREN {
        $$ = $io_relation_list;
        for (auto&& kvp : $non_empty_key_value_pairs) {
            for (auto&& io : $$) {
                io->addDirective(kvp.first, kvp.second);
            }
        }
    }
  ;

/* IO relation list */
/* use a dummy `AstIoType` for now. `io_head` will replace it */
io_relation_list
  :                         identifier {          $$.push_back(mk<AstIO>(AstIoType::input, $1, @1)); }
  | io_relation_list COMMA  identifier { $$ = $1; $$.push_back(mk<AstIO>(AstIoType::input, $3, @3)); }
  ;

/* Key-value pairs */
non_empty_key_value_pairs
  :                                 IDENT EQUALS kvp_value {          $$.push_back({ $1, $3 }); }
  | non_empty_key_value_pairs COMMA IDENT EQUALS kvp_value { $$ = $1; $$.push_back({ $3, $5 }); }
  ;
kvp_value
  : STRING  { $$ = $STRING; }
  | IDENT   { $$ = $IDENT; }
  | TRUE    { $$ = "true"; }
  | FALSE   { $$ = "false"; }
  ;

%%

void yy::parser::error(const location_type &l, const std::string &m) {
    driver.error(l, m);
}
