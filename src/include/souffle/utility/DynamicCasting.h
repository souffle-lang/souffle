
/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file DynamicCasting.h
 *
 * Common utilities for dynamic casting.
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/Types.h"
#include <cassert>
#include <type_traits>

namespace souffle {

/**
 * This class is used to tell as<> that cross-casting is allowed.
 * I use a named type rather than just a bool to make the code stand out.
 */
class AllowCrossCast {};

/**
 * Helpers for `dynamic_cast`ing without having to specify redundant type qualifiers.
 * e.g. `as<AstLiteral>(p)` instead of `as<AstLiteral>(p)`.
 */
template <typename B, typename CastType = void, typename A>
auto as(A* x) {
    if constexpr (!std::is_same_v<CastType, AllowCrossCast> &&
                  !std::is_base_of_v<std::remove_const_t<B>, std::remove_const_t<A>>) {
        static_assert(std::is_base_of_v<std::remove_const_t<A>, std::remove_const_t<B>>,
                "`as<B, A>` does not allow cross-type dyn casts. "
                "(i.e. `as<B, A>` where `B <: A` is not true.) "
                "Such a cast is likely a mistake or typo.");
    }
    return dynamic_cast<copy_const<A, B>*>(x);
}

template <typename B, typename CastType = void, typename A,
        typename = std::enable_if_t<std::is_same_v<CastType, AllowCrossCast> || std::is_base_of_v<A, B>>,
        typename = std::enable_if_t<std::is_class_v<A> && !is_pointer_like<A>>>
auto as(A& x) {
    return as<B, CastType>(&x);
}

template <typename B, typename CastType = void, typename A, typename = std::enable_if_t<is_pointer_like<A>>>
auto as(const A& x) {
    return as<B, CastType>(x.get());
}

template <typename B, typename CastType = void, typename A>
auto as(const std::reference_wrapper<A>& x) {
    return as<B, CastType>(x.get());
}

/**
 * Down-casts and checks the cast has succeeded
 */
template <typename B, typename CastType = void, typename A>
auto& asAssert(A&& a) {
    auto* cast = as<B, CastType>(std::forward<A>(a));
    assert(cast && "Invalid cast");
    return *cast;
}

template <typename B, typename CastType = void, typename A>
Own<B> UNSAFE_cast(Own<A> x) {
    if constexpr (std::is_assignable_v<Own<B>, Own<A>>) {
        return x;
    } else {
        if (!x) return {};

        auto y = Own<B>(as<B, CastType>(x));
        assert(y && "incorrect typed return");
        x.release();  // release after assert so dbgr can report `x` if it fails
        return y;
    }
}

/**
 * Checks if the object of type Source can be casted to type Destination.
 */
template <typename B, typename CastType = void, typename A>
// [[deprecated("Use `as` and implicit boolean conversion instead.")]]
bool isA(A&& src) {
    return as<B, CastType>(std::forward<A>(src));
}

}  // namespace souffle
