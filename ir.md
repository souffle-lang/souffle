
# RAM IR to S-expression

## Variable

Variable -> Expression -> Node
```
(VARIABLE ,(? string?))
```

Example:
```
variable("foo")
=>
(VARIABLE  "foo")
```

## UserDefinedOperator

UserDefinedOperator -> AbstractOperator -> Expression -> Node

```
(USER_FUNCTOR ,name ,(? bool? stateful?) ,(? list? arguments))

```

```
@functor_name(variable("foo"))
(USER_FUNCTOR functor_name ((VARIABLE "foo")))
```

## UserDefinedAggregator

UserDefinedAggregator -> Aggregator
```
(USER_AGGREGATOR ,name INIT ,values )
```

## UnsignedConstant
UnsignedConstant -> NumericConstant -> Expression -> Node
```
(UNSIGNED ,num)
```

## UnpackRecord
UnpackRecord -> TupleOperation -> NestedOperation -> Operation -> Node
``
(UNPACK ,arity ,from ,ops)
``

## Undefined
UndefValue -> Expression -> Node
```
UNDEF
```

## TupleElement
TupleElement -> Expression -> Node

In the following example, the tuple element t0.1 is accessed:
```
IF t0.1 in A
```

```
(TUPLE ,identifier ,element)
```

## True
True -> Condition -> Node
```
TRUE
```

## Swap
Swap -> BinRelationStatement -> Statement -> Node

```
(SWAP ,first ,second)
```

## SubroutineReturn
SubroutineReturn -> Operation -> Node

```
RETURN (t0.0, t0.1)
=>
(RETURN ,exprs ...)
```

## SubroutineArgument
SubroutineArgument -> Expression -> Node

```
 * Arguments are number from zero 0 to n-1
 * where n is the number of arguments of the
 * subroutine.

(ARGUMENT ,number)
```

## StringConstant
StringConstant -> Expression -> Node

```
(STRING ",str")
```

## SignedConstant
SignedConstant -> NumericConstant -> Expression -> Node

```
number(5)
=>
(NUMBER ,num)
```

## Sequence
Sequence -> ListStatement -> Statement -> Node

```
Execute statement one by one from an ordered list of statements.

(,statement ...)
```

## Scan
Scan -> RelationOperation -> TupleOperation -> NestedOperation -> Operation -> Node

```
 * The following example iterates over all tuples
 * in the set A:
 *  QUERY
 *   ...
 *   FOR t0 IN A
 *     ...
 
 =>
 (FOR_IN ,tuple_id ,relation)
```

## RelationSize
RelationSize -> Expression -> Node

```
size(B)
=>
(SIZE ,num)
```

## Relation
Relation -> Node

```
(RELATION ,name 
          (,?(or  (ATTRIBUTE ,name ,type)
                  (AUXILIARY ,name ,type)) ...)
          ,representation)
```

## Query
Query -> Statement -> Node

```
 * Corresponds to the core machinery of semi-naive evaluation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * QUERY
 *   FOR t0 in A
 *     FOR t1 in B
 *       ...
 * END QUERY
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (QUERY ...)
```

## ProvenanceExistenceCheck
ProvenanceExistenceCheck -> AbstractExistenceCheck -> Condition -> Node

```
(PROV ,existencecheck)
```

## AbstractExistenceCheck
AbstractExistenceCheck -> Condition -> Node

```
(EXISTS ,relation (,values ...))
```

## Program
Program -> Node

```
 * A typical example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PROGRAM
 *   DECLARATION
 *     A(x:i:number)
 *   END DECLARATION
 *   BEGIN MAIN
 *     ...
 *   END MAIN
 * END PROGRAM
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

(PROGAM
    (DECLARATION ,relations ...)
    (MAIN ,main)
    )
```


## ParallelScan
ParallelScan -> Scan, AbstractParallel

```
 * An example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *   PARALLEL FOR t0 IN A
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (PARALLEL_FOR ,relation ,tuple_id ,relation_operation)
```

## IndexOperation
IndexOperation -> RelationOperation -> TupleOperation -> NestedOperation -> Operation -> Node
```
     * <expr1> <= Tuple[level, element] <= <expr2>
     * We have that at an index for an attribute, its bounds are <expr1> and <expr2> respectively

    (INDICES ,(or (= (T ,tuple_id ,id) ,pattern)
                  (<= ,pattern?  (T ,tuple_id ,id) ,pattern? ))
             ...)
```

## IndexScan
IndexScan -> IndexOperation -> RelationOperation -> TupleOperation -> NestedOperation -> Operation -> Node

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *	 FOR t1 IN X ON INDEX t1.c = t0.0
 *	 ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (FOR ,tuple_id ,relation ,indices)
```

## ParallelIndexScan
ParallelIndexScan -> IndexScan, AbstractParallel

```
(PARALLEL_FOR ,tuple_id ,relation ,indices)
```

## ParallelIndexIfExists
ParallelIndexIfExists -> IndexScan, AbstractParallel

```
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    PARALLEL IF ∃t1 in A1  ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (PARALLEL_IF_EXISTS ,tuple_id ,relation ,indices ,condition ,operation)
```


## AbstractAggregate
```
(AGGRAGTE_FUNCTION ,function ,expression)
```

## IndexAggregate
IndexAggregate -> IndexOperation, AbstractAggregate

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0=sum t0.1 SEARCH t0 ∈ S ON INDEX t0.0 = number(1)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 =>
 (INDEX_AGGREGATE (= (T ,tuple_id 0) ,aggregate) ,tuple_id ,condition ,indices)
```

## ParallelIndexAggregate
ParallelIndexAggregate -> IndexAggregate, AbstractParallel
```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0=sum t0.1 SEARCH t0 ∈ S ON INDEX t0.0 = number(1)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 =>
 (PARALLEL_INDEX_AGGREGATE (= (T ,tuple_id 0) ,aggregate) ,tuple_id ,condition ,indices)
```


## IfExists
IfExists -> RelationOperation, AbstractIfExists -> TupleOperation -> NestedOperation -> Operation -> Node

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF ∃ t1 IN A WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 =>
 (IF_EXISTS ,tuple_id ,relation ,condition ,operation)
```

## ParallelIfExists

ParallelIfExists -> IfExists, AbstractParallel
```
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    PARIF t1 IN A WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 =>
 (PARALLEL_INDEX_AGGREGATE ,tuple_id ,relation ,condition ,operation)
```

## Aggregate
Aggregate -> RelationOperation, AbstractAggregate

```
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.0 = COUNT FOR ALL t0 IN A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Applies the function COUNT to determine the number
 * of elements in A.

 (AGGREGATE ,tuple_id , agg_func ,tuple_id ,relation ,where_cond ,rel_op)
```


## ParallelAggregate


## Parallel

Parallel -> ListStatement -> Statement -> Node

```
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * PARALLEL
 *   BEGIN DEBUG...
 *     QUERY
 *       ...
 * END PARALLEL
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

(PARALLEL ,stmts ...)
```

## PackRecord
PackRecord -> Expression -> Node

```
(PACK ,args ...)
```

## NestedIntrinsicOperator
NestedIntrinsicOperator -> TupleOperation -> NestedOperation -> Operation -> Node
```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * RANGE(t0.0, t0.1, t0.2) INTO t1
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (INTRINSIC (,args ...) ,tuple_id)
```

## Negation
Negation -> Condition -> Node

```
(NOT t0 IN A)

(NOT ,arg)
```

## MergeExtend
MergeExtend -> BinRelationStatement -> Statement -> Node

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * MERGE EXTEND B WITH A
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (MERGE_EXTEND ,target_relation ,source_relation)
```

## Loop
Loop -> Statement -> Node
```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * LOOP
 *   PARALLEL
 *     ...
 *   END PARALLEL
 * END LOOP
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (LOOP ,statement)
```

## LogTimer
LogTimer -> Statement, AbstractLog

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * START_TIMER "@runtime\;"
 *   BEGIN_STRATUM 0
 *     ...
 *   ...
 * END_TIMER
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

(TIMER ",msg" ,stmt)
```

## LogSize
LogSize -> RelationStatement -> Statement -> Node

```
(LOGSIZE ",msg" ,stmt)
```

## LogRelationTimer

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * START_TIMER ON A "file.dl [8:1-8:8]\;"
 *   ...
 * END_TIMER
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (TIMER_ON ",msg" ,stmt)
```

## IO
IO -> RelationStatement -> Statement -> Node

```
(IO ,relation (,directives ...))
```

## IntrinsicOperator

IntrinsicOperator -> AbstractOperator -> Expression -> Node

```
(INTRINSIC ,args ...)
```

# IntrinsicAggregator

IntrinsicAggregator -> AGGREGATOR

# Insert
Insert  -> Operation -> Node

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     INSERT (t0.a, t0.b, t0.c) INTO @new_X
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (INSERT (,statements ...) ,relation)
```

## IndexIfExists
IndexIfExists -> IndexOperation, AbstractIfExists
```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  QUERY
 *   ...
 *    IF ∃ t1 in A ON INDEX t1.x=10 AND t1.y = 20
 *    WHERE (t1.x, t1.y) NOT IN A
 *      ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
(INDEXED_IF_EXISTS ,tuple_id ,relation ,index ,condition ,operation)
```

## GuardedInsert
GuardedInsert -> Insert -> Operation -> Node

```
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     INSERT (t0.a, t0.b, t0.c) INTO @new_X IF (c1 /\ c2 /\ ..)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Where c1, c2 are existenceCheck.

 (GUARDED_INSERT (,exprs ...) ,relation ,cond)
```

## FloatConstant
FloatConstant -> NumericConstant -> Expression -> Node

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * float(3.3)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (FLOAT ,num)
```

## Filter
Filter -> AbstractConditional

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * IF C1 AND C2
 *  ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (IF ,condition ,operation)
```

## FALSE

## EXIT
EXIT -> Statement -> Node

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * EXIT (A = ∅)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 
 (EXIT ,expr)
```

## ESTIMATEJOINSIZE

```
* For example:
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ESTIMATEJOINSIZE rel A0 = 1, A1
* ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Estimates the size of the join on rel with a unique tuple value for attribute 1,
 * while also having the first attribute with a value of 1

 NOT SUPPORTED
```


## Erase
Erase -> Operation -> Node

```
(ERASE (,expr ...) ,relation)
```

## EmptinessCheck
EmptinessCheck -> Condition -> Node

```
(ISEMPTY ,relation)
```

## DebugInfo

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * BEGIN_DEBUG "gen(1) \nin file /file.dl [7:7-7:10]\;"
 *   ...
 * END_DEBUG
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 
 (DEBUG ",str" stmt)
```

## Constraint
Constraint -> Condition

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * t0.1 = t1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (CONSTRAINT ,op ,lhs ,rhs)
```

## Conjunction
Conjunction -> Condition

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * C1 AND C2 AND C3
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Is a Conjunction, which may have LHS "C1"
 * and RHS "C2 AND C3"

 (AND ,lhs ,rhs)
```

## Clear

```
(CLEAR ,relation)
```


## Call

```
(CALL ,proc)
```

## Break

```
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 in A
 *   FOR t1 in B
 *     IF t0.1 = 4 BREAK
 *     ...
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~

 (IF_BREAK ,cond ,operation)
```

## AutoIncrement
```
(AUTOINC)
```

## Assign

```
(ASSIGN ,var ,value)
```

