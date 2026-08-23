// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL lowering into Verilator's AST
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

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlLower.h"

#include "V3Global.h"
#include "V3VhdlEval.h"
#include "V3VhdlStd.h"
#include "V3VhdlSym.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################

namespace {

// The nine values of STD_ULOGIC, mapped as the user of a two-state simulator
// needs them: 'H' reads as '1', 'L' as '0', and the values that mean "no
// information" as X. Verilator then treats X exactly as it does in Verilog.
enum class LogicVal : uint8_t { ZERO, ONE, X, Z };

// Whether an enumeration is one of the logic types -- BIT, STD_ULOGIC and
// anything else declared the same way -- and so lowers to a single bit rather
// than to a numbered enumeration. Recognising it by shape rather than by name
// means a user's own nine-value type behaves like the IEEE one.
bool enumIsLogic(const VhdlType* typep) {
    if (!typep || typep->base()->kind() != VhdlTypeKind::ENUM) return false;
    const std::vector<VhdlSymbol*>& lits = typep->base()->literals();
    if (lits.size() < 2 || lits.size() > 9) return false;
    bool sawZero = false;
    bool sawOne = false;
    for (const VhdlSymbol* const litp : lits) {
        const std::string& name = litp->name();
        if (name.size() != 3 || name.front() != '\'' || name.back() != '\'') return false;
        if (name[1] == '0') sawZero = true;
        if (name[1] == '1') sawOne = true;
    }
    return sawZero && sawOne;
}

// What one literal of such a type means
LogicVal logicValueOf(const std::string& litName) {
    if (litName.size() != 3) return LogicVal::X;
    switch (litName[1]) {
    case '0':
    case 'L':
    case 'l': return LogicVal::ZERO;
    case '1':
    case 'H':
    case 'h': return LogicVal::ONE;
    case 'Z':
    case 'z': return LogicVal::Z;
    default: return LogicVal::X;  // 'U', 'X', 'W', '-'
    }
}

// Bits needed to hold n distinct values
int bitsFor(size_t n) {
    int bits = 1;
    while ((size_t{1} << bits) < n) ++bits;
    return bits;
}

}  // namespace

//######################################################################
// The lowering pass

class VhdlLowerVisitor final {
    // TYPES
    // What a VHDL declaration became
    using VarMap = std::map<const VhdlSymbol*, AstVar*>;

    // STATE
    AstNetlist* const m_netlistp;
    AstNodeModule* m_modp = nullptr;  // Module being built
    // Where lowered declarations and statements go. A generate body collects
    // into its own list rather than straight into the module.
    AstNode* m_stmtsp = nullptr;
    AstNodeFTask* m_ftaskp = nullptr;  // Subprogram being built, if any
    std::map<const VhdlSymbol*, AstNodeFTask*> m_funcs;  // Subprogram -> what it became
    VarMap m_vars;  // Declaration -> the variable it became
    std::map<std::string, AstNodeVhdlUnit*> m_entities;  // "lib.name" -> entity
    std::string m_prefix;  // Name prefix for declarations inside a process
    int m_processNum = 0;  // For naming unlabelled processes
    bool m_errored = false;

    VL_UNCOPYABLE(VhdlLowerVisitor);

public:
    // CONSTRUCTORS
    explicit VhdlLowerVisitor(AstNetlist* netlistp)
        : m_netlistp{netlistp} {}
    ~VhdlLowerVisitor() = default;

    bool errored() const { return m_errored; }

    // Where a lowered declaration or statement belongs: the subprogram being
    // built, or the enclosing module or generate body
    void addStmt(AstNode* nodep) {
        if (!nodep) return;
        if (m_ftaskp) {
            m_ftaskp->addStmtsp(nodep);
        } else {
            m_stmtsp = AstNode::addNextNull(m_stmtsp, nodep);
        }
    }

    void lowerUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;

private:
    // METHODS - diagnostics

    void err(FileLine* flp, const std::string& msg) {
        flp->v3error(msg);
        m_errored = true;
    }
    // A construct the frontend understands but cannot yet emit. Kept distinct
    // from an error in the source, which is what the analyzer reports.
    void unsup(FileLine* flp, const std::string& what) {
        const std::string msg = "Unsupported: VHDL " + what + " cannot be lowered yet";
        flp->v3error(msg);
        m_errored = true;
    }

    // METHODS - types

    // The storage a VHDL type needs (5.2 - 5.5). A fresh tree each call, since
    // a data type node belongs to one parent.
    AstNodeDType* lowerType(VhdlType* typep, FileLine* flp) {
        if (!typep || typep->isError()) return newLogicDType(flp, 1);
        VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::ENUM:
            if (enumIsLogic(basep)) return newLogicDType(flp, 1);
            return newLogicDType(flp, bitsFor(basep->literals().size()));
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::UNIVERSAL_INTEGER:
            return new AstBasicDType{flp, VBasicDTypeKwd::INTEGER, VSigning::SIGNED};
        case VhdlTypeKind::FLOATING:
        case VhdlTypeKind::UNIVERSAL_REAL:
            return new AstBasicDType{flp, VBasicDTypeKwd::DOUBLE};
        case VhdlTypeKind::PHYSICAL:
            // Counted in the primary unit, which for TIME is femtoseconds
            return new AstBasicDType{flp, VBasicDTypeKwd::LOGIC, VSigning::SIGNED,
                                     VNumRange{63, 0}, -1};
        case VhdlTypeKind::ARRAY: return lowerArrayType(typep, flp);
        case VhdlTypeKind::RECORD: return lowerRecordType(basep, flp);
        default:
            unsup(flp, "type '" + basep->prettyName() + "'");
            return newLogicDType(flp, 1);
        }
    }

    static AstNodeDType* newLogicDType(FileLine* flp, int bits) {
        if (bits <= 1) return new AstBasicDType{flp, VBasicDTypeKwd::LOGIC};
        return new AstBasicDType{flp, VFlagLogicPacked{}, bits};
    }

    // An array whose element packs becomes a packed array, so that a
    // std_logic_vector is one machine word rather than a vector of them
    AstNodeDType* lowerArrayType(VhdlType* typep, FileLine* flp) {
        VhdlType* const basep = typep->base();
        VhdlType* const elemp = basep->elementp();
        AstNodeDType* dtypep = elemp ? lowerType(elemp, flp) : newLogicDType(flp, 1);
        const bool packed = elemp && elementPacks(elemp);
        // Innermost dimension last, so the ranges nest the way VHDL reads
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        for (size_t i = indexes.size(); i-- > 0;) {
            const VhdlType* const idxp = indexes[i];
            if (!idxp || !idxp->bounded() || typep->unconstrained()) {
                // Bounds arrive at elaboration, or never: a dynamic array
                unsup(flp, "unconstrained array '" + typep->prettyName() + "'");
                return dtypep;
            }
            AstRange* const rangep = new AstRange{
                flp, static_cast<int>(idxp->left()), static_cast<int>(idxp->right())};
            if (packed) {
                dtypep = new AstPackArrayDType{flp, VFlagChildDType{}, dtypep, rangep};
            } else {
                dtypep = new AstUnpackArrayDType{flp, VFlagChildDType{}, dtypep, rangep};
            }
        }
        return dtypep;
    }

    // Whether an element type can live inside a packed array
    static bool elementPacks(const VhdlType* typep) {
        const VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::ENUM: return true;
        case VhdlTypeKind::ARRAY: return basep->elementp() && elementPacks(basep->elementp());
        default: return false;
        }
    }

    AstNodeDType* lowerRecordType(VhdlType* typep, FileLine* flp) {
        // Packed, so that a record of vectors is one machine word
        AstStructDType* const structp = new AstStructDType{flp, VSigning::UNSIGNED};
        structp->name(typep->name());
        for (VhdlSymbol* const elemp : typep->elements()) {
            structp->addMembersp(new AstMemberDType{flp, elemp->name(), VFlagChildDType{},
                                                    lowerType(elemp->typep(), flp), nullptr});
        }
        return structp;
    }

    // METHODS - constants

    // A folded value as the constant it stands for
    AstNodeExpr* lowerValue(const VhdlValue& val, VhdlType* typep, FileLine* flp) {
        if (!val.known()) return nullptr;
        if (val.isReal()) return new AstConst{flp, AstConst::RealDouble{}, val.toReal()};
        if (val.isStr()) return lowerBitString(val.toStr(), typep, flp);
        VhdlType* const basep = typep ? typep->base() : nullptr;
        if (basep && basep->kind() == VhdlTypeKind::ENUM && enumIsLogic(basep)) {
            return newLogicConst(flp, logicValueOf(basep->literals()[val.toInt()]->name()));
        }
        if (basep && basep->kind() == VhdlTypeKind::ENUM) {
            const int bits = bitsFor(basep->literals().size());
            return new AstConst{flp, AstConst::WidthedValue{}, bits,
                                static_cast<uint32_t>(val.toInt())};
        }
        if (basep && basep->kind() == VhdlTypeKind::PHYSICAL) {
            return new AstConst{flp, AstConst::SizedEData{},
                                static_cast<uint64_t>(val.toInt())};
        }
        return new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(val.toInt())};
    }

    // One bit of a logic type. Z and X survive into the AST exactly as the
    // Verilog ones do, so V3Unknown handles them identically.
    static AstNodeExpr* newLogicConst(FileLine* flp, LogicVal value) {
        V3Number num{flp, 1, 0};
        switch (value) {
        case LogicVal::ZERO: num.setLong(0); break;
        case LogicVal::ONE: num.setLong(1); break;
        case LogicVal::X: num.setAllBitsX(); break;
        case LogicVal::Z: num.setAllBitsZ(); break;
        }
        return new AstConst{flp, num};
    }

    // A string of '0'/'1' characters as the vector it stands for
    AstNodeExpr* lowerBitString(const std::string& bits, VhdlType* typep, FileLine* flp) {
        if (bits.empty()) return new AstConst{flp, AstConst::WidthedValue{}, 1, 0};
        // A STRING is text, not a vector
        if (typep && V3VhdlStd::stringType()
            && typep->base() == V3VhdlStd::stringType()->base()) {
            return new AstConst{flp, AstConst::String{}, bits};
        }
        V3Number num{flp, static_cast<int>(bits.size()), 0};
        num.setAllBits0();
        for (size_t i = 0; i < bits.size(); ++i) {
            // The leftmost character is the most significant bit
            const int bit = static_cast<int>(bits.size() - 1 - i);
            switch (logicValueOf(std::string{'\'', bits[i], '\''})) {
            case LogicVal::ZERO: break;
            case LogicVal::ONE: num.setBit(bit, '1'); break;
            case LogicVal::X: num.setBit(bit, 'x'); break;
            case LogicVal::Z: num.setBit(bit, 'z'); break;
            }
        }
        return new AstConst{flp, num};
    }

    // METHODS - declarations

    // Declare a variable for a VHDL object and remember what it came from, so
    // that a name the analyzer already resolved can be emitted as a bound
    // reference rather than left for V3LinkDot to look up again
    AstVar* newVar(VhdlSymbol* symp, VVarType varType, FileLine* flp) {
        const std::string name = m_prefix + symp->name();
        AstVar* const varp
            = new AstVar{flp, varType, name, VFlagChildDType{}, lowerType(symp->typep(), flp)};
        m_vars.emplace(symp, varp);
        return varp;
    }

    // An interface element of a generic or port clause (6.5)
    void lowerInterface(AstVhdlIfaceDecl* nodep, bool isGeneric, int& pinNum) {
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp) return;
        FileLine* const flp = nodep->fileline();
        if (nodep->kind() == VhdlIfaceKind::TYPE
            || nodep->kind() == VhdlIfaceKind::SUBPROGRAM
            || nodep->kind() == VhdlIfaceKind::PACKAGE) {
            unsup(flp, "generic type, subprogram or package");
            return;
        }
        if (isGeneric) {
            // A generic is a parameter, so V3Param clones a module per set of
            // values exactly as VHDL elaboration would
            AstVar* const varp = newVar(symp, VVarType::GPARAM, flp);
            if (nodep->defaultp()) varp->valuep(lowerExpr(nodep->defaultp()));
            addStmt(varp);
            return;
        }
        // A VHDL port is a signal, which a process may assign as readily as a
        // concurrent statement, so it is a variable with a direction rather
        // than Verilog's PORT, which is a net
        AstVar* const varp = newVar(symp, VVarType::VAR, flp);
        switch (nodep->mode()) {
        case VhdlMode::IN: varp->direction(VDirection::INPUT); break;
        case VhdlMode::OUT: varp->direction(VDirection::OUTPUT); break;
        case VhdlMode::BUFFER: varp->direction(VDirection::OUTPUT); break;
        case VhdlMode::INOUT: varp->direction(VDirection::INOUT); break;
        case VhdlMode::LINKAGE: varp->direction(VDirection::INOUT); break;
        }
        varp->declDirection(varp->direction());
        varp->ansi(true);
        if (nodep->defaultp()) varp->valuep(lowerExpr(nodep->defaultp()));
        addStmt(new AstPort{flp, ++pinNum, varp->name()});
        addStmt(varp);
    }

    // A declarative item of an entity, architecture, block or process
    void lowerDecl(AstNode* nodep) {
        FileLine* const flp = nodep->fileline();
        if (AstVhdlObjectDecl* const objp = VN_CAST(nodep, VhdlObjectDecl)) {
            VhdlSymbol* const symp = objp->vhdlSymp();
            if (!symp) return;
            VVarType varType = VVarType::VAR;
            switch (objp->kind()) {
            case VhdlObjKind::SIGNAL: varType = VVarType::VAR; break;
            case VhdlObjKind::CONSTANT:
                // A constant with a folded value needs no storage at all
                if (symp->hasValue()) return;
                varType = VVarType::VAR;
                break;
            case VhdlObjKind::VARIABLE:
            case VhdlObjKind::SHARED_VARIABLE: varType = VVarType::VAR; break;
            case VhdlObjKind::FILE: unsup(flp, "file object"); return;
            }
            AstVar* const varp = newVar(symp, varType, flp);
            // A VHDL process variable keeps its value between activations, so
            // it is a static variable of the module, not a local one
            if (objp->defaultp()) {
                varp->valuep(lowerExpr(objp->defaultp()));
            } else {
                varp->valuep(defaultValueFor(symp->typep(), flp));
            }
            addStmt(varp);
            return;
        }
        if (VN_IS(nodep, VhdlTypeDecl) || VN_IS(nodep, VhdlSubtypeDecl)
            || VN_IS(nodep, VhdlAttrDecl) || VN_IS(nodep, VhdlAttrSpec)
            || VN_IS(nodep, VhdlComponentDecl) || VN_IS(nodep, VhdlContextItem)) {
            return;  // Nothing to emit: analysis already used these
        }
        if (VN_IS(nodep, VhdlAliasDecl)) return;  // Names, not storage
        if (AstVhdlSubprogDecl* const subp = VN_CAST(nodep, VhdlSubprogDecl)) {
            lowerSubprogram(subp);
            return;
        }
        unsup(flp, std::string{"declaration "} + nodep->typeName());
    }

    // VHDL initializes every object to the leftmost value of its subtype
    // (6.4.2.2), which for STD_LOGIC is 'U'. Two-state, that is X, and
    // --x-initial gives the randomization that most nearly stands in for it.
    AstNodeExpr* defaultValueFor(VhdlType* typep, FileLine* flp) {
        if (!typep || typep->isError()) return nullptr;
        VhdlType* const basep = typep->base();
        if (basep->kind() == VhdlTypeKind::ENUM && typep->bounded()) {
            const std::vector<VhdlSymbol*>& lits = basep->literals();
            const int64_t left = typep->left();
            if (left >= 0 && left < static_cast<int64_t>(lits.size())) {
                return lowerValue(VhdlValue::mkInt(left, typep), typep, flp);
            }
        }
        return nullptr;  // Everything else starts as Verilator initializes it
    }



    // METHODS - expressions (clauses 8, 9)

    // Whether values of a type are signed. For a vector that is decided by
    // the type's name, since SIGNED and UNSIGNED differ in nothing else;
    // VHDL-2008 declares each as a subtype of an unresolved one, so both
    // spellings count.
    static bool isSignedName(const std::string& name) {
        return name == "signed" || name == "unresolved_signed";
    }
    static bool isSignedType(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::UNIVERSAL_INTEGER:
        case VhdlTypeKind::PHYSICAL: return true;
        case VhdlTypeKind::ARRAY:
            return isSignedName(typep->name()) || isSignedName(basep->name());
        default: return false;
        }
    }

    AstNodeExpr* lowerExpr(AstNodeVhdlExpr* nodep) {
        if (!nodep) return nullptr;
        FileLine* const flp = nodep->fileline();
        // Whatever its shape, an expression the analyzer could fold is a
        // constant. That covers literals, constants, generics with values and
        // every attribute of a static subtype, in one place.
        const VhdlValue val = V3VhdlEval::eval(nodep);
        if (val.known()) return lowerValue(val, nodep->vhdlTypep(), flp);

        if (AstVhdlName* const p = VN_CAST(nodep, VhdlName)) {
            return lowerRef(p->vhdlSymp(), p->name(), flp);
        }
        if (AstVhdlSelected* const p = VN_CAST(nodep, VhdlSelected)) return lowerSelected(p);
        if (AstVhdlUnary* const p = VN_CAST(nodep, VhdlUnary)) return lowerUnary(p);
        if (AstVhdlBinary* const p = VN_CAST(nodep, VhdlBinary)) return lowerBinary(p);
        if (AstVhdlCall* const p = VN_CAST(nodep, VhdlCall)) return lowerCall(p);
        if (AstVhdlAggregate* const p = VN_CAST(nodep, VhdlAggregate)) return lowerAggregate(p);
        if (AstVhdlQualified* const p = VN_CAST(nodep, VhdlQualified)) {
            return lowerExpr(p->valuep());
        }
        if (AstVhdlAttrName* const p = VN_CAST(nodep, VhdlAttrName)) return lowerAttr(p);
        unsup(flp, std::string{"expression "} + nodep->typeName());
        return nullptr;
    }

    // The attributes that mean something at run time. Everything static has
    // already folded, so what is left either reads a value or asks about a
    // signal's history, and the latter needs the postponed region of M7.
    AstNodeExpr* lowerAttr(AstVhdlAttrName* nodep) {
        FileLine* const flp = nodep->fileline();
        const std::string& attr = nodep->name();
        AstNodeExpr* const argp = nodep->argp() ? lowerExpr(nodep->argp()) : nullptr;
        // An enumeration value is its own position, and a position its own
        // value, so 'POS and 'VAL are identities in the emitted code (16.2.3)
        if (attr == "pos" || attr == "val") {
            if (argp) return argp;
        } else if (attr == "succ" || attr == "rightof") {
            if (argp) return new AstAdd{flp, argp, new AstConst{flp, AstConst::Signed32{}, 1}};
        } else if (attr == "pred" || attr == "leftof") {
            if (argp) return new AstSub{flp, argp, new AstConst{flp, AstConst::Signed32{}, 1}};
        }
        if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
        unsup(flp, "attribute '" + attr);
        return nullptr;
    }

    // A condition is of type BOOLEAN, which is one bit here
    AstNodeExpr* lowerCondition(AstNodeVhdlExpr* nodep) {
        AstNodeExpr* const exprp = lowerExpr(nodep);
        if (!exprp) return new AstConst{nodep->fileline(), AstConst::BitFalse{}};
        return exprp;
    }

    AstNodeExpr* lowerRef(VhdlSymbol* symp, const std::string& name, FileLine* flp) {
        if (!symp) {
            unsup(flp, "reference to '" + name + "'");
            return nullptr;
        }
        const auto it = m_vars.find(symp);
        if (it != m_vars.end()) {
            return new AstVarRef{flp, it->second, VAccess::READ};
        }
        if (symp->hasValue()) return lowerValue(symp->value(), symp->typep(), flp);
        unsup(flp, std::string{symp->kindName()} + " '" + symp->name() + "'");
        return nullptr;
    }

    AstNodeExpr* lowerSelected(AstVhdlSelected* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (symp && symp->kind() == VhdlSymKind::RECORD_ELEMENT) {
            AstNodeExpr* const fromp = lowerExpr(nodep->prefixp());
            if (!fromp) return nullptr;
            return new AstMemberSel{flp, fromp, VFlagChildDType{}, symp->name()};
        }
        if (symp) return lowerRef(symp, symp->name(), flp);
        // A selected name of something in another design unit
        std::vector<std::string> parts;
        flattenName(nodep, parts);
        unsup(flp, "name '" + (parts.empty() ? std::string{"?"} : parts.back()) + "'");
        return nullptr;
    }

    AstNodeExpr* lowerUnary(AstVhdlUnary* nodep) {
        FileLine* const flp = nodep->fileline();
        const VhdlSymbol* const symp = nodep->vhdlSymp();
        VhdlType* const otypep = nodep->lhsp()->vhdlTypep();
        // std_logic_1164 declares "not" for STD_ULOGIC itself, so an operator
        // over a logic type is one of the intrinsics even when it is explicit
        if (symp && !symp->predefined() && !isIeeeOperator(symp, otypep, nullptr)) {
            unsup(flp, "user-defined unary operator \"" + symp->name() + "\"");
            return nullptr;
        }
        AstNodeExpr* const lhsp = lowerExpr(nodep->lhsp());
        if (!lhsp) return nullptr;
        const bool real = otypep && otypep->base()->kind() == VhdlTypeKind::FLOATING;
        switch (nodep->op()) {
        case VhdlUnaryOp::IDENTITY: return lhsp;
        case VhdlUnaryOp::NEGATE:
            return real ? static_cast<AstNodeExpr*>(new AstNegateD{flp, lhsp})
                        : static_cast<AstNodeExpr*>(new AstNegate{flp, lhsp});
        case VhdlUnaryOp::ABS:
            if (real) {
                return new AstCond{
                    flp,
                    new AstLtD{flp, lhsp->cloneTree(false),
                               new AstConst{flp, AstConst::RealDouble{}, 0.0}},
                    new AstNegateD{flp, lhsp->cloneTree(false)}, lhsp};
            }
            return new AstCond{flp,
                               new AstLtS{flp, lhsp->cloneTree(false),
                                          new AstConst{flp, AstConst::Signed32{}, 0}},
                               new AstNegate{flp, lhsp->cloneTree(false)}, lhsp};
        case VhdlUnaryOp::NOT: return new AstNot{flp, lhsp};
        case VhdlUnaryOp::AND: return new AstRedAnd{flp, lhsp};
        case VhdlUnaryOp::OR: return new AstRedOr{flp, lhsp};
        case VhdlUnaryOp::XOR: return new AstRedXor{flp, lhsp};
        case VhdlUnaryOp::NAND: return new AstNot{flp, new AstRedAnd{flp, lhsp}};
        case VhdlUnaryOp::NOR: return new AstNot{flp, new AstRedOr{flp, lhsp}};
        case VhdlUnaryOp::XNOR: return new AstNot{flp, new AstRedXor{flp, lhsp}};
        case VhdlUnaryOp::COND_CONV: return lhsp;  // Already one bit
        }
        unsup(flp, "unary operator");
        return nullptr;
    }

    AstNodeExpr* lowerBinary(AstVhdlBinary* nodep) {
        FileLine* const flp = nodep->fileline();
        const VhdlSymbol* const symp = nodep->vhdlSymp();
        VhdlType* const ltypep = nodep->lhsp()->vhdlTypep();
        VhdlType* const rtypep = nodep->rhsp()->vhdlTypep();
        if (symp && !symp->predefined() && !isIeeeOperator(symp, ltypep, rtypep)) {
            unsup(flp, "user-defined operator \"" + symp->name() + "\"");
            return nullptr;
        }
        AstNodeExpr* lhsp = lowerExpr(nodep->lhsp());
        AstNodeExpr* rhsp = lowerExpr(nodep->rhsp());
        if (!lhsp || !rhsp) return nullptr;
        matchVectorWidth(nodep, ltypep, rtypep, lhsp, rhsp, flp);
        const bool signd = isSignedType(ltypep) || isSignedType(rtypep);
        const bool real = (ltypep && ltypep->base()->kind() == VhdlTypeKind::FLOATING)
                          || (rtypep && rtypep->base()->kind() == VhdlTypeKind::FLOATING);
        switch (nodep->op()) {
        case VhdlBinaryOp::AND: return new AstAnd{flp, lhsp, rhsp};
        case VhdlBinaryOp::OR: return new AstOr{flp, lhsp, rhsp};
        case VhdlBinaryOp::XOR: return new AstXor{flp, lhsp, rhsp};
        case VhdlBinaryOp::NAND: return new AstNot{flp, new AstAnd{flp, lhsp, rhsp}};
        case VhdlBinaryOp::NOR: return new AstNot{flp, new AstOr{flp, lhsp, rhsp}};
        case VhdlBinaryOp::XNOR: return new AstNot{flp, new AstXor{flp, lhsp, rhsp}};
        case VhdlBinaryOp::EQ:
        case VhdlBinaryOp::MATCH_EQ:
            return real ? static_cast<AstNodeExpr*>(new AstEqD{flp, lhsp, rhsp})
                        : static_cast<AstNodeExpr*>(new AstEq{flp, lhsp, rhsp});
        case VhdlBinaryOp::NE:
        case VhdlBinaryOp::MATCH_NE:
            return real ? static_cast<AstNodeExpr*>(new AstNeqD{flp, lhsp, rhsp})
                        : static_cast<AstNodeExpr*>(new AstNeq{flp, lhsp, rhsp});
        case VhdlBinaryOp::LT:
        case VhdlBinaryOp::MATCH_LT:
            if (real) return new AstLtD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstLtS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstLt{flp, lhsp, rhsp});
        case VhdlBinaryOp::LE:
        case VhdlBinaryOp::MATCH_LE:
            if (real) return new AstLteD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstLteS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstLte{flp, lhsp, rhsp});
        case VhdlBinaryOp::GT:
        case VhdlBinaryOp::MATCH_GT:
            if (real) return new AstGtD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstGtS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstGt{flp, lhsp, rhsp});
        case VhdlBinaryOp::GE:
        case VhdlBinaryOp::MATCH_GE:
            if (real) return new AstGteD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstGteS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstGte{flp, lhsp, rhsp});
        case VhdlBinaryOp::ADD:
            return real ? static_cast<AstNodeExpr*>(new AstAddD{flp, lhsp, rhsp})
                        : static_cast<AstNodeExpr*>(new AstAdd{flp, lhsp, rhsp});
        case VhdlBinaryOp::SUB:
            return real ? static_cast<AstNodeExpr*>(new AstSubD{flp, lhsp, rhsp})
                        : static_cast<AstNodeExpr*>(new AstSub{flp, lhsp, rhsp});
        case VhdlBinaryOp::MUL:
            if (real) return new AstMulD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstMulS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstMul{flp, lhsp, rhsp});
        case VhdlBinaryOp::DIV:
            if (real) return new AstDivD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstDivS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstDiv{flp, lhsp, rhsp});
        case VhdlBinaryOp::REM:
            // VHDL's rem takes the sign of its left operand, which is what
            // the truncating remainder gives (9.2.7)
            return signd ? static_cast<AstNodeExpr*>(new AstModDivS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstModDiv{flp, lhsp, rhsp});
        case VhdlBinaryOp::MOD:
            if (!signd) return new AstModDiv{flp, lhsp, rhsp};
            // mod takes the sign of its right operand, so bias the remainder
            // back into range: ((a rem b) + b) rem b
            return new AstModDivS{
                flp,
                new AstAdd{flp, new AstModDivS{flp, lhsp, rhsp->cloneTree(false)},
                           rhsp->cloneTree(false)},
                rhsp};
        case VhdlBinaryOp::POW:
            if (real) return new AstPowD{flp, lhsp, rhsp};
            return signd ? static_cast<AstNodeExpr*>(new AstPowSS{flp, lhsp, rhsp})
                         : static_cast<AstNodeExpr*>(new AstPow{flp, lhsp, rhsp});
        case VhdlBinaryOp::CONCAT: return new AstConcat{flp, lhsp, rhsp};
        case VhdlBinaryOp::SLL:
        case VhdlBinaryOp::SLA: return new AstShiftL{flp, lhsp, rhsp};
        case VhdlBinaryOp::SRL: return new AstShiftR{flp, lhsp, rhsp};
        case VhdlBinaryOp::SRA: return new AstShiftRS{flp, lhsp, rhsp};
        case VhdlBinaryOp::ROL:
        case VhdlBinaryOp::ROR: unsup(flp, "rotate operator"); return nullptr;
        }
        unsup(flp, "binary operator");
        return nullptr;
    }

    // The length of a constrained one-dimensional vector, or 0
    static int vectorWidth(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return 0;
        if (!typep->isConstrainedArray() || typep->dimensions() != 1) return 0;
        return static_cast<int>(typep->dimLength(0));
    }

    // numeric_std declares "+"(UNSIGNED, NATURAL) to give a result as long as
    // the vector operand (16.5), so the integer side is taken at that width.
    // Without this the literal in "c + 1" would be 32 bits wide and every
    // arithmetic expression would warn and then truncate.
    // The length an expression's value will have. An operator of numeric_std
    // gives a result as long as its operand, and the operand's own type may
    // be the unconstrained UNSIGNED, so the length has to be followed down
    // the expression rather than read off one type.
    static int exprVectorWidth(AstNodeVhdlExpr* nodep) {
        if (!nodep) return 0;
        if (const int width = vectorWidth(nodep->vhdlTypep())) return width;
        if (AstVhdlBinary* const binp = VN_CAST(nodep, VhdlBinary)) {
            const int lw = exprVectorWidth(binp->lhsp());
            const int rw = exprVectorWidth(binp->rhsp());
            if (binp->op() == VhdlBinaryOp::CONCAT) return lw + rw;
            return std::max(lw, rw);
        }
        if (AstVhdlUnary* const unp = VN_CAST(nodep, VhdlUnary)) {
            return exprVectorWidth(unp->lhsp());
        }
        if (AstVhdlQualified* const qp = VN_CAST(nodep, VhdlQualified)) {
            return exprVectorWidth(qp->valuep());
        }
        return 0;
    }

    static void matchVectorWidth(AstVhdlBinary* nodep, const VhdlType* ltypep,
                                 const VhdlType* rtypep, AstNodeExpr*& lhsp,
                                 AstNodeExpr*& rhsp, FileLine* flp) {
        switch (nodep->op()) {
        case VhdlBinaryOp::ADD:
        case VhdlBinaryOp::SUB:
        case VhdlBinaryOp::MUL:
        case VhdlBinaryOp::DIV:
        case VhdlBinaryOp::MOD:
        case VhdlBinaryOp::REM:
        case VhdlBinaryOp::EQ:
        case VhdlBinaryOp::NE:
        case VhdlBinaryOp::LT:
        case VhdlBinaryOp::LE:
        case VhdlBinaryOp::GT:
        case VhdlBinaryOp::GE: break;
        default: return;
        }
        const int lw = exprVectorWidth(nodep->lhsp());
        const int rw = exprVectorWidth(nodep->rhsp());
        if (lw && !rw && isIntegerType(rtypep)) {
            rhsp = sizedTo(rhsp, lw, flp);
        } else if (rw && !lw && isIntegerType(ltypep)) {
            lhsp = sizedTo(lhsp, rw, flp);
        }
    }
    static bool isIntegerType(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlTypeKind kind = typep->base()->kind();
        return kind == VhdlTypeKind::INTEGER || kind == VhdlTypeKind::UNIVERSAL_INTEGER;
    }
    static AstNodeExpr* sizedTo(AstNodeExpr* exprp, int width, FileLine* flp) {
        return new AstCastSize{flp, exprp,
                               new AstConst{flp, AstConst::Unsized32{},
                                            static_cast<uint32_t>(width)}};
    }

    // The arithmetic of ieee.numeric_std is recognised rather than compiled.
    // Those packages describe, in VHDL, operations every processor already
    // has; emitting the native operation is what makes a VHDL adder as fast
    // as a Verilog one, and it is the reason this frontend does not need the
    // packages' bodies at all.
    static bool isIeeeOperator(const VhdlSymbol* symp, const VhdlType* ltypep,
                               const VhdlType* rtypep) {
        if (!symp) return false;
        if (symp->kind() != VhdlSymKind::FUNCTION) return false;
        return isLogicish(ltypep) || isLogicish(rtypep);
    }
    // A logic value or an array of them, which is what the IEEE packages
    // operate on. Recognising the shape rather than the name means SIGNED,
    // UNRESOLVED_SIGNED, STD_LOGIC_VECTOR and a user's own equivalent are all
    // treated alike.
    static bool isLogicish(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        if (enumIsLogic(basep)) return true;
        return basep->kind() == VhdlTypeKind::ARRAY && basep->elementp()
               && enumIsLogic(basep->elementp()->base());
    }

    // "prefix ( ... )" is by now known to be an index, a slice, a type
    // conversion or a call (8.1)
    AstNodeExpr* lowerCall(AstVhdlCall* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const prefixSymp
            = nodep->prefixp() ? nodep->prefixp()->vhdlSymp() : nullptr;
        // A type mark makes it a conversion (9.3.6)
        if (prefixSymp && prefixSymp->kind() == VhdlSymKind::TYPE) {
            AstVhdlAssoc* const argp = VN_CAST(nodep->argsp(), VhdlAssoc);
            if (!argp) return nullptr;
            return lowerConversion(prefixSymp->typep(), argp->actualp(), flp);
        }
        // An object prefix makes it an index or a slice (8.4, 8.5)
        VhdlSymbol* const objSymp = nodep->vhdlSymp();
        if (objSymp && objSymp->isObject()) {
            AstNodeExpr* const fromp = lowerExpr(nodep->prefixp());
            if (!fromp) return nullptr;
            if (AstVhdlRange* const rp = VN_CAST(nodep->argsp(), VhdlRange)) {
                if (!rp->leftp() || !rp->rightp()) {
                    unsup(flp, "slice by an attribute range");
                    return nullptr;
                }
                AstNodeExpr* const leftp = lowerExpr(rp->leftp());
                AstNodeExpr* const rightp = lowerExpr(rp->rightp());
                if (!leftp || !rightp) {
                    VL_DO_DANGLING(fromp->deleteTree(), fromp);
                    return nullptr;
                }
                // A slice whose bounds move together but keep their width --
                // v(i + 7 downto i) -- is Verilog's indexed part select. VHDL
                // writes both ends, so the width has to be recovered from
                // them before Verilator's fixed-width form can be used.
                if (!VN_IS(leftp, Const) || !VN_IS(rightp, Const)) {
                    if (AstNodeExpr* const selp
                        = indexedSlice(rp, fromp, leftp, rightp, flp)) {
                        return selp;
                    }
                }
                return new AstSelExtract{flp, fromp, leftp, rightp};
            }
            AstNodeExpr* outp = fromp;
            for (AstNode* p = nodep->argsp(); p; p = p->nextp()) {
                AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->actualp()) continue;
                AstNodeExpr* const indexp = lowerExpr(asp->actualp());
                if (!indexp) {
                    VL_DO_DANGLING(outp->deleteTree(), outp);
                    return nullptr;
                }
                outp = new AstSelBit{flp, outp, indexp};
            }
            return outp;
        }
        // Otherwise a function call
        return lowerFuncCall(nodep, flp);
    }

    // Peel "expr + k" or "expr - k" down to its base, accumulating the
    // constant offset
    static AstNodeVhdlExpr* peelOffset(AstNodeVhdlExpr* nodep, int64_t& offset) {
        while (AstVhdlBinary* const binp = VN_CAST(nodep, VhdlBinary)) {
            const bool add = binp->op() == VhdlBinaryOp::ADD;
            if (!add && binp->op() != VhdlBinaryOp::SUB) break;
            const VhdlValue rhs = V3VhdlEval::eval(binp->rhsp());
            if (!rhs.isInt()) break;
            offset += add ? rhs.toInt() : -rhs.toInt();
            nodep = binp->lhsp();
        }
        return nodep;
    }

    // The indexed part select a moving slice becomes, or null if its bounds
    // do not move together
    AstNodeExpr* indexedSlice(AstVhdlRange* rp, AstNodeExpr* fromp, AstNodeExpr* leftp,
                              AstNodeExpr* rightp, FileLine* flp) {
        int64_t leftOff = 0;
        int64_t rightOff = 0;
        AstNodeVhdlExpr* const leftBasep = peelOffset(rp->leftp(), leftOff);
        AstNodeVhdlExpr* const rightBasep = peelOffset(rp->rightp(), rightOff);
        if (leftBasep == rightBasep) {
            // Same expression node on both ends, as in v(i + 7 downto i)
        } else {
            AstNodeExpr* const ap = lowerExpr(leftBasep);
            AstNodeExpr* const bp = lowerExpr(rightBasep);
            const bool same = ap && bp && ap->sameTree(bp);
            if (ap) VL_DO_DANGLING(ap->deleteTree(), ap);
            if (bp) VL_DO_DANGLING(bp->deleteTree(), bp);
            if (!same) return nullptr;
        }
        const bool downto = rp->dir() == VhdlDir::DOWNTO;
        const int64_t width = downto ? leftOff - rightOff + 1 : rightOff - leftOff + 1;
        if (width < 1 || width > (1 << 20)) return nullptr;
        AstNodeExpr* const widthp
            = new AstConst{flp, AstConst::Unsized32{}, static_cast<uint32_t>(width)};
        // The select counts up from the low end and down from the high one
        if (downto) {
            VL_DO_DANGLING(rightp->deleteTree(), rightp);
            return new AstSelMinus{flp, fromp, leftp, widthp};
        }
        VL_DO_DANGLING(rightp->deleteTree(), rightp);
        return new AstSelPlus{flp, fromp, leftp, widthp};
    }

    // A type conversion between closely related types (9.3.6). Between
    // arrays of the same length it is a no-op on the bits; between numeric
    // types it may round.
    AstNodeExpr* lowerConversion(VhdlType* top, AstNodeVhdlExpr* argp, FileLine* flp) {
        AstNodeExpr* const valuep = lowerExpr(argp);
        if (!valuep || !top) return valuep;
        VhdlType* const fromp = argp->vhdlTypep();
        const VhdlTypeKind toKind = top->base()->kind();
        const VhdlTypeKind fromKind = fromp ? fromp->base()->kind() : toKind;
        if (toKind == VhdlTypeKind::FLOATING && fromKind != VhdlTypeKind::FLOATING) {
            return new AstIToRD{flp, valuep};
        }
        if (toKind != VhdlTypeKind::FLOATING && fromKind == VhdlTypeKind::FLOATING) {
            return new AstRToIRoundS{flp, valuep};
        }
        if (toKind == VhdlTypeKind::ARRAY) {
            // unsigned(x), signed(x) and std_logic_vector(x) reinterpret the
            // same bits; only the signedness changes
            if (isSignedType(top)) return new AstSigned{flp, valuep};
            return new AstUnsigned{flp, valuep};
        }
        return valuep;
    }

    AstNodeExpr* lowerFuncCall(AstVhdlCall* nodep, FileLine* flp) {
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp) {
            unsup(flp, "call");
            return nullptr;
        }
        std::vector<AstNodeVhdlExpr*> args;
        for (AstNode* p = nodep->argsp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            if (asp && asp->actualp()) args.push_back(asp->actualp());
        }
        if (AstNodeExpr* const outp = lowerIntrinsic(symp, args, nodep, flp)) return outp;
        const auto it = m_funcs.find(symp);
        if (it != m_funcs.end() && VN_IS(it->second, Func)) {
            AstFuncRef* const refp = new AstFuncRef{flp, it->second->name(), nullptr};
            refp->taskp(it->second);
            for (AstNodeVhdlExpr* const argp : args) {
                refp->addArgsp(new AstArg{flp, "", lowerExpr(argp)});
            }
            return refp;
        }
        unsup(flp, "call to '" + symp->name() + "'");
        return nullptr;
    }

    // The functions of the IEEE packages that every design uses, emitted as
    // native operations (16.5, 16.8)
    AstNodeExpr* lowerIntrinsic(const VhdlSymbol* symp,
                                const std::vector<AstNodeVhdlExpr*>& args,
                                AstVhdlCall* nodep, FileLine* flp) {
        const std::string& name = symp->name();
        VhdlType* const resultp = nodep->vhdlTypep();
        if (args.empty()) return nullptr;
        AstNodeExpr* const firstp = lowerExpr(args[0]);
        if (!firstp) return nullptr;

        if (name == "to_integer") {
            return isSignedType(args[0]->vhdlTypep())
                       ? static_cast<AstNodeExpr*>(new AstSigned{flp, firstp})
                       : static_cast<AstNodeExpr*>(new AstUnsigned{flp, firstp});
        }
        if (name == "to_unsigned" || name == "to_signed" || name == "resize") {
            if (args.size() < 2) return nullptr;
            const VhdlValue widthVal = V3VhdlEval::eval(args[1]);
            AstNodeExpr* sized = firstp;
            if (widthVal.isInt()) {
                sized = new AstCastSize{
                    flp, firstp,
                    new AstConst{flp, AstConst::Unsized32{},
                                 static_cast<uint32_t>(widthVal.toInt())}};
            } else if (name == "resize") {
                // The width comes from another object's range, which V3Width
                // resolves from the context of the assignment
                sized = firstp;
            } else {
                unsup(flp, "conversion to a width that is not locally static");
                return nullptr;
            }
            return isSignedType(resultp) ? static_cast<AstNodeExpr*>(new AstSigned{flp, sized})
                                         : static_cast<AstNodeExpr*>(new AstUnsigned{flp, sized});
        }
        if (name == "shift_left") {
            if (args.size() < 2) return nullptr;
            return new AstShiftL{flp, firstp, lowerExpr(args[1])};
        }
        if (name == "shift_right") {
            if (args.size() < 2) return nullptr;
            if (isSignedType(args[0]->vhdlTypep())) {
                return new AstShiftRS{flp, firstp, lowerExpr(args[1])};
            }
            return new AstShiftR{flp, firstp, lowerExpr(args[1])};
        }
        if (name == "and_reduce") return new AstRedAnd{flp, firstp};
        if (name == "or_reduce") return new AstRedOr{flp, firstp};
        if (name == "xor_reduce") return new AstRedXor{flp, firstp};
        if (name == "nand_reduce") return new AstNot{flp, new AstRedAnd{flp, firstp}};
        if (name == "nor_reduce") return new AstNot{flp, new AstRedOr{flp, firstp}};
        if (name == "xnor_reduce") return new AstNot{flp, new AstRedXor{flp, firstp}};
        if (name == "maximum" || name == "minimum") {
            if (args.size() < 2) return nullptr;
            AstNodeExpr* const secondp = lowerExpr(args[1]);
            if (!secondp) return nullptr;
            const bool signd = isSignedType(args[0]->vhdlTypep());
            AstNodeExpr* const testp
                = signd ? static_cast<AstNodeExpr*>(
                      new AstGtS{flp, firstp->cloneTree(false), secondp->cloneTree(false)})
                        : static_cast<AstNodeExpr*>(
                            new AstGt{flp, firstp->cloneTree(false), secondp->cloneTree(false)});
            return name == "maximum" ? new AstCond{flp, testp, firstp, secondp}
                                     : new AstCond{flp, testp, secondp, firstp};
        }
        // The conversions between the logic families keep the same bits
        if (name == "to_stdlogicvector" || name == "to_stdulogicvector"
            || name == "to_bitvector" || name == "to_bit" || name == "to_stdulogic"
            || name == "to_stdlogic" || name == "to_x01" || name == "to_01"
            || name == "to_ux01") {
            return firstp;
        }
        VL_DO_DANGLING(firstp->deleteTree(), firstp);
        return nullptr;
    }

    // An aggregate builds a value out of its elements (9.3.3). Every choice
    // has already been checked to be static, so the whole value is a
    // concatenation over the target's index range.
    AstNodeExpr* lowerAggregate(AstVhdlAggregate* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlType* const typep = nodep->vhdlTypep();
        if (typep && typep->base()->kind() == VhdlTypeKind::RECORD) {
            return lowerRecordAggregate(nodep, typep, flp);
        }
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY
            || !typep->isConstrainedArray()) {
            unsup(flp, "aggregate of this type");
            return nullptr;
        }
        const VhdlType* const indexp = typep->indexTypes()[0];
        const int64_t left = indexp->left();
        const int64_t right = indexp->right();
        const int64_t step = indexp->ascending() ? 1 : -1;
        const int64_t count = indexp->length();
        if (count <= 0 || count > 65536) {
            unsup(flp, "aggregate of this size");
            return nullptr;
        }
        // Which element association gives each index
        std::vector<AstNodeVhdlExpr*> byIndex(static_cast<size_t>(count), nullptr);
        AstNodeVhdlExpr* othersp = nullptr;
        int64_t positional = 0;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(p, VhdlElemAssoc);
            if (!eap->choicesp()) {
                const int64_t index = left + positional * step;
                ++positional;
                const int64_t slot = (index - left) * step;
                if (slot >= 0 && slot < count) byIndex[static_cast<size_t>(slot)] = eap->valuep();
                continue;
            }
            for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                if (chp->isOthers()) {
                    othersp = eap->valuep();
                    continue;
                }
                int64_t lo = 0;
                int64_t hi = 0;
                if (AstVhdlRange* const rp = chp->rangep()) {
                    const VhdlValue a = V3VhdlEval::eval(rp->leftp());
                    const VhdlValue b = V3VhdlEval::eval(rp->rightp());
                    if (!a.isInt() || !b.isInt()) {
                        unsup(chp->fileline(), "aggregate choice that is not static");
                        return nullptr;
                    }
                    lo = std::min(a.toInt(), b.toInt());
                    hi = std::max(a.toInt(), b.toInt());
                } else {
                    const VhdlValue v = V3VhdlEval::eval(chp->valuep());
                    if (!v.isInt()) {
                        unsup(chp->fileline(), "aggregate choice that is not static");
                        return nullptr;
                    }
                    lo = hi = v.toInt();
                }
                for (int64_t index = lo; index <= hi; ++index) {
                    const int64_t slot = (index - left) * step;
                    if (slot >= 0 && slot < count) {
                        byIndex[static_cast<size_t>(slot)] = eap->valuep();
                    }
                }
            }
        }
        // Left to right is most significant to least, as a vector reads
        AstNodeExpr* outp = nullptr;
        for (int64_t i = 0; i < count; ++i) {
            AstNodeVhdlExpr* const valuep
                = byIndex[static_cast<size_t>(i)] ? byIndex[static_cast<size_t>(i)] : othersp;
            if (!valuep) {
                unsup(flp, "aggregate with an element left unset");
                return nullptr;
            }
            AstNodeExpr* const elemp = lowerExpr(valuep);
            if (!elemp) return nullptr;
            outp = outp ? new AstConcat{flp, outp, elemp} : elemp;
        }
        (void)right;
        return outp;
    }

    // A record aggregate lays its elements out in declaration order, which is
    // how the packed structure it lowers to is laid out (9.3.3.2)
    AstNodeExpr* lowerRecordAggregate(AstVhdlAggregate* nodep, VhdlType* typep, FileLine* flp) {
        const std::vector<VhdlSymbol*>& elems = typep->base()->elements();
        if (elems.empty()) {
            unsup(flp, "aggregate of an empty record");
            return nullptr;
        }
        std::vector<AstNodeVhdlExpr*> byElem(elems.size(), nullptr);
        AstNodeVhdlExpr* othersp = nullptr;
        size_t position = 0;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(p, VhdlElemAssoc);
            if (!eap->choicesp()) {
                if (position < byElem.size()) byElem[position] = eap->valuep();
                ++position;
                continue;
            }
            for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                if (chp->isOthers()) {
                    othersp = eap->valuep();
                    continue;
                }
                const AstVhdlName* const namep = VN_CAST(chp->valuep(), VhdlName);
                if (!namep) {
                    unsup(chp->fileline(), "record aggregate choice");
                    return nullptr;
                }
                for (size_t i = 0; i < elems.size(); ++i) {
                    if (elems[i]->name() == namep->name()) byElem[i] = eap->valuep();
                }
            }
        }
        // The first element is the most significant, as the structure packs
        AstNodeExpr* outp = nullptr;
        for (size_t i = 0; i < elems.size(); ++i) {
            AstNodeVhdlExpr* const valuep = byElem[i] ? byElem[i] : othersp;
            if (!valuep) {
                unsup(flp, "record aggregate with element '" + elems[i]->name() + "' unset");
                return nullptr;
            }
            AstNodeExpr* const memberp = lowerExpr(valuep);
            if (!memberp) return nullptr;
            outp = outp ? new AstConcat{flp, outp, memberp} : memberp;
        }
        return outp;
    }

    // METHODS - names

    static bool flattenName(const AstNodeVhdlExpr* nodep, std::vector<std::string>& out) {
        if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            out.push_back(namep->name());
            return true;
        }
        if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            if (!flattenName(selp->prefixp(), out)) return false;
            if (!selp->suffixp()) return false;
            return flattenName(selp->suffixp(), out);
        }
        return false;
    }

    // METHODS - concurrent statements (clause 11)

    void lowerConcurrent(AstNodeVhdlStmt* nodep) {
        FileLine* const flp = nodep->fileline();
        if (AstVhdlProcess* const procp = VN_CAST(nodep, VhdlProcess)) {
            lowerProcess(procp);
            return;
        }
        if (AstVhdlSigAssign* const asgp = VN_CAST(nodep, VhdlSigAssign)) {
            lowerConcurrentAssign(asgp);
            return;
        }
        if (AstVhdlInstance* const instp = VN_CAST(nodep, VhdlInstance)) {
            lowerInstance(instp);
            return;
        }
        if (AstVhdlBlock* const blockp = VN_CAST(nodep, VhdlBlock)) {
            // A block is a scope, not a construct of its own; its guard and
            // its header are for later milestones
            if (blockp->guardp() || blockp->headerp()) unsup(flp, "guarded block");
            for (AstNode* p = blockp->declsp(); p; p = p->nextp()) lowerDecl(p);
            for (AstNode* p = blockp->stmtsp(); p; p = p->nextp()) {
                lowerConcurrent(VN_AS(p, NodeVhdlStmt));
            }
            return;
        }
        if (AstVhdlGenerate* const genp = VN_CAST(nodep, VhdlGenerate)) {
            lowerGenerate(genp);
            return;
        }
        if (AstVhdlAssert* const assertp = VN_CAST(nodep, VhdlAssert)) {
            // A concurrent assertion is a process that runs when its
            // expression changes (11.5)
            AstNode* const bodyp = lowerAssert(assertp);
            addStmt(new AstAlways{
                flp, VAlwaysKwd::ALWAYS, new AstSenTree{flp, new AstSenItem{flp, AstSenItem::Combo{}}},
                bodyp});
            return;
        }
        if (AstVhdlProcCall* const callp = VN_CAST(nodep, VhdlProcCall)) {
            AstNode* const bodyp = lowerStmt(callp);
            addStmt(new AstAlways{
                flp, VAlwaysKwd::ALWAYS, new AstSenTree{flp, new AstSenItem{flp, AstSenItem::Combo{}}},
                bodyp});
            return;
        }
        unsup(flp, std::string{"concurrent statement "} + nodep->typeName());
    }

    // A process becomes an always block. Which sensitivity it gets is the
    // single most consequential choice in this file: a clocked process must
    // become edge-triggered, or every register would be re-evaluated whenever
    // any of its inputs changed and the speed the project exists for is lost.
    void lowerProcess(AstVhdlProcess* nodep) {
        FileLine* const flp = nodep->fileline();
        VL_RESTORER_COPY(m_prefix);
        const std::string label
            = nodep->label().empty() ? "__Vprocess" + std::to_string(++m_processNum)
                                     : nodep->label();
        m_prefix = label + "__";
        for (AstNode* p = nodep->declsp(); p; p = p->nextp()) lowerDecl(p);

        bool rising = false;
        AstNodeVhdlExpr* clockp = nullptr;
        AstVhdlIf* const ifp = clockedIf(nodep, clockp, rising);
        AstNode* bodyp = nullptr;
        AstSenTree* sentreep = nullptr;
        if (ifp) {
            sentreep = new AstSenTree{
                flp, new AstSenItem{flp, rising ? VEdgeType::ET_POSEDGE : VEdgeType::ET_NEGEDGE,
                                    lowerExpr(clockp)}};
            bodyp = lowerClockedIf(ifp, sentreep);
            // Every VHDL process runs once when the simulation starts, before
            // any signal has changed (11.3). For a clocked process that is
            // what applies an asynchronous reset that is already asserted at
            // time zero -- something an "always @(posedge)" would miss,
            // because no edge has happened yet. The clocked branch does not
            // run, since no clock edge has happened either.
            if (AstNode* const initialp = resetChain(ifp)) {
                allowInitialDriver(ifp->altsp(), true);
                addStmt(new AstInitial{flp, initialp});
            }
        } else {
            sentreep = sensitivity(nodep);
            bodyp = lowerStmts(nodep->stmtsp());
            // The same rule for a process that is not clocked: it runs once
            // at the start, whatever its sensitivity list says
            if (bodyp && nodep->hasSensList() && !nodep->sensAll() && nodep->sensp()) {
                allowInitialDriver(nodep->stmtsp(), false);
                addStmt(new AstInitial{flp, lowerStmts(nodep->stmtsp())});
            }
        }
        if (!sentreep && nodep->hasSensList()) return;  // Already reported
        if (!sentreep && !bodyp) return;
        addStmt(new AstAlways{flp, VAlwaysKwd::ALWAYS, sentreep, bodyp});
    }

    // The time-zero run and the always block both drive whatever the process
    // assigns, which Verilator would otherwise report as two drivers. They
    // cannot conflict: the time-zero run happens before any edge, and the two
    // are the same statements.
    void allowInitialDriver(AstNode* stmtsp, bool skipLast) {
        for (AstNode* p = stmtsp; p; p = p->nextp()) {
            if (skipLast && !p->nextp()) return;  // The clocked branch
            if (AstVhdlAlternative* const altp = VN_CAST(p, VhdlAlternative)) {
                allowInitialDriver(altp->stmtsp(), false);
                continue;
            }
            if (AstVhdlSigAssign* const asgp = VN_CAST(p, VhdlSigAssign)) {
                VhdlSymbol* const symp = targetSymbol(asgp->targetp());
                const auto it = symp ? m_vars.find(symp) : m_vars.end();
                if (it != m_vars.end()) {
                    it->second->fileline()->warnOff(V3ErrorCode::MULTIDRIVEN, true);
                }
                continue;
            }
            if (AstVhdlIf* const ifp = VN_CAST(p, VhdlIf)) {
                allowInitialDriver(ifp->altsp(), false);
                continue;
            }
            if (AstVhdlCase* const casep = VN_CAST(p, VhdlCase)) {
                allowInitialDriver(casep->altsp(), false);
                continue;
            }
            if (AstVhdlLoop* const loopp = VN_CAST(p, VhdlLoop)) {
                allowInitialDriver(loopp->stmtsp(), false);
                continue;
            }
        }
    }

    // The object an assignment target names, however it is indexed or
    // selected
    static VhdlSymbol* targetSymbol(AstNodeVhdlExpr* nodep) {
        while (nodep) {
            if (VhdlSymbol* const symp = nodep->vhdlSymp()) {
                if (symp->isObject()) return symp;
            }
            if (AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
                nodep = callp->prefixp();
                continue;
            }
            if (AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
                nodep = selp->prefixp();
                continue;
            }
            return nodep->vhdlSymp();
        }
        return nullptr;
    }

    // The asynchronous branches of a clocked process, without the clocked one.
    // This is what the process does when it runs at time zero, since no clock
    // edge has happened then.
    AstNode* resetChain(AstVhdlIf* ifp) {
        std::vector<AstVhdlAlternative*> alts;
        for (AstNode* p = ifp->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlAlternative));
        }
        if (alts.size() < 2) return nullptr;  // Nothing but the clock test
        AstNode* outp = nullptr;
        for (size_t i = alts.size() - 1; i-- > 0;) {
            outp = new AstIf{alts[i]->fileline(), lowerCondition(alts[i]->condp()),
                             lowerStmts(alts[i]->stmtsp()), outp};
        }
        return outp;
    }

    // The sensitivity of a process that is not clocked
    AstSenTree* sensitivity(AstVhdlProcess* nodep) {
        FileLine* const flp = nodep->fileline();
        if (!nodep->hasSensList()) {
            // A process with no list runs its statements over and over,
            // suspending only where it waits (11.3). That is Verilog's
            // "always" with no event control, which needs the suspendable
            // processes of --timing.
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "process that waits without --timing");
                return nullptr;
            }
            return nullptr;  // No sensitivity: the waits inside do the work
        }
        if (nodep->sensAll() || !nodep->sensp()) {
            return new AstSenTree{flp, new AstSenItem{flp, AstSenItem::Combo{}}};
        }
        // VHDL runs the process when any listed signal changes, which is what
        // an any-edge sensitivity item means. Keeping the written list rather
        // than substituting "all" preserves the semantics of an incomplete
        // one, deliberate or not.
        AstSenItem* itemsp = nullptr;
        for (AstNode* p = nodep->sensp(); p; p = p->nextp()) {
            AstNodeExpr* const sensp = lowerExpr(VN_AS(p, NodeVhdlExpr));
            if (!sensp) continue;
            itemsp = AstNode::addNext(
                itemsp, new AstSenItem{flp, VEdgeType::ET_BOTHEDGE, sensp});
        }
        if (!itemsp) return new AstSenTree{flp, new AstSenItem{flp, AstSenItem::Combo{}}};
        return new AstSenTree{flp, itemsp};
    }

    // If the process is a clocked one, the "if" that tests the edge, with the
    // clock expression and its direction. Recognises the two forms that
    // matter: a body that is one edge test, and one that is an asynchronous
    // reset followed by an edge test (11.3).
    AstVhdlIf* clockedIf(AstVhdlProcess* nodep, AstNodeVhdlExpr*& clockp, bool& rising) {
        if (!nodep->hasSensList()) return nullptr;
        AstVhdlIf* const ifp = VN_CAST(nodep->stmtsp(), VhdlIf);
        if (!ifp || ifp->nextp()) return nullptr;  // The body must be just the "if"
        AstVhdlAlternative* lastp = nullptr;
        for (AstNode* p = ifp->altsp(); p; p = p->nextp()) {
            lastp = VN_AS(p, VhdlAlternative);
            // An "else" branch cannot follow the edge test: what would it do
            // between clock edges?
            if (!lastp->condp() && p->nextp()) return nullptr;
        }
        if (!lastp || !lastp->condp()) return nullptr;
        clockp = edgeTest(lastp->condp(), rising);
        return clockp ? ifp : nullptr;
    }

    // The signal an edge test names, or null if the expression is not one
    AstNodeVhdlExpr* edgeTest(AstNodeVhdlExpr* condp, bool& rising) {
        if (AstVhdlCall* const callp = VN_CAST(condp, VhdlCall)) {
            const VhdlSymbol* const symp = callp->vhdlSymp();
            if (!symp) return nullptr;
            if (symp->name() == "rising_edge") {
                rising = true;
            } else if (symp->name() == "falling_edge") {
                rising = false;
            } else {
                return nullptr;
            }
            AstVhdlAssoc* const argp = VN_CAST(callp->argsp(), VhdlAssoc);
            if (!argp || argp->nextp()) return nullptr;
            return argp->actualp();
        }
        // clk'event and clk = '1'
        AstVhdlBinary* const andp = VN_CAST(condp, VhdlBinary);
        if (!andp || andp->op() != VhdlBinaryOp::AND) return nullptr;
        AstNodeVhdlExpr* eventOn = nullptr;
        AstNodeVhdlExpr* levelp = nullptr;
        for (AstNodeVhdlExpr* const sidep : {andp->lhsp(), andp->rhsp()}) {
            if (AstVhdlAttrName* const attrp = VN_CAST(sidep, VhdlAttrName)) {
                if (attrp->name() == "event") eventOn = attrp->prefixp();
            } else {
                levelp = sidep;
            }
        }
        if (!eventOn || !levelp) return nullptr;
        AstVhdlBinary* const eqp = VN_CAST(levelp, VhdlBinary);
        if (!eqp || eqp->op() != VhdlBinaryOp::EQ) return nullptr;
        const VhdlValue val = V3VhdlEval::eval(eqp->rhsp());
        if (!val.isInt() || !val.typep()) return nullptr;
        const VhdlType* const basep = val.typep()->base();
        if (!enumIsLogic(basep)) return nullptr;
        rising = logicValueOf(basep->literals()[val.toInt()]->name()) == LogicVal::ONE;
        return eventOn;
    }

    // The body of a clocked process, with the edge test removed and any
    // asynchronous resets kept as ordinary conditions. The sentree gains an
    // edge item for each reset, so that the process runs when one asserts.
    AstNode* lowerClockedIf(AstVhdlIf* ifp, AstSenTree* sentreep) {
        std::vector<AstVhdlAlternative*> alts;
        for (AstNode* p = ifp->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlAlternative));
        }
        AstNode* elsep = lowerStmts(alts.back()->stmtsp());
        for (size_t i = alts.size() - 1; i-- > 0;) {
            AstVhdlAlternative* const altp = alts[i];
            FileLine* const flp = altp->fileline();
            // An asynchronous reset makes the process run on its own edge too
            bool resetHigh = true;
            if (AstNodeVhdlExpr* const resetp = levelTest(altp->condp(), resetHigh)) {
                sentreep->addSensesp(new AstSenItem{
                    flp, resetHigh ? VEdgeType::ET_POSEDGE : VEdgeType::ET_NEGEDGE,
                    lowerExpr(resetp)});
            } else {
                err(flp, "A clocked process may only test an asynchronous reset before its "
                         "clock edge, and this condition is not one");
            }
            elsep = new AstIf{flp, lowerCondition(altp->condp()),
                              lowerStmts(altp->stmtsp()), elsep};
        }
        return elsep;
    }

    // "rst = '1'", "rst = '0'" or plain "rst", giving the signal and the
    // level that asserts it
    AstNodeVhdlExpr* levelTest(AstNodeVhdlExpr* condp, bool& high) {
        high = true;
        if (AstVhdlBinary* const eqp = VN_CAST(condp, VhdlBinary)) {
            if (eqp->op() != VhdlBinaryOp::EQ) return nullptr;
            const VhdlValue val = V3VhdlEval::eval(eqp->rhsp());
            if (!val.isInt() || !val.typep()) return nullptr;
            const VhdlType* const basep = val.typep()->base();
            if (basep->kind() != VhdlTypeKind::ENUM) return nullptr;
            const std::string& litName = basep->literals()[val.toInt()]->name();
            high = enumIsLogic(basep) ? logicValueOf(litName) == LogicVal::ONE
                                      : val.toInt() != 0;
            return eqp->lhsp();
        }
        // A BOOLEAN or BIT signal used directly as a condition
        if (VN_IS(condp, VhdlName) || VN_IS(condp, VhdlSelected)) return condp;
        return nullptr;
    }

    // A concurrent signal assignment is a process of its own (11.6)
    void lowerConcurrentAssign(AstVhdlSigAssign* nodep) {
        FileLine* const flp = nodep->fileline();
        AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
        if (!lhsp) return;
        AstNodeExpr* rhsp = nullptr;
        AstNode* delayp = nullptr;
        switch (nodep->kind()) {
        case VhdlAssignKind::SIMPLE:
            rhsp = waveformValue(nodep->wavesp());
            delayp = waveformDelay(nodep->wavesp());
            break;
        case VhdlAssignKind::CONDITIONAL: rhsp = conditionalValue(nodep); break;
        case VhdlAssignKind::SELECTED: rhsp = selectedValue(nodep); break;
        }
        if (!rhsp) {
            VL_DO_DANGLING(lhsp->deleteTree(), lhsp);
            return;
        }
        addStmt(new AstAlways{new AstAssignW{flp, lhsp, rhsp, delayp}});
    }

    AstNodeExpr* waveformValue(AstVhdlWaveform* wfp) {
        if (!wfp || !wfp->valuep()) return nullptr;
        if (wfp->nextp()) {
            unsup(wfp->fileline(), "concurrent assignment of a multi-element waveform");
            return nullptr;
        }
        return lowerExpr(wfp->valuep());
    }

    // The delay of a concurrent assignment, if it has one (11.6)
    AstNode* waveformDelay(AstVhdlWaveform* wfp) {
        if (!wfp || !wfp->afterp()) return nullptr;
        if (!v3Global.opt.timing().isTrue()) {
            unsup(wfp->fileline(), "'after' delay without --timing");
            return nullptr;
        }
        AstNodeExpr* const timep = lowerExpr(wfp->afterp());
        return timep ? new AstDelay{wfp->fileline(), timep, false} : nullptr;
    }

    // "q <= a when c else b" is a conditional expression (11.6)
    AstNodeExpr* conditionalValue(AstVhdlSigAssign* nodep) {
        std::vector<AstVhdlAlternative*> alts;
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlAlternative));
        }
        if (alts.empty()) return nullptr;
        AstNodeExpr* outp = nullptr;
        for (size_t i = alts.size(); i-- > 0;) {
            AstNodeExpr* const valuep = waveformValue(alts[i]->wavesp());
            if (!valuep) return nullptr;
            if (!alts[i]->condp()) {
                outp = valuep;  // The trailing "else"
            } else {
                if (!outp) {
                    // Without an else the signal keeps its value, which for a
                    // concurrent assignment means assigning itself
                    outp = lowerExpr(nodep->targetp());
                }
                outp = new AstCond{alts[i]->fileline(), lowerCondition(alts[i]->condp()),
                                   valuep, outp};
            }
        }
        return outp;
    }

    // "with s select q <= ..." (11.6)
    AstNodeExpr* selectedValue(AstVhdlSigAssign* nodep) {
        AstNodeVhdlExpr* const selp = nodep->selectorp();
        if (!selp) return nullptr;
        std::vector<AstVhdlAlternative*> alts;
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlAlternative));
        }
        AstNodeExpr* outp = nullptr;
        for (size_t i = alts.size(); i-- > 0;) {
            AstVhdlAlternative* const altp = alts[i];
            AstNodeExpr* const valuep = waveformValue(altp->wavesp());
            if (!valuep) return nullptr;
            AstNodeExpr* const testp = choiceTest(altp->choicesp(), selp);
            if (!testp) {
                outp = valuep;  // "when others"
                continue;
            }
            if (!outp) outp = lowerExpr(nodep->targetp());
            outp = new AstCond{altp->fileline(), testp, valuep, outp};
        }
        return outp;
    }

    // The condition that one set of choices matches the selector, or null for
    // "others"
    AstNodeExpr* choiceTest(AstNode* choicesp, AstNodeVhdlExpr* selp) {
        AstNodeExpr* testp = nullptr;
        for (AstNode* p = choicesp; p; p = p->nextp()) {
            AstVhdlChoice* const chp = VN_AS(p, VhdlChoice);
            if (chp->isOthers()) return nullptr;
            FileLine* const flp = chp->fileline();
            AstNodeExpr* onep = nullptr;
            if (AstVhdlRange* const rp = chp->rangep()) {
                const VhdlValue lo = V3VhdlEval::eval(rp->leftp());
                const VhdlValue hi = V3VhdlEval::eval(rp->rightp());
                if (!lo.known() || !hi.known()) {
                    unsup(flp, "non-static range choice");
                    return nullptr;
                }
                AstNodeExpr* const sel1p = lowerExpr(selp);
                AstNodeExpr* const sel2p = lowerExpr(selp);
                if (!sel1p || !sel2p) return nullptr;
                onep = new AstAnd{flp,
                                  new AstGteS{flp, sel1p, lowerValue(lo, lo.typep(), flp)},
                                  new AstLteS{flp, sel2p, lowerValue(hi, hi.typep(), flp)}};
            } else if (chp->valuep()) {
                AstNodeExpr* const selValp = lowerExpr(selp);
                AstNodeExpr* const chValp = lowerExpr(chp->valuep());
                if (!selValp || !chValp) return nullptr;
                onep = new AstEq{flp, selValp, chValp};
            }
            if (!onep) continue;
            testp = testp ? new AstOr{flp, testp, onep} : onep;
        }
        return testp;
    }

    // An instance binds to the entity of the same name, which is VHDL's
    // default binding rule (7.3.3)
    void lowerInstance(AstVhdlInstance* nodep) {
        FileLine* const flp = nodep->fileline();
        std::vector<std::string> parts;
        if (!flattenName(nodep->unitp(), parts) || parts.empty()) {
            unsup(flp, "instantiated unit name");
            return;
        }
        const std::string modName = parts.back();
        AstPin* pinsp = nullptr;
        int pinNum = 0;
        for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
            AstPin* const pinp = lowerAssoc(VN_AS(p, VhdlAssoc), ++pinNum);
            if (pinp) pinsp = AstNode::addNext(pinsp, pinp);
        }
        AstPin* paramsp = nullptr;
        pinNum = 0;
        for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
            AstPin* const pinp = lowerAssoc(VN_AS(p, VhdlAssoc), ++pinNum);
            if (pinp) paramsp = AstNode::addNext(paramsp, pinp);
        }
        const std::string label
            = nodep->label().empty() ? "__Vinst" + std::to_string(++m_processNum)
                                     : nodep->label();
        addStmt(new AstCell{flp, flp, label, modName, pinsp, paramsp, nullptr});
    }

    AstPin* lowerAssoc(AstVhdlAssoc* nodep, int pinNum) {
        FileLine* const flp = nodep->fileline();
        std::string formal;
        if (nodep->formalp()) {
            std::vector<std::string> parts;
            if (flattenName(nodep->formalp(), parts) && !parts.empty()) formal = parts.back();
        }
        if (nodep->isOpen()) return new AstPin{flp, pinNum, formal, nullptr};
        return new AstPin{flp, pinNum, formal, lowerExpr(nodep->actualp())};
    }

    // A generate statement elaborates its body once per value of its
    // parameter, or once if its condition holds (11.8). Verilator's own
    // generate machinery does the elaboration, so lowering only has to build
    // the right shape.
    void lowerGenerate(AstVhdlGenerate* nodep) {
        FileLine* const flp = nodep->fileline();
        const std::string label = nodep->label().empty()
                                      ? "__Vgen" + std::to_string(++m_processNum)
                                      : nodep->label();
        if (nodep->kind() == VhdlLoopKind::FOR) {
            VhdlSymbol* const symp = nodep->vhdlSymp();
            if (!symp || !symp->typep() || !symp->typep()->bounded()) {
                unsup(flp, "generate over a range that is not locally static");
                return;
            }
            VhdlType* const rangep = symp->typep();
            const bool ascending = rangep->ascending();
            AstVar* const genvarp
                = new AstVar{flp, VVarType::GENVAR, m_prefix + symp->name(), VFlagChildDType{},
                             lowerType(rangep, flp)};
            m_vars.emplace(symp, genvarp);
            AstNode* const initp = new AstAssign{
                flp, new AstVarRef{flp, genvarp, VAccess::WRITE},
                lowerValue(VhdlValue::mkInt(rangep->left(), rangep), rangep, flp)};
            AstNodeExpr* const limitp
                = lowerValue(VhdlValue::mkInt(rangep->right(), rangep), rangep, flp);
            AstNodeExpr* const condp
                = ascending ? static_cast<AstNodeExpr*>(new AstLteS{
                                  flp, new AstVarRef{flp, genvarp, VAccess::READ}, limitp})
                            : static_cast<AstNodeExpr*>(new AstGteS{
                                  flp, new AstVarRef{flp, genvarp, VAccess::READ}, limitp});
            AstNode* const incp = new AstAssign{
                flp, new AstVarRef{flp, genvarp, VAccess::WRITE},
                ascending ? static_cast<AstNodeExpr*>(new AstAdd{
                                flp, new AstVarRef{flp, genvarp, VAccess::READ},
                                new AstConst{flp, AstConst::Signed32{}, 1}})
                          : static_cast<AstNodeExpr*>(new AstSub{
                                flp, new AstVarRef{flp, genvarp, VAccess::READ},
                                new AstConst{flp, AstConst::Signed32{}, 1}})};
            AstNode* const itemsp = lowerGenerateBody(nodep->altsp());
            AstGenBlock* const blockp = new AstGenBlock{flp, label, nullptr, false};
            blockp->addItemsp(genvarp);
            blockp->genforp(new AstGenFor{flp, initp, condp, incp, itemsp});
            addStmt(blockp);
            return;
        }
        if (nodep->exprp()) {
            unsup(flp, "case generate statement");
            return;
        }
        // An if generate, with its elsif and else branches
        std::vector<AstVhdlGenerateBody*> alts;
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlGenerateBody));
        }
        AstNode* outp = nullptr;
        for (size_t i = alts.size(); i-- > 0;) {
            AstNode* const itemsp = lowerGenerateBody(alts[i]);
            const std::string altLabel
                = alts[i]->name().empty() ? label : label + "__" + alts[i]->name();
            AstNode* const blockp = new AstGenBlock{flp, altLabel, itemsp, false};
            if (!alts[i]->condp()) {
                outp = blockp;  // The trailing "else"
                continue;
            }
            outp = new AstGenIf{alts[i]->fileline(), lowerCondition(alts[i]->condp()), blockp,
                                outp};
        }
        addStmt(outp);
    }

    // The declarations and statements of one generate alternative, collected
    // rather than added to the enclosing module
    AstNode* lowerGenerateBody(AstVhdlGenerateBody* nodep) {
        if (!nodep) return nullptr;
        AstNode* const outerp = m_stmtsp;
        m_stmtsp = nullptr;
        for (AstNode* p = nodep->declsp(); p; p = p->nextp()) lowerDecl(p);
        for (AstNode* p = nodep->stmtsp(); p; p = p->nextp()) {
            lowerConcurrent(VN_AS(p, NodeVhdlStmt));
        }
        AstNode* const itemsp = m_stmtsp;
        m_stmtsp = outerp;
        return itemsp;
    }

    // METHODS - sequential statements (clause 10)

    AstNode* lowerStmts(AstNode* stmtsp) {
        AstNode* outp = nullptr;
        for (AstNode* p = stmtsp; p; p = p->nextp()) {
            outp = AstNode::addNextNull(outp, lowerStmt(VN_AS(p, NodeVhdlStmt)));
        }
        return outp;
    }

    AstNode* lowerStmt(AstNodeVhdlStmt* nodep) {
        FileLine* const flp = nodep->fileline();
        if (AstVhdlSigAssign* const asgp = VN_CAST(nodep, VhdlSigAssign)) {
            return lowerSeqAssign(asgp, true);
        }
        if (AstVhdlVarAssign* const asgp = VN_CAST(nodep, VhdlVarAssign)) {
            return lowerVarAssign(asgp);
        }
        if (AstVhdlIf* const ifp = VN_CAST(nodep, VhdlIf)) return lowerIf(ifp);
        if (AstVhdlCase* const casep = VN_CAST(nodep, VhdlCase)) return lowerCase(casep);
        if (AstVhdlLoop* const loopp = VN_CAST(nodep, VhdlLoop)) return lowerLoop(loopp);
        if (VN_IS(nodep, VhdlNull)) return nullptr;
        if (AstVhdlAssert* const assertp = VN_CAST(nodep, VhdlAssert)) {
            return lowerAssert(assertp);
        }
        if (AstVhdlNext* const nextp = VN_CAST(nodep, VhdlNext)) {
            AstNode* const jumpp = nextp->isExit() ? static_cast<AstNode*>(new AstBreak{flp})
                                                   : static_cast<AstNode*>(new AstContinue{flp});
            if (!nextp->condp()) return jumpp;
            return new AstIf{flp, lowerCondition(nextp->condp()), jumpp, nullptr};
        }
        if (AstVhdlReturn* const retp = VN_CAST(nodep, VhdlReturn)) {
            return new AstReturn{flp, retp->valuep() ? lowerExpr(retp->valuep()) : nullptr};
        }
        if (AstVhdlWait* const waitp = VN_CAST(nodep, VhdlWait)) return lowerWait(waitp);
        if (AstVhdlProcCall* const callp = VN_CAST(nodep, VhdlProcCall)) {
            return lowerProcCall(callp);
        }
        unsup(flp, std::string{"statement "} + nodep->typeName());
        return nullptr;
    }

    // A signal assignment takes effect at the end of the delta cycle, which
    // is exactly what Verilator's non-blocking assignment region means
    AstNode* lowerSeqAssign(AstVhdlSigAssign* nodep, bool sequential) {
        FileLine* const flp = nodep->fileline();
        if (nodep->kind() != VhdlAssignKind::SIMPLE) {
            unsup(flp, "conditional signal assignment in a process");
            return nullptr;
        }
        AstVhdlWaveform* const wfp = nodep->wavesp();
        if (!wfp || !wfp->valuep()) return nullptr;
        if (wfp->nextp()) {
            unsup(flp, "multi-element waveform");
            return nullptr;
        }
        AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
        AstNodeExpr* const rhsp = lowerExpr(wfp->valuep());
        if (!lhsp || !rhsp) return nullptr;
        AstNode* delayp = nullptr;
        if (wfp->afterp()) {
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "'after' delay without --timing");
                return nullptr;
            }
            delayp = new AstDelay{flp, lowerExpr(wfp->afterp()), false};
        }
        if (!sequential) return new AstAssignW{flp, lhsp, rhsp, delayp};
        return new AstAssignDly{flp, lhsp, rhsp, delayp};
    }

    // A variable assignment takes effect at once (10.6)
    AstNode* lowerVarAssign(AstVhdlVarAssign* nodep) {
        FileLine* const flp = nodep->fileline();
        if (nodep->kind() != VhdlAssignKind::SIMPLE) {
            unsup(flp, "conditional variable assignment");
            return nullptr;
        }
        AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
        AstNodeExpr* const rhsp = nodep->valuep() ? lowerExpr(nodep->valuep()) : nullptr;
        if (!lhsp || !rhsp) return nullptr;
        return new AstAssign{flp, lhsp, rhsp};
    }

    AstNode* lowerIf(AstVhdlIf* nodep) {
        std::vector<AstVhdlAlternative*> alts;
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlAlternative));
        }
        AstNode* outp = nullptr;
        for (size_t i = alts.size(); i-- > 0;) {
            AstVhdlAlternative* const altp = alts[i];
            if (!altp->condp()) {
                outp = lowerStmts(altp->stmtsp());
                continue;
            }
            outp = new AstIf{altp->fileline(), lowerCondition(altp->condp()),
                             lowerStmts(altp->stmtsp()), outp};
        }
        return outp;
    }

    AstNode* lowerCase(AstVhdlCase* nodep) {
        FileLine* const flp = nodep->fileline();
        AstNodeExpr* const exprp = lowerExpr(nodep->exprp());
        if (!exprp) return nullptr;
        AstCaseItem* itemsp = nullptr;
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            AstVhdlAlternative* const altp = VN_AS(p, VhdlAlternative);
            AstNodeExpr* condsp = nullptr;
            bool others = false;
            for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                if (chp->isOthers()) {
                    others = true;
                    continue;
                }
                if (AstVhdlRange* const rp = chp->rangep()) {
                    // Verilator's case has no ranges, so each value is listed
                    const VhdlValue lo = V3VhdlEval::eval(rp->leftp());
                    const VhdlValue hi = V3VhdlEval::eval(rp->rightp());
                    if (!lo.isInt() || !hi.isInt()) {
                        unsup(chp->fileline(), "non-static range choice");
                        continue;
                    }
                    const int64_t low = rp->dir() == VhdlDir::TO ? lo.toInt() : hi.toInt();
                    const int64_t high = rp->dir() == VhdlDir::TO ? hi.toInt() : lo.toInt();
                    if (high - low > 4096) {
                        unsup(chp->fileline(), "range choice covering many values");
                        continue;
                    }
                    for (int64_t v = low; v <= high; ++v) {
                        condsp = AstNode::addNext(
                            condsp, lowerValue(VhdlValue::mkInt(v, lo.typep()), lo.typep(),
                                               chp->fileline()));
                    }
                    continue;
                }
                if (chp->valuep()) {
                    condsp = AstNode::addNextNull(condsp, lowerExpr(chp->valuep()));
                }
            }
            AstNode* const stmtsp = lowerStmts(altp->stmtsp());
            itemsp = AstNode::addNextNull(
                itemsp, new AstCaseItem{flp, others ? nullptr : condsp, stmtsp});
        }
        AstCase* const casep = new AstCase{flp, VCaseType::CT_CASE, exprp, itemsp};
        // A VHDL case must name every value of its expression's subtype, and
        // analysis has already checked that it does (10.9). An enumeration
        // lowers to the smallest number of bits that holds it, so the spare
        // encodings are unreachable and Verilator should not ask about them.
        casep->fullPragma(true);
        return casep;
    }

    AstNode* lowerLoop(AstVhdlLoop* nodep) {
        FileLine* const flp = nodep->fileline();
        AstLoop* const loopp = new AstLoop{flp};
        switch (nodep->kind()) {
        case VhdlLoopKind::LOOP: break;
        case VhdlLoopKind::WHILE:
            loopp->addStmtsp(new AstLoopTest{flp, loopp, lowerCondition(nodep->condp())});
            break;
        case VhdlLoopKind::FOR: return lowerForLoop(nodep);
        }
        loopp->addStmtsp(lowerStmts(nodep->stmtsp()));
        return loopp;
    }

    // A for loop declares its own parameter, which VHDL makes a constant of
    // the range's subtype (10.10)
    AstNode* lowerForLoop(AstVhdlLoop* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp || !symp->typep() || !symp->typep()->bounded()) {
            unsup(flp, "loop over a range that is not locally static");
            return nullptr;
        }
        VhdlType* const rangep = symp->typep();
        AstVar* const varp = newVar(symp, VVarType::VAR, flp);
        varp->usedLoopIdx(true);
        addStmt(varp);
        const bool ascending = rangep->ascending();
        AstNodeExpr* const initp = lowerValue(
            VhdlValue::mkInt(rangep->left(), rangep), rangep, flp);
        AstNodeExpr* const limitp = lowerValue(
            VhdlValue::mkInt(rangep->right(), rangep), rangep, flp);
        AstLoop* const loopp = new AstLoop{flp};
        AstNodeExpr* const condp
            = ascending ? static_cast<AstNodeExpr*>(new AstLteS{
                              flp, new AstVarRef{flp, varp, VAccess::READ}, limitp})
                        : static_cast<AstNodeExpr*>(new AstGteS{
                              flp, new AstVarRef{flp, varp, VAccess::READ}, limitp});
        loopp->addStmtsp(new AstLoopTest{flp, loopp, condp});
        loopp->addStmtsp(lowerStmts(nodep->stmtsp()));
        loopp->addContsp(new AstAssign{
            flp, new AstVarRef{flp, varp, VAccess::WRITE},
            ascending ? static_cast<AstNodeExpr*>(new AstAdd{
                            flp, new AstVarRef{flp, varp, VAccess::READ},
                            new AstConst{flp, AstConst::Signed32{}, 1}})
                      : static_cast<AstNodeExpr*>(new AstSub{
                            flp, new AstVarRef{flp, varp, VAccess::READ},
                            new AstConst{flp, AstConst::Signed32{}, 1}})});
        AstNode* const outp
            = new AstAssign{flp, new AstVarRef{flp, varp, VAccess::WRITE}, initp};
        return AstNode::addNext(outp, static_cast<AstNode*>(loopp));
    }


    AstNode* lowerProcCall(AstVhdlProcCall* nodep) {
        FileLine* const flp = nodep->fileline();
        AstVhdlCall* const callp = VN_CAST(nodep->callp(), VhdlCall);
        AstNodeVhdlExpr* const namep = callp ? callp->prefixp() : nodep->callp();
        VhdlSymbol* const symp = callp ? callp->vhdlSymp() : nodep->callp()->vhdlSymp();
        // std.env's control procedures are the simulator, not VHDL (16.5).
        // Both end the run here: STOP means "suspend", which only an
        // interactive simulator can offer.
        if (symp && symp->predefined()
            && (symp->name() == "stop" || symp->name() == "finish")) {
            return new AstFinish{flp};
        }
        const auto it = symp ? m_funcs.find(symp) : m_funcs.end();
        if (it == m_funcs.end() || !VN_IS(it->second, Task)) {
            unsup(flp, "procedure call to '"
                           + (symp ? symp->name() : std::string{"<name>"}) + "'");
            return nullptr;
        }
        AstTaskRef* const refp = new AstTaskRef{flp, it->second->name(), nullptr};
        refp->taskp(it->second);
        if (callp) {
            for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
                AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->actualp()) continue;
                refp->addArgsp(new AstArg{flp, "", lowerExpr(asp->actualp())});
            }
        }
        (void)namep;
        return new AstStmtExpr{flp, refp};
    }

    // The sensitivity a list of signal names gives
    AstSenTree* sensitivityOf(AstNode* sensp, FileLine* flp) {
        AstSenItem* itemsp = nullptr;
        for (AstNode* p = sensp; p; p = p->nextp()) {
            AstNodeExpr* const exprp = lowerExpr(VN_AS(p, NodeVhdlExpr));
            if (!exprp) continue;
            itemsp
                = AstNode::addNext(itemsp, new AstSenItem{flp, VEdgeType::ET_BOTHEDGE, exprp});
        }
        return itemsp ? new AstSenTree{flp, itemsp} : nullptr;
    }

    // The sensitivity a condition implies: every signal it reads, which is
    // what "wait until" waits on when no list is written (10.2)
    static AstSenTree* sensitivityOfExpr(const AstNodeExpr* condp, FileLine* flp) {
        std::vector<AstVar*> varps;
        collectVars(condp, varps);
        AstSenItem* itemsp = nullptr;
        for (AstVar* const varp : varps) {
            itemsp = AstNode::addNext(
                itemsp, new AstSenItem{flp, VEdgeType::ET_BOTHEDGE,
                                       new AstVarRef{flp, varp, VAccess::READ}});
        }
        return itemsp ? new AstSenTree{flp, itemsp} : nullptr;
    }

    static void collectVars(const AstNode* nodep, std::vector<AstVar*>& out) {
        if (!nodep) return;
        if (const AstVarRef* const refp = VN_CAST(nodep, VarRef)) {
            AstVar* const varp = refp->varp();
            if (varp && std::find(out.begin(), out.end(), varp) == out.end()) out.push_back(varp);
        }
        collectVars(nodep->op1p(), out);
        collectVars(nodep->op2p(), out);
        collectVars(nodep->op3p(), out);
        collectVars(nodep->op4p(), out);
        collectVars(nodep->nextp(), out);
    }

    // A wait statement suspends the process until something happens (10.2).
    // Each of its three clauses has a direct equivalent, and the combination
    // "until C" already implies waiting for the signals C reads.
    AstNode* lowerWait(AstVhdlWait* nodep) {
        FileLine* const flp = nodep->fileline();
        if (!v3Global.opt.timing().isTrue()) {
            unsup(flp, "wait statement without --timing");
            return nullptr;
        }
        if (nodep->condp() && nodep->timeoutp()) {
            unsup(flp, "wait with both a condition and a timeout");
            return nullptr;
        }
        if (nodep->condp()) {
            // "wait until C" always suspends, and resumes when an event makes
            // C true (10.2). Verilog's wait(C) returns at once if C is
            // already true, which for "wait until clk = '1'" written while
            // the clock is high would spin without advancing time. So the
            // process is suspended on an event first, and only then waits.
            AstNodeExpr* const condp = lowerCondition(nodep->condp());
            if (!condp) return nullptr;
            AstSenTree* sensp = sensitivityOf(nodep->sensp(), flp);
            if (!sensp) sensp = sensitivityOfExpr(condp, flp);
            if (!sensp) return new AstWait{flp, condp, nullptr};
            AstNode* const outp = new AstEventControl{flp, sensp, nullptr};
            return AstNode::addNext(outp, static_cast<AstNode*>(
                                              new AstWait{flp, condp, nullptr}));
        }
        if (nodep->timeoutp()) {
            AstNodeExpr* const timep = lowerExpr(nodep->timeoutp());
            return timep ? new AstDelay{flp, timep, false} : nullptr;
        }
        if (nodep->sensp()) {
            AstSenTree* const sensp = sensitivityOf(nodep->sensp(), flp);
            return sensp ? new AstEventControl{flp, sensp, nullptr} : nullptr;
        }
        // A bare "wait" suspends for good: the process never resumes, so
        // whatever follows it never runs again
        return new AstWait{flp, new AstConst{flp, AstConst::BitFalse{}}, nullptr};
    }

    // "assert c report m severity s" is a check plus a message (10.3)
    AstNode* lowerAssert(AstVhdlAssert* nodep) {
        FileLine* const flp = nodep->fileline();
        // The severity decides whether the run stops (10.3)
        int64_t severity = nodep->isReport() ? 0 : 2;  // NOTE : ERROR
        if (nodep->severityp()) {
            const VhdlValue val = V3VhdlEval::eval(nodep->severityp());
            if (val.isInt()) severity = val.toInt();
        }
        static const char* const LEVELS[] = {"note", "warning", "error", "failure"};
        const char* const levelp = LEVELS[severity < 0 || severity > 3 ? 2 : severity];
        // The shape simulators print, so that a run can be diffed against one
        std::string format = flp->filename() + ":" + std::to_string(flp->lineno()) + ":"
                             + std::to_string(flp->firstColumn()) + ":@%0t:(report "
                             + levelp + "): ";
        AstNodeExpr* argsp = new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}};
        if (nodep->reportp()) {
            argsp = AstNode::addNext(argsp, lowerMessage(nodep->reportp(), format));
        } else {
            format += "Assertion violation";
        }
        AstNode* actionp
            = new AstDisplay{flp, VDisplayType::DT_DISPLAY, format, nullptr, argsp};
        if (severity >= 2) {
            actionp = AstNode::addNext(actionp,
                                       static_cast<AstNode*>(new AstStop{flp, severity >= 3}));
        }
        if (nodep->isReport() || !nodep->condp()) return actionp;
        return new AstIf{flp, new AstLogNot{flp, lowerCondition(nodep->condp())}, actionp,
                         nullptr};
    }

    // The message of a report or assertion, built as a format string and its
    // arguments rather than as a string value.
    //
    // VHDL builds such a message by concatenating text with 'IMAGE of a
    // value, which in an event-driven simulator means allocating strings at
    // run time. Turning the whole expression into one $display avoids that
    // entirely: the text becomes part of the format and each value a
    // conversion, which is how Verilog testbenches already print.
    AstNodeExpr* lowerMessage(AstNodeVhdlExpr* nodep, std::string& format) {
        if (!nodep) return nullptr;
        // A message that is entirely static is just text
        const VhdlValue val = V3VhdlEval::eval(nodep);
        if (val.isStr()) {
            appendLiteral(format, val.toStr());
            return nullptr;
        }
        if (AstVhdlBinary* const binp = VN_CAST(nodep, VhdlBinary)) {
            if (binp->op() == VhdlBinaryOp::CONCAT) {
                AstNodeExpr* const leftp = lowerMessage(binp->lhsp(), format);
                AstNodeExpr* const rightp = lowerMessage(binp->rhsp(), format);
                return AstNode::addNextNull(leftp, rightp);
            }
        }
        if (AstVhdlQualified* const qp = VN_CAST(nodep, VhdlQualified)) {
            return lowerMessage(qp->valuep(), format);
        }
        // 'IMAGE and TO_STRING both turn a value into text
        AstNodeVhdlExpr* valuep = nodep;
        if (AstVhdlAttrName* const attrp = VN_CAST(nodep, VhdlAttrName)) {
            if (attrp->name() == "image" && attrp->argp()) valuep = attrp->argp();
        } else if (AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
            const VhdlSymbol* const symp = callp->vhdlSymp();
            if (symp && symp->name() == "to_string") {
                if (AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc)) {
                    if (asp->actualp() && !asp->nextp()) valuep = asp->actualp();
                }
            }
        }
        AstNodeExpr* const exprp = lowerExpr(valuep);
        if (!exprp) {
            appendLiteral(format, "<?>");
            return nullptr;
        }
        format += formatFor(valuep->vhdlTypep());
        return exprp;
    }

    // Text that goes into a format string must not be read as a conversion
    static void appendLiteral(std::string& format, const std::string& text) {
        for (const char c : text) {
            if (c == '%') format += '%';
            format += c;
        }
    }

    // How a value of that type reads
    static std::string formatFor(const VhdlType* typep) {
        if (!typep) return "%0d";
        const VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::FLOATING:
        case VhdlTypeKind::UNIVERSAL_REAL: return "%g";
        case VhdlTypeKind::PHYSICAL: return "%0t";
        case VhdlTypeKind::ARRAY:
            // A vector of logic reads in binary, as VHDL prints it
            return basep->elementp() && enumIsLogic(basep->elementp()->base()) ? "%b" : "%0d";
        case VhdlTypeKind::ENUM: return enumIsLogic(basep) ? "%b" : "%0d";
        default: return "%0d";
        }
    }

    // A subprogram becomes a function or a task. VHDL's own rule that a
    // function may not assign a signal (4.2.1) is what makes this safe:
    // everything a function touches is local, so it emits as an ordinary C++
    // function that the compiler can inline.
    void lowerSubprogram(AstVhdlSubprogDecl* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp) return;
        if (!nodep->hasBody()) return;  // A declaration alone emits nothing
        if (m_funcs.count(symp)) return;  // Its declaration already made one
        // An overloaded name needs a distinct one in the emitted code
        const std::string name = uniqueName(symp);
        AstNodeFTask* ftaskp = nullptr;
        if (nodep->isFunction()) {
            AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
            funcp->fvarp(new AstVar{flp, VVarType::PORT, name, VFlagChildDType{},
                                    lowerType(symp->returnTypep(), flp)});
            ftaskp = funcp;
        } else {
            ftaskp = new AstTask{flp, name, nullptr};
        }
        // A VHDL subprogram's variables are created anew on each call (4.3),
        // which is what an automatic lifetime means
        ftaskp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        m_funcs.emplace(symp, ftaskp);
        VL_RESTORER(m_ftaskp);
        VL_RESTORER_COPY(m_prefix);
        m_ftaskp = ftaskp;
        m_prefix.clear();
        for (AstNode* p = nodep->paramsp(); p; p = p->nextp()) {
            AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            VhdlSymbol* const psymp = ifp->vhdlSymp();
            if (!psymp) continue;
            AstVar* const varp = new AstVar{flp, VVarType::PORT, ifp->name(), VFlagChildDType{},
                                            lowerType(psymp->typep(), flp)};
            switch (ifp->mode()) {
            case VhdlMode::OUT: varp->direction(VDirection::OUTPUT); break;
            case VhdlMode::INOUT:
            case VhdlMode::BUFFER: varp->direction(VDirection::INOUT); break;
            default: varp->direction(VDirection::INPUT); break;
            }
            varp->declDirection(varp->direction());
            varp->funcLocal(true);
            m_vars.emplace(psymp, varp);
            ftaskp->addStmtsp(varp);
        }
        for (AstNode* p = nodep->declsp(); p; p = p->nextp()) lowerDecl(p);
        ftaskp->addStmtsp(lowerStmts(nodep->stmtsp()));
        m_ftaskp = nullptr;  // The task itself belongs to the module
        addStmt(ftaskp);
    }

    // Two subprograms may share a name and differ only in their parameters,
    // which the emitted code cannot; number them apart
    std::string uniqueName(const VhdlSymbol* symp) {
        std::string base = symp->name();
        // An operator's designator is a string such as "and", which is not an
        // identifier in the generated code
        for (char& c : base) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') c = '_';
        }
        if (base.empty() || std::isdigit(static_cast<unsigned char>(base[0]))) {
            base = "op_" + base;
        }
        const int n = ++m_nameCounts[base];
        return n == 1 ? base : base + "__" + std::to_string(n);
    }
    std::map<std::string, int> m_nameCounts;

    // METHODS - design units

    // The module an entity and its architecture become
    void lowerEntity(AstVhdlEntity* entp, AstVhdlArchitecture* archp) {
        FileLine* const flp = entp->fileline();
        const std::string libname = entp->libname().empty() ? "work" : entp->libname();
        m_modp = new AstModule{flp, entp->name(), libname};
        // VHDL counts time in its primary unit, which for TIME is the
        // femtosecond (16.3), so that is the module's time unit, and the
        // design's precision must be at least as fine. Verilator keeps the
        // finest of the two, so a mixed-language design does not lose VHDL's
        // resolution.
        m_modp->timeunit(VTimescale::TS_1FS);
        m_netlistp->timeprecisionMerge(flp, VTimescale{VTimescale::TS_1FS});
        m_vars.clear();
        m_funcs.clear();
        m_prefix.clear();
        m_stmtsp = nullptr;
        m_processNum = 0;
        int pinNum = 0;
        for (AstNode* p = entp->genericsp(); p; p = p->nextp()) {
            lowerInterface(VN_AS(p, VhdlIfaceDecl), true, pinNum);
        }
        pinNum = 0;
        for (AstNode* p = entp->portsp(); p; p = p->nextp()) {
            lowerInterface(VN_AS(p, VhdlIfaceDecl), false, pinNum);
        }
        for (AstNode* p = entp->declsp(); p; p = p->nextp()) lowerDecl(p);
        if (archp) {
            for (AstNode* p = archp->declsp(); p; p = p->nextp()) lowerDecl(p);
            for (AstNode* p = archp->stmtsp(); p; p = p->nextp()) {
                lowerConcurrent(VN_AS(p, NodeVhdlStmt));
            }
        }
        m_modp->addStmtsp(m_stmtsp);
        m_stmtsp = nullptr;
        m_netlistp->addModulesp(m_modp);
        m_modp = nullptr;
    }
};

//######################################################################
// Design units

void VhdlLowerVisitor::lowerUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    // An entity and an architecture become one module, so pair them up
    // first. VHDL's default binding takes the architecture analyzed last
    // (7.3.3), which is what keeping the newest one gives.
    std::map<std::string, AstVhdlEntity*> entities;
    std::map<std::string, AstVhdlArchitecture*> archs;
    std::vector<std::string> order;  // Entities, in the order they arrived
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        const std::string lib = unitp->libname().empty() ? "work" : unitp->libname();
        if (AstVhdlEntity* const entp = VN_CAST(unitp, VhdlEntity)) {
            const std::string key = lib + "." + entp->name();
            if (entities.emplace(key, entp).second) order.push_back(key);
            continue;
        }
        if (AstVhdlArchitecture* const archp = VN_CAST(unitp, VhdlArchitecture)) {
            std::vector<std::string> parts;
            if (!flattenName(archp->entityNamep(), parts) || parts.empty()) continue;
            archs[lib + "." + parts.back()] = archp;
            continue;
        }
        if (VN_IS(unitp, VhdlPackage) || VN_IS(unitp, VhdlPackageBody)
            || VN_IS(unitp, VhdlContextDecl)) {
            // A package contributes types, constants and subprograms, which
            // are used where they are referenced rather than emitted here
            continue;
        }
        if (VN_IS(unitp, VhdlConfiguration)) {
            unsup(unitp->fileline(), "configuration declaration");
            continue;
        }
        unsup(unitp->fileline(), std::string{"design unit "} + unitp->typeName());
    }
    for (const std::string& key : order) {
        const auto archIt = archs.find(key);
        lowerEntity(entities[key], archIt == archs.end() ? nullptr : archIt->second);
    }
}

//######################################################################
// Entry point

bool V3VhdlLower::lower(const std::vector<AstNodeVhdlUnit*>& unitsp, AstNetlist* rootp) {
    VhdlLowerVisitor visitor{rootp};
    visitor.lowerUnits(unitsp);
    return !visitor.errored();
}
