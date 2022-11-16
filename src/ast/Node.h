/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Node.h
 *
 * Defines the AST abstract node class
 *
 ***********************************************************************/

#pragma once

#include "Annotation.h"
#include "QualifiedName.h"
#include "TokenTree.h"
#include "parser/SrcLocation.h"
#include "souffle/utility/Iteration.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/Types.h"
#include "souffle/utility/VisitorFwd.h"

#include <functional>
#include <iosfwd>
#include <list>
#include <string>
#include <vector>

namespace souffle::ast {

class Node;
using NodeMapper = souffle::detail::NodeMapper<Node>;

namespace detail {
// Seems the gcc in Jenkins is not happy with the inline lambdas
struct RefCaster {
    auto operator()(Node const* node) const -> Node const& {
        return *node;
    }
};

struct ConstCaster {
    auto operator()(Node const* node) const -> Node& {
        return *const_cast<Node*>(node);
    }
};
}  // namespace detail

/**
 *  @class Node
 *  @brief Abstract class for syntactic elements in an input program.
 */
class Node {
public:
    /// LLVM-style RTTI
    ///
    /// Each class under the ast::Node hierarchy must appear here and must implement
    /// `static bool classof(const Node*)`.
    ///
    /// When class T is final, we must provide a single enum:
    ///
    ///   ...
    ///   NK_T,
    ///   ...
    ///
    /// When class T is non-final, we must provide enums like this:
    ///
    ///   NK_T,
    ///     NK_Child1,
    ///     ...
    ///     NK_ChildN,
    ///   NK_LastT
    ///
    ///
    // clang-format off
    enum NodeKind {
        NK_NONE,
        NK_Argument,
            NK_Constant,
                NK_NilConstant,
                NK_NumericConstant,
                NK_StringConstant,
            NK_LastConstant,

            NK_Counter,
            NK_ExecutionOrder,
            NK_ExecutionPlan,
            NK_IterationCounter,

            NK_Aggregator,
                NK_IntrinsicAggregator,
                NK_UserDefinedAggregator,
            NK_LastAggregator,

            NK_Term,
                NK_BranchInit,
                NK_FirstFunctor,
                    NK_IntrinsicFunctor,
                    NK_UserDefinedFunctor,
                NK_LastFunctor,
                NK_RecordInit,
            NK_LastTerm,

            NK_UnnamedVariable,
            NK_Variable,
            NK_TypeCast,
        NK_LastArgument,

        NK_Attribute,
        NK_BranchType,
        NK_Clause,
          NK_SubsumptiveClause,
        NK_LastClause,
        NK_Component,
        NK_ComponentInit,
        NK_ComponentType,
        NK_Directive,
        NK_FunctorDeclaration,
        NK_Lattice,

        NK_Literal,
            NK_Atom,

            NK_Constraint,
                NK_BinaryConstraint,
                NK_BooleanConstraint,
                NK_FunctionalConstraint,
            NK_LastConstraint,

            NK_Negation,
        NK_LastLiteral,

        NK_Pragma,
        NK_Program,
        NK_Relation,

        NK_Type,
            NK_AlgebraicDataType,
            NK_AliasType,
            NK_RecordType,
            NK_SubsetType,
            NK_UnionType,
        NK_LastType,
    };
    // clang-format on
private:
    const NodeKind Kind;

public:
    explicit Node(NodeKind K, SrcLocation loc = {});
    virtual ~Node() = default;
    // Make sure we don't accidentally copy/slice
    Node(Node const&) = delete;
    Node& operator=(Node const&) = delete;

    NodeKind getKind() const;

    /** Return source location of the Node */
    const SrcLocation& getSrcLoc() const;

    /** Set source location for the Node */
    void setSrcLoc(SrcLocation l);

    /** Return source location of the syntactic element */
    std::string extloc() const;

    /** Equivalence check for two AST nodes
     *
     * Annotations are ignored.
     */
    bool operator==(const Node& other) const;

    /** Inequality check for two AST nodes
     *
     * Annotations are ignored.
     */
    bool operator!=(const Node& other) const;

    /** Create a clone (i.e. deep copy) of this node */
    Own<Node> cloneImpl() const;

    /** Apply the mapper to all child nodes */
    virtual void apply(const NodeMapper& /* mapper */);

    using NodeVec = std::vector<Node const*>;  // std::reference_wrapper<Node const>>;

    using ConstChildNodes = OwningTransformRange<NodeVec, detail::RefCaster>;
    /** Obtain a list of all embedded AST child nodes */
    ConstChildNodes getChildNodes() const;

    /*
     * Using the ConstCastRange saves the user from having to write
     * getChildNodes() and getChildNodes() const
     */
    using ChildNodes = OwningTransformRange<NodeVec, detail::ConstCaster>;
    ChildNodes getChildNodes();

    /** Add an annotation to this object. */
    void addAnnotation(Annotation);

    /** Add a list of annotations to this object. */
    void addAnnotations(AnnotationList);

    void prependAnnotation(Annotation);

    void prependAnnotations(AnnotationList);

    /** Reset annotations using annotations from the other object */
    void setAnnotationsFrom(const Node& other);

    /** Reset annotations using the given list */
    void setAnnotations(AnnotationList);

    /** Reset annotations using the given list */
    void setAnnotations(std::unique_ptr<AnnotationList>);

    /// Replace this node annotations with annotations stollen from other node.
    void stealAnnotationsFrom(Node& other);

    /** Enumerate annotations attached to this object */
    void eachAnnotation(const std::function<void(const Annotation&)>& f) const;

    /** Enumerate annotations attached to this object that have the given annotation */
    void eachAnnotation(const QualifiedName& label, const std::function<void(const TokenStream&)>& f) const;

    /** Enumerate annotations attached to this object that have the given annotation */
    void eachAnnotation(const QualifiedName& label, const std::function<void(const Annotation&)>& f) const;

    /** Enumerate annotations attached to this object */
    void eachAnnotation(const std::function<void(const QualifiedName&, const TokenStream&)>& f) const;

    /** Return the number of attached annotations that have the given label */
    std::size_t countAnnotations(const QualifiedName& label) const;

    /** Return a pointer to the list of annotations or nullptr */
    AnnotationList* getAnnotations();

    /** Return a pointer to the list of annotations or nullptr */
    const AnnotationList* getAnnotations() const;

    /**
     * Return the first attached annotation that have the given label.
     *
     * Throws an exception if no such annotation exists.
     */
    const Annotation& getAnnotation(const QualifiedName& label) const;

    /**
     * Print annotations, except doc comments
     *
     * Inner annotations are printed like outer annotations
     */
    void printAnnotations(std::ostream& os) const;

    /** Print node onto an output stream */
    friend std::ostream& operator<<(std::ostream& out, const Node& node);

protected:
    /** Output to a given output stream */
    virtual void print(std::ostream& os) const = 0;

    virtual NodeVec getChildren() const;

private:
    /** Abstract equality check for two AST nodes */
    virtual bool equal(const Node& /* other */) const = 0;

    virtual Node* cloning() const = 0;

    /** Ensure that this node has an annotations list and return it by reference. */
    AnnotationList& ensureAnnotations();

private:
    /** Source location of a syntactic element */
    SrcLocation location;

    /** The attached annotations */
    std::unique_ptr<AnnotationList> annotations;
};

}  // namespace souffle::ast

SOUFFLE_DECLARE_VISITABLE_ROOT_TYPE(::souffle::ast::Node)
