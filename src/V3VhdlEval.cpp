// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL static expression evaluation
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2026-2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
//
// Folds the static expressions of clause 9.4 to values.
//
// Two rules shape the code. Only a *predefined* operator can be folded, since
// a user-defined one is a function call and is folded by running its body
// rather than by knowing what its symbol means. And every fold is guarded
// against overflow, because a VHDL source may legally contain
// integer_type'high + 1 in a context where the expression is never evaluated,
// and folding must not trap on it.
//
// A call to a pure function whose actuals are static is itself static
// (9.4.3 i), so the body is interpreted here over the values the call passed.
// That is not an optimisation: an array bound may be written as a call to a
// helper declared beside it, and until the call folds the bound is not a
// number and the object cannot be given a width.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlEval.h"

#include "V3Global.h"
#include "V3VhdlStd.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <string>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################

namespace {

// The value of an enumeration literal is its position, so BOOLEAN and BIT are
// both 0 for false/'0' and 1 for true/'1'
constexpr int64_t VAL_FALSE = 0;
constexpr int64_t VAL_TRUE = 1;

// Checked integer arithmetic. VHDL lets a source name a value it never
// evaluates, so overflow means "does not fold", never a trap.
bool addOvf(int64_t a, int64_t b, int64_t& out) {
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return false;
    out = a + b;
    return true;
}
bool subOvf(int64_t a, int64_t b, int64_t& out) {
    if ((b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b)) return false;
    out = a - b;
    return true;
}
bool mulOvf(int64_t a, int64_t b, int64_t& out) {
    if (a == 0 || b == 0) {
        out = 0;
        return true;
    }
    if (a == -1 && b == INT64_MIN) return false;
    if (b == -1 && a == INT64_MIN) return false;
    if (a > 0 ? (b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a)
              : (b > 0 ? a < INT64_MIN / b : a < INT64_MAX / b)) {
        return false;
    }
    out = a * b;
    return true;
}
// The position number of a physical literal is the largest integer that is
// not greater than the product of its abstract literal and the unit
// (5.2.4.1). Working from the literal's decimal digits rather than from the
// double it parsed to keeps "8.2 ms" and "8200 us" the same value. False if
// the text is not a plain decimal literal, or the product does not fit.
bool scaleDecimal(const std::string& text, int64_t unit, int64_t& out) {
    if (unit <= 0) return false;
    unsigned __int128 mant = 0;
    int fracDigits = 0;
    int exp10 = 0;
    bool anyDigit = false;
    bool inFrac = false;
    size_t i = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '_') continue;
        if (c == '.') {
            if (inFrac) return false;
            inFrac = true;
            continue;
        }
        if (c == 'e' || c == 'E') break;
        if (c < '0' || c > '9') return false;  // A based literal, or worse
        mant = mant * 10 + static_cast<unsigned>(c - '0');
        if (mant > (static_cast<unsigned __int128>(1) << 60)) return false;
        if (inFrac) ++fracDigits;
        anyDigit = true;
    }
    if (!anyDigit) return false;
    if (i < text.size()) {  // An exponent, which for a real literal may be signed
        ++i;
        bool neg = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) neg = text[i++] == '-';
        bool anyExp = false;
        for (; i < text.size(); ++i) {
            if (text[i] == '_') continue;
            if (text[i] < '0' || text[i] > '9') return false;
            exp10 = exp10 * 10 + (text[i] - '0');
            if (exp10 > 4096) return false;
            anyExp = true;
        }
        if (!anyExp) return false;
        if (neg) exp10 = -exp10;
    }
    exp10 -= fracDigits;
    unsigned __int128 acc = mant * static_cast<unsigned __int128>(unit);
    if (exp10 >= 0) {
        for (int k = 0; k < exp10; ++k) {
            acc *= 10;
            if (acc > static_cast<unsigned __int128>(INT64_MAX)) return false;
        }
    } else {
        for (int k = 0; k < -exp10; ++k) acc /= 10;  // Floor, the values being positive
    }
    if (acc > static_cast<unsigned __int128>(INT64_MAX)) return false;
    out = static_cast<int64_t>(acc);
    return true;
}

bool powOvf(int64_t base, int64_t exp, int64_t& out) {
    if (exp < 0) return false;
    int64_t acc = 1;
    for (int64_t i = 0; i < exp; ++i) {
        if (!mulOvf(acc, base, acc)) return false;
    }
    out = acc;
    return true;
}

// VHDL's rem takes the sign of its left operand and mod the sign of its
// right (9.2.7), which C++ gives directly only for rem
int64_t vhdlRem(int64_t a, int64_t b) { return a % b; }
int64_t vhdlMod(int64_t a, int64_t b) {
    const int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) return r + b;
    return r;
}

VhdlValue mkBool(bool v) { return VhdlValue::mkInt(v ? VAL_TRUE : VAL_FALSE, V3VhdlStd::booleanType()); }

// Compare two values of the same type, giving -1, 0 or 1, or false if the
// comparison cannot be made
bool compareValues(const VhdlValue& lhs, const VhdlValue& rhs, int& out) {
    if (lhs.isStr() && rhs.isStr()) {
        const int cmp = lhs.toStr().compare(rhs.toStr());
        out = cmp < 0 ? -1 : (cmp > 0 ? 1 : 0);
        return true;
    }
    if (lhs.isStr() || rhs.isStr()) return false;
    // An array is compared element by element in index order, and where every
    // element they share is equal the shorter of the two is the lesser
    // (5.2.6). Without this the fall-through below read a composite as an
    // integer, which is zero for both, so any two arrays folded to equal.
    if (lhs.isArray() && rhs.isArray()) {
        const size_t shared = std::min(lhs.elements(), rhs.elements());
        for (size_t i = 0; i < shared; ++i) {
            int sub = 0;
            if (!compareValues(lhs.elementAt(i), rhs.elementAt(i), sub)) return false;
            if (sub != 0) {
                out = sub;
                return true;
            }
        }
        out = lhs.elements() < rhs.elements() ? -1 : (lhs.elements() > rhs.elements() ? 1 : 0);
        return true;
    }
    // A record has equality and nothing else (5.2.6), so an unequal pair only
    // has to be said to be unequal
    if (lhs.isRecord() && rhs.isRecord()) {
        if (lhs.elements() != rhs.elements()) return false;
        for (size_t i = 0; i < lhs.elements(); ++i) {
            int sub = 0;
            if (!compareValues(lhs.elementAt(i), rhs.elementAt(i), sub)) return false;
            if (sub != 0) {
                out = 1;
                return true;
            }
        }
        out = 0;
        return true;
    }
    if (lhs.isComposite() || rhs.isComposite()) return false;
    if (lhs.isReal() || rhs.isReal()) {
        const double l = lhs.toReal();
        const double r = rhs.toReal();
        out = l < r ? -1 : (l > r ? 1 : 0);
        return true;
    }
    const int64_t l = lhs.toInt();
    const int64_t r = rhs.toInt();
    out = l < r ? -1 : (l > r ? 1 : 0);
    return true;
}

// The characters of a one-dimensional character array value, so that "abc",
// 'a', '1' and x"41" can all be concatenated with each other.
//
// A scalar element contributes the character its literal is written with,
// which is why this works for BIT as well as for CHARACTER: BIT's values are
// named '0' and '1', so a BIT_VECTOR folds to a string of ones and zeros.
bool asCharString(const VhdlValue& val, std::string& out) {
    if (val.isStr()) {
        out = val.toStr();
        return true;
    }
    if (!val.isInt() || !val.typep()) return false;
    const VhdlType* const basep = val.typep()->base();
    if (basep->kind() != VhdlTypeKind::ENUM) return false;
    const std::vector<VhdlSymbol*>& lits = basep->literals();
    if (val.toInt() < 0 || val.toInt() >= static_cast<int64_t>(lits.size())) return false;
    // A character's position is its code (16.3). The printable ones are
    // named with their quotes, as in 'c'; the control characters are named
    // rather than written, so the position is what says which they are.
    const VhdlType* const charp = V3VhdlStd::characterType();
    if (charp && basep == charp->base()) {
        out.assign(1, static_cast<char>(val.toInt()));
        return true;
    }
    const std::string& name = lits[val.toInt()]->name();
    if (name.size() != 3 || name.front() != '\'' || name.back() != '\'') return false;
    out.assign(1, name[1]);
    return true;
}

}  // namespace

//######################################################################
// The evaluator

class VhdlEvaluator final {
    // MEMBERS
    std::string* const m_errp;  // Where to leave the reason a fold failed
    int m_depth = 0;  // Guards against a cyclic declaration

    // One activation of a subprogram body being run for a static call: the
    // value of every parameter, local object and loop parameter it holds
    using Frame = std::map<const VhdlSymbol*, VhdlValue>;
    // A deque and not a vector: runBody holds a reference to the activation
    // it is filling in while evaluating an initial value, and that may be a
    // call, which pushes an activation of its own. A vector would move the
    // one being filled out from under the reference.
    std::deque<Frame> m_frames;
    int m_calls = 0;  // Activations nested, against a recursion that has no end
    int m_steps = 0;  // Statements run, against a loop that has none either
    bool m_bail = false;  // The activation being run cannot be folded
    // Whether a body that is not pure may be run. Only elaboration asks for
    // that: a handle a verification library builds is made by an impure
    // function, and what elaboration wants from it is the widths it carries,
    // not a value to emit in place of the object (9.4.3)
    const bool m_allowImpure;

    // A constant folded from a self-referential declaration would recurse
    // forever; analysis reports the cycle separately, so just stop
    static constexpr int MAX_DEPTH = 64;
    // A body may name a function that names a body, and a loop may be written
    // so that it never ends. Neither is a reason to hang: a fold that runs out
    // of either budget is simply not one that happened.
    static constexpr int MAX_CALLS = 16;
    static constexpr int MAX_STEPS = 20000;

public:
    // CONSTRUCTORS
    explicit VhdlEvaluator(std::string* errp, bool allowImpure = false)
        : m_errp{errp}
        , m_allowImpure{allowImpure} {}

    // METHODS
    // The same, with names given values from outside: what an instantiation
    // hands an entity's generics, so that a bound written in terms of one
    // folds to the number that instance gives it
    VhdlValue evalWith(AstNodeVhdlExpr* nodep,
                       const std::map<const VhdlSymbol*, VhdlValue>& bindings) {
        m_frames.emplace_back();
        for (const auto& pair : bindings) m_frames.back()[pair.first] = pair.second;
        const VhdlValue out = eval(nodep);
        m_frames.pop_back();
        return out;
    }

    VhdlValue eval(AstNodeVhdlExpr* nodep) {
        if (!nodep) return VhdlValue{};
        if (m_depth > MAX_DEPTH) return VhdlValue{};
        VL_RESTORER(m_depth);
        ++m_depth;
        if (AstVhdlLiteral* const p = VN_CAST(nodep, VhdlLiteral)) return evalLiteral(p);
        if (AstVhdlName* const p = VN_CAST(nodep, VhdlName)) return evalSymbol(p->vhdlSymp());
        if (AstVhdlSelected* const p = VN_CAST(nodep, VhdlSelected)) {
            // "r.f" where r has a value of its own is that value's element;
            // where the name denotes an object in a package it is that object
            if (const AstVhdlName* const suffixp = VN_CAST(p->suffixp(), VhdlName)) {
                const VhdlValue rec = eval(p->prefixp());
                if (rec.isRecord()) return rec.field(suffixp->name());
            }
            return evalSymbol(p->vhdlSymp());
        }
        if (AstVhdlAggregate* const p = VN_CAST(nodep, VhdlAggregate)) return evalAggregate(p);
        if (AstVhdlUnary* const p = VN_CAST(nodep, VhdlUnary)) return evalUnary(p);
        if (AstVhdlBinary* const p = VN_CAST(nodep, VhdlBinary)) return evalBinary(p);
        if (AstVhdlQualified* const p = VN_CAST(nodep, VhdlQualified)) {
            VhdlValue val = eval(p->valuep());
            if (val.known() && p->vhdlTypep()) val.typep(p->vhdlTypep());
            return val;
        }
        if (AstVhdlAttrName* const p = VN_CAST(nodep, VhdlAttrName)) return evalAttr(p);
        if (AstVhdlCall* const p = VN_CAST(nodep, VhdlCall)) return evalCall(p);
        return VhdlValue{};
    }

private:
    void fail(const std::string& msg) {
        if (m_errp && m_errp->empty()) *m_errp = msg;
    }

    VhdlValue evalSymbol(VhdlSymbol* symp) {
        if (!symp) return VhdlValue{};
        // An alias is another name for what it denotes (6.6), and so has the
        // same value. A chain of them is bounded by the source.
        for (int depth = 0; symp->kind() == VhdlSymKind::ALIAS && depth < 32; ++depth) {
            if (!symp->aliasp()) return VhdlValue{};
            symp = symp->aliasp();
        }
        // Inside a body being run for a static call, an object belongs to that
        // activation and has whatever value it has been given there
        if (!m_frames.empty()) {
            const Frame& frame = m_frames.back();
            const auto it = frame.find(symp);
            if (it != frame.end()) return it->second;
        }
        switch (symp->kind()) {
        case VhdlSymKind::ENUM_LITERAL:
            return VhdlValue::mkInt(symp->position(), symp->typep());
        case VhdlSymKind::PHYSICAL_UNIT:
            // A unit name alone is the physical literal 1 of that unit (15.8)
            return symp->value();
        case VhdlSymKind::CONSTANT:
        case VhdlSymKind::GENERIC:
            // A deferred constant has no value until its body is analyzed, and
            // a generic none until elaboration
            if (symp->deferred()) return VhdlValue{};
            // A composite value is held apart from the one the emitted code
            // is built from, and is what a width taken from a record's
            // element is folded with
            if (!symp->hasValue() && symp->elabValue().known()) return symp->elabValue();
            return symp->value();
        case VhdlSymKind::PARAMETER:
            // A parameter has a value only in a copy of a subprogram made for
            // one call site, which is where the call's own values were fixed
            return symp->value();
        case VhdlSymKind::FUNCTION:
            // A function taking no parameters is called by writing its name
            // with no argument list at all (4.2.2.1)
            return evalUserCall(nullptr, symp);
        default: return VhdlValue{};
        }
    }

    // A record aggregate, element by element (9.3.3.2). An element whose own
    // value does not fold is left unknown rather than stopping the fold: a
    // handle a verification library builds carries an actor no one can work
    // out beside the widths everything else is sized from, and it is the
    // widths elaboration needs.
    // Whether an array's elements are numbers. Text is held as a string
    // rather than element by element, and an array of logic is a bit pattern
    // the emitted code carries, so neither is turned into a value of
    // elements here; a table of numbers is what a package writes its
    // parameters as, and is what this is for.
    static bool numericElement(const VhdlType* typep) {
        const VhdlType* const basep = typep->base();
        // One dimension only. A multidimensional aggregate is written as
        // rows -- "((1, 2), (3, 4))" -- and its elements are not values of
        // the element type at all, so counting them as though they were
        // gives an array of the wrong shape.
        if (basep->indexTypes().size() != 1) return false;
        const VhdlType* const elemp = typep->elementp() ? typep->elementp() : basep->elementp();
        if (!elemp) return false;
        switch (elemp->base()->kind()) {
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::PHYSICAL:
        case VhdlTypeKind::FLOATING: return true;
        default: return false;
        }
    }

    // An array aggregate whose elements are static is a static value in its
    // turn (9.4.2), and holding it is what lets a package write a table and
    // read it back: "constant POLYS : nat_array := (91, 121)" is how a design
    // states its parameters, and a function that works a bound out of them
    // cannot fold until the table can be read.
    VhdlValue evalArrayAggregate(AstVhdlAggregate* nodep, VhdlType* typep) {
        if (typep->indexTypes().empty()) return VhdlValue{};
        const VhdlType* const idxp = typep->indexTypes()[0];
        if (!idxp) return VhdlValue{};
        // How many elements the value has. A positional aggregate says so
        // itself, and that is the answer whether or not anything constrains
        // it: where the target is constrained the two agree, and where it is
        // not -- the value an attribute specification gives -- the aggregate
        // is all there is to go on (9.3.3.3). Asking the index subtype
        // instead is no help, since an unconstrained array is indexed by a
        // subtype that is perfectly bounded: NATURAL runs to two billion.
        int64_t positional = 0;
        bool anyChoice = false;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            const AstVhdlElemAssoc* const ap = VN_CAST(p, VhdlElemAssoc);
            if (!ap) return VhdlValue{};
            if (ap->choicesp()) {
                anyChoice = true;
            } else {
                ++positional;
            }
        }
        int64_t left = 0;
        int64_t length = 0;
        bool ascending = true;
        if (!anyChoice && positional > 0) {
            length = positional;
            ascending = idxp->ascending();
            left = idxp->bounded() ? idxp->left() : 0;
        } else if (idxp->bounded()) {
            left = idxp->left();
            length = idxp->length();
            ascending = idxp->ascending();
        } else {
            return VhdlValue{};
        }
        if (length <= 0 || length > (1 << 16)) return VhdlValue{};
        std::vector<VhdlValue> elems(static_cast<size_t>(length));
        VhdlValue out = VhdlValue::mkArray(std::move(elems), left, ascending, typep);
        std::set<int64_t> given;
        AstNodeVhdlExpr* othersp = nullptr;
        int64_t at = left;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            AstVhdlElemAssoc* const ap = VN_CAST(p, VhdlElemAssoc);
            if (!ap || !ap->valuep()) return VhdlValue{};
            if (!ap->choicesp()) {
                // Positional, from the leftmost index onwards
                const VhdlValue v = eval(ap->valuep());
                if (!v.known()) return VhdlValue{};
                out.setElement(at, v);
                given.insert(at);
                at = ascending ? at + 1 : at - 1;
                continue;
            }
            for (AstNode* c = ap->choicesp(); c; c = c->nextp()) {
                AstVhdlChoice* const cp = VN_CAST(c, VhdlChoice);
                if (!cp) return VhdlValue{};
                if (cp->isOthers()) {
                    othersp = ap->valuep();
                    continue;
                }
                // A choice may be one index or a whole discrete range
                if (AstVhdlRange* const rp = VN_CAST(cp->exprp(), VhdlRange)) {
                    const VhdlValue lo = rp->leftp() ? eval(rp->leftp()) : VhdlValue{};
                    const VhdlValue hi = rp->rightp() ? eval(rp->rightp()) : VhdlValue{};
                    if (!lo.isInt() || !hi.isInt()) return VhdlValue{};
                    const VhdlValue v = eval(ap->valuep());
                    if (!v.known()) return VhdlValue{};
                    const int64_t from = std::min(lo.toInt(), hi.toInt());
                    const int64_t to = std::max(lo.toInt(), hi.toInt());
                    for (int64_t i = from; i <= to; ++i) {
                        out.setElement(i, v);
                        given.insert(i);
                    }
                    continue;
                }
                AstNodeVhdlExpr* const cexprp = VN_CAST(cp->exprp(), NodeVhdlExpr);
                const VhdlValue idx = cexprp ? eval(cexprp) : VhdlValue{};
                if (!idx.isInt()) return VhdlValue{};
                const VhdlValue v = eval(ap->valuep());
                if (!v.known()) return VhdlValue{};
                out.setElement(idx.toInt(), v);
                given.insert(idx.toInt());
            }
        }
        if (othersp) {
            const VhdlValue rest = eval(othersp);
            if (!rest.known()) return VhdlValue{};
            for (int64_t i = 0; i < length; ++i) {
                const int64_t index = ascending ? left + i : left - i;
                if (!given.count(index)) out.setElement(index, rest);
            }
        } else if (static_cast<int64_t>(given.size()) != length) {
            // Every element must have been given one, or the value is not one
            return VhdlValue{};
        }
        return out;
    }

    VhdlValue evalAggregate(AstVhdlAggregate* nodep) {
        VhdlType* const typep = nodep->vhdlTypep();
        if (typep && !typep->isError() && typep->base()->kind() == VhdlTypeKind::ARRAY
            && numericElement(typep)) {
            return evalArrayAggregate(nodep, typep);
        }
        if (!typep || typep->base()->kind() != VhdlTypeKind::RECORD) return VhdlValue{};
        const std::vector<VhdlSymbol*>& elems = typep->base()->elements();
        if (elems.empty()) return VhdlValue{};
        std::vector<std::pair<std::string, VhdlValue>> fields;
        fields.reserve(elems.size());
        for (const VhdlSymbol* const ep : elems) fields.emplace_back(ep->name(), VhdlValue{});
        VhdlValue out = VhdlValue::mkRecord(std::move(fields), typep);
        size_t at = 0;
        AstNodeVhdlExpr* othersp = nullptr;
        std::set<std::string> named;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            AstVhdlElemAssoc* const ap = VN_CAST(p, VhdlElemAssoc);
            if (!ap || !ap->valuep()) return VhdlValue{};
            if (!ap->choicesp()) {
                // Positional, in declaration order
                if (at >= elems.size()) return VhdlValue{};
                out.setField(elems[at]->name(), eval(ap->valuep()));
                named.insert(elems[at]->name());
                ++at;
                continue;
            }
            for (AstNode* c = ap->choicesp(); c; c = c->nextp()) {
                AstVhdlChoice* const cp = VN_CAST(c, VhdlChoice);
                if (!cp) return VhdlValue{};
                if (cp->isOthers()) {
                    othersp = ap->valuep();
                    continue;
                }
                const AstVhdlName* const namep = VN_CAST(cp->exprp(), VhdlName);
                if (!namep) return VhdlValue{};
                out.setField(namep->name(), eval(ap->valuep()));
                named.insert(namep->name());
            }
        }
        if (othersp) {
            const VhdlValue rest = eval(othersp);
            for (const VhdlSymbol* const ep : elems) {
                if (!named.count(ep->name())) out.setField(ep->name(), rest);
            }
        }
        return out;
    }

    VhdlValue evalLiteral(AstVhdlLiteral* nodep) {
        VhdlType* const typep = nodep->vhdlTypep();
        switch (nodep->kind()) {
        case VhdlLitKind::INTEGER: return VhdlValue::mkInt(nodep->intVal(), typep);
        case VhdlLitKind::REAL: return VhdlValue::mkReal(nodep->realVal(), typep);
        case VhdlLitKind::CHARACTER: {
            VhdlSymbol* const symp = nodep->vhdlSymp();
            if (!symp) return VhdlValue{};
            return VhdlValue::mkInt(symp->position(), typep ? typep : symp->typep());
        }
        case VhdlLitKind::PHYSICAL: {
            // abstract_literal unit_name: the literal scales the unit (15.8)
            VhdlValue unit;
            if (nodep->unitp()) unit = evalSymbol(nodep->unitp()->vhdlSymp());
            if (!unit.known()) return VhdlValue{};
            int64_t out = 0;
            if (nodep->realVal() != 0.0 || nodep->intVal() == 0) {
                // A real abstract literal scales and then takes the largest
                // integer not greater than the product (5.2.4.1). Worked out
                // on the literal's own digits, because "8.2 ms" and "8200 us"
                // are the same value while 8.2 times ten to the twelfth, as
                // doubles, is not the integer 8200000000000
                if (!scaleDecimal(nodep->text(), unit.toInt(), out)) {
                    const double scaled = nodep->realVal() * static_cast<double>(unit.toInt());
                    if (!(scaled >= -9.2e18 && scaled <= 9.2e18)) return VhdlValue{};
                    out = static_cast<int64_t>(scaled);
                }
                if (nodep->realVal() == 0.0) out = 0;
            }
            if (nodep->intVal() != 0) {
                if (!mulOvf(nodep->intVal(), unit.toInt(), out)) return VhdlValue{};
            }
            return VhdlValue::mkInt(out, typep ? typep : unit.typep());
        }
        case VhdlLitKind::STRING:
        case VhdlLitKind::BITSTRING:
            // The scanner has already expanded a bit string to its bits and
            // applied the length given, so both kinds carry their characters
            return VhdlValue::mkStr(nodep->text(), typep);
        case VhdlLitKind::NULLLIT: return VhdlValue{};
        }
        return VhdlValue{};  // LCOV_EXCL_LINE
    }

    VhdlValue evalUnary(AstVhdlUnary* nodep) {
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp || !symp->predefined()) return VhdlValue{};
        const VhdlValue val = eval(nodep->lhsp());
        if (!val.known()) return VhdlValue{};
        VhdlType* const resp = nodep->vhdlTypep() ? nodep->vhdlTypep() : symp->returnTypep();
        switch (nodep->op()) {
        case VhdlUnaryOp::IDENTITY: return val;
        case VhdlUnaryOp::NEGATE:
            if (val.isReal()) return VhdlValue::mkReal(-val.toReal(), resp);
            if (val.toInt() == INT64_MIN) return VhdlValue{};
            return VhdlValue::mkInt(-val.toInt(), resp);
        case VhdlUnaryOp::ABS:
            if (val.isReal()) return VhdlValue::mkReal(std::fabs(val.toReal()), resp);
            if (val.toInt() == INT64_MIN) return VhdlValue{};
            return VhdlValue::mkInt(val.toInt() < 0 ? -val.toInt() : val.toInt(), resp);
        case VhdlUnaryOp::NOT:
            if (val.isStr()) {
                std::string out = val.toStr();
                for (char& c : out) {
                    if (c == '0') {
                        c = '1';
                    } else if (c == '1') {
                        c = '0';
                    } else {
                        return VhdlValue{};
                    }
                }
                return VhdlValue::mkStr(out, resp);
            }
            if (!val.isInt() || val.toInt() < 0 || val.toInt() > 1) return VhdlValue{};
            return VhdlValue::mkInt(val.toInt() ^ 1, resp);
        case VhdlUnaryOp::COND_CONV:
            if (!val.isInt() || val.toInt() < 0 || val.toInt() > 1) return VhdlValue{};
            return mkBool(val.toInt() == VAL_TRUE);
        default: return VhdlValue{};  // Reduction operators, over values we do not hold
        }
    }

    VhdlValue evalBinary(AstVhdlBinary* nodep) {
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp || !symp->predefined()) return VhdlValue{};
        const VhdlValue lhs = eval(nodep->lhsp());
        const VhdlValue rhs = eval(nodep->rhsp());
        if (!lhs.known() || !rhs.known()) return VhdlValue{};
        VhdlType* const resp = nodep->vhdlTypep() ? nodep->vhdlTypep() : symp->returnTypep();
        const bool anyReal = lhs.isReal() || rhs.isReal();

        switch (nodep->op()) {
        case VhdlBinaryOp::EQ:
        case VhdlBinaryOp::NE:
        case VhdlBinaryOp::LT:
        case VhdlBinaryOp::LE:
        case VhdlBinaryOp::GT:
        case VhdlBinaryOp::GE: {
            int cmp = 0;
            if (!compareValues(lhs, rhs, cmp)) return VhdlValue{};
            switch (nodep->op()) {
            case VhdlBinaryOp::EQ: return mkBool(cmp == 0);
            case VhdlBinaryOp::NE: return mkBool(cmp != 0);
            case VhdlBinaryOp::LT: return mkBool(cmp < 0);
            case VhdlBinaryOp::LE: return mkBool(cmp <= 0);
            case VhdlBinaryOp::GT: return mkBool(cmp > 0);
            default: return mkBool(cmp >= 0);
            }
        }
        case VhdlBinaryOp::AND:
        case VhdlBinaryOp::OR:
        case VhdlBinaryOp::NAND:
        case VhdlBinaryOp::NOR:
        case VhdlBinaryOp::XOR:
        case VhdlBinaryOp::XNOR: {
            if (!lhs.isInt() || !rhs.isInt()) return VhdlValue{};
            if (lhs.toInt() < 0 || lhs.toInt() > 1 || rhs.toInt() < 0 || rhs.toInt() > 1) {
                return VhdlValue{};
            }
            const bool a = lhs.toInt() == VAL_TRUE;
            const bool b = rhs.toInt() == VAL_TRUE;
            bool r = false;
            switch (nodep->op()) {
            case VhdlBinaryOp::AND: r = a && b; break;
            case VhdlBinaryOp::OR: r = a || b; break;
            case VhdlBinaryOp::NAND: r = !(a && b); break;
            case VhdlBinaryOp::NOR: r = !(a || b); break;
            case VhdlBinaryOp::XOR: r = a != b; break;
            default: r = a == b; break;
            }
            return VhdlValue::mkInt(r ? VAL_TRUE : VAL_FALSE, resp);
        }
        case VhdlBinaryOp::CONCAT: {
            std::string l;
            std::string r;
            if (!asCharString(lhs, l) || !asCharString(rhs, r)) return VhdlValue{};
            return VhdlValue::mkStr(l + r, resp);
        }
        case VhdlBinaryOp::ADD: {
            if (anyReal) return VhdlValue::mkReal(lhs.toReal() + rhs.toReal(), resp);
            int64_t out = 0;
            if (!addOvf(lhs.toInt(), rhs.toInt(), out)) return VhdlValue{};
            return VhdlValue::mkInt(out, resp);
        }
        case VhdlBinaryOp::SUB: {
            if (anyReal) return VhdlValue::mkReal(lhs.toReal() - rhs.toReal(), resp);
            int64_t out = 0;
            if (!subOvf(lhs.toInt(), rhs.toInt(), out)) return VhdlValue{};
            return VhdlValue::mkInt(out, resp);
        }
        case VhdlBinaryOp::MUL: {
            // A physical value times a real truncates back to whole units
            if (resp && resp->base()->kind() == VhdlTypeKind::PHYSICAL && anyReal) {
                const double scaled = lhs.toReal() * rhs.toReal();
                if (!(scaled >= -9.2e18 && scaled <= 9.2e18)) return VhdlValue{};
                return VhdlValue::mkInt(static_cast<int64_t>(scaled), resp);
            }
            if (anyReal) return VhdlValue::mkReal(lhs.toReal() * rhs.toReal(), resp);
            int64_t out = 0;
            if (!mulOvf(lhs.toInt(), rhs.toInt(), out)) return VhdlValue{};
            return VhdlValue::mkInt(out, resp);
        }
        case VhdlBinaryOp::DIV: {
            if (resp && resp->base()->kind() == VhdlTypeKind::PHYSICAL && anyReal) {
                if (rhs.toReal() == 0.0) {
                    fail("Division by zero in a static expression");
                    return VhdlValue{};
                }
                const double scaled = lhs.toReal() / rhs.toReal();
                if (!(scaled >= -9.2e18 && scaled <= 9.2e18)) return VhdlValue{};
                return VhdlValue::mkInt(static_cast<int64_t>(scaled), resp);
            }
            if (anyReal) {
                if (rhs.toReal() == 0.0) {
                    fail("Division by zero in a static expression");
                    return VhdlValue{};
                }
                return VhdlValue::mkReal(lhs.toReal() / rhs.toReal(), resp);
            }
            if (rhs.toInt() == 0) {
                fail("Division by zero in a static expression");
                return VhdlValue{};
            }
            if (lhs.toInt() == INT64_MIN && rhs.toInt() == -1) return VhdlValue{};
            return VhdlValue::mkInt(lhs.toInt() / rhs.toInt(), resp);
        }
        case VhdlBinaryOp::MOD:
        case VhdlBinaryOp::REM: {
            if (anyReal) return VhdlValue{};
            if (rhs.toInt() == 0) {
                fail("Division by zero in a static expression");
                return VhdlValue{};
            }
            if (lhs.toInt() == INT64_MIN && rhs.toInt() == -1) return VhdlValue{};
            const int64_t out = nodep->op() == VhdlBinaryOp::MOD
                                    ? vhdlMod(lhs.toInt(), rhs.toInt())
                                    : vhdlRem(lhs.toInt(), rhs.toInt());
            return VhdlValue::mkInt(out, resp);
        }
        case VhdlBinaryOp::POW: {
            if (!rhs.isInt()) return VhdlValue{};
            if (lhs.isReal()) {
                return VhdlValue::mkReal(std::pow(lhs.toReal(), static_cast<double>(rhs.toInt())),
                                         resp);
            }
            if (rhs.toInt() < 0) {
                fail("An integer raised to a negative power is not a legal value");
                return VhdlValue{};
            }
            int64_t out = 0;
            if (!powOvf(lhs.toInt(), rhs.toInt(), out)) return VhdlValue{};
            return VhdlValue::mkInt(out, resp);
        }
        default: return VhdlValue{};  // Shifts and matching relationals, over array values
        }
    }

    // The type an attribute's prefix denotes, whether it was written as a type
    // mark or as an object of that type
    static VhdlType* prefixType(AstVhdlAttrName* nodep) {
        AstNodeVhdlExpr* const prefixp = nodep->prefixp();
        if (!prefixp) return nullptr;
        VhdlSymbol* const symp = prefixp->vhdlSymp();
        if (symp && symp->kind() == VhdlSymKind::TYPE) return symp->typep();
        // An indexed or selected name denotes a part of the object, not the
        // whole of it, and carries the symbol of the object it is part of --
        // so "a(0)'length" is the length of an element, not of a. The
        // expression's own type is the one the attribute asks about.
        if (VhdlType* const exprp = prefixp->vhdlTypep()) {
            if (!exprp->isError()) {
                // An access value is dereferenced without saying so (8.1)
                if (exprp->base()->kind() == VhdlTypeKind::ACCESS
                    && exprp->base()->designatedp()) {
                    return exprp->base()->designatedp();
                }
                return exprp;
            }
        }
        if (symp && symp->typep()) return symp->typep();
        return nullptr;
    }

    // The index type an array attribute asks about, honouring the optional
    // dimension argument (16.2.4)
    VhdlType* indexTypeOf(AstVhdlAttrName* nodep, VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return typep;
        // An unconstrained array has no range of its own: its index subtype's
        // bounds are the limits an index must satisfy, not the array's extent
        if (typep->unconstrained()) return nullptr;
        size_t dim = 0;
        if (nodep->argp()) {
            const VhdlValue arg = eval(nodep->argp());
            if (!arg.isInt() || arg.toInt() < 1) return nullptr;
            dim = static_cast<size_t>(arg.toInt() - 1);
        }
        if (dim >= typep->indexTypes().size()) return nullptr;
        return typep->indexTypes()[dim];
    }

    // Every region from the design unit down to this one, each followed by a
    // colon. An unnamed process contributes an empty name, as VHDL writes it.
    // The name a copy the analysis made goes by in the source. A copy is one
    // call site's, and its made-up suffix is no part of what the design was
    // written as -- and cannot be part of an identifier either, since VHDL
    // does not let two underlines stand together (15.4.2)
    static std::string sourceName(const std::string& name) {
        const size_t at = name.find("__V");
        return at == std::string::npos ? name : name.substr(0, at);
    }

    static std::string scopePath(const VhdlScope* scopep, bool inst) {
        if (!scopep) return "";
        const std::string& prefix = inst ? scopep->instPrefix() : scopep->pathPrefix();
        if (!prefix.empty()) return prefix + ":";
        std::string name = scopep->name();
        // A subprogram's region is written with the subprogram's profile, so
        // that two of the same name are told apart (16.2.7)
        if (const VhdlSymbol* const ownerp = scopep->ownerp()) {
            return scopePath(scopep->parentp(), inst) + sourceName(ownerp->name())
                   + signatureOf(ownerp) + ":";
        }
        // The body of a generate is not a named region of its own: the
        // generate statement's own label is what the path holds (16.2.7), so
        // an unlabelled body contributes nothing rather than an empty name
        if (name == "generate body") return scopePath(scopep->parentp(), inst);
        // The region a design unit's context clause is analyzed in is not a
        // region of the design: what the unit declares is named from the unit
        // itself (12.1, 16.2.7)
        if (name.size() > 8 && name.compare(name.size() - 8, 8, "$context") == 0) {
            return scopePath(scopep->parentp(), inst);
        }
        if (name == "process" || name == "block" || name == "generate"
            || name == "loop") {
            name.clear();
        }
        const std::string up = scopePath(scopep->parentp(), inst);
        if (up.empty() && name.empty()) return "";
        return up + name + ":";
    }

    // "[bit, bit return integer]", the parameter and result type profile
    static std::string signatureOf(const VhdlSymbol* symp) {
        std::string out = "[";
        bool first = true;
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (!first) out += ",";
            first = false;
            out += typep ? typep->prettyName() : "";
        }
        if (symp->returnTypep()) out += " return " + symp->returnTypep()->prettyName();
        return out + "]";
    }

    VhdlValue evalAttr(AstVhdlAttrName* nodep) {
        const std::string& attr = nodep->name();

        // A user-defined attribute stands for the static expression that an
        // attribute specification gave the prefix (7.2). Checked before the
        // prefix's type, since the prefix may be a label or a design unit.
        if (VhdlSymbol* const attrp = nodep->vhdlSymp()) {
            if (attrp->kind() == VhdlSymKind::ATTRIBUTE) {
                VhdlSymbol* prefixSymp
                    = nodep->prefixp() ? nodep->prefixp()->vhdlSymp() : nullptr;
                if (prefixSymp) prefixSymp = prefixSymp->resolved();
                AstNodeVhdlExpr* const valuep
                    = prefixSymp ? prefixSymp->userAttr(attr) : nullptr;
                if (!valuep) return VhdlValue{};
                const VhdlValue whole = eval(valuep);
                // "obj'attr(i)" names one element of what the specification
                // gave, not the whole of it (7.2, 8.4)
                if (nodep->argp() && whole.isArray()) {
                    const VhdlValue idx = eval(nodep->argp());
                    if (!idx.isInt()) return VhdlValue{};
                    return whole.element(idx.toInt());
                }
                // Text is held as text rather than element by element, and
                // the index is the attribute's own subtype's: "attribute a :
                // string (10 downto 1)" puts the first character at 10
                if (nodep->argp() && whole.isStr()) {
                    const VhdlValue idx = eval(nodep->argp());
                    if (!idx.isInt()) return VhdlValue{};
                    const VhdlType* const atp = attrp->typep();
                    int64_t left = 1;
                    bool ascending = true;
                    if (atp && !atp->indexTypes().empty() && atp->indexTypes()[0]
                        && atp->indexTypes()[0]->bounded()) {
                        left = atp->indexTypes()[0]->left();
                        ascending = atp->indexTypes()[0]->ascending();
                    }
                    const int64_t off = ascending ? idx.toInt() - left : left - idx.toInt();
                    const std::string text = whole.toStr();
                    if (off < 0 || static_cast<size_t>(off) >= text.size()) return VhdlValue{};
                    return VhdlValue::mkInt(static_cast<unsigned char>(text[off]),
                                            V3VhdlStd::characterType());
                }
                return whole;
            }
        }

        if (attr == "instance_name" || attr == "path_name") {
            // The path to what the prefix names, written the way VHDL writes
            // one (16.2.7): every region from the design unit down, with the
            // library ahead of a package and the architecture written beside
            // its entity. A subprogram or a package ends with a colon; an
            // object does not.
            VhdlSymbol* const symp = nodep->prefixp() ? nodep->prefixp()->vhdlSymp() : nullptr;
            if (!symp) return VhdlValue{};
            const bool inst = attr == "instance_name";
            std::string out
                = ":" + scopePath(symp->declScopep(), inst) + sourceName(symp->name());
            switch (symp->kind()) {
            case VhdlSymKind::FUNCTION:
            case VhdlSymKind::PROCEDURE:
                // A subprogram is named with its profile, since the name
                // alone need not pick out one declaration (16.2.7)
                out += signatureOf(symp) + ":";
                break;
            case VhdlSymKind::PACKAGE:
            case VhdlSymKind::ENTITY:
            case VhdlSymKind::TYPE: out += ":"; break;
            default: break;
            }
            return VhdlValue::mkStr(out, V3VhdlStd::stringType());
        }

        VhdlType* typep = prefixType(nodep);
        if (!typep || typep->isError()) return VhdlValue{};

        // Attributes of an array ask about one of its index ranges; those of a
        // scalar type about the type's own range (16.2.3, 16.2.4)
        const bool isArray = typep->base()->kind() == VhdlTypeKind::ARRAY;
        if (attr == "left" || attr == "right" || attr == "high" || attr == "low"
            || attr == "ascending" || attr == "length" || attr == "range"
            || attr == "reverse_range") {
            VhdlType* const rangep = isArray ? indexTypeOf(nodep, typep) : typep;
            if (!rangep || !rangep->bounded()) return VhdlValue{};
            const bool isReal = rangep->base()->kind() == VhdlTypeKind::FLOATING;
            if (attr == "left") {
                return isReal ? VhdlValue::mkReal(rangep->leftReal(), rangep)
                              : VhdlValue::mkInt(rangep->left(), rangep);
            }
            if (attr == "right") {
                return isReal ? VhdlValue::mkReal(rangep->rightReal(), rangep)
                              : VhdlValue::mkInt(rangep->right(), rangep);
            }
            if (attr == "high") {
                return isReal ? VhdlValue::mkReal(rangep->highReal(), rangep)
                              : VhdlValue::mkInt(rangep->high(), rangep);
            }
            if (attr == "low") {
                return isReal ? VhdlValue::mkReal(rangep->lowReal(), rangep)
                              : VhdlValue::mkInt(rangep->low(), rangep);
            }
            if (attr == "ascending") return mkBool(rangep->ascending());
            if (attr == "length") {
                return VhdlValue::mkInt(rangep->length(), V3VhdlStd::integerType());
            }
            return VhdlValue{};  // 'RANGE denotes a range, not a value
        }
        if (attr == "pos" || attr == "val" || attr == "succ" || attr == "pred"
            || attr == "leftof" || attr == "rightof") {
            if (!nodep->argp()) return VhdlValue{};
            const VhdlValue arg = eval(nodep->argp());
            if (!arg.isInt()) return VhdlValue{};
            const int64_t pos = arg.toInt();
            if (attr == "pos") return VhdlValue::mkInt(pos, V3VhdlStd::integerType());
            if (attr == "val") return VhdlValue::mkInt(pos, typep);
            int64_t out = pos;
            if (attr == "succ" || (attr == "rightof" && typep->ascending())
                || (attr == "leftof" && !typep->ascending())) {
                if (!addOvf(pos, 1, out)) return VhdlValue{};
            } else {
                if (!subOvf(pos, 1, out)) return VhdlValue{};
            }
            return VhdlValue::mkInt(out, typep);
        }
        if (attr == "image") {
            if (!nodep->argp()) return VhdlValue{};
            const VhdlValue arg = eval(nodep->argp());
            if (!arg.known()) return VhdlValue{};
            return VhdlValue::mkStr(V3VhdlEval::image(arg), V3VhdlStd::stringType());
        }
        if (attr == "value") {
            if (!nodep->argp()) return VhdlValue{};
            const VhdlValue arg = eval(nodep->argp());
            if (!arg.isStr()) return VhdlValue{};
            return V3VhdlEval::fromImage(arg.toStr(), typep);
        }
        if (attr == "simple_name") {
            VhdlSymbol* const symp = nodep->prefixp() ? nodep->prefixp()->vhdlSymp() : nullptr;
            if (!symp) return VhdlValue{};
            return VhdlValue::mkStr(symp->name(), V3VhdlStd::stringType());
        }
        return VhdlValue{};
    }

    // MINIMUM and MAXIMUM are declared for every scalar type by the language
    // itself (5.2.6), and a call to an implicitly defined operation whose
    // arguments are static is static in its turn (9.4.2 e). So this runs no
    // user code: the comparison is the type's own predefined one, which for
    // an enumeration is by position and for a physical type is by the value
    // scaled to its primary unit -- both of which are what the integer here
    // already holds
    VhdlValue evalMinMax(AstVhdlCall* nodep, const VhdlSymbol* symp) {
        const bool wantMax = symp->name() == "maximum";
        if (!wantMax && symp->name() != "minimum") return VhdlValue{};
        // The one-argument form answers over the elements of an array
        // (5.3.2.4), which is not a value held here
        if (symp->paramTypes().size() != 2) return VhdlValue{};
        VhdlValue args[2];
        size_t at = 0;
        for (AstNode* p = nodep->argsp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            AstNodeVhdlExpr* const actualp = asp ? asp->actualp() : VN_CAST(p, NodeVhdlExpr);
            size_t which = at;
            if (asp && asp->formalp()) {
                // The formals of an operation the language declares are
                // named but have no declaration of their own to resolve to,
                // so the name is read as it was written
                const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
                const int idx = namep ? symp->paramIndex(namep->name()) : -1;
                if (idx < 0) return VhdlValue{};
                which = static_cast<size_t>(idx);
            }
            if (which > 1 || !actualp) return VhdlValue{};
            const VhdlValue val = eval(actualp);
            // An array compares element by element, which is not done here
            if (!val.known() || val.isStr()) return VhdlValue{};
            args[which] = val;
            ++at;
        }
        if (at != 2 || !args[0].known() || !args[1].known()) return VhdlValue{};
        // "MINIMUM returns L if L < R, or R otherwise; MAXIMUM returns R if
        // L < R, or L otherwise" (5.2.6)
        const bool less = (args[0].isReal() || args[1].isReal())
                              ? args[0].toReal() < args[1].toReal()
                              : args[0].toInt() < args[1].toInt();
        return wantMax ? (less ? args[1] : args[0]) : (less ? args[0] : args[1]);
    }

    // Running a subprogram body
    //
    // A function call is a static expression when the function is pure and
    // each of its actuals is static (9.4.3 i), and its value is whatever the
    // body computes. So the body is run here, statement by statement, over
    // the values the call passed. This is not a shortcut for any particular
    // function: an array bound written as a call to a helper declared beside
    // it is ordinary VHDL, and IEEE's own floating-point package sizes its
    // fraction that way.
    //
    // Only the scalar values this evaluator holds can be computed, so a body
    // that needs an array or a record stops -- which is the ordinary "not
    // static" answer rather than an error. Two budgets bound the work: how
    // many activations may be nested, and how many statements may run in all.

    enum class Flow : uint8_t {
        NORMAL,  // Ran to the end of the statement list
        RETURNED,  // A return statement, or the fold gave up
        EXITED,  // An exit statement
        NEXTED  // A next statement
    };

    // Give up on the fold from inside a body. The value is unknown however
    // far the walk had got, so the flow only has to unwind.
    Flow bail() {
        m_bail = true;
        return Flow::RETURNED;
    }

    // Whether a condition holds. BOOLEAN is an enumeration, so TRUE is the
    // literal at position one (5.2.2). Clears okr where it does not fold.
    bool condTrue(AstNodeVhdlExpr* nodep, bool& okr) {
        const VhdlValue val = eval(nodep);
        if (!val.known() || !val.isInt()) {
            okr = false;
            return false;
        }
        return val.toInt() == VAL_TRUE;
    }

    // The object a variable assignment writes, or null where the target is
    // not a plain name of something this activation holds. A pure function
    // may not write anything outside itself (4.4), so anything else stops
    const VhdlSymbol* targetSym(AstNodeVhdlExpr* nodep) {
        if (m_frames.empty()) return nullptr;
        AstVhdlName* const namep = VN_CAST(nodep, VhdlName);
        VhdlSymbol* symp = namep ? namep->vhdlSymp() : nullptr;
        if (!symp) return nullptr;
        symp = symp->resolved();
        return m_frames.back().count(symp) ? symp : nullptr;
    }

    Flow execStmts(AstNode* stmtsp, VhdlValue& retr) {
        for (AstNode* p = stmtsp; p; p = p->nextp()) {
            const Flow f = execStmt(p, retr);
            if (f != Flow::NORMAL) return f;
        }
        return Flow::NORMAL;
    }

    Flow execStmt(AstNode* nodep, VhdlValue& retr) {
        if (++m_steps > MAX_STEPS) return bail();
        if (VN_IS(nodep, VhdlNull)) return Flow::NORMAL;
        if (AstVhdlReturn* const p = VN_CAST(nodep, VhdlReturn)) {
            if (p->valuep()) {
                retr = eval(p->valuep());
                if (!retr.known()) return bail();
            }
            return Flow::RETURNED;
        }
        if (VN_IS(nodep, VhdlAssert)) {
            // An assertion produces no value. Raising it here would put the
            // message out once for every expression that folds a call to
            // this function rather than once for every execution, so the
            // check is left to the code the back end emits.
            return Flow::NORMAL;
        }
        if (AstVhdlVarAssign* const p = VN_CAST(nodep, VhdlVarAssign)) {
            if (p->kind() != VhdlAssignKind::SIMPLE) return bail();
            const VhdlSymbol* const symp = targetSym(p->targetp());
            if (!symp) return bail();
            const VhdlValue val = eval(p->valuep());
            // Elaboration carries an unknown on rather than giving up on the
            // whole activation: what a handle's actor is decides nothing
            // about the widths beside it
            if (!val.known() && !m_allowImpure) return bail();
            m_frames.back()[symp] = val;
            return Flow::NORMAL;
        }
        if (AstVhdlIf* const p = VN_CAST(nodep, VhdlIf)) {
            for (AstNode* ap = p->altsp(); ap; ap = ap->nextp()) {
                AstVhdlAlternative* const altp = VN_CAST(ap, VhdlAlternative);
                if (!altp) return bail();
                // The last alternative may have no condition, meaning "else"
                if (altp->condp()) {
                    bool ok = true;
                    const bool taken = condTrue(altp->condp(), ok);
                    if (!ok) return bail();
                    if (!taken) continue;
                }
                return execStmts(altp->stmtsp(), retr);
            }
            return Flow::NORMAL;
        }
        if (AstVhdlCase* const p = VN_CAST(nodep, VhdlCase)) return execCase(p, retr);
        if (AstVhdlLoop* const p = VN_CAST(nodep, VhdlLoop)) return execLoop(p, retr);
        if (AstVhdlNext* const p = VN_CAST(nodep, VhdlNext)) {
            // A labelled exit leaves a loop further out than the one this
            // walk is in, which it does not keep track of (10.12)
            if (!p->loopLabel().empty()) return bail();
            if (p->condp()) {
                bool ok = true;
                const bool taken = condTrue(p->condp(), ok);
                if (!ok) return bail();
                if (!taken) return Flow::NORMAL;
            }
            return p->isExit() ? Flow::EXITED : Flow::NEXTED;
        }
        // A wait or a signal assignment cannot appear in a function at all,
        // and a procedure call would have to carry values back out
        return bail();
    }

    // Whether a case choice covers the selector. Clears okr where the choice
    // is one this walk cannot decide, such as a discrete range given by a
    // type mark.
    bool choiceMatches(AstVhdlChoice* nodep, const VhdlValue& sel, bool& okr) {
        if (nodep->isOthers()) return true;
        if (AstVhdlRange* const rp = nodep->rangep()) {
            const VhdlValue lo = rp->leftp() ? eval(rp->leftp()) : VhdlValue{};
            const VhdlValue hi = rp->rightp() ? eval(rp->rightp()) : VhdlValue{};
            if (!sel.isInt() || !lo.isInt() || !hi.isInt()) {
                okr = false;
                return false;
            }
            return rp->dir() == VhdlDir::TO
                       ? (sel.toInt() >= lo.toInt() && sel.toInt() <= hi.toInt())
                       : (sel.toInt() <= lo.toInt() && sel.toInt() >= hi.toInt());
        }
        AstNodeVhdlExpr* const valuep = nodep->valuep();
        const VhdlValue val = valuep ? eval(valuep) : VhdlValue{};
        if (!val.known() || val.kind() != sel.kind()) {
            okr = false;
            return false;
        }
        return val.isStr() ? val.toStr() == sel.toStr() : val.toInt() == sel.toInt();
    }

    Flow execCase(AstVhdlCase* nodep, VhdlValue& retr) {
        // "case?" matches with "?=", where a '-' in a choice stands for any
        // value (10.9). That is a comparison over the elements of an array,
        // and no array is held here
        if (nodep->matching()) return bail();
        const VhdlValue sel = eval(nodep->exprp());
        if (!sel.known()) return bail();
        for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
            AstVhdlAlternative* const altp = VN_CAST(ap, VhdlAlternative);
            if (!altp) return bail();
            for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_CAST(cp, VhdlChoice);
                if (!chp) return bail();
                bool ok = true;
                const bool hit = choiceMatches(chp, sel, ok);
                if (!ok) return bail();
                if (hit) return execStmts(altp->stmtsp(), retr);
            }
        }
        // The choices of a case statement are exhaustive (10.9), so this is
        // only reached where analysis has already reported that they are not
        return Flow::NORMAL;
    }

    Flow execLoop(AstVhdlLoop* nodep, VhdlValue& retr) {
        const bool isFor = nodep->kind() == VhdlLoopKind::FOR;
        int64_t from = 0;
        int64_t to = 0;
        int64_t step = 1;
        const VhdlSymbol* parmp = nullptr;
        VhdlType* parmTypep = nullptr;
        if (isFor) {
            // The discrete range decides both the values and how many there
            // are, so it has to fold before the first iteration (10.10)
            AstVhdlRange* const rp = VN_CAST(nodep->rangep(), VhdlRange);
            const VhdlValue lo = rp && rp->leftp() ? eval(rp->leftp()) : VhdlValue{};
            const VhdlValue hi = rp && rp->rightp() ? eval(rp->rightp()) : VhdlValue{};
            if (!lo.isInt() || !hi.isInt()) return bail();
            from = lo.toInt();
            to = hi.toInt();
            step = rp->dir() == VhdlDir::TO ? 1 : -1;
            parmp = nodep->vhdlSymp();
            if (!parmp) return bail();
            parmTypep = nodep->vhdlSymp()->typep();
        }
        for (int64_t i = from;; i += step) {
            if (isFor) {
                if (step > 0 ? i > to : i < to) break;
                m_frames.back()[parmp] = VhdlValue::mkInt(i, parmTypep);
            } else if (nodep->kind() == VhdlLoopKind::WHILE) {
                bool ok = nodep->condp() != nullptr;
                const bool go = ok && condTrue(nodep->condp(), ok);
                if (!ok) return bail();
                if (!go) break;
            }
            if (++m_steps > MAX_STEPS) return bail();
            const Flow f = execStmts(nodep->stmtsp(), retr);
            if (f == Flow::RETURNED) return f;
            if (f == Flow::EXITED) break;
        }
        // The loop parameter is not visible outside the loop (10.10)
        if (parmp) m_frames.back().erase(parmp);
        return Flow::NORMAL;
    }

    // One activation: bind the parameters, elaborate the declarations, run
    // the statements. The frame is already pushed.
    VhdlValue runBody(AstVhdlSubprogDecl* declp, const std::vector<AstVhdlIfaceDecl*>& parms,
                      const std::vector<VhdlValue>& args, const std::vector<bool>& given) {
        Frame& frame = m_frames.back();
        for (size_t i = 0; i < parms.size(); ++i) {
            VhdlSymbol* const symp = parms[i]->vhdlSymp();
            if (!symp) return VhdlValue{};
            if (given[i]) {
                frame[symp->resolved()] = args[i];
                continue;
            }
            // A formal the call left out takes the default written in its
            // declaration, read in the subprogram's own region (6.5.7.1)
            if (!parms[i]->defaultp()) return VhdlValue{};
            const VhdlValue val = eval(parms[i]->defaultp());
            if (!val.known()) return VhdlValue{};
            frame[symp->resolved()] = val;
        }
        for (AstNode* p = declp->declsp(); p; p = p->nextp()) {
            // Anything that is not an object declares no value to hold -- a
            // type, a nested subprogram, a use clause -- and a name that
            // denotes one of those folds by its own rules or not at all
            AstVhdlObjectDecl* const op = VN_CAST(p, VhdlObjectDecl);
            if (!op) continue;
            if (op->kind() != VhdlObjKind::CONSTANT && op->kind() != VhdlObjKind::VARIABLE) {
                continue;
            }
            VhdlSymbol* const symp = op->vhdlSymp();
            if (!symp || !valueHeld(symp->typep())) continue;
            // A variable with no initial value starts at the leftmost value
            // of its subtype, which is not a value held here. Left unknown,
            // so that reading it before it is written stops the fold rather
            // than inventing a number.
            frame[symp->resolved()] = op->defaultp() ? eval(op->defaultp()) : VhdlValue{};
        }
        VhdlValue ret;
        const Flow f = execStmts(declp->stmtsp(), ret);
        // Reaching the end of a function without a return statement is an
        // error the running model reports; there is no value to give here
        if (f != Flow::RETURNED) return VhdlValue{};
        return ret;
    }

    // Whether a value of this type is one this evaluator holds at all: a
    // scalar, or a record whose elements are themselves held. An array is
    // not: a value here carries no index range of its own, and a bound is
    // exactly what such a value is usually asked for.
    static bool valueHeld(const VhdlType* typep) {
        if (!typep) return false;
        if (scalarValued(typep)) return true;
        if (typep->base()->kind() != VhdlTypeKind::RECORD) return false;
        for (const VhdlSymbol* const ep : typep->base()->elements()) {
            if (!ep || !valueHeld(ep->typep())) return false;
        }
        return true;
    }

    // Whether a value of this type is one this evaluator holds faithfully:
    // a number, a position or a physical value, and nothing composite
    static bool scalarValued(const VhdlType* typep) {
        if (!typep) return false;
        switch (typep->base()->kind()) {
        case VhdlTypeKind::ENUM:
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::FLOATING:
        case VhdlTypeKind::PHYSICAL:
        case VhdlTypeKind::UNIVERSAL_INTEGER:
        case VhdlTypeKind::UNIVERSAL_REAL: return true;
        default: return false;
        }
    }

    // A call to a user-defined function, folded by running it. Null argsp is
    // a call written as a bare name, which is how a function taking no
    // parameters is called (4.2.2.1).
    VhdlValue evalUserCall(AstNode* argsp, const VhdlSymbol* funcp) {
        // A function bound to a C symbol has no body to run, and an impure
        // one may read a signal or a shared variable, so neither is static
        if (funcp->predefined() || funcp->isForeign()) return VhdlValue{};
        AstVhdlSubprogDecl* const declp = funcp->bodyp();
        if (!declp || !declp->isFunction()) return VhdlValue{};
        if (!declp->vhdlPure() && !m_allowImpure) return VhdlValue{};
        if (m_calls >= MAX_CALLS || m_steps >= MAX_STEPS) return VhdlValue{};
        // Only where every value that goes in and the one that comes out is
        // a scalar. A value here is a number, not an array with bounds of its
        // own, so a body that builds one would be run over values that had
        // lost how they were indexed -- and a bound is exactly what such a
        // call is usually asked for.
        if (!valueHeld(funcp->returnTypep())) return VhdlValue{};

        // The parameters, in order. A generic subprogram keeps its generics
        // ahead of them, and a call associates only the parameters (4.2.1).
        std::vector<AstVhdlIfaceDecl*> parms;
        std::vector<bool> unheld;
        int skip = declp->genericCount();
        for (AstNode* p = declp->paramsp(); p; p = p->nextp()) {
            AstVhdlIfaceDecl* const ip = VN_CAST(p, VhdlIfaceDecl);
            if (!ip) return VhdlValue{};
            if (skip > 0) {
                --skip;
                continue;
            }
            // A signal, file or subprogram parameter is not a value passed
            if (ip->kind() != VhdlIfaceKind::CONSTANT && ip->kind() != VhdlIfaceKind::VARIABLE) {
                return VhdlValue{};
            }
            if (ip->mode() != VhdlMode::IN) return VhdlValue{};
            // At elaboration a parameter of a type not held here -- a string
            // -- is carried as unknown. Whatever reads it is unknown too, and
            // a branch on it stops the fold, so what folds is only what does
            // not depend on it: VUnit's testbenches size their buses from
            // the literal widths of a record decoded from a string generic.
            const bool held = valueHeld(ip->vhdlSymp() ? ip->vhdlSymp()->typep() : nullptr);
            if (!held && !m_allowImpure) return VhdlValue{};
            parms.push_back(ip);
            unheld.push_back(!held);
        }

        // The actuals, evaluated where the call was written, before this
        // call's own names come into view
        std::vector<VhdlValue> args(parms.size());
        std::vector<bool> given(parms.size(), false);
        size_t at = 0;
        for (AstNode* p = argsp; p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            AstNodeVhdlExpr* const actualp = asp ? asp->actualp() : VN_CAST(p, NodeVhdlExpr);
            if (!actualp) return VhdlValue{};  // Written OPEN
            size_t which = at++;
            if (asp && asp->formalp()) {
                const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
                if (!namep) return VhdlValue{};
                which = parms.size();
                for (size_t i = 0; i < parms.size(); ++i) {
                    if (parms[i]->name() == namep->name()) {
                        which = i;
                        break;
                    }
                }
            }
            if (which >= parms.size() || given[which]) return VhdlValue{};
            if (unheld[which]) {
                given[which] = true;
                continue;
            }
            const VhdlValue val = eval(actualp);
            if (!val.known() && !m_allowImpure) return VhdlValue{};
            args[which] = val;
            given[which] = true;
        }

        // One left out is as unknown as one given
        for (size_t i = 0; i < parms.size(); ++i) {
            if (unheld[i]) given[i] = true;
        }
        // Giving up inside one activation says nothing about the expression
        // that contains the call, so the flag is this call's own
        VL_RESTORER(m_bail);
        m_bail = false;
        ++m_calls;
        m_frames.emplace_back();
        const VhdlValue out = runBody(declp, parms, args, given);
        m_frames.pop_back();
        --m_calls;
        return m_bail ? VhdlValue{} : out;
    }

    VhdlValue evalCall(AstVhdlCall* nodep) {
        // Only a type conversion and the operations the language itself
        // declares fold; any other function call would mean running code,
        // and an indexed name reading a value we do not hold (9.3.6)
        AstNodeVhdlExpr* const prefixp = nodep->prefixp();
        // What a call resolved to is recorded on the call itself; a type
        // conversion names its type through the prefix instead
        const VhdlSymbol* namedp = nodep->vhdlSymp();
        if (!namedp && prefixp) namedp = prefixp->vhdlSymp();
        if (namedp) {
            const VhdlSymbol* const funcp = namedp->resolved();
            if (funcp->kind() == VhdlSymKind::FUNCTION) {
                // A call to a pure function whose actuals are static is static
                // in its turn (9.4.3 i), whoever declared it
                return funcp->predefined() ? evalMinMax(nodep, funcp)
                                           : evalUserCall(nodep->argsp(), funcp);
            }
        }
        VhdlSymbol* const symp = prefixp ? prefixp->vhdlSymp() : nullptr;
        // An indexed name is static where the array it names is and the index
        // is (9.4.2), and reading one element of a value already in hand is
        // not running code. It is how a package writes a table -- "constant
        // POLYS : nat_array := (91, 121)" -- and reads it back in a function
        // that works a bound out.
        if (symp && symp->kind() != VhdlSymKind::TYPE && prefixp) {
            AstVhdlAssoc* const idxp = VN_CAST(nodep->argsp(), VhdlAssoc);
            if (idxp && !idxp->nextp() && !idxp->formalp() && idxp->actualp()) {
                const VhdlValue whole = eval(prefixp);
                if (whole.isArray()) {
                    const VhdlValue idx = eval(idxp->actualp());
                    if (idx.isInt()) return whole.element(idx.toInt());
                    return VhdlValue{};
                }
            }
        }
        if (!symp || symp->kind() != VhdlSymKind::TYPE || !symp->typep()) return VhdlValue{};
        AstVhdlAssoc* const asp = VN_CAST(nodep->argsp(), VhdlAssoc);
        if (!asp || asp->nextp()) return VhdlValue{};
        const VhdlValue arg = eval(asp->actualp());
        if (!arg.known()) return VhdlValue{};
        VhdlType* const top = symp->typep();
        const VhdlTypeKind kind = top->base()->kind();
        if (kind == VhdlTypeKind::FLOATING) return VhdlValue::mkReal(arg.toReal(), top);
        if (kind == VhdlTypeKind::INTEGER || kind == VhdlTypeKind::UNIVERSAL_INTEGER) {
            if (arg.isStr()) return VhdlValue{};
            if (!arg.isReal()) return VhdlValue::mkInt(arg.toInt(), top);
            // Rounding is away from zero at a half (9.3.6)
            const double r = arg.toReal();
            if (!(r >= -9.2e18 && r <= 9.2e18)) return VhdlValue{};
            return VhdlValue::mkInt(static_cast<int64_t>(r < 0 ? r - 0.5 : r + 0.5), top);
        }
        // A conversion between closely related array types keeps the value
        if (kind == VhdlTypeKind::ARRAY && arg.isStr()) {
            return VhdlValue::mkStr(arg.toStr(), top);
        }
        return VhdlValue{};
    }
};

//######################################################################
// Entry points

VhdlValue V3VhdlEval::eval(AstNodeVhdlExpr* nodep, std::string* errp) {
    VhdlEvaluator evaluator{errp};
    return evaluator.eval(nodep);
}

VhdlValue V3VhdlEval::evalElab(AstNodeVhdlExpr* nodep) {
    VhdlEvaluator evaluator{nullptr, true};
    return evaluator.eval(nodep);
}

VhdlValue V3VhdlEval::evalElabWith(AstNodeVhdlExpr* nodep,
                                   const std::map<const VhdlSymbol*, VhdlValue>& bindings) {
    VhdlEvaluator evaluator{nullptr, true};
    return evaluator.evalWith(nodep, bindings);
}

std::string V3VhdlEval::image(const VhdlValue& val) {
    switch (val.kind()) {
    case VhdlValKind::UNKNOWN: return "";
    // A composite has no 'IMAGE of its own before VHDL-2019
    case VhdlValKind::RECORD:
    case VhdlValKind::ARRAY: return "";
    case VhdlValKind::STR: return val.toStr();
    case VhdlValKind::REAL: {
        // 'IMAGE of a real always shows a decimal point, so that the result
        // reads back through 'VALUE (16.2.6)
        // 16.4 asks for the smallest number of digits that represents the
        // value accurately, which is the shortest decimal that reads back as
        // the same value -- the rule the emitted code follows too, so a value
        // that folds and one that does not print alike
        char buf[64];
        const double v = val.toReal();
        for (int prec = 1; prec < 17; ++prec) {
            VL_SNPRINTF(buf, sizeof(buf), "%.*g", prec, v);
            if (std::strtod(buf, nullptr) == v) break;
            if (prec == 16) VL_SNPRINTF(buf, sizeof(buf), "%.17g", v);
        }
        std::string out{buf};
        if (out.find('.') == std::string::npos && out.find('e') == std::string::npos
            && out.find("inf") == std::string::npos && out.find("nan") == std::string::npos) {
            out += ".0";
        }
        return out;
    }
    case VhdlValKind::INT: break;
    }
    VhdlType* const typep = val.typep();
    VhdlType* const basep = typep ? typep->base() : nullptr;
    if (basep && basep->kind() == VhdlTypeKind::ENUM) {
        const std::vector<VhdlSymbol*>& lits = basep->literals();
        if (val.toInt() >= 0 && val.toInt() < static_cast<int64_t>(lits.size())) {
            // A character literal is already named with its quotes, which is
            // exactly how 'IMAGE spells it (16.2.6)
            return lits[val.toInt()]->name();
        }
        return "";
    }
    std::string out = std::to_string(val.toInt());
    if (basep && basep->kind() == VhdlTypeKind::PHYSICAL && !basep->units().empty()) {
        out += " " + basep->units().front().first;
    }
    return out;
}

VhdlValue V3VhdlEval::fromImage(const std::string& text, VhdlType* typep) {
    if (!typep) return VhdlValue{};
    // Leading and trailing whitespace is ignored (16.2.6)
    size_t b = text.find_first_not_of(" \t");
    if (b == std::string::npos) return VhdlValue{};
    size_t e = text.find_last_not_of(" \t");
    const std::string body = text.substr(b, e - b + 1);
    VhdlType* const basep = typep->base();
    switch (basep->kind()) {
    case VhdlTypeKind::INTEGER:
    case VhdlTypeKind::UNIVERSAL_INTEGER: {
        char* endp = nullptr;
        const long long v = std::strtoll(body.c_str(), &endp, 10);
        if (!endp || *endp) return VhdlValue{};
        return VhdlValue::mkInt(static_cast<int64_t>(v), typep);
    }
    case VhdlTypeKind::FLOATING: {
        char* endp = nullptr;
        const double v = std::strtod(body.c_str(), &endp);
        if (!endp || *endp) return VhdlValue{};
        return VhdlValue::mkReal(v, typep);
    }
    case VhdlTypeKind::ENUM: {
        // Case insensitive for an identifier, exact for a character literal
        std::string want = body;
        if (want.size() != 3 || want.front() != '\'' || want.back() != '\'') {
            for (char& c : want) {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
        }
        for (const VhdlSymbol* const symp : basep->literals()) {
            if (symp->name() == want) return VhdlValue::mkInt(symp->position(), typep);
        }
        return VhdlValue{};
    }
    default: return VhdlValue{};
    }
}

bool V3VhdlEval::inRange(const VhdlValue& val, const VhdlType* typep) {
    if (!val.known() || !typep || !typep->bounded() || typep->isError()) return true;
    if (typep->base()->kind() == VhdlTypeKind::FLOATING) {
        if (!val.isReal() && !val.isInt()) return true;
        if (typep->isNull()) return false;
        const double v = val.toReal();
        return v >= typep->lowReal() && v <= typep->highReal();
    }
    if (!val.isInt()) return true;
    if (typep->isNull()) return false;
    return val.toInt() >= typep->low() && val.toInt() <= typep->high();
}

//######################################################################
// Self test

void V3VhdlEval::selfTest() {
    UASSERT_SELFTEST(int64_t, vhdlMod(7, 3), 1);
    UASSERT_SELFTEST(int64_t, vhdlMod(-7, 3), 2);
    UASSERT_SELFTEST(int64_t, vhdlMod(7, -3), -2);
    UASSERT_SELFTEST(int64_t, vhdlMod(-7, -3), -1);
    UASSERT_SELFTEST(int64_t, vhdlRem(7, 3), 1);
    UASSERT_SELFTEST(int64_t, vhdlRem(-7, 3), -1);
    UASSERT_SELFTEST(int64_t, vhdlRem(7, -3), 1);
    UASSERT_SELFTEST(int64_t, vhdlRem(-7, -3), -1);

    int64_t out = 0;
    UASSERT_SELFTEST(bool, addOvf(INT64_MAX, 1, out), false);
    UASSERT_SELFTEST(bool, subOvf(INT64_MIN, 1, out), false);
    UASSERT_SELFTEST(bool, mulOvf(INT64_MAX, 2, out), false);
    UASSERT_SELFTEST(bool, mulOvf(3, 4, out), true);
    UASSERT_SELFTEST(int64_t, out, 12);
    UASSERT_SELFTEST(bool, powOvf(2, 10, out), true);
    UASSERT_SELFTEST(int64_t, out, 1024);
    UASSERT_SELFTEST(bool, powOvf(2, 64, out), false);
    UASSERT_SELFTEST(bool, powOvf(2, -1, out), false);
}

//######################################################################
// Whether the back end can fold an expression

namespace {

// A type the back end has no value for: a handle, not a number
bool unfoldableType(const VhdlType* typep) {
    if (!typep) return false;
    const VhdlTypeKind kind = typep->base()->kind();
    return kind == VhdlTypeKind::ACCESS || kind == VhdlTypeKind::PROTECTED
           || kind == VhdlTypeKind::FILE;
}

bool exprFolds(const AstNode* nodep, std::set<const VhdlSymbol*>& seen, bool inBody);

bool bodyFolds(const VhdlSymbol* symp, std::set<const VhdlSymbol*>& seen) {
    if (!seen.insert(symp).second) return true;  // Already being asked about
    const AstVhdlSubprogDecl* bodyp = VN_CAST(symp->fullDeclp(), VhdlSubprogDecl);
    if (!bodyp || !bodyp->hasBody()) bodyp = VN_CAST(symp->declp(), VhdlSubprogDecl);
    // No body to read means the back end implements the function itself, as
    // it does the mathematics of IEEE.MATH_REAL
    if (!bodyp || !bodyp->hasBody()) return true;
    return exprFolds(bodyp, seen, true);
}

// inBody: a subprogram's own variables are read and written while it runs,
// which the back end does too; it is a variable of something *outside* that
// has no value to fold
bool exprFolds(const AstNode* nodep, std::set<const VhdlSymbol*>& seen, bool inBody) {
    bool folds = true;
    nodep->foreach([&](const AstNodeVhdlExpr* exprp) {
        if (!folds) return;
        if (unfoldableType(exprp->vhdlTypep())) {
            folds = false;
            return;
        }
        const VhdlSymbol* const symp = exprp->vhdlSymp();
        if (!symp) return;
        const VhdlSymbol* const realp = symp->resolved();
        switch (realp->kind()) {
        case VhdlSymKind::FUNCTION:
        case VhdlSymKind::PROCEDURE: {
            // An operator the language itself defines is one the back end
            // works out directly; a foreign one it cannot work out at all
            if (realp->predefined()) break;
            if (realp->isForeign()) {
                folds = false;
                break;
            }
            const AstVhdlSubprogDecl* const subp = VN_CAST(realp->declp(), VhdlSubprogDecl);
            if (!subp || !subp->vhdlPure() || !bodyFolds(realp, seen)) folds = false;
            break;
        }
        case VhdlSymKind::VARIABLE:
            if (!inBody) folds = false;
            break;
        case VhdlSymKind::SHARED_VARIABLE:
        case VhdlSymKind::SIGNAL:
        case VhdlSymKind::PORT:
        case VhdlSymKind::FILE: folds = false; break;
        default: break;
        }
    });
    return folds;
}

}  // namespace

bool V3VhdlEval::backEndFolds(const AstNode* nodep) {
    if (!nodep) return false;
    std::set<const VhdlSymbol*> seen;
    return exprFolds(nodep, seen, false);
}
