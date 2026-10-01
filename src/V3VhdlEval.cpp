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
    // An element nothing could work out -- what an impure function gives,
    // in "(ref => get(8))" -- compares with nothing: read as an integer it
    // was zero, and a record holding one folded to unequal
    if (!lhs.known() || !rhs.known()) return false;
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
        // Two records are equal when every element is, so one pair of
        // elements known to differ settles it whatever the others hold: a
        // verification library's null and default handles differ in a kind
        // beside an actor no one can work out
        bool undecided = false;
        for (size_t i = 0; i < lhs.elements(); ++i) {
            int sub = 0;
            if (!compareValues(lhs.elementAt(i), rhs.elementAt(i), sub)) {
                undecided = true;
                continue;
            }
            if (sub != 0) {
                out = 1;
                return true;
            }
        }
        if (undecided) return false;
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
    // Whether a number a generic of the root entity is given is worked out
    // here. A bound of an object's own subtype leaves it to the parameter
    // it becomes: a null range written over one is then the same object as
    // the port it is connected to, which keeps it over the parameter.
    const bool m_rootNumbers;
    // What allocators made while elaborating (9.3.7). An access value is held
    // as an integer of its access type: the object's place here plus one, and
    // zero for null. Nothing here outlives the evaluation, and no access value
    // is handed out of it (stripAccess).
    std::vector<VhdlValue> m_heap;
    // A procedure this evaluation could not run changed what it changed in
    // shared variables, so none of them is read from here on
    bool m_sharedStale = false;

    // A constant folded from a self-referential declaration would recurse
    // forever; analysis reports the cycle separately, so just stop
    static constexpr int MAX_DEPTH = 64;
    // A body may name a function that names a body, and a loop may be written
    // so that it never ends. Neither is a reason to hang: a fold that runs out
    // of either budget is simply not one that happened. Elaboration runs text
    // functions a character at a time -- VUnit decodes a configuration string
    // with split, replace and strip -- so it is given room for that.
    static constexpr int MAX_CALLS = 16;
    static constexpr int MAX_STEPS = 20000;
    static constexpr int MAX_ELAB_STEPS = 4000000;
    static constexpr size_t MAX_HEAP = 100000;
    int maxSteps() const { return m_allowImpure ? MAX_ELAB_STEPS : MAX_STEPS; }

public:
    // CONSTRUCTORS
    explicit VhdlEvaluator(std::string* errp, bool allowImpure = false, bool rootNumbers = true)
        : m_errp{errp}
        , m_allowImpure{allowImpure}
        , m_rootNumbers{rootNumbers} {}

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
            // "p.all" is what an access value designates (8.3)
            if (p->isAll()) return m_allowImpure ? deref(eval(p->prefixp())) : VhdlValue{};
            // "r.f" where r has a value of its own is that value's element;
            // where the name denotes an object in a package it is that object
            if (const AstVhdlName* const suffixp = VN_CAST(p->suffixp(), VhdlName)) {
                VhdlValue rec = eval(p->prefixp());
                if (m_allowImpure && isAccessValue(rec)) rec = deref(rec);
                if (rec.isRecord()) return rec.field(suffixp->name());
            }
            return evalSymbol(p->vhdlSymp());
        }
        if (AstVhdlAllocator* const p = VN_CAST(nodep, VhdlAllocator)) return evalAllocator(p);
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

    // Access values, text and arrays of them, as elaboration holds them

    static bool isAccessType(const VhdlType* typep) {
        return typep && typep->base()->kind() == VhdlTypeKind::ACCESS;
    }
    static bool isAccessValue(const VhdlValue& val) {
        return val.isInt() && isAccessType(val.typep());
    }
    // The object an access value designates, to be read or written in place
    VhdlValue* heapObject(const VhdlValue& val) {
        if (!isAccessValue(val) || val.toInt() <= 0
            || val.toInt() > static_cast<int64_t>(m_heap.size())) {
            return nullptr;
        }
        return &m_heap[static_cast<size_t>(val.toInt() - 1)];
    }
    VhdlValue deref(const VhdlValue& val) {
        const VhdlValue* const objp = heapObject(val);
        return objp ? *objp : VhdlValue{};
    }
    VhdlValue allocate(const VhdlValue& obj, VhdlType* accessTypep) {
        if (!obj.known() || m_heap.size() >= MAX_HEAP) return VhdlValue{};
        m_heap.push_back(obj);
        return VhdlValue::mkInt(static_cast<int64_t>(m_heap.size()), accessTypep);
    }
    // A value handed out of the evaluation keeps no access value: the heap it
    // points into is gone
public:
    static VhdlValue stripAccess(const VhdlValue& val) {
        if (isAccessValue(val)) return VhdlValue{};
        if (val.isRecord()) {
            std::vector<std::pair<std::string, VhdlValue>> fields;
            for (size_t i = 0; i < val.elements(); ++i) {
                fields.emplace_back("", stripAccess(val.elementAt(i)));
            }
            VhdlValue out = val;
            out.ownFields();
            for (const VhdlSymbol* const ep :
                 val.typep() ? val.typep()->base()->elements() : std::vector<VhdlSymbol*>{}) {
                out.setField(ep->name(), stripAccess(val.field(ep->name())));
            }
            return out;
        }
        if (val.isArray()) {
            for (size_t i = 0; i < val.elements(); ++i) {
                if (isAccessValue(val.elementAt(i))) return VhdlValue{};
            }
        }
        return val;
    }

private:
    static bool isCharType(const VhdlType* typep) {
        const VhdlType* const charp = V3VhdlStd::characterType();
        return typep && charp && typep->base() == charp->base();
    }
    // A one-dimensional array of CHARACTER, which is held as text
    static bool isTextType(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return false;
        if (typep->base()->indexTypes().size() != 1) return false;
        const VhdlType* const elemp
            = typep->elementp() ? typep->elementp() : typep->base()->elementp();
        return isCharType(elemp);
    }
    // Where an unconstrained array type's values start: its index subtype's
    // left bound, POSITIVE'LEFT for STRING (5.3.2.2)
    static int64_t typeLeft(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return 1;
        const std::vector<VhdlType*>& idx
            = typep->indexTypes().empty() ? typep->base()->indexTypes() : typep->indexTypes();
        return !idx.empty() && idx[0] && idx[0]->bounded() ? idx[0]->left() : 1;
    }
    static int64_t valLength(const VhdlValue& val) {
        return val.isStr() ? static_cast<int64_t>(val.toStr().size())
                           : static_cast<int64_t>(val.elements());
    }
    static int64_t valRight(const VhdlValue& val) {
        const int64_t len = valLength(val);
        return val.arrayAscending() ? val.arrayLeft() + len - 1 : val.arrayLeft() - len + 1;
    }
    // One element of text or of an array, at its own index
    static VhdlValue elementOf(const VhdlValue& whole, int64_t index) {
        if (whole.isArray()) return whole.element(index);
        if (!whole.isStr()) return VhdlValue{};
        const int64_t off
            = whole.arrayAscending() ? index - whole.arrayLeft() : whole.arrayLeft() - index;
        if (off < 0 || off >= valLength(whole)) return VhdlValue{};
        return VhdlValue::mkInt(
            static_cast<unsigned char>(whole.toStr()[static_cast<size_t>(off)]),
            V3VhdlStd::characterType());
    }
    // A slice keeps the indices it was taken at, and runs the way the value
    // does; a null slice is empty whichever way it is written (8.5)
    static VhdlValue sliceOf(const VhdlValue& whole, int64_t left, int64_t right, bool ascending) {
        const int64_t len = ascending ? right - left + 1 : left - right + 1;
        if (len <= 0) {
            if (whole.isStr()) return VhdlValue::mkStrAt("", left, ascending, whole.typep());
            return VhdlValue::mkArray({}, left, ascending, whole.typep());
        }
        if (ascending != whole.arrayAscending()) return VhdlValue{};
        const int64_t off
            = whole.arrayAscending() ? left - whole.arrayLeft() : whole.arrayLeft() - left;
        if (off < 0 || off + len > valLength(whole)) return VhdlValue{};
        if (whole.isStr()) {
            return VhdlValue::mkStrAt(
                whole.toStr().substr(static_cast<size_t>(off), static_cast<size_t>(len)), left,
                ascending, whole.typep());
        }
        std::vector<VhdlValue> elems;
        for (int64_t i = 0; i < len; ++i) elems.push_back(whole.elementAt(off + i));
        return VhdlValue::mkArray(std::move(elems), left, ascending, whole.typep());
    }
    // The bounds of a constrained array type, working out any that did not
    // fold when it was declared -- "string(1 to s'length)" in a body run for
    // one call
    bool boundsOf(const VhdlType* typep, int64_t& left, int64_t& right, bool& ascending) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY
            || typep->indexTypes().size() != 1 || !typep->indexTypes()[0]) {
            return false;
        }
        const VhdlType* const idxp = typep->indexTypes()[0];
        if (typep->isConstrainedArray() && idxp->bounded()) {
            left = idxp->left();
            right = idxp->right();
            ascending = idxp->ascending();
            return true;
        }
        AstVhdlRange* const rp = VN_CAST(idxp->boundsp(), VhdlRange);
        if (!rp) return false;
        return rangeValues(rp, left, right, ascending);
    }
    // The values a range runs over: its bounds, or those of the object whose
    // 'RANGE it is
    bool rangeValues(AstVhdlRange* rp, int64_t& left, int64_t& right, bool& ascending) {
        if (rp->leftp() && rp->rightp()) {
            const VhdlValue l = eval(rp->leftp());
            const VhdlValue r = eval(rp->rightp());
            if (!l.isInt() || !r.isInt()) return false;
            left = l.toInt();
            right = r.toInt();
            ascending = rp->dir() == VhdlDir::TO;
            return true;
        }
        return attrRangeValues(VN_CAST(rp->attrp(), VhdlAttrName), left, right, ascending);
    }
    // The values "x'range" runs over, where x is text or an array held here
    bool attrRangeValues(AstVhdlAttrName* attrp, int64_t& left, int64_t& right, bool& ascending) {
        if (!attrp || !m_allowImpure || attrp->argp()) return false;
        const bool reverse = attrp->name() == "reverse_range";
        if (!reverse && attrp->name() != "range") return false;
        VhdlValue val = eval(attrp->prefixp());
        if (isAccessValue(val)) val = deref(val);
        if (!val.isStr() && !val.isArray()) return false;
        left = val.arrayLeft();
        right = valRight(val);
        ascending = val.arrayAscending();
        if (reverse) {
            std::swap(left, right);
            ascending = !ascending;
        }
        return true;
    }
    // The value an object of this subtype starts with where none is written
    // (6.4.2.3): null for an access type, and for text and arrays the left
    // value of their element in every place
    VhdlValue defaultOf(VhdlType* typep) {
        if (!typep) return VhdlValue{};
        if (isAccessType(typep)) return VhdlValue::mkInt(0, typep);
        if (typep->base()->kind() == VhdlTypeKind::ARRAY) {
            int64_t left = 0;
            int64_t right = 0;
            bool ascending = true;
            if (!boundsOf(typep, left, right, ascending)) return VhdlValue{};
            const int64_t len = ascending ? right - left + 1 : left - right + 1;
            if (len > (1 << 20)) return VhdlValue{};
            if (isTextType(typep)) {
                return VhdlValue::mkStrAt(
                    std::string(static_cast<size_t>(std::max<int64_t>(len, 0)), '\0'), left,
                    ascending, typep);
            }
            VhdlType* const elemp
                = typep->elementp() ? typep->elementp() : typep->base()->elementp();
            const VhdlValue elem = defaultOf(elemp);
            if (!elem.known()) return VhdlValue{};
            std::vector<VhdlValue> elems(static_cast<size_t>(std::max<int64_t>(len, 0)), elem);
            return VhdlValue::mkArray(std::move(elems), left, ascending, typep);
        }
        if (scalarValued(typep) && typep->bounded()
            && typep->base()->kind() != VhdlTypeKind::FLOATING) {
            return VhdlValue::mkInt(typep->left(), typep);
        }
        return VhdlValue{};
    }
    // A value given to an object of a constrained subtype takes the object's
    // bounds, element for element (10.6.2.1); where the lengths differ the
    // assignment is an error, and nothing is held
    VhdlValue conformTo(const VhdlValue& val, VhdlType* typep) {
        if (!val.isStr() && !val.isArray()) return val;
        int64_t left = 0;
        int64_t right = 0;
        bool ascending = true;
        if (!typep || !boundsOf(typep, left, right, ascending)) return val;
        const int64_t len = ascending ? right - left + 1 : left - right + 1;
        if (std::max<int64_t>(len, 0) != valLength(val)) return VhdlValue{};
        VhdlValue out = val;
        if (out.isArray()) out.ownFields();
        out.setBounds(left, ascending);
        return out;
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
            if (m_allowImpure && symp->kind() == VhdlSymKind::GENERIC && !symp->value().known()) {
                return rootGenericValue(symp);
            }
            return symp->value();
        case VhdlSymKind::PARAMETER:
            // A parameter has a value only in a copy of a subprogram made for
            // one call site, which is where the call's own values were fixed
            return symp->value();
        case VhdlSymKind::SHARED_VARIABLE:
            if (m_sharedStale) return VhdlValue{};
            return V3VhdlEval::s_sharedInitValues ? symp->elabValue() : VhdlValue{};
        case VhdlSymKind::FUNCTION:
            // A function taking no parameters is called by writing its name
            // with no argument list at all (4.2.2.1)
            return evalUserCall(nullptr, symp);
        default: return VhdlValue{};
        }
    }

    // What a generic of the design's root entity is elaborated with: the value
    // the command line gives it -- text between quotes, a number as digits --
    // or else its default (14.2). A generic of any other entity is given its
    // value by the instance, which hands it in as a binding.
    VhdlValue rootGenericValue(const VhdlSymbol* symp) {
        const AstVhdlIfaceDecl* const ifp = VN_CAST(symp->declp(), VhdlIfaceDecl);
        if (!ifp) return VhdlValue{};
        const bool text = isTextType(symp->typep());
        const bool number
            = symp->typep() && symp->typep()->base()->kind() == VhdlTypeKind::INTEGER;
        if (!text && !(number && m_rootNumbers)) return VhdlValue{};
        const AstVhdlEntity* entp = nullptr;
        for (const AstNode* up = ifp->backp(); up && !entp; up = up->backp()) {
            entp = VN_CAST(up, VhdlEntity);
        }
        const std::string top = v3Global.opt.topModule();
        if (!entp || top.empty() || VString::downcase(entp->name()) != VString::downcase(top)) {
            return VhdlValue{};
        }
        const std::string given = v3Global.opt.peekParameterCaseless(symp->name());
        if (!given.empty()) {
            if (number) {
                // Decimal digits, as the command line writes a number
                char* endp = nullptr;
                const long long v = std::strtoll(given.c_str(), &endp, 10);
                if (!endp || *endp || endp == given.c_str()) return VhdlValue{};
                return VhdlValue::mkInt(v, symp->typep());
            }
            if (given.size() < 2 || given.front() != '"' || given.back() != '"') {
                return VhdlValue{};
            }
            return VhdlValue::mkStrAt(given.substr(1, given.size() - 2), typeLeft(symp->typep()),
                                      true, symp->typep());
        }
        return ifp->defaultp() ? eval(ifp->defaultp()) : VhdlValue{};
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

    // "new T'(value)" or "new T(constraint)", made while elaborating (9.3.7)
    VhdlValue evalAllocator(AstVhdlAllocator* nodep) {
        if (!m_allowImpure) return VhdlValue{};
        VhdlType* const accessTypep = nodep->vhdlTypep();
        if (!isAccessType(accessTypep)) return VhdlValue{};
        if (nodep->valuep()) {
            VhdlValue val = eval(nodep->valuep());
            // A qualified text value with no constraint of its own starts
            // where its type's index does
            return allocate(val, accessTypep);
        }
        VhdlType* typep = nodep->subtypep() ? nodep->subtypep()->vhdlTypep() : nullptr;
        if (!typep) typep = accessTypep->base()->designatedp();
        return allocate(defaultOf(typep), accessTypep);
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
            // applied the length given, so both kinds carry their characters.
            // Unconstrained, it starts where its type's index does (9.3.2).
            return VhdlValue::mkStrAt(nodep->text(), typeLeft(typep), true, typep);
        case VhdlLitKind::NULLLIT:
            // The one value of every access type that designates nothing
            return m_allowImpure && isAccessType(typep) ? VhdlValue::mkInt(0, typep) : VhdlValue{};
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
            // An unconstrained result starts where its left operand does, or
            // at its type's left where that operand is null (9.2.5)
            const int64_t left = lhs.isStr() && !l.empty() ? lhs.arrayLeft() : typeLeft(resp);
            return VhdlValue::mkStrAt(l + r, left, true, resp);
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

    // The range the declaration of an attribute's prefix was constrained
    // with, for the dimension the attribute asks about. Null where the
    // prefix is not a whole object declared with an index constraint.
    const AstVhdlRange* declaredRange(AstVhdlAttrName* nodep) {
        if (!VN_IS(nodep->prefixp(), VhdlName)) return nullptr;
        const VhdlSymbol* const symp = nodep->prefixp()->vhdlSymp();
        if (!symp) return nullptr;
        const AstVhdlObjectDecl* const declp = VN_CAST(symp->resolved()->declp(), VhdlObjectDecl);
        if (!declp || !declp->subtypep()) return nullptr;
        // "std_logic_vector(n - 1 downto 0)" is kept the way it parsed, as a
        // call of the type mark with the range for its argument
        AstNode* rp = nullptr;
        if (const AstVhdlIndexConstraint* const consp
            = VN_CAST(declp->subtypep()->constraintp(), VhdlIndexConstraint)) {
            rp = consp->rangesp();
        } else if (const AstVhdlCall* const callp
                   = VN_CAST(declp->subtypep()->typeMarkp(), VhdlCall)) {
            rp = callp->argsp();
        }
        int64_t dim = 0;
        if (nodep->argp()) {
            const VhdlValue arg = eval(nodep->argp());
            if (!arg.isInt() || arg.toInt() < 1) return nullptr;
            dim = arg.toInt() - 1;
        }
        for (; rp && dim > 0; --dim) rp = rp->nextp();
        return VN_CAST(rp, VhdlRange);
    }

    // A range attribute of a range as it was written, its bounds worked out
    // here (16.2.4)
    VhdlValue writtenRangeAttr(const std::string& attr, const AstVhdlRange* rp) {
        if (!rp || !rp->leftp() || !rp->rightp() || rp->dirUnknown()) return VhdlValue{};
        const VhdlValue lv = eval(rp->leftp());
        const VhdlValue rv = eval(rp->rightp());
        if (!lv.isInt() || !rv.isInt()) return VhdlValue{};
        const int64_t left = lv.toInt();
        const int64_t right = rv.toInt();
        const bool ascending = rp->dir() == VhdlDir::TO;
        const int64_t low = ascending ? left : right;
        const int64_t high = ascending ? right : left;
        VhdlType* const intp = V3VhdlStd::integerType();
        if (attr == "left") return VhdlValue::mkInt(left, intp);
        if (attr == "right") return VhdlValue::mkInt(right, intp);
        if (attr == "high") return VhdlValue::mkInt(high, intp);
        if (attr == "low") return VhdlValue::mkInt(low, intp);
        if (attr == "ascending") return mkBool(ascending);
        if (attr == "length") return VhdlValue::mkInt(high < low ? 0 : high - low + 1, intp);
        return VhdlValue{};
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
                    // The value of an array of another enumeration -- "101" for a
                    // BIT_VECTOR -- is held as the characters its elements are written
                    // with, so the element is the literal the character names
                    VhdlType* const elemp = atp ? atp->base()->elementp() : nullptr;
                    const VhdlType* const charp = V3VhdlStd::characterType();
                    if (elemp && charp && elemp->base() != charp->base()
                        && elemp->base()->kind() == VhdlTypeKind::ENUM) {
                        const std::string want{'\'', text[off], '\''};
                        for (const VhdlSymbol* const litp : elemp->base()->literals()) {
                            if (litp->name() == want) {
                                return VhdlValue::mkInt(litp->position(), elemp);
                            }
                        }
                        return VhdlValue{};
                    }
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
            // Under a design entity the path runs through the instances above
            // it, which differ from one instance to the next: the design
            // carries it, and it is not one value here
            for (const VhdlScope* scopep = symp->declScopep(); scopep;
                 scopep = scopep->parentp()) {
                if (scopep->instPrefix().empty()) continue;
                if (scopep->designRoot()) return VhdlValue{};
                break;
            }
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

        // Elaboration asks text and arrays it holds about the bounds they were
        // built with: "s'length" of a parameter is the actual's (4.2.2.2). A
        // user-defined attribute's value is static, and is asked the same way
        // at any time: "i'a'left" (7.2)
        const AstVhdlAttrName* const pattrp = VN_CAST(nodep->prefixp(), VhdlAttrName);
        const bool userAttrPrefix
            = pattrp && pattrp->vhdlSymp() && pattrp->vhdlSymp()->kind() == VhdlSymKind::ATTRIBUTE;
        if ((m_allowImpure || userAttrPrefix) && nodep->prefixp() && !nodep->argp()
            && (attr == "left" || attr == "right" || attr == "high" || attr == "low"
                || attr == "ascending" || attr == "length")) {
            const VhdlSymbol* const psymp = nodep->prefixp()->vhdlSymp();
            if (!psymp || psymp->kind() != VhdlSymKind::TYPE) {
                VhdlValue val = eval(nodep->prefixp());
                if (isAccessValue(val)) val = deref(val);
                if (val.isStr() || val.isArray()) {
                    const int64_t left = val.arrayLeft();
                    const int64_t right = valRight(val);
                    const bool ascending = val.arrayAscending();
                    VhdlType* const intp = V3VhdlStd::integerType();
                    if (attr == "length") return VhdlValue::mkInt(valLength(val), intp);
                    if (attr == "left") return VhdlValue::mkInt(left, intp);
                    if (attr == "right") return VhdlValue::mkInt(right, intp);
                    if (attr == "ascending") return mkBool(ascending);
                    if (attr == "high") return VhdlValue::mkInt(ascending ? right : left, intp);
                    return VhdlValue::mkInt(ascending ? left : right, intp);
                }
            }
        }

        VhdlType* typep = prefixType(nodep);
        if (!typep || typep->isError()) return VhdlValue{};

        // Attributes of an array ask about one of its index ranges; those of a
        // scalar type about the type's own range (16.2.3, 16.2.4)
        const bool isArray = typep->base()->kind() == VhdlTypeKind::ARRAY;
        if (attr == "left" || attr == "right" || attr == "high" || attr == "low"
            || attr == "ascending" || attr == "length" || attr == "range"
            || attr == "reverse_range") {
            // An object whose constraint names a generic of the root --
            // "signal tdata : std_logic_vector(data_length - 1 downto 0)" --
            // was left the unconstrained type by analysis, and only its
            // declaration says what the bounds are. VUnit builds its protocol
            // checker from "tdata'length" while elaborating.
            if (m_allowImpure && isArray && typep->unconstrained()) {
                return writtenRangeAttr(attr, declaredRange(nodep));
            }
            VhdlType* const rangep = isArray ? indexTypeOf(nodep, typep) : typep;
            if (!rangep) return VhdlValue{};
            // Bounds analysis left as written, since they name a generic of
            // the root, are worked out here, where the root's generics have
            // values
            if (!rangep->bounded()) {
                if (!m_allowImpure) return VhdlValue{};
                return writtenRangeAttr(attr, VN_CAST(rangep->boundsp(), VhdlRange));
            }
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
        // A condition of another type than BOOLEAN goes through the
        // condition operator (9.2.9): "if x then" over a STD_ULOGIC is true
        // for '1' and 'H', which are not where TRUE is. A pure function
        // called with '1' is folded here, and answered FALSE.
        const VhdlType* const typep = val.typep() ? val.typep()->base() : nullptr;
        if (typep && typep->kind() == VhdlTypeKind::ENUM) {
            const std::vector<VhdlSymbol*>& lits = typep->literals();
            const int64_t pos = val.toInt();
            if (pos >= 0 && pos < static_cast<int64_t>(lits.size()) && lits[pos]) {
                const std::string& name = lits[pos]->name();
                if (name == "'1'" || name == "'H'") return true;
                if (name == "'0'" || name == "'L'" || name.size() == 3) return false;
            }
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
        if (++m_steps > maxSteps()) return bail();
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
            const VhdlSymbol* const symp = targetSym(p->targetp());
            // Elaboration also writes an element, a slice or what an access
            // value designates, in place (assignInPlace)
            const bool inPlace = m_allowImpure && !VN_IS(p->targetp(), VhdlName);
            if (!symp && !inPlace) return bail();
            // "v := a when c else b" and "with s select v := ..." stand for
            // the if and case statements they are written as (10.6.3,
            // 10.6.4): the value is the one the first alternative that holds
            // gives. VUnit's new_bus writes "p_actor := actor when actor /=
            // null_actor else new_actor".
            AstNodeVhdlExpr* valuep = p->valuep();
            if (p->kind() != VhdlAssignKind::SIMPLE) {
                valuep = nullptr;
                VhdlValue sel;
                if (p->kind() == VhdlAssignKind::SELECTED) {
                    if (p->matching() || !p->selectorp()) return bail();
                    sel = eval(p->selectorp());
                    if (!sel.known()) return bail();
                }
                for (AstNode* ap = p->altsp(); ap && !valuep; ap = ap->nextp()) {
                    AstVhdlAlternative* const altp = VN_CAST(ap, VhdlAlternative);
                    if (!altp || !altp->wavesp()) return bail();
                    bool taken = true;
                    if (p->kind() == VhdlAssignKind::CONDITIONAL) {
                        if (altp->condp()) {
                            bool ok = true;
                            taken = condTrue(altp->condp(), ok);
                            if (!ok) {
                                // Which value it is, elaboration cannot tell;
                                // the variable is not known, as it would not
                                // be had the value itself been unknown
                                if (!m_allowImpure || !symp) return bail();
                                m_frames.back()[symp] = VhdlValue{};
                                return Flow::NORMAL;
                            }
                        }
                    } else {
                        taken = false;
                        for (AstNode* cp = altp->choicesp(); cp && !taken; cp = cp->nextp()) {
                            AstVhdlChoice* const chp = VN_CAST(cp, VhdlChoice);
                            if (!chp) return bail();
                            bool ok = true;
                            taken = choiceMatches(chp, sel, ok);
                            if (!ok) return bail();
                        }
                    }
                    if (taken) valuep = altp->wavesp()->valuep();
                }
                // No alternative holds: the variable is left as it was
                if (!valuep) return Flow::NORMAL;
            }
            const VhdlValue val = eval(valuep);
            // Elaboration carries an unknown on rather than giving up on the
            // whole activation: what a handle's actor is decides nothing
            // about the widths beside it
            if (!val.known() && !m_allowImpure) return bail();
            if (inPlace) return assignInPlace(p->targetp(), val) ? Flow::NORMAL : bail();
            // A variable keeps its own bounds, and takes the value's elements
            // in order (10.6.2.1)
            if (m_allowImpure) {
                const auto it = m_frames.back().find(symp);
                if (it != m_frames.back().end() && (it->second.isStr() || it->second.isArray())
                    && symp->typep() && symp->typep()->isConstrainedArray()) {
                    m_frames.back()[symp] = conformTo(val, symp->typep());
                    return Flow::NORMAL;
                }
            }
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
        if (AstVhdlProcCall* const p = VN_CAST(nodep, VhdlProcCall)) {
            if (m_allowImpure && (execTextio(p) || execUserProc(p))) return Flow::NORMAL;
            // A procedure that hands nothing back through its parameters
            // changes only what is shared -- VUnit's "set(handle.p_config,
            // ...)" writes a protected store -- and nothing this evaluation
            // holds depends on that once shared values are no longer read
            if (m_allowImpure && writesNoParameter(p)) {
                m_sharedStale = true;
                return Flow::NORMAL;
            }
            return bail();
        }
        // A wait or a signal assignment cannot appear in a function at all,
        // and a procedure call would have to carry values back out
        return bail();
    }

    // Whether a procedure call has no parameter of mode out or inout, and no
    // signal or file parameter, so that what it does reaches nothing a caller
    // holds but what is shared
    static bool writesNoParameter(AstVhdlProcCall* nodep) {
        AstNodeVhdlExpr* const callExprp = nodep->callp();
        const VhdlSymbol* const symp = callExprp ? callExprp->vhdlSymp() : nullptr;
        if (!symp) return false;
        const VhdlSymbol* const procp = symp->resolved();
        if (procp->kind() != VhdlSymKind::PROCEDURE) return false;
        const AstVhdlSubprogDecl* const declp
            = procp->bodyp() ? procp->bodyp() : VN_CAST(procp->declp(), VhdlSubprogDecl);
        if (!declp) return false;
        for (AstNode* p = declp->paramsp(); p; p = p->nextp()) {
            const AstVhdlIfaceDecl* const ip = VN_CAST(p, VhdlIfaceDecl);
            if (!ip) return false;
            if (ip->kind() != VhdlIfaceKind::CONSTANT && ip->kind() != VhdlIfaceKind::VARIABLE) {
                return false;
            }
            if (ip->mode() != VhdlMode::IN) return false;
        }
        return true;
    }

    // The object a variable target names, to be written in place: one of
    // this activation's, an element of one, or what an access value in one
    // designates. Null where it is none of these.
    VhdlValue* targetObject(AstNodeVhdlExpr* targetp) {
        if (AstVhdlName* const namep = VN_CAST(targetp, VhdlName)) {
            if (!namep->vhdlSymp() || m_frames.empty()) return nullptr;
            const auto it = m_frames.back().find(namep->vhdlSymp()->resolved());
            return it == m_frames.back().end() ? nullptr : &it->second;
        }
        if (AstVhdlSelected* const selp = VN_CAST(targetp, VhdlSelected)) {
            if (!selp->isAll()) return nullptr;
            VhdlValue* const accp = targetObject(selp->prefixp());
            return accp ? heapObject(*accp) : nullptr;
        }
        AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall);
        if (!callp) return nullptr;
        AstVhdlAssoc* const idxp = VN_CAST(callp->argsp(), VhdlAssoc);
        if (!idxp || idxp->nextp() || idxp->formalp() || !idxp->actualp()) return nullptr;
        const VhdlValue idx = eval(idxp->actualp());
        if (!idx.isInt()) return nullptr;
        VhdlValue* wholep = targetObject(callp->prefixp());
        if (wholep && isAccessValue(*wholep)) wholep = heapObject(*wholep);
        if (!wholep || !wholep->isArray()) return nullptr;
        wholep->ownFields();
        return wholep->elementPtr(idx.toInt());
    }
    // "x(i) := v", "x.all := v", "x(i to j) := v": the value is written into
    // the places the target names, which keep their own bounds
    bool assignInPlace(AstNodeVhdlExpr* targetp, const VhdlValue& val) {
        if (AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall)) {
            VhdlValue* wholep = targetObject(callp->prefixp());
            if (wholep && isAccessValue(*wholep)) wholep = heapObject(*wholep);
            if (wholep && wholep->isStr()) {
                // One character, or a run of them, of text
                std::string text = wholep->toStr();
                int64_t left = 0;
                int64_t right = 0;
                bool ascending = true;
                if (AstVhdlRange* const rp = VN_CAST(callp->argsp(), VhdlRange)) {
                    if (!rangeValues(rp, left, right, ascending) || !val.isStr()) return false;
                } else {
                    AstVhdlAssoc* const idxp = VN_CAST(callp->argsp(), VhdlAssoc);
                    const VhdlValue idx
                        = idxp && idxp->actualp() ? eval(idxp->actualp()) : VhdlValue{};
                    std::string one;
                    if (!idx.isInt() || !asCharString(val, one) || one.size() != 1) return false;
                    left = right = idx.toInt();
                    ascending = wholep->arrayAscending();
                    const int64_t off
                        = ascending ? left - wholep->arrayLeft() : wholep->arrayLeft() - left;
                    if (off < 0 || off >= valLength(*wholep)) return false;
                    text[static_cast<size_t>(off)] = one[0];
                    wholep->setStr(text);
                    return true;
                }
                const int64_t len = ascending ? right - left + 1 : left - right + 1;
                if (std::max<int64_t>(len, 0) != valLength(val)) return false;
                if (len <= 0) return true;
                if (ascending != wholep->arrayAscending()) return false;
                const int64_t off
                    = ascending ? left - wholep->arrayLeft() : wholep->arrayLeft() - left;
                if (off < 0 || off + len > valLength(*wholep)) return false;
                text.replace(static_cast<size_t>(off), static_cast<size_t>(len), val.toStr());
                wholep->setStr(text);
                return true;
            }
        }
        VhdlValue* const objp = targetObject(targetp);
        if (!objp) return false;
        if ((objp->isStr() || objp->isArray()) && (val.isStr() || val.isArray())) {
            // The target keeps its own bounds, and the lengths must agree
            if (valLength(*objp) != valLength(val)) return false;
            VhdlValue out = val;
            if (out.isArray()) out.ownFields();
            out.setBounds(objp->arrayLeft(), objp->arrayAscending());
            *objp = out;
            return true;
        }
        *objp = val;
        return true;
    }
    // TEXTIO's WRITE of text to a LINE, and DEALLOCATE, which a function
    // building text writes with (16.4): "write(ret_val(i), s(a to b))"
    bool execTextio(AstVhdlProcCall* nodep) {
        AstVhdlCall* const callp = VN_CAST(nodep->callp(), VhdlCall);
        if (!callp || !callp->vhdlSymp()) return false;
        const VhdlSymbol* const procp = callp->vhdlSymp()->resolved();
        const std::string& name = procp->name();
        std::vector<AstNodeVhdlExpr*> actuals;
        for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            // Written by position only; a named association is not taken
            if (!asp || asp->formalp() || !asp->actualp()) return false;
            actuals.push_back(asp->actualp());
        }
        if (actuals.empty()) return false;
        VhdlValue* const linep = targetObject(actuals[0]);
        if (!linep || !isAccessValue(*linep)) return false;
        if (name == "deallocate" && actuals.size() == 1) {
            *linep = VhdlValue::mkInt(0, linep->typep());
            return true;
        }
        if (name != "write" || actuals.size() != 2) return false;
        const VhdlType* const desigp = linep->typep()->base()->designatedp();
        if (!isTextType(desigp)) return false;
        const VhdlValue val = eval(actuals[1]);
        std::string text;
        if (!val.isStr() || !asCharString(val, text)) return false;
        const VhdlValue old = deref(*linep);
        const std::string before = old.isStr() ? old.toStr() : "";
        VhdlType* const accessTypep = linep->typep();
        // A LINE written to holds all it held and then the text, from 1. The
        // new object moves the heap, so the target is found again after it.
        const VhdlValue newp = allocate(
            VhdlValue::mkStrAt(before + text, 1, true, old.typep() ? old.typep() : val.typep()),
            accessTypep);
        if (!newp.known()) return false;
        VhdlValue* const againp = targetObject(actuals[0]);
        if (!againp) return false;
        *againp = newp;
        return true;
    }
    // A call of a procedure the design declares, run while elaborating: its
    // constant and variable parameters, each out or inout one written back
    // to its actual when the body is done (4.2.2.1). A procedure given a
    // signal or a file is not run.
    bool execUserProc(AstVhdlProcCall* nodep) {
        AstNodeVhdlExpr* const callExprp = nodep->callp();
        AstVhdlCall* const callp = VN_CAST(callExprp, VhdlCall);
        const VhdlSymbol* const symp = callp       ? callp->vhdlSymp()
                                       : callExprp ? callExprp->vhdlSymp()
                                                   : nullptr;
        if (!symp) return false;
        const VhdlSymbol* const procp = symp->resolved();
        if (procp->kind() != VhdlSymKind::PROCEDURE || procp->predefined() || procp->isForeign()) {
            return false;
        }
        AstVhdlSubprogDecl* const declp = procp->bodyp();
        if (!declp || declp->isFunction() || m_calls >= MAX_CALLS) return false;
        std::vector<AstVhdlIfaceDecl*> parms;
        int skip = declp->genericCount();
        for (AstNode* p = declp->paramsp(); p; p = p->nextp()) {
            AstVhdlIfaceDecl* const ip = VN_CAST(p, VhdlIfaceDecl);
            if (!ip) return false;
            if (skip > 0) {
                --skip;
                continue;
            }
            if (ip->kind() != VhdlIfaceKind::CONSTANT && ip->kind() != VhdlIfaceKind::VARIABLE) {
                return false;
            }
            parms.push_back(ip);
        }
        std::vector<AstNodeVhdlExpr*> actuals(parms.size(), nullptr);
        size_t at = 0;
        for (AstNode* p = callp ? callp->argsp() : nullptr; p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            AstNodeVhdlExpr* const actualp = asp ? asp->actualp() : VN_CAST(p, NodeVhdlExpr);
            if (!actualp) return false;
            size_t which = at++;
            if (asp && asp->formalp()) {
                const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
                if (!namep) return false;
                which = parms.size();
                for (size_t i = 0; i < parms.size(); ++i) {
                    if (parms[i]->name() == namep->name()) which = i;
                }
            }
            if (which >= parms.size() || actuals[which]) return false;
            actuals[which] = actualp;
        }
        // The values that go in, worked out where the call was written
        std::vector<VhdlValue> args(parms.size());
        for (size_t i = 0; i < parms.size(); ++i) {
            VhdlSymbol* const psymp = parms[i]->vhdlSymp();
            if (!psymp) return false;
            const VhdlMode mode = parms[i]->mode();
            if (!actuals[i]) {
                if (mode != VhdlMode::IN || !parms[i]->defaultp()) return false;
                args[i] = eval(parms[i]->defaultp());
            } else if (mode == VhdlMode::IN) {
                args[i] = eval(actuals[i]);
            } else {
                const VhdlValue* const objp = targetObject(actuals[i]);
                if (!objp) return false;
                args[i] = mode == VhdlMode::OUT ? defaultOf(psymp->typep()) : *objp;
                // An out parameter of a type left open takes the actual's
                // bounds, and what it holds is the actual's (4.2.2.1)
                if (mode == VhdlMode::OUT && !args[i].known()) args[i] = *objp;
            }
        }
        VL_RESTORER(m_bail);
        m_bail = false;
        ++m_calls;
        m_frames.emplace_back();
        Frame& frame = m_frames.back();
        for (size_t i = 0; i < parms.size(); ++i)
            frame[parms[i]->vhdlSymp()->resolved()] = args[i];
        declareLocals(declp, frame);
        VhdlValue ret;
        const Flow f = execStmts(declp->stmtsp(), ret);
        std::vector<VhdlValue> outs(parms.size());
        for (size_t i = 0; i < parms.size(); ++i) {
            outs[i] = m_frames.back()[parms[i]->vhdlSymp()->resolved()];
        }
        m_frames.pop_back();
        --m_calls;
        if (m_bail || f == Flow::EXITED || f == Flow::NEXTED) return false;
        // Written back, each target found again now the body has run
        for (size_t i = 0; i < parms.size(); ++i) {
            if (parms[i]->mode() == VhdlMode::IN) continue;
            VhdlValue* const objp = targetObject(actuals[i]);
            if (!objp) return false;
            *objp = outs[i];
        }
        return true;
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
            bool ascending = true;
            AstVhdlRange* const rp = VN_CAST(nodep->rangep(), VhdlRange);
            AstVhdlAttrName* const attrp = VN_CAST(nodep->rangep(), VhdlAttrName);
            if (!(rp ? rangeValues(rp, from, to, ascending)
                     : attrRangeValues(attrp, from, to, ascending))) {
                return bail();
            }
            step = ascending ? 1 : -1;
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
            if (++m_steps > maxSteps()) return bail();
            const Flow f = execStmts(nodep->stmtsp(), retr);
            if (f == Flow::RETURNED) return f;
            if (f == Flow::EXITED) break;
        }
        // The loop parameter is not visible outside the loop (10.10)
        if (parmp) m_frames.back().erase(parmp);
        return Flow::NORMAL;
    }

    // What a body declares, given the values its objects start with
    void declareLocals(AstVhdlSubprogDecl* declp, Frame& frame) {
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
            if (!symp || !held(symp->typep())) continue;
            // A variable with no initial value starts at the leftmost value
            // of its subtype, which is not a value held here. Left unknown,
            // so that reading it before it is written stops the fold rather
            // than inventing a number. Elaboration does hold the ones text,
            // access values and arrays of them start with.
            VhdlValue init = op->defaultp() ? eval(op->defaultp()) : VhdlValue{};
            if (m_allowImpure) {
                if (!op->defaultp() && !scalarValued(symp->typep())) {
                    init = defaultOf(symp->typep());
                }
                init = conformTo(init, symp->typep());
            }
            frame[symp->resolved()] = init;
        }
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
            // At elaboration a default no one can work out -- VUnit's
            // new_bus defaults its logger to one get_logger made -- is as
            // unknown as an actual would be, and only what reads it is lost
            if (!val.known() && !m_allowImpure) return VhdlValue{};
            frame[symp->resolved()] = val;
        }
        declareLocals(declp, frame);
        VhdlValue ret;
        const Flow f = execStmts(declp->stmtsp(), ret);
        // Reaching the end of a function without a return statement is an
        // error the running model reports; there is no value to give here
        if (f != Flow::RETURNED) return VhdlValue{};
        return ret;
    }

    // What elaboration holds besides: text, an access value, and an array of
    // either -- what a verification library's string functions are written
    // over (VUnit's split gives back an array of LINE)
    bool held(const VhdlType* typep) const {
        if (valueHeld(typep)) return true;
        if (!m_allowImpure || !typep) return false;
        if (isAccessType(typep)) return true;
        if (isTextType(typep)) return true;
        if (typep->base()->kind() != VhdlTypeKind::ARRAY
            || typep->base()->indexTypes().size() != 1) {
            return false;
        }
        const VhdlType* const elemp
            = typep->elementp() ? typep->elementp() : typep->base()->elementp();
        return isAccessType(elemp) || isTextType(elemp);
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
        if (m_calls >= MAX_CALLS || m_steps >= maxSteps()) return VhdlValue{};
        // Only where every value that goes in and the one that comes out is
        // a scalar. A value here is a number, not an array with bounds of its
        // own, so a body that builds one would be run over values that had
        // lost how they were indexed -- and a bound is exactly what such a
        // call is usually asked for.
        if (!held(funcp->returnTypep())) return VhdlValue{};

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
            const bool isHeld = held(ip->vhdlSymp() ? ip->vhdlSymp()->typep() : nullptr);
            if (!isHeld && !m_allowImpure) return VhdlValue{};
            parms.push_back(ip);
            unheld.push_back(!isHeld);
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
        // Elaboration reads text and arrays it holds, and what an access value
        // designates, an element or a slice at a time (8.4, 8.5)
        if (m_allowImpure && prefixp && (!symp || symp->kind() != VhdlSymKind::TYPE)) {
            VhdlValue whole = eval(prefixp);
            if (isAccessValue(whole)) whole = deref(whole);
            if (whole.isStr() || whole.isArray()) {
                if (AstVhdlRange* const rp = VN_CAST(nodep->argsp(), VhdlRange)) {
                    int64_t left = 0;
                    int64_t right = 0;
                    bool ascending = true;
                    if (!rangeValues(rp, left, right, ascending)) return VhdlValue{};
                    return sliceOf(whole, left, right, ascending);
                }
                AstVhdlAssoc* const idxp = VN_CAST(nodep->argsp(), VhdlAssoc);
                if (idxp && !idxp->nextp() && !idxp->formalp() && idxp->actualp()) {
                    const VhdlValue idx = eval(idxp->actualp());
                    if (!idx.isInt()) return VhdlValue{};
                    return elementOf(whole, idx.toInt());
                }
                return VhdlValue{};
            }
        }
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

bool V3VhdlEval::s_sharedInitValues = false;

VhdlValue V3VhdlEval::eval(AstNodeVhdlExpr* nodep, std::string* errp) {
    VhdlEvaluator evaluator{errp};
    return evaluator.eval(nodep);
}

VhdlValue V3VhdlEval::evalElab(AstNodeVhdlExpr* nodep) {
    VhdlEvaluator evaluator{nullptr, true};
    return VhdlEvaluator::stripAccess(evaluator.eval(nodep));
}

VhdlValue V3VhdlEval::evalElabBound(AstNodeVhdlExpr* nodep) {
    VhdlEvaluator evaluator{nullptr, true, false};
    return VhdlEvaluator::stripAccess(evaluator.eval(nodep));
}

VhdlValue V3VhdlEval::evalElabWith(AstNodeVhdlExpr* nodep,
                                   const std::map<const VhdlSymbol*, VhdlValue>& bindings) {
    VhdlEvaluator evaluator{nullptr, true};
    return VhdlEvaluator::stripAccess(evaluator.evalWith(nodep, bindings));
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
