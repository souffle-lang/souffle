/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file visitor_test.cpp
 *
 * Test cases for AST visiting utilities.
 *
 ***********************************************************************/

#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/NodeMapper.h"
#include "souffle/utility/NodeMapperFwd.h"
#include "souffle/utility/Visitor.h"
#include "souffle/utility/VisitorFwd.h"
#include "tests/test.h"
#include <initializer_list>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

using namespace souffle;
using namespace std;

namespace {

// template <typename... Operands>
// static VecOwn<int> asVec(Operands... ops) {
//     Own<int> ary[] = {move(ops)...};
//     VecOwn<int> xs;
//     for (auto&& x : ary) {
//         xs.push_back(move(x));
//     }
//     return xs;
// }

template <typename A, typename... XS>
auto asVec(XS&&... xs) {
    vector<A> v;
    (v.push_back(forward<XS>(xs)), ...);
    return v;
}

struct Node {
    virtual ~Node() = default;

    VecOwn<Node> kids;

    template <typename... A>
    Node(A&&... xs) : kids(asVec<Own<Node>>(forward<A>(xs)...)) {}

    virtual vector<Node const*> getChildNodes() const {
        return toPtrVector<Node const>(kids);
    }

    virtual vector<Node*> getChildNodes() {
        return toPtrVector(kids);
    }

    virtual void apply(const detail::NodeMapper<Node>& m);

    bool operator==(Node const& rhs) const {
        if (typeid(*this) != typeid(rhs)) return false;
        if (kids.size() != rhs.kids.size()) return false;

        for (size_t i = 0; i < kids.size(); ++i) {
            assert(kids[i] && rhs.kids[i] && "child ptrs must not be null");
            if (*kids[i] != *rhs.kids[i]) return false;
        }

        return true;
    }

    bool operator!=(Node const& rhs) const {
        return !(*this == rhs);
    }

    virtual char const* kind() const {
        return "Node";
    }

    friend ostream& operator<<(ostream& os, Node const& x) {
        return os << x.kind() << "{" << join(x.kids, ",") << "}";
    }
};

}  // namespace

SOUFFLE_DECLARE_VISITABLE_ROOT_TYPE(Node)

namespace {
static_assert(detail::is_visitable_node<Node>);

void Node::apply(const detail::NodeMapper<Node>& m) {
    mapAll(kids, m);
}

struct NFoo : Node {
    using Node::Node;
    char const* kind() const override {
        return "NFoo";
    }
};

struct NBar : Node {
    using Node::Node;
    char const* kind() const override {
        return "NBar";
    }
};

template <typename R = void, typename NodeType = Node const, typename... Params>
struct VisitorImpl : souffle::detail::VisitorBase<R, NodeType, Params...> {
    using Base = souffle::detail::VisitorBase<R, NodeType, Params...>;
    using Base::visit_;

    R dispatch(NodeType& node, Params const&... args) override {
        SOUFFLE_VISITOR_FORWARD(NFoo);
        SOUFFLE_VISITOR_FORWARD(NBar);

        // did not work ...
        fatal("unsupported type: %s", typeid(node).name());
    }

    // -- types --
    SOUFFLE_VISITOR_LINK(NFoo, Node);
    SOUFFLE_VISITOR_LINK(NBar, Node);
};

}  // namespace

SOUFFLE_VISITOR_DEFINE_PARTIAL_SPECIALISATION(Node, VisitorImpl);

template <typename A, typename B>
void TEST_INSTANTIATION_VISIT() {
    A x;
    visit(x, [&](B const&) {});
    visitExists(x, [&](B const&) { return false; });
    visitForAll(x, [&](B const&) { return false; });
    visitFrontier(x, [&](B const&) { return false; });
}

template <typename A, typename B>
void TEST_INSTANTIATION_MAPPER() {
    A x;
    mapPost(x, [&](Own<B> x) { return x; });
    mapFrontier(x, [&](Own<B> x) { return pair{move(x), false}; });
}

// Check that visitor function instantiates successfully.
// Don't verify semantics/behaviour, just check that they instantiate.
// Do this with trivial no-op functions b/c the type errors are horrific when things go wrong.
[[maybe_unused]] void TEST_INSTANTIATION() {
    // basic
    TEST_INSTANTIATION_VISIT<Node, Node>();
    TEST_INSTANTIATION_VISIT<Node, Node const>();  // okay to view mutable as const
    TEST_INSTANTIATION_VISIT<Node const, Node const>();

    // implicit select/filter by node subtype
    TEST_INSTANTIATION_VISIT<Node, NFoo>();
    TEST_INSTANTIATION_VISIT<Node, NFoo const>();  // okay to view mutable as const
    TEST_INSTANTIATION_VISIT<Node const, NFoo const>();

    // verify implicit traversal of `fmap`-able datastructs
    TEST_INSTANTIATION_VISIT<Own<Node>, NFoo>();
    TEST_INSTANTIATION_VISIT<Own<Node> const, NFoo const>();
    TEST_INSTANTIATION_VISIT<VecOwn<Node>, NFoo>();
    TEST_INSTANTIATION_VISIT<VecOwn<Node> const, NFoo const>();

    TEST_INSTANTIATION_MAPPER<Own<Node>, Node>();
    TEST_INSTANTIATION_MAPPER<Own<Node>, NFoo>();
}

TEST(Visitor, visitCount) {
    VecOwn<Node> roots;
    roots.push_back(
            mk<Node>(mk<NFoo>(mk<Node>(mk<NFoo>(), mk<NBar>(mk<Node>())), mk<NFoo>(), mk<NFoo>(), mk<Node>()),
                    mk<NBar>()));

    size_t total = 0;
    size_t foos = 0;
    size_t bars = 0;
    visit(roots, [&](Node&) { total += 1; });
    visit(roots, [&](NFoo&) { foos += 1; });
    visit(roots, [&](NBar&) { bars += 1; });

    EXPECT_EQ(10, total);
    EXPECT_EQ(4, foos);
    EXPECT_EQ(2, bars);
}

TEST(NodeMapper, mapPrePost_BarToFoo) {
    auto mkTree = []() {
        return mk<Node>(
                mk<NFoo>(mk<Node>(mk<NFoo>(), mk<NBar>(mk<Node>())), mk<NFoo>(), mk<NFoo>(), mk<Node>()),
                mk<NBar>());
    };

    auto expected =
            mk<Node>(mk<NFoo>(mk<Node>(mk<NFoo>(), mk<NFoo>(mk<Node>())), mk<NFoo>(), mk<NFoo>(), mk<Node>()),
                    mk<NFoo>());

    auto go = [](Own<NBar> n) {
        auto x = mk<NFoo>();
        x->kids = move(n->kids);
        return x;
    };
    EXPECT_EQ(*expected, *mapPre(mkTree(), go));
    EXPECT_EQ(*expected, *mapPost(mkTree(), go));
}

TEST(NodeMapper, mapFrontier_FooToBar) {
    auto mkTree = []() {
        return mk<Node>(
                mk<NFoo>(mk<Node>(mk<NFoo>(), mk<NBar>(mk<Node>())), mk<NFoo>(), mk<NFoo>(), mk<Node>()),
                mk<NBar>());
    };

    auto expected =
            mk<Node>(mk<NBar>(mk<Node>(mk<NFoo>(), mk<NBar>(mk<Node>())), mk<NFoo>(), mk<NFoo>(), mk<Node>()),
                    mk<NBar>());

    auto go = [](Own<NFoo> n) {
        auto x = mk<NBar>();
        x->kids = move(n->kids);
        return pair{move(x), true};
    };
    auto&& [produced, n] = mapFrontier(mkTree(), go);

    EXPECT_EQ(n, 1);
    EXPECT_EQ(*expected, *produced);
}
