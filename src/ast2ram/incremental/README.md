## Incremental Evaluation

This directory contains our implementation for _incremental view maintenance_,
or as we refer to it, _incremental evaluation_. Incremental evaluation is a
general approach of allowing computation results to be _updated_. In the context
of Soufflé, incremental evaluation allows for a user to insert or delete input
tuples. For example, the user could type `insert R(1,2)` or `remove R(2,3)` to
respectively insert or remove the corresponding input tuple. Algorithmically,
our approach is based on the ideas in [Differential
Dataflow](https://docs.rs/crate/differential-dataflow/0.10.0/source/differentialdataflow.pdf). 

Currently, incremental evaluation is experimental and only supports simple
positive Datalog (i.e., negations, aggregations, subsumption, etc. are not yet
supported). Moreover, optimizations are not yet implemented, so performance may
be lacking.

#### Usage

To use incremental evaluation, add the `-i` or `--incremental` flag to your
invocation of Soufflé. For example,

```
souffle --incremental test.dl
```
will run `test.dl` in incremental mode. In this mode, the system will show a
command line interface after the initial Datalog evaluation. The command line
supports three commands:

- `insert R(x,y)` inserts a tuple `R(x,y)` into the database.
- `remove R(x,y)` removes a tuple `R(x,y)` from the database. It is the user's
  responsibility to ensure that `R` is an input relation, for both inserts and
  removals.
- `commit` runs the incremental evaluation algorithm (starting a new _epoch_),
  to update the result of the Datalog computation after inserting or remove a
  number of input tuples.

#### Encoding

To allow for incremental evaluation, the Datalog program must be instrumented
with a notion of _incremental state_. In particular, for a relation `R(x,y)`, we
add two attributes: `R(x,y,@iteration,@count)`.

The first of these attributes, `@iteration`, encodes which iteration of a
recursive stratum the tuple is computed in. The second attribute, `@count`,
encodes the number of different derivations for the tuple in that particular
iteration.

For example, let's pretend we are in iteration 3 and the following two rule
evaluations are valid:

```
R(1,2) :- R1(1,3), R2(3,2).
R(1,2) :- R1(1,5), R2(5,2).
```
Then, the relation `R` will contain the tuple `R(1,2,3,2)`, signifying that
`R(1,2)` is derived in iteration 3 in 2 different ways.

Note that under this encoding, the same tuple can exist in multiple iterations.
For example, the following is a valid state that a relation could be in, despite
all tuples referring to the same base tuple `R(1,2)`:

```
R(1,2,3,2).
R(1,2,5,1).
R(1,2,8,1).
```

#### Rule Evaluation

To perform incremental evaluation, our approach has two phases: a _bootstrap_
and an _update_. The bootstrap phase is for the initial evaluation, and it
resembles a standard semi-naive evaluation, just with keeping track the of
iteration and count attributes. On the other hand, the update phase performs an
incremental update, and is much more involved.

The first consideration is that we need extra auxiliary relations. For a
relation `R`, incremental update requires `prev_R`, storing tuples from the
previous epoch, and `diff_minus_R` and `diff_plus_R`, storing tuples deleted and
inserted in the current epoch respectively. (There are also a number of other
auxiliary relations explained later, but these are the main ones.)

Then, the Datalog rule evaluation is instrumented so that it correctly computes
the updated tuples. For example, consider the simple rule:

```
R :- R1, R2, R3.
```

To compute the updates, the following set of rules is generated:

```
diff_minus_R :- diff_minus_R1, prev_R2, prev_R3.
diff_minus_R :- prev_R1, diff_minus_R2, prev_R3.
diff_minus_R :- prev_R1, prev_R2, diff_minus_R3.
diff_plus_R :- diff_plus_R1, R2, R3.
diff_plus_R :- R1, diff_plus_R2, R3.
diff_plus_R :- R1, R2, diff_plus_R3.
```

There are also a number of extra conditions associated with each version of the
rule, ensuring that the counts are accurate and we are not double-counting when
inserting or removing tuples. Of course, if this is a recursive rule, then we
also need to create `n` different delta versions for semi-naive evaluation, thus
potentially creating `O(n^2)` different rule versions in total. 

#### Auxiliary Relations in More Detail

As mentioned above, for each relation `R` we have auxiliary relations `prev_R`,
`diff_minus_R`, and `diff_plus_R`. However, we also have a number of extra
auxiliary relations, detailed below.

- `actual_diff_minus_R`: while `diff_minus_R` stores tuples deleted in the
  current epoch (i.e., tuples with decremented count), those tuples may not be
  _actually_ deleted. For example, consider the above example, where

  ```
  R(1,2) :- R1(1,3), R2(3,2).
  R(1,2) :- R2(1,5), R2(5,2).
  ```
  are both valid rule instantiations. Then, if we remove `R1(1,3)`, then the
  tuple `R(1,2)` is actually still derivable through the second rule
  instantiation. Therefore, `R(1,2)` is not _actually_ deleted.

  Under this circumstance, we will have `diff_minus_R(1,2,3,-1)` (`-1` signifies
  that one derivation is deleted), but `actual_diff_minus_R` will not contain
  any tuples.
- `actual_diff_plus_R`: the same concept as `actual_diff_minus_R`, but for
  inserted tuples. In the case where a tuple already exists, but a different
  derivation is computed from the incremental update, then that tuple will exist
  in `diff_plus_R`, but not `actual_diff_plus_R`.
- `updated_diff_minus_R`: this relation ensures that the state of
  `actual_diff_minus_R` is correct during a recursive stratum. In a recursive
  stratum, a tuple may be deleted in an early iteration, but still exists in a
  later iteration.

  For example, imagine that tuple `R(1,2)` is deleted (in `actual_diff_minus_R`)
  in iteration 2, but actually, it still exists in iteration 5. Then, while the
  presence of `actual_diff_minus_R(1,2,2,-1)` is correct in iterations 2-4, once
  iteration 5 is reached, then `R(1,2)` is no longer _actually_ deleted. In this
  case, we would add a tuple `updated_diff_minus_R(1,2,5,1)`, indicating that
  `R(1,2)` was previously _actually_ deleted, but its state is updated in
  iteration 5.

  In usage, `updated_diff_minus_R` is used to 'reverse' deletions that occurred
  previously but are no longer valid due to the tuple still existing in the
  later iteration.
- `updated_diff_plus_R`: the same concept as `updated_diff_minus_R`.

#### TODOs for Incremental Evaluation:

In its current experimental stage, incremental evaluation works only for simple
positive Datalog programs, including recursion. In the future, the following
additions and optimizations should be made:

1. Implement semantics and transformations for `IndexIfNotExists`, enabling
   indexed lookups instead of scanning as `IfNotExists` does. This should be
   reasonably straightforward, copying what happens for `IfExists` and
   `IndexIfExists`.
2. Implement incremental update rules for negations. For example, consider the
   rule:

   ```
   R :- R1, !R2.
   ```
   In this case, the negated literals should also provide semantics for
   insertion/deletion of tuples. This rule with negation should be translated
   into:

   ```
   diff_plus_R :- diff_plus_R1, !R2.
   diff_plus_R :- R1, diff_minus_R2, !R2.
   diff_minus_R :- diff_minus_R1, !prev_R2.
   diff_minus_R :- R1, diff_plus_R2, !prev_R2.
   ```
3. Implement a basic auto scheduler. Using the naive schedules which may work
   well for a non-incremental evaluation will likely be terribly slow for
   incremental evaluation. This is because all the different rule versions have
   different performance characteristics, and using the same schedule across
   them is not a great idea.

   For example, consider the following rule versions:

   ```
   R :- diff_plus_R1, R2.
   R :- R1, diff_plus_R2.
   ```
   In this case, the second rule version may be slow, since we would usually
   assume that `diff_plus` is a lot smaller than the full relation. In this
   case, it may be advantageous to schedule the rule as:

   ```
   R :- diff_plus_R2, R1.
   ```
   Of course, if this scheduling heuristic is adopted, we should also consider
   scheduling patterns such as cross-products and common attributes. This would
   therefore be a little more sophisticated than simply putting the `diff_`
   version of the relation at the front.
4. Potentially implement semantics for aggregation, subsumption, etc., however,
   this would likely be novel research.
