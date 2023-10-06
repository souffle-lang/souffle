
#include "FunctorOps.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <cctype>
#include <map>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

namespace souffle {

namespace {
char const* functorOpNameLegacy(FunctorOp op) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::ORD: return "ord";
        case FunctorOp::STRLEN: return "strlen";
        case FunctorOp::NEG:
        case FunctorOp::FNEG: return "-";
        case FunctorOp::BNOT:
        case FunctorOp::UBNOT: return "bnot";
        case FunctorOp::LNOT:
        case FunctorOp::ULNOT: return "lnot";

        case FunctorOp::F2F:
        case FunctorOp::I2F:
        case FunctorOp::S2F:
        case FunctorOp::U2F: return "to_float";

        case FunctorOp::I2I:
        case FunctorOp::F2I:
        case FunctorOp::S2I:
        case FunctorOp::U2I: return "to_number";

        case FunctorOp::S2S:
        case FunctorOp::I2S:
        case FunctorOp::F2S:
        case FunctorOp::U2S: return "to_string";

        case FunctorOp::U2U:
        case FunctorOp::F2U:
        case FunctorOp::I2U:
        case FunctorOp::S2U: return "to_unsigned";

        /** Binary Functor Operators */
        case FunctorOp::ADD:
        case FunctorOp::FADD:
        case FunctorOp::SSADD:
        case FunctorOp::UADD: return "+";
        case FunctorOp::SUB:
        case FunctorOp::USUB:
        case FunctorOp::FSUB: return "-";
        case FunctorOp::MUL:
        case FunctorOp::UMUL:
        case FunctorOp::FMUL: return "*";
        case FunctorOp::DIV:
        case FunctorOp::UDIV:
        case FunctorOp::FDIV: return "/";
        case FunctorOp::EXP:
        case FunctorOp::FEXP:
        case FunctorOp::UEXP: return "^";
        case FunctorOp::MOD:
        case FunctorOp::UMOD: return "%";
        case FunctorOp::BAND:
        case FunctorOp::UBAND: return "band";
        case FunctorOp::BOR:
        case FunctorOp::UBOR: return "bor";
        case FunctorOp::BXOR:
        case FunctorOp::UBXOR: return "bxor";
        case FunctorOp::BSHIFT_L:
        case FunctorOp::UBSHIFT_L: return "bshl";
        case FunctorOp::BSHIFT_R:
        case FunctorOp::UBSHIFT_R: return "bshr";
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::UBSHIFT_R_UNSIGNED: return "bshru";
        case FunctorOp::LAND:
        case FunctorOp::ULAND: return "land";
        case FunctorOp::LOR:
        case FunctorOp::ULOR: return "lor";
        case FunctorOp::LXOR:
        case FunctorOp::ULXOR: return "lxor";
        case FunctorOp::RANGE:
        case FunctorOp::URANGE:
        case FunctorOp::FRANGE: return "range";

        /* N-ary Functor Operators */
        case FunctorOp::MAX:
        case FunctorOp::UMAX:
        case FunctorOp::FMAX:
        case FunctorOp::SMAX: return "max";
        case FunctorOp::MIN:
        case FunctorOp::UMIN:
        case FunctorOp::FMIN:
        case FunctorOp::SMIN: return "min";
        case FunctorOp::CAT: return "cat";

        /** Ternary Functor Operators */
        case FunctorOp::SUBSTR: return "substr";
    }

    UNREACHABLE_BAD_CASE_ANALYSIS
}

char const* functorOpNameSymbol(FunctorOp op) {
    switch (op) {
        /** Unary Functor Operators */
        case FunctorOp::NEG:
        case FunctorOp::FNEG: return FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME;
        case FunctorOp::BNOT:
        case FunctorOp::UBNOT: return "~";
        case FunctorOp::LNOT:
        case FunctorOp::ULNOT: return "!";
        case FunctorOp::EXP:
        case FunctorOp::FEXP:
        case FunctorOp::UEXP: return "**";
        case FunctorOp::BAND:
        case FunctorOp::UBAND: return "&";
        case FunctorOp::BOR:
        case FunctorOp::UBOR: return "|";
        case FunctorOp::BXOR:
        case FunctorOp::UBXOR: return "^";
        case FunctorOp::BSHIFT_L:
        case FunctorOp::UBSHIFT_L: return "<<";
        case FunctorOp::BSHIFT_R:
        case FunctorOp::UBSHIFT_R: return ">>";
        case FunctorOp::BSHIFT_R_UNSIGNED:
        case FunctorOp::UBSHIFT_R_UNSIGNED: return ">>>";
        case FunctorOp::LAND:
        case FunctorOp::ULAND: return "&&";
        case FunctorOp::LOR:
        case FunctorOp::ULOR: return "||";
        case FunctorOp::LXOR:
        case FunctorOp::ULXOR: return "^^";

        default: return functorOpNameLegacy(op);
    }
}

using TAttr = TypeAttribute;
using FOp = FunctorOp;
#define OP_1(op, t0, tDst) \
    { functorOpNameSymbol(FOp::op), {TAttr::t0}, TAttr::tDst, FOp::op }
#define OP_2(op, t0, t1, tDst, multi) \
    { functorOpNameSymbol(FOp::op), {TAttr::t0, TAttr::t1}, TAttr::tDst, FOp::op, false, multi }
#define OP_3(op, t0, t1, t2, tDst, multi) \
    { functorOpNameSymbol(FOp::op), {TAttr::t0, TAttr::t1, TAttr::t2}, TAttr::tDst, FOp::op, false, multi }

#define OP_1_MONO(op, ty) OP_1(op, ty, ty)
#define OP_2_MONO(op, ty, multi) OP_2(op, ty, ty, ty, multi)
#define OP_3_MONO(op, ty, multi) OP_3(op, ty, ty, ty, ty, multi)
#define OP_1_INTEGRAL(op) OP_1_MONO(op, Signed), OP_1_MONO(U##op, Unsigned)
#define OP_2_INTEGRAL_EX(op, multi) OP_2_MONO(op, Signed, multi), OP_2_MONO(U##op, Unsigned, multi)
#define OP_3_INTEGRAL_EX(op, multi) OP_3_MONO(op, Signed, multi), OP_3_MONO(U##op, Unsigned, multi)
#define OP_2_INTEGRAL(op) OP_2_INTEGRAL_EX(op, false)
#define OP_2_NUMERIC(op) OP_2_MONO(F##op, Float, false), OP_2_INTEGRAL(op)
#define OP_2_NUMERIC_MULTI(op) OP_2_MONO(F##op, Float, true), OP_2_INTEGRAL_EX(op, true)
#define OP_3_NUMERIC_MULTI(op) OP_3_MONO(F##op, Float, true), OP_3_INTEGRAL_EX(op, true)
#define VARIADIC(op, ty) \
    { functorOpNameLegacy(FOp::op), {TAttr::ty}, TAttr::ty, FOp::op, true }
#define VARIADIC_ORDERED(op) \
    VARIADIC(op, Signed), VARIADIC(U##op, Unsigned), VARIADIC(F##op, Float), VARIADIC(S##op, Symbol)

const std::vector<IntrinsicFunctorInfo> FUNCTOR_INTRINSICS = {
        {FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME, {TAttr::Signed}, TAttr::Signed, FunctorOp::NEG},
        {FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME, {TAttr::Float}, TAttr::Float, FunctorOp::FNEG},

        OP_1(F2F, Float, Float),
        OP_1(F2I, Float, Signed),
        OP_1(F2S, Float, Symbol),
        OP_1(F2U, Float, Unsigned),

        OP_1(I2I, Signed, Signed),
        OP_1(I2F, Signed, Float),
        OP_1(I2S, Signed, Symbol),
        OP_1(I2U, Signed, Unsigned),

        OP_1(S2S, Symbol, Symbol),
        OP_1(S2F, Symbol, Float),
        OP_1(S2I, Symbol, Signed),
        OP_1(S2U, Symbol, Unsigned),

        OP_1(U2U, Unsigned, Unsigned),
        OP_1(U2F, Unsigned, Float),
        OP_1(U2I, Unsigned, Signed),
        OP_1(U2S, Unsigned, Symbol),

        OP_2_NUMERIC(ADD),
        OP_2_NUMERIC(SUB),
        OP_2_NUMERIC(MUL),
        OP_2_NUMERIC(DIV),
        OP_2_INTEGRAL(MOD),
        OP_2_NUMERIC(EXP),

        OP_2_INTEGRAL(LAND),
        OP_1_INTEGRAL(LNOT),
        OP_2_INTEGRAL(LOR),
        OP_2_INTEGRAL(LXOR),

        OP_2_INTEGRAL(BAND),
        OP_1_INTEGRAL(BNOT),
        OP_2_INTEGRAL(BOR),
        OP_2_INTEGRAL(BSHIFT_L),
        OP_2_INTEGRAL(BSHIFT_R),
        OP_2_INTEGRAL(BSHIFT_R_UNSIGNED),
        OP_2_INTEGRAL(BXOR),

        OP_2_NUMERIC_MULTI(RANGE),
        OP_3_NUMERIC_MULTI(RANGE),

        VARIADIC_ORDERED(MAX),
        VARIADIC_ORDERED(MIN),

        // `ord` is a weird functor that exposes the internal raw value of any type.
        OP_1(ORD, Signed, Signed),
        OP_1(ORD, Unsigned, Signed),
        OP_1(ORD, Float, Signed),
        OP_1(ORD, Symbol, Signed),
        OP_1(ORD, Record, Signed),
        OP_1(ORD, ADT, Signed),

        VARIADIC(CAT, Symbol),
        OP_1(STRLEN, Symbol, Signed),
        OP_3(SUBSTR, Symbol, Signed, Signed, Symbol, false),

        {functorOpNameSymbol(FOp::SSADD), {TAttr::Symbol, TAttr::Symbol}, TAttr::Symbol, FOp::SSADD, false,
                false},
};

template <typename F>
IntrinsicFunctors pickFunctors(F&& f) {
    IntrinsicFunctors xs;
    for (auto&& x : FUNCTOR_INTRINSICS)
        if (f(x)) xs.push_back(x);
    return xs;
}
}  // namespace

IntrinsicFunctors functorBuiltIn(FunctorOp op) {
    return pickFunctors([&](auto&& x) { return x.op == op; });
}

IntrinsicFunctors functorBuiltIn(std::string_view symbol) {
    return pickFunctors([&](auto&& x) { return x.symbol == symbol; });
}

IntrinsicFunctors functorBuiltIn(std::string_view symbol, const std::vector<TypeAttribute>& params) {
    return pickFunctors([&](auto&& x) {
        auto paramsOk =
                x.variadic ? all_of(params, [&](auto t) { return t == x.params[0]; }) : x.params == params;
        return x.symbol == symbol && paramsOk;
    });
}

bool isValidFunctorOpArity(std::string_view symbol, std::size_t arity) {
    return any_of(FUNCTOR_INTRINSICS, [&](auto&& x) {
        auto arityOk = x.params.size() == arity || x.variadic;
        return x.symbol == symbol && arityOk;
    });
}

bool isFunctorMultiResult(FunctorOp op) {
    auto&& xs = functorBuiltIn(op);
    return xs.front().get().multipleResults;
}

bool isInfixFunctorOp(std::string_view symbol) {
    assert(!symbol.empty() && "no functors have an empty name");
    // arithmetic/bitwise/logical negation are prefix ops
    if (symbol == FUNCTOR_INTRINSIC_PREFIX_NEGATE_NAME) return false;
    if (symbol == "!") return false;
    if (symbol == "~") return false;
    auto alphaUnderscore = symbol == "_" || isalpha(symbol.at(0));
    return !alphaUnderscore;
}

bool isInfixFunctorOp(const FunctorOp op) {
    auto&& xs = functorBuiltIn(op);
    return isInfixFunctorOp(xs.front().get().symbol);
}

FunctorOp getMinOp(const std::string& type) {
    switch (type[0]) {
        case 'f': return FunctorOp::FMIN;
        case 'u': return FunctorOp::UMIN;
        case 'i': return FunctorOp::MIN;
        default: return FunctorOp::MIN;
    }
}

FunctorOp getMaxOp(const std::string& type) {
    switch (type[0]) {
        case 'f': return FunctorOp::FMAX;
        case 'u': return FunctorOp::UMAX;
        case 'i': return FunctorOp::MAX;
        default: return FunctorOp::MAX;
    }
}

std::ostream& operator<<(std::ostream& os, FunctorOp op) {
    return os << functorOpNameLegacy(op);
}

// must defined after `FUNCTOR_INTRINSICS` to ensure correct ctor ordering.
#ifndef NDEBUG
namespace {
struct FUNCTOR_INTRINSIC_SANCHECKER {
    FUNCTOR_INTRINSIC_SANCHECKER() {
        std::map<FunctorOp, IntrinsicFunctors> byOp;
        for (auto&& x : FUNCTOR_INTRINSICS) {
            byOp[x.op].push_back(x);
            assert((!x.variadic || x.params.size() == 1) && "variadics must have a single parameter");
        }

        for (auto&& [_, xs] : byOp) {
            auto&& multiResult = xs.front().get().multipleResults;
            auto&& symbol = xs.front().get().symbol;
            for (auto&& x : xs) {
                // this can be relaxed but would require removing the `isFunctorMultiResult : FunctorOp`
                // overload
                assert(x.get().multipleResults == multiResult &&
                        "all overloads for op must have same `multipleResults`");
                // this can be relaxed but would require removing the `isInfixFunctorOp : FunctorOp` overload
                assert(x.get().symbol == symbol && "all overloads for op must have same `symbol`");
            }
        }
    }
} FUNCTOR_INTRINSIC_SANCHECKER;
}  // namespace
#endif

}  // namespace souffle
