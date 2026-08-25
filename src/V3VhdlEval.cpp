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
// a user-defined one means running code the frontend has not compiled yet; a
// user-defined "+" therefore stops the fold, which is correct rather than a
// limitation. And every fold is guarded against overflow, because a VHDL
// source may legally contain integer_type'high + 1 in a context where the
// expression is never evaluated, and folding must not trap on it.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlEval.h"

#include "V3Global.h"
#include "V3VhdlStd.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

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
    // A character literal is named with its quotes, as in 'c'
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

    // A constant folded from a self-referential declaration would recurse
    // forever; analysis reports the cycle separately, so just stop
    static constexpr int MAX_DEPTH = 64;

public:
    // CONSTRUCTORS
    explicit VhdlEvaluator(std::string* errp)
        : m_errp{errp} {}

    // METHODS
    VhdlValue eval(AstNodeVhdlExpr* nodep) {
        if (!nodep) return VhdlValue{};
        if (m_depth > MAX_DEPTH) return VhdlValue{};
        VL_RESTORER(m_depth);
        ++m_depth;
        if (AstVhdlLiteral* const p = VN_CAST(nodep, VhdlLiteral)) return evalLiteral(p);
        if (AstVhdlName* const p = VN_CAST(nodep, VhdlName)) return evalSymbol(p->vhdlSymp());
        if (AstVhdlSelected* const p = VN_CAST(nodep, VhdlSelected)) {
            return evalSymbol(p->vhdlSymp());
        }
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
            return symp->deferred() ? VhdlValue{} : symp->value();
        case VhdlSymKind::PARAMETER:
            // A parameter has a value only in a copy of a subprogram made for
            // one call site, which is where the call's own values were fixed
            return symp->value();
        default: return VhdlValue{};
        }
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
                // A real abstract literal scales and then truncates (9.3.2)
                const double scaled = nodep->realVal() * static_cast<double>(unit.toInt());
                if (!(scaled >= -9.2e18 && scaled <= 9.2e18)) return VhdlValue{};
                out = static_cast<int64_t>(scaled);
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
        if (symp && symp->typep()) return symp->typep();
        return prefixp->vhdlTypep();
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
    static std::string scopePath(const VhdlScope* scopep, bool inst) {
        if (!scopep) return "";
        const std::string& prefix = inst ? scopep->instPrefix() : scopep->pathPrefix();
        if (!prefix.empty()) return prefix + ":";
        std::string name = scopep->name();
        if (name == "process" || name == "block" || name == "generate"
            || name == "generate body" || name == "loop") {
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
                return valuep ? eval(valuep) : VhdlValue{};
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
            std::string out = ":" + scopePath(symp->declScopep(), inst) + symp->name();
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

    VhdlValue evalCall(AstVhdlCall* nodep) {
        // Only a type conversion folds; a function call would mean running
        // code, and an indexed name reading a value we do not hold (9.3.6)
        AstNodeVhdlExpr* const prefixp = nodep->prefixp();
        VhdlSymbol* const symp = prefixp ? prefixp->vhdlSymp() : nullptr;
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

std::string V3VhdlEval::image(const VhdlValue& val) {
    switch (val.kind()) {
    case VhdlValKind::UNKNOWN: return "";
    case VhdlValKind::STR: return val.toStr();
    case VhdlValKind::REAL: {
        // 'IMAGE of a real always shows a decimal point, so that the result
        // reads back through 'VALUE (16.2.6)
        char buf[64];
        VL_SNPRINTF(buf, sizeof(buf), "%.17g", val.toReal());
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
