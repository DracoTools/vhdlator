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
#include <set>
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
    std::set<const VhdlSymbol*> m_emitting;  // Subprograms being emitted, to stop recursion
    // A signal driven from more than one place is resolved (6.5.2). Each
    // driver keeps its own value and its own enable, and the signal becomes a
    // net those drivers are wired to, so the resolution Verilator already
    // does for nets settles it.
    struct VhdlDriver final {
        AstVar* valp = nullptr;
        AstVar* enp = nullptr;
        AstVar* weakp = nullptr;  // Driving, but only until something forces
    };
    std::map<const VhdlSymbol*, std::vector<VhdlDriver>> m_drivers;
    std::map<const VhdlSymbol*, int> m_driverCount;
    // How many of a signal's drivers have been lowered, which is the index of
    // the one being lowered now
    std::map<const VhdlSymbol*, int> m_driverUsed;
    // A protected type becomes a class, and a shared variable of it an
    // instance of that class (5.6). The class is built once, on first use.
    std::map<const VhdlType*, AstClass*> m_protClasses;
    AstClass* m_protClassp = nullptr;  // Protected type being built, if any
    VarMap m_vars;  // Declaration -> the variable it became
    std::map<std::string, AstNodeVhdlUnit*> m_entities;  // "lib.name" -> entity
    // The loops being lowered, innermost last. A labelled exit or next names
    // a loop that need not be the innermost (10.11, 10.12), which Verilator's
    // break and continue cannot express, so those become a jump out of a
    // block wrapping the loop or its body.
    struct VhdlLoopFrame final {
        std::string label;
        AstJumpBlock* exitp = nullptr;  // Wraps the loop; jumping leaves it
        AstJumpBlock* bodyp = nullptr;  // Wraps the body; jumping ends the pass
    };
    std::vector<VhdlLoopFrame> m_loops;
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
        } else if (m_protClassp) {
            m_protClassp->addStmtsp(nodep);
        } else {
            m_stmtsp = AstNode::addNextNull(m_stmtsp, nodep);
        }
    }

    void lowerUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;
    static std::string topArchitectureOf(AstVhdlBlockConfig* nodep) VL_MT_DISABLED;

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
            // Verilog's "integer" is four-state and VHDL's has no metavalues
            // at all, so "int" is the closer of the two -- and it is the type
            // DPI can carry across to C
            return new AstBasicDType{flp, VBasicDTypeKwd::INT, VSigning::SIGNED};
        case VhdlTypeKind::FLOATING:
        case VhdlTypeKind::UNIVERSAL_REAL:
            return new AstBasicDType{flp, VBasicDTypeKwd::DOUBLE};
        case VhdlTypeKind::PHYSICAL:
            // Counted in the primary unit, which for TIME is femtoseconds
            return new AstBasicDType{flp, VBasicDTypeKwd::LOGIC, VSigning::SIGNED,
                                     VNumRange{63, 0}, -1};
        case VhdlTypeKind::ARRAY:
            // STRING is text, not a vector of characters: it is what LINE
            // points at and what textio reads and writes (16.4)
            if (isStringType(basep)) return new AstBasicDType{flp, VBasicDTypeKwd::STRING};
            return lowerArrayType(typep, flp);
        case VhdlTypeKind::RECORD: return lowerRecordType(basep, flp);
        case VhdlTypeKind::FILE:
            // A file object is a descriptor, exactly as Verilog's is
            return new AstBasicDType{flp, VBasicDTypeKwd::INTEGER, VSigning::SIGNED};
        case VhdlTypeKind::ACCESS:
            // An access to STRING is a LINE, which is a string of its own.
            // Other access types wait on the general dynamic-object work.
            if (basep->designatedp() && isStringType(basep->designatedp()->base())) {
                return new AstBasicDType{flp, VBasicDTypeKwd::STRING};
            }
            unsup(flp, "access type '" + basep->prettyName() + "'");
            return newLogicDType(flp, 1);
        case VhdlTypeKind::PROTECTED:
            return new AstClassRefDType{flp, protectedClass(basep, flp), nullptr};
        default:
            unsup(flp, "type '" + basep->prettyName() + "'");
            return newLogicDType(flp, 1);
        }
    }

    // A protected type is an object with methods and state of its own, which
    // only those methods may reach (5.6). That is a class, so it becomes one:
    // the body's variables are its members, its subprograms are its methods,
    // and a shared variable of the type is an instance. The class is built
    // once per type, on first use.
    AstClass* protectedClass(VhdlType* basep, FileLine* flp) {
        const auto it = m_protClasses.find(basep);
        if (it != m_protClasses.end()) return it->second;
        AstClass* const classp = new AstClass{
            flp, "__Vprot_" + basep->name(), m_modp ? m_modp->libname() : ""};
        m_protClasses.emplace(basep, classp);
        v3Global.setHasClasses();
        // A class belongs to a design unit, and a protected type may be
        // declared in a package that several modules use, so it goes in the
        // one unit every design has
        m_netlistp->dollarUnitPkgAddp()->addStmtsp(classp);
        // Every class needs a constructor; nothing else makes one this early
        AstFunc* const newp = new AstFunc{flp, "new", nullptr, nullptr};
        newp->isConstructor(true);
        newp->classMethod(true);
        newp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        classp->addStmtsp(newp);
        AstVhdlProtectedType* const bodyp = VN_CAST(basep->bodyp(), VhdlProtectedType);
        if (!bodyp) {
            unsup(flp, "protected type '" + basep->prettyName() + "' with no body");
            return classp;
        }
        VL_RESTORER(m_protClassp);
        VL_RESTORER(m_ftaskp);
        VL_RESTORER(m_stmtsp);
        VL_RESTORER_COPY(m_prefix);
        m_protClassp = classp;
        m_ftaskp = nullptr;
        m_stmtsp = nullptr;
        m_prefix.clear();
        for (AstNode* p = bodyp->declsp(); p; p = p->nextp()) lowerDecl(p);
        return classp;
    }

    // The actuals of a call. An association may name its formal (6.5.7.1),
    // which V3Task matches to the parameter of that name, so a call written
    // out of order or leaving out a defaulted parameter comes out right.
    void addCallArgs(AstNodeFTaskRef* refp, AstVhdlCall* callp) {
        if (!callp) return;
        for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            if (!asp || !asp->actualp()) continue;
            const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
            refp->addArgsp(new AstArg{asp->fileline(), namep ? namep->name() : "",
                                      lowerExpr(asp->actualp())});
        }
    }

    // The object a method is called on, which the analyzer left on the prefix
    // of the selected name (5.6.2). Null if this is not such a call.
    AstNodeExpr* protectedInstance(AstNodeVhdlExpr* objNamep) {
        if (!objNamep) return nullptr;
        VhdlSymbol* const objp = objNamep->vhdlSymp();
        if (!objp || !objp->isObject() || !objp->typep()) return nullptr;
        if (objp->typep()->base()->kind() != VhdlTypeKind::PROTECTED) return nullptr;
        const auto it = m_vars.find(objp);
        if (it == m_vars.end()) return nullptr;
        return new AstVarRef{objNamep->fileline(), it->second, VAccess::READ};
    }

    // What one of a protected type's methods is called in the emitted code
    std::string methodName(VhdlSymbol* symp) {
        const auto it = m_funcs.find(symp);
        return it == m_funcs.end() ? uniqueName(symp) : it->second->name();
    }

    // A LINE is an access to STRING, which is how textio passes text about
    static bool isLineType(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        return basep->kind() == VhdlTypeKind::ACCESS && basep->designatedp()
               && isStringType(basep->designatedp()->base());
    }

    // Whether a type is STRING, or a subtype of it
    static bool isStringType(const VhdlType* typep) {
        const VhdlType* const strp = V3VhdlStd::stringType();
        return typep && strp && typep->base() == strp->base();
    }

    static AstNodeDType* newLogicDType(FileLine* flp, int bits) {
        if (bits <= 1) return new AstBasicDType{flp, VBasicDTypeKwd::LOGIC};
        return new AstBasicDType{flp, VFlagLogicPacked{}, bits};
    }

    // An array whose element packs becomes a packed array, so that a
    // std_logic_vector is one machine word rather than a vector of them
    AstNodeDType* lowerArrayType(VhdlType* typep, FileLine* flp) {
        VhdlType* const basep = typep->base();
        // A subtype may constrain the element as well as the index --
        // "t(1 to 3)(1 to 2)" -- so its own element is the one to emit
        VhdlType* const elemp = typep->elementp() ? typep->elementp() : basep->elementp();
        AstNodeDType* dtypep = elemp ? lowerType(elemp, flp) : newLogicDType(flp, 1);
        const bool packed = elemp && elementPacks(elemp);
        // Innermost dimension last, so the ranges nest the way VHDL reads
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        for (size_t i = indexes.size(); i-- > 0;) {
            const VhdlType* const idxp = indexes[i];
            AstRange* rangep = nullptr;
            if (!idxp || !idxp->bounded()) {
                // A bound that names a generic is fixed once the instance is
                // elaborated but is not a number now, so it goes to the back
                // end as an expression and V3Param folds it per instance --
                // which is what makes one entity with a WIDTH generic emit
                // one module per width, as VHDL elaboration says it should
                rangep = boundsRange(idxp, flp);
                if (!rangep) {
                    // Bounds that arrive with the value, or never
                    unsup(flp, "unconstrained array '" + typep->prettyName() + "'");
                    return dtypep;
                }
            } else {
                rangep = new AstRange{flp, static_cast<int>(idxp->left()),
                                      static_cast<int>(idxp->right())};
            }
            if (packed) {
                dtypep = new AstPackArrayDType{flp, VFlagChildDType{}, dtypep, rangep};
            } else {
                dtypep = new AstUnpackArrayDType{flp, VFlagChildDType{}, dtypep, rangep};
            }
        }
        return dtypep;
    }

    // The range an index subtype was written with, where its bounds did not
    // fold. Null if there is none to emit.
    AstRange* boundsRange(const VhdlType* idxp, FileLine* flp) {
        if (!idxp || !idxp->boundsp()) return nullptr;
        AstVhdlRange* const rp = VN_CAST(idxp->boundsp(), VhdlRange);
        if (!rp || !rp->leftp() || !rp->rightp()) return nullptr;
        AstNodeExpr* const leftp = lowerExpr(rp->leftp());
        AstNodeExpr* const rightp = leftp ? lowerExpr(rp->rightp()) : nullptr;
        if (!leftp || !rightp) {
            if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
            if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
            return nullptr;
        }
        return new AstRange{flp, leftp, rightp};
    }

    // The index subtype of one dimension, or of a scalar type itself
    static const VhdlType* indexSubtype(const VhdlType* typep, int dim = 0) {
        if (!typep) return nullptr;
        if (typep->base()->kind() != VhdlTypeKind::ARRAY) return typep;
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        if (dim < 0 || dim >= static_cast<int>(indexes.size())) return nullptr;
        return indexes[dim];
    }

    // The bounds of a range that did not fold, as expressions. False when
    // there is no such range, which means either it folded or there is none.
    bool dynamicBounds(const VhdlType* typep, int dim, FileLine* flp, AstNodeExpr*& leftp,
                       AstNodeExpr*& rightp, bool& ascending) {
        const VhdlType* const idxp = indexSubtype(typep, dim);
        if (!idxp || !idxp->boundsp()) return false;
        AstVhdlRange* const rp = VN_CAST(idxp->boundsp(), VhdlRange);
        if (!rp || !rp->leftp() || !rp->rightp()) return false;
        leftp = lowerExpr(rp->leftp());
        rightp = leftp ? lowerExpr(rp->rightp()) : nullptr;
        if (!leftp || !rightp) {
            if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
            if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
            return false;
        }
        ascending = rp->dir() == VhdlDir::TO;
        return true;
    }

    // How many elements such a range holds, as an expression
    AstNodeExpr* boundsLength(const VhdlType* typep, FileLine* flp) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return nullptr;
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        if (indexes.size() != 1) return nullptr;
        const VhdlType* const idxp = indexes[0];
        if (!idxp || !idxp->boundsp()) return nullptr;
        AstVhdlRange* const rp = VN_CAST(idxp->boundsp(), VhdlRange);
        if (!rp || !rp->leftp() || !rp->rightp()) return nullptr;
        AstNodeExpr* const leftp = lowerExpr(rp->leftp());
        AstNodeExpr* const rightp = leftp ? lowerExpr(rp->rightp()) : nullptr;
        if (!leftp || !rightp) {
            if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
            if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
            return nullptr;
        }
        // "left to right" holds right-left+1, "left downto right" left-right+1
        AstNodeExpr* const diffp = rp->dir() == VhdlDir::TO
                                       ? static_cast<AstNodeExpr*>(new AstSub{flp, rightp, leftp})
                                       : static_cast<AstNodeExpr*>(new AstSub{flp, leftp, rightp});
        return new AstAdd{flp, diffp, new AstConst{flp, AstConst::Signed32{}, 1}};
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
            // A physical value is counted in its primary unit, which for
            // TIME is the femtosecond -- far more than 32 bits holds
            return new AstConst{flp, AstConst::Unsized64{},
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
        // VHDL nests declarative regions that the emitted module does not, so
        // two loops in one process may both declare "i". The first keeps the
        // name it was written with, since that is what a trace shows.
        std::string name = m_prefix + symp->name();
        if (!m_names.insert(name).second) {
            std::string tryp;
            do {
                tryp = name + "__" + std::to_string(++m_nameNum);
            } while (!m_names.insert(tryp).second);
            name = tryp;
        }
        AstVar* const varp
            = new AstVar{flp, varType, name, VFlagChildDType{}, lowerType(symp->typep(), flp)};
        m_vars.emplace(symp, varp);
        return varp;
    }
    std::set<std::string> m_names;  // Names already used in this module
    int m_nameNum = 0;

    // What a signal attribute needs kept about a signal (16.2.2). VHDL's
    // model has a value history; a compiled model does not, so what the
    // attributes ask about is recorded as it happens, in a block that runs
    // whenever the signal changes.
    struct VhdlShadow final {
        AstVar* prevp = nullptr;  // Its value, one delta behind
        AstVar* lastp = nullptr;  // Its value before the previous event
        AstVar* timep = nullptr;  // When the previous event happened
        AstVar* transp = nullptr;  // Flips on every event
    };
    std::map<const VhdlSymbol*, VhdlShadow> m_shadows;
    AstNode* m_shadowsp = nullptr;  // The blocks that keep them, for the module

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
            case VhdlObjKind::FILE: varType = VVarType::VAR; break;
            }
            AstVar* const varp = newVar(symp, varType, flp);
            // A variable declared in a protected type body belongs to each
            // object of the type, so it is a member of the class (5.6.2)
            if (m_protClassp && !m_ftaskp) {
                varp->varType(VVarType::MEMBER);
                varp->funcLocal(false);
                varp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
            }
            // A VHDL process variable keeps its value between activations, so
            // it is a static variable of the module, not a local one
            if (objp->defaultp()) {
                varp->valuep(lowerExpr(objp->defaultp()));
            } else {
                varp->valuep(defaultValueFor(symp->typep(), flp));
            }
            addStmt(varp);
            if (objp->kind() == VhdlObjKind::SIGNAL && isResolvedSignal(symp)) {
                if (isLogicish(symp->typep())) {
                    makeResolved(symp, varp, flp);
                } else {
                    // Any other resolved type would need its own resolution
                    // function run over the drivers, which is not here yet
                    makeResolvedByFunction(symp, varp, flp);
                }
            }
            return;
        }
        if (AstVhdlConfigSpec* const cfgp = VN_CAST(nodep, VhdlConfigSpec)) {
            recordBinding(cfgp->specp(), cfgp->bindingp());
            return;
        }
        if (VN_IS(nodep, VhdlTypeDecl) || VN_IS(nodep, VhdlSubtypeDecl)
            || VN_IS(nodep, VhdlAttrDecl) || VN_IS(nodep, VhdlAttrSpec)
            || VN_IS(nodep, VhdlComponentDecl) || VN_IS(nodep, VhdlContextItem)
            || VN_IS(nodep, VhdlDisconnectSpec) || VN_IS(nodep, VhdlGroupDecl)
            || VN_IS(nodep, VhdlGroupTemplate)) {
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
        switch (basep->kind()) {
        case VhdlTypeKind::PROTECTED:
            // A shared variable holds an object of the type, made once, at
            // elaboration (5.6.1)
            return new AstNew{flp};
        case VhdlTypeKind::ENUM: {
            if (!typep->bounded()) return nullptr;
            const std::vector<VhdlSymbol*>& lits = basep->literals();
            const int64_t left = typep->left();
            if (left >= 0 && left < static_cast<int64_t>(lits.size())) {
                return lowerValue(VhdlValue::mkInt(left, typep), typep, flp);
            }
            return nullptr;
        }
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::PHYSICAL:
            // INTEGER'LEFT, not zero: a variable of a subtype that excludes
            // zero would otherwise start outside its own subtype
            if (!typep->bounded()) return nullptr;
            return lowerValue(VhdlValue::mkInt(typep->left(), typep), typep, flp);
        case VhdlTypeKind::FLOATING:
            if (!typep->bounded()) return nullptr;
            return lowerValue(VhdlValue::mkReal(typep->leftReal(), typep), typep, flp);
        case VhdlTypeKind::ARRAY: {
            // A length that is not known until elaboration has no default to
            // write here; the variable simply starts at zero
            if (!typep->isConstrainedArray()) return nullptr;
            // Every element takes the element type's default (6.4.2.2). An
            // array of logic is packed and starts as Verilator starts it; an
            // array of anything else is unpacked, and a sparse initializer
            // with only a default says "all elements" without writing them
            // out, whatever the array's size.
            if (!typep->isConstrainedArray()) return nullptr;
            if (!basep->elementp()) return nullptr;
            // One dimension only: a sparse initializer for a nested array
            // would have to be nested too, and say nothing a design is likely
            // to depend on
            if (typep->indexTypes().size() != 1) return nullptr;
            if (basep->elementp()->base()->kind() == VhdlTypeKind::ARRAY) return nullptr;
            if (enumIsLogic(basep->elementp()->base())) return nullptr;
            if (isStringType(basep)) return nullptr;  // A string, not an array here
            AstNodeExpr* const elemp = defaultValueFor(basep->elementp(), flp);
            if (!elemp) return nullptr;
            AstNodeDType* const dtypep = lowerType(typep, flp);
            if (!VN_IS(dtypep, UnpackArrayDType)) {
                VL_DO_DANGLING(elemp->deleteTree(), elemp);
                VL_DO_DANGLING(dtypep->deleteTree(), dtypep);
                return nullptr;
            }
            return new AstInitArray{flp, dtypep, elemp};
        }
        case VhdlTypeKind::RECORD: {
            // A record's default is each element's default (6.4.2.2). The
            // record is packed, so the elements concatenate into one constant
            // in the order they were declared.
            AstNodeExpr* outp = nullptr;
            for (VhdlSymbol* const elemp : basep->elements()) {
                AstNodeExpr* const onep = defaultValueFor(elemp->typep(), flp);
                if (!onep) {
                    if (outp) VL_DO_DANGLING(outp->deleteTree(), outp);
                    return nullptr;  // One element with no default gives none
                }
                outp = outp ? new AstConcat{flp, outp, onep} : onep;
            }
            return outp;
        }
        default: return nullptr;  // Handled by the element type, where at all
        }
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
        if (val.known()) {
            // A name of an array constant keeps its storage, so that its
            // index range travels with it and an index reads the right end
            const bool keepRange = (VN_IS(nodep, VhdlName) || VN_IS(nodep, VhdlSelected))
                                   && foldsToBareNumber(nodep->vhdlTypep());
            if (!keepRange) return lowerValue(val, nodep->vhdlTypep(), flp);
        }

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
        // The signal attributes ask about what has happened to a signal, not
        // about its type (16.2.2)
        if (attr == "event" || attr == "active" || attr == "last_value"
            || attr == "last_event" || attr == "last_active" || attr == "stable"
            || attr == "quiet" || attr == "transaction") {
            if (AstNodeExpr* const outp = lowerSignalAttr(nodep, attr, argp, flp)) return outp;
        }
        // A bound that names a generic does not fold during analysis, so the
        // attributes that ask about it are answered with the same expression
        // the range itself was emitted with (16.2)
        if (attr == "left" || attr == "right" || attr == "high" || attr == "low"
            || attr == "length" || attr == "ascending") {
            const VhdlType* const typep = nodep->prefixp()->vhdlTypep();
            int dim = 0;
            if (argp) {
                if (const AstConst* const constp = VN_CAST(argp, Const)) {
                    dim = static_cast<int>(constp->toSInt()) - 1;
                }
            }
            AstNodeExpr* leftp = nullptr;
            AstNodeExpr* rightp = nullptr;
            bool ascending = true;
            if (dynamicBounds(typep, dim, flp, leftp, rightp, ascending)) {
                if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
                if (attr == "ascending") {
                    VL_DO_DANGLING(leftp->deleteTree(), leftp);
                    VL_DO_DANGLING(rightp->deleteTree(), rightp);
                    return new AstConst{flp, AstConst::BitTrue{}, ascending};
                }
                if (attr == "length") {
                    AstNodeExpr* const diffp
                        = ascending ? static_cast<AstNodeExpr*>(new AstSub{flp, rightp, leftp})
                                    : static_cast<AstNodeExpr*>(new AstSub{flp, leftp, rightp});
                    return new AstAdd{flp, diffp, new AstConst{flp, AstConst::Signed32{}, 1}};
                }
                const bool wantLeft = (attr == "left") || (attr == "high" && !ascending)
                                      || (attr == "low" && ascending);
                AstNodeExpr* const keepp = wantLeft ? leftp : rightp;
                AstNodeExpr* const dropp = wantLeft ? rightp : leftp;
                VL_DO_DANGLING(dropp->deleteTree(), dropp);
                return keepp;
            }
        }
        if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
        unsup(flp, "attribute '" + attr);
        return nullptr;
    }

    // What a signal attribute needs kept about one signal, made on first use.
    // One block, sensitive to the signal, records what VHDL's value history
    // would have held: the value one delta back, the value before that, when
    // the last event was, and a bit that flips on each one. The writes are
    // non-blocking, so a process reading them in the same delta sees what was
    // true before the event, which is what the attributes are defined against.
    const VhdlShadow* shadowFor(AstNodeVhdlExpr* prefixp, FileLine* flp) {
        VhdlSymbol* const symp = prefixp->vhdlSymp();
        if (!symp || symp->kind() != VhdlSymKind::SIGNAL) {
            if (!symp || symp->kind() != VhdlSymKind::PORT) return nullptr;
        }
        const auto found = m_shadows.find(symp);
        if (found != m_shadows.end()) return &found->second;
        const auto varIt = m_vars.find(symp);
        if (varIt == m_vars.end()) return nullptr;
        AstVar* const sigp = varIt->second;
        VhdlShadow sh;
        const std::string base = "__Vsig" + std::to_string(++m_nameNum) + "__" + symp->name();
        AstNodeDType* const valDTypep = lowerType(symp->typep(), flp);
        sh.prevp = newTempVar(base + "__prev", valDTypep, flp);
        sh.lastp = newTempVar(base + "__last", lowerType(symp->typep(), flp), flp);
        sh.timep = newTempVar(base + "__time",
                              new AstBasicDType{flp, VBasicDTypeKwd::LOGIC, VSigning::SIGNED,
                                                VNumRange{63, 0}, -1},
                              flp);
        sh.transp = newTempVar(base + "__trans", newLogicDType(flp, 1), flp);
        // A signal starts out equal to its own past, so nothing reads as an
        // event before anything has happened
        sh.prevp->valuep(defaultValueFor(symp->typep(), flp));
        sh.lastp->valuep(defaultValueFor(symp->typep(), flp));
        AstNode* stmtsp = new AstAssignDly{
            flp, new AstVarRef{flp, sh.lastp, VAccess::WRITE},
            new AstVarRef{flp, sh.prevp, VAccess::READ}};
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(new AstAssignDly{
                        flp, new AstVarRef{flp, sh.prevp, VAccess::WRITE},
                        new AstVarRef{flp, sigp, VAccess::READ}}));
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(new AstAssignDly{
                        flp, new AstVarRef{flp, sh.timep, VAccess::WRITE},
                        new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}}}));
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(new AstAssignDly{
                        flp, new AstVarRef{flp, sh.transp, VAccess::WRITE},
                        new AstNot{flp, new AstVarRef{flp, sh.transp, VAccess::READ}}}));
        AstSenTree* const senp = new AstSenTree{
            flp, new AstSenItem{flp, VEdgeType::ET_BOTHEDGE,
                                new AstVarRef{flp, sigp, VAccess::READ}}};
        AstAlways* const alwaysp = new AstAlways{flp, VAlwaysKwd::ALWAYS, senp, stmtsp};
        m_shadowsp = AstNode::addNextNull(m_shadowsp, static_cast<AstNode*>(alwaysp));
        return &m_shadows.emplace(symp, sh).first->second;
    }

    // "s /= s'past value", which is true for exactly the delta in which the
    // signal changed
    AstNodeExpr* eventOf(AstNodeVhdlExpr* prefixp, const VhdlShadow* shp, FileLine* flp) {
        AstNodeExpr* const nowp = lowerExpr(prefixp);
        if (!nowp) return nullptr;
        return new AstNeq{flp, nowp, new AstVarRef{flp, shp->prevp, VAccess::READ}};
    }

    // The signal attributes of 16.2.2, answered from what shadowFor keeps
    AstNodeExpr* lowerSignalAttr(AstVhdlAttrName* nodep, const std::string& attr,
                                 AstNodeExpr* argp, FileLine* flp) {
        const VhdlShadow* const shp = shadowFor(nodep->prefixp(), flp);
        if (!shp) return nullptr;
        if (attr == "transaction") return new AstVarRef{flp, shp->transp, VAccess::READ};
        if (attr == "last_value") {
            // During the delta in which it changed, the value before the
            // change is what prev still holds
            AstNodeExpr* const evp = eventOf(nodep->prefixp(), shp, flp);
            if (!evp) return nullptr;
            return new AstCond{flp, evp, new AstVarRef{flp, shp->prevp, VAccess::READ},
                               new AstVarRef{flp, shp->lastp, VAccess::READ}};
        }
        if (attr == "event" || attr == "active") {
            return eventOf(nodep->prefixp(), shp, flp);
        }
        if (attr == "last_event" || attr == "last_active") {
            AstNodeExpr* const evp = eventOf(nodep->prefixp(), shp, flp);
            if (!evp) return nullptr;
            AstNodeExpr* const sincep = new AstSub{
                flp, new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}},
                new AstVarRef{flp, shp->timep, VAccess::READ}};
            return new AstCond{flp, evp, new AstConst{flp, AstConst::Unsized64{}, 0}, sincep};
        }
        if (attr == "stable" || attr == "quiet") {
            AstNodeExpr* const evp = eventOf(nodep->prefixp(), shp, flp);
            if (!evp) return nullptr;
            AstNodeExpr* const steadyp = new AstLogNot{flp, evp};
            if (!argp) return steadyp;
            // "stable(t)" also asks that the last event was longer ago than t
            AstNodeExpr* const sincep = new AstSub{
                flp, new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}},
                new AstVarRef{flp, shp->timep, VAccess::READ}};
            return new AstLogAnd{flp, steadyp, new AstGte{flp, sincep, argp}};
        }
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
        if (symp->hasValue()) {
            // A folded array constant is a plain number, and a number has no
            // index range, so indexing or slicing it would count from the
            // wrong end -- VHDL's ranges are the array's, not Verilog's. Such
            // a constant keeps its storage, so that its range travels with it.
            if (symp->typep() && symp->typep()->base()->kind() == VhdlTypeKind::ARRAY
                && !isStringType(symp->typep())) {
                return new AstVarRef{flp, constVarFor(symp, flp), VAccess::READ};
            }
            return lowerValue(symp->value(), symp->typep(), flp);
        }
        // A function of no arguments is called by naming it, with no
        // parentheses, so a name may be a call (4.2.2.1)
        if (symp->kind() == VhdlSymKind::FUNCTION && symp->paramTypes().empty()) {
            if (symp->name() == "now") {
                return new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}};
            }
            auto funcIt = m_funcs.find(symp);
            if (funcIt == m_funcs.end() && emitOnDemand(symp)) funcIt = m_funcs.find(symp);
            if (funcIt != m_funcs.end() && VN_IS(funcIt->second, Func)) {
                AstFuncRef* const refp = new AstFuncRef{flp, funcIt->second->name(), nullptr};
                refp->taskp(funcIt->second);
                return refp;
            }
        }
        // OUTPUT and INPUT of std.textio are the standard streams (16.4)
        if (symp->kind() == VhdlSymKind::FILE && symp->predefined()) {
            if (symp->name() == "output") return new AstConst{flp, AstConst::Signed32{}, 1};
            if (symp->name() == "input") return new AstConst{flp, AstConst::Signed32{}, 0};
        }
        unsup(flp, std::string{symp->kindName()} + " '" + symp->name() + "'");
        return nullptr;
    }

    // Whether folding a value of this type to a plain number would lose
    // something. A number has no index range, and Verilog counts its bits
    // from the right; an array VHDL wrote as "0 to 3" counts from the left,
    // so indexing the folded number would read the wrong end of it.
    static bool foldsToBareNumber(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return false;
        if (isStringType(typep)) return false;
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        if (indexes.size() != 1 || !indexes[0]) return false;
        return indexes[0]->ascending() && indexes[0]->length() > 1;
    }

    // The variable a folded array constant is kept in, made on first use and
    // belonging to the module rather than to whatever is being built now
    AstVar* constVarFor(VhdlSymbol* symp, FileLine* flp) {
        AstVar* const varp = newVar(symp, VVarType::VAR, flp);
        varp->valuep(lowerValue(symp->value(), symp->typep(), flp));
        if (m_protClassp) {
            m_protClassp->addStmtsp(varp);
        } else {
            m_stmtsp = AstNode::addNextNull(m_stmtsp, varp);
        }
        return varp;
    }

    AstNodeExpr* lowerSelected(AstVhdlSelected* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (symp && symp->kind() == VhdlSymKind::RECORD_ELEMENT) {
            AstNodeExpr* const fromp = lowerExpr(nodep->prefixp());
            if (!fromp) return nullptr;
            return new AstMemberSel{flp, fromp, VFlagChildDType{}, symp->name()};
        }
        if (symp && symp->kind() == VhdlSymKind::FUNCTION) {
            if (AstNodeExpr* const instp = protectedInstance(nodep->prefixp())) {
                return new AstMethodCall{flp, instp, methodName(symp), nullptr};
            }
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

    void matchVectorWidth(AstVhdlBinary* nodep, const VhdlType* ltypep,
                          const VhdlType* rtypep, AstNodeExpr*& lhsp, AstNodeExpr*& rhsp,
                          FileLine* flp) {
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
        } else if (!lw && !rw) {
            // A vector whose width comes from a generic has no width here,
            // but its length is an expression, and V3Param folds it before
            // widths are worked out
            if (isIntegerType(rtypep) && isDynamicVector(ltypep)) {
                rhsp = new AstCast{flp, rhsp, VFlagChildDType{},
                                   lowerType(const_cast<VhdlType*>(ltypep), flp)};
            } else if (isIntegerType(ltypep) && isDynamicVector(rtypep)) {
                lhsp = new AstCast{flp, lhsp, VFlagChildDType{},
                                   lowerType(const_cast<VhdlType*>(rtypep), flp)};
            }
        }
    }
    // A vector whose length is not a number now, but is once the instance is
    // elaborated
    static bool isDynamicVector(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return false;
        if (typep->isConstrainedArray()) return false;
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        return indexes.size() == 1 && indexes[0] && indexes[0]->boundsp();
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
        // An object prefix makes it an index or a slice (8.4, 8.5). The
        // prefix need not be a name of its own: "r.v(3)" indexes a record
        // element, and "p.all(3)" what an access value points at.
        VhdlSymbol* const objSymp = nodep->vhdlSymp();
        const VhdlType* const prefixTypep
            = nodep->prefixp() ? nodep->prefixp()->vhdlTypep() : nullptr;
        const bool indexing
            = (objSymp && objSymp->isObject())
              || (!objSymp && prefixTypep
                  && prefixTypep->base()->kind() == VhdlTypeKind::ARRAY);
        if (indexing) {
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

    // A subprogram declared in a package is not emitted with the package --
    // a package holds no state of its own here -- so the first call to one
    // emits it into the module that calls it, and later calls find it. The
    // same path serves a subprogram declared in an enclosing region that has
    // not been reached yet.
    AstNodeFTask* emitOnDemand(VhdlSymbol* symp) {
        if (!symp) return nullptr;
        AstVhdlSubprogDecl* declp = symp->bodyp();
        if (!declp) declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
        if (!declp) return nullptr;
        if (m_emitting.count(symp)) return nullptr;  // A recursive call, mid-emission
        m_emitting.insert(symp);
        {
            VL_RESTORER(m_ftaskp);
            VL_RESTORER_COPY(m_prefix);
            m_ftaskp = nullptr;
            m_prefix.clear();
            lowerSubprogram(declp);
        }
        m_emitting.erase(symp);
        auto it = m_funcs.find(symp);
        if (it != m_funcs.end()) return it->second;
        // The body may carry a symbol of its own; the call's symbol is what
        // later lookups use, so record it under that too
        VhdlSymbol* const bodySymp = declp->vhdlSymp();
        if (bodySymp && bodySymp != symp) {
            it = m_funcs.find(bodySymp);
            if (it != m_funcs.end()) {
                m_funcs.emplace(symp, it->second);
                return it->second;
            }
        }
        return nullptr;
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
        // A function of a protected type runs on the object it is called on
        if (AstVhdlSelected* const selp = VN_CAST(nodep->prefixp(), VhdlSelected)) {
            if (AstNodeExpr* const instp = protectedInstance(selp->prefixp())) {
                AstMethodCall* const callp
                    = new AstMethodCall{flp, instp, methodName(symp), nullptr};
                addCallArgs(callp, nodep);
                return callp;
            }
        }
        auto it = m_funcs.find(symp);
        if (it == m_funcs.end() && emitOnDemand(symp)) it = m_funcs.find(symp);
        if (it != m_funcs.end() && VN_IS(it->second, Func)) {
            AstFuncRef* const refp = new AstFuncRef{flp, it->second->name(), nullptr};
            refp->taskp(it->second);
            addCallArgs(refp, nodep);
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
        // NOW is the current simulation time, which VHDL counts in the
        // primary unit of TIME (16.3) and Verilator in the design's precision
        if (name == "now" && args.empty()) {
            return new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}};
        }
        if (args.empty()) return nullptr;
        AstNodeExpr* const firstp = lowerExpr(args[0]);
        if (!firstp) return nullptr;

        if (name == "to_integer") {
            // The operand's bits, read with its own signedness ...
            AstNodeExpr* const valuep
                = isSignedType(args[0]->vhdlTypep())
                      ? static_cast<AstNodeExpr*>(new AstSigned{flp, firstp})
                      : static_cast<AstNodeExpr*>(new AstUnsigned{flp, firstp});
            // ... widened to INTEGER, which is what the function returns.
            // Leaving it at the operand's width instead would make an
            // ordinary "acc + to_integer(v)" add two different widths.
            AstNodeExpr* const widep = new AstCastSize{
                flp, valuep, new AstConst{flp, AstConst::Unsized32{}, VL_IDATASIZE}};
            return new AstSigned{flp, widep};
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
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) {
            unsup(flp, "aggregate of this type");
            return nullptr;
        }
        // An aggregate written entirely positionally, of an element type that
        // packs, is a concatenation: each value contributes its own width, so
        // an element that is itself an array of the aggregate's type spans as
        // many indices as it is long, which VHDL-2008 allows (9.3.3.3)
        if (typep->base()->elementp() && elementPacks(typep->base()->elementp())
            && typep->dimensions() == 1 && allPositional(nodep)) {
            AstNodeExpr* outp = nullptr;
            for (AstVhdlElemAssoc* p = nodep->elemsp(); p;
                 p = VN_AS(p->nextp(), VhdlElemAssoc)) {
                AstNodeExpr* const elemp = lowerExpr(p->valuep());
                if (!elemp) {
                    if (outp) VL_DO_DANGLING(outp->deleteTree(), outp);
                    return nullptr;
                }
                outp = outp ? new AstConcat{flp, outp, elemp} : elemp;
            }
            if (outp) return outp;
        }
        // Where the context gives no constraint -- an aggregate standing as
        // an operand, or returned from a function whose result is
        // unconstrained -- the aggregate's own elements fix its index range
        // (9.3.3.3)
        int64_t left = 0;
        int64_t step = 1;
        int64_t count = 0;
        if (typep->isConstrainedArray()) {
            const VhdlType* const indexp = typep->indexTypes()[0];
            left = indexp->left();
            step = indexp->ascending() ? 1 : -1;
            count = indexp->length();
        } else if (AstNodeExpr* const outp = othersOfDynamicLength(nodep, typep, flp)) {
            return outp;
        } else if (!ownConstraint(nodep, typep, left, step, count)) {
            unsup(flp, "aggregate of this type");
            return nullptr;
        }
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
        // An array of packed elements is a vector, and its aggregate a
        // concatenation with the leftmost element most significant. An array
        // of anything else is unpacked, and its aggregate an assignment
        // pattern, which is the only form that can carry element values.
        const bool packed = typep->base()->elementp()
                            && elementPacks(typep->base()->elementp());
        AstNodeExpr* outp = nullptr;
        AstNode* itemsp = nullptr;
        for (int64_t i = 0; i < count; ++i) {
            AstNodeVhdlExpr* const valuep
                = byIndex[static_cast<size_t>(i)] ? byIndex[static_cast<size_t>(i)] : othersp;
            if (!valuep) {
                unsup(flp, "aggregate with an element left unset");
                return nullptr;
            }
            AstNodeExpr* const elemp = lowerExpr(valuep);
            if (!elemp) return nullptr;
            if (packed) {
                outp = outp ? new AstConcat{flp, outp, elemp} : elemp;
            } else {
                itemsp = AstNode::addNextNull(
                    itemsp, new AstPatMember{flp, elemp, nullptr, nullptr});
            }
        }
        if (packed) return outp;
        return itemsp ? new AstPattern{flp, itemsp} : nullptr;
    }

    // Whether every association is positional, so the values follow the
    // aggregate's index range in order
    static bool allPositional(AstVhdlAggregate* nodep) {
        if (!nodep->elemsp()) return false;
        for (AstVhdlElemAssoc* p = nodep->elemsp(); p; p = VN_AS(p->nextp(), VhdlElemAssoc)) {
            if (p->choicesp()) return false;
        }
        return true;
    }

    // "(others => v)" where the length is not a number now but is fixed once
    // the instance is elaborated -- a port or signal whose width comes from a
    // generic. One value repeated as many times as the range is long, which
    // V3Param settles along with the range itself.
    AstNodeExpr* othersOfDynamicLength(AstVhdlAggregate* nodep, VhdlType* typep, FileLine* flp) {
        AstVhdlElemAssoc* const eap = nodep->elemsp();
        if (!eap || eap->nextp() || !eap->choicesp()) return nullptr;
        AstVhdlChoice* const chp = eap->choicesp();
        if (chp->nextp() || !chp->isOthers()) return nullptr;
        if (!typep->base()->elementp() || !elementPacks(typep->base()->elementp())) return nullptr;
        AstNodeExpr* const countp = boundsLength(typep, flp);
        if (!countp) return nullptr;
        AstNodeExpr* const valuep = lowerExpr(eap->valuep());
        if (!valuep) {
            VL_DO_DANGLING(countp->deleteTree(), countp);
            return nullptr;
        }
        return new AstReplicate{flp, valuep, countp};
    }

    // The index range an aggregate gives itself when nothing else does. A
    // positional aggregate starts at its index subtype's left; a named one
    // spans its choices (9.3.3.3). An "others" needs a constraint from
    // somewhere, so there is nothing to work out.
    // How many elements one positional association supplies: usually one, but
    // VHDL-2008 lets a value of the aggregate's own type stand for a whole
    // slice of it (9.3.3.3)
    static int64_t elementSpan(AstNodeVhdlExpr* valuep, const VhdlType* arrayTypep) {
        if (!valuep || !arrayTypep) return 1;
        const VhdlType* const typep = valuep->vhdlTypep();
        if (!typep || typep->base() != arrayTypep->base()) return 1;
        if (!typep->isConstrainedArray() || typep->dimensions() != 1) return -1;
        return typep->dimLength(0);
    }

    bool ownConstraint(AstVhdlAggregate* nodep, VhdlType* typep, int64_t& left, int64_t& step,
                       int64_t& count) {
        if (typep->dimensions() != 1) return false;
        const VhdlType* const indexp = typep->base()->indexTypes().empty()
                                           ? nullptr
                                           : typep->base()->indexTypes()[0];
        if (!indexp || !indexp->bounded()) return false;
        int64_t positional = 0;
        bool named = false;
        int64_t lowest = INT64_MAX;
        int64_t highest = INT64_MIN;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(p, VhdlElemAssoc);
            if (!eap->choicesp()) {
                const int64_t span = elementSpan(eap->valuep(), typep);
                if (span < 0) return false;
                positional += span;
                continue;
            }
            for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                if (chp->isOthers()) return false;
                named = true;
                int64_t lo = 0;
                int64_t hi = 0;
                if (AstVhdlRange* const rp = chp->rangep()) {
                    const VhdlValue a = V3VhdlEval::eval(rp->leftp());
                    const VhdlValue b = V3VhdlEval::eval(rp->rightp());
                    if (!a.isInt() || !b.isInt()) return false;
                    lo = std::min(a.toInt(), b.toInt());
                    hi = std::max(a.toInt(), b.toInt());
                } else {
                    const VhdlValue v = V3VhdlEval::eval(chp->valuep());
                    if (!v.isInt()) return false;
                    lo = hi = v.toInt();
                }
                lowest = std::min(lowest, lo);
                highest = std::max(highest, hi);
            }
        }
        step = indexp->ascending() ? 1 : -1;
        if (named) {
            if (lowest > highest) return false;
            left = step > 0 ? lowest : highest;
            count = highest - lowest + 1;
        } else {
            if (positional <= 0) return false;
            left = indexp->left();
            count = positional;
        }
        return count > 0 && count <= 65536;
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

    // Each concurrent statement is one driver of whatever it assigns, so the
    // index of the driver a signal is being written from is known from where
    // the lowering is, not from the statement itself
    void lowerConcurrentDriver(AstNodeVhdlStmt* nodep) {
        std::set<const VhdlSymbol*> driven;
        collectDriven(nodep, driven);
        std::vector<const VhdlSymbol*> resolved;
        for (const VhdlSymbol* const symp : driven) {
            if (m_drivers.count(symp)) resolved.push_back(symp);
        }
        lowerConcurrent(nodep);
        for (const VhdlSymbol* const symp : resolved) ++m_driverUsed[symp];
    }

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
            // A block is a declarative region, not a construct of its own, so
            // its declarations and statements belong to the module (11.2).
            // A guard, or a header giving the block its own generics and
            // ports, makes it more than that.
            if (blockp->guardp()) unsup(flp, "guarded block");
            if (AstVhdlBlockHeader* const headerp = blockp->headerp()) {
                lowerBlockHeader(headerp);
            }
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
                flp, VAlwaysKwd::ALWAYS, new AstSenTree{flp, comboSenItem(flp)},
                bodyp});
            return;
        }
        if (AstVhdlProcCall* const callp = VN_CAST(nodep, VhdlProcCall)) {
            AstNode* const bodyp = lowerStmt(callp);
            addStmt(new AstAlways{
                flp, VAlwaysKwd::ALWAYS, new AstSenTree{flp, comboSenItem(flp)},
                bodyp});
            return;
        }
        unsup(flp, std::string{"concurrent statement "} + nodep->typeName());
    }

    // A block may give itself generics and ports (11.2). It is not a module
    // of its own -- it is elaborated where it stands -- so each generic
    // becomes a constant with the value the map supplies, and each port a
    // signal wired to its actual.
    void lowerBlockHeader(AstVhdlBlockHeader* nodep) {
        lowerBlockInterface(nodep->genericsp(), nodep->genericMapp(), true);
        lowerBlockInterface(nodep->portsp(), nodep->portMapp(), false);
    }
    void lowerBlockInterface(AstNode* formalsp, AstNode* mapp, bool isGeneric) {
        // The actual for each formal, by position or by name
        std::vector<AstVhdlAssoc*> assocs;
        for (AstNode* p = mapp; p; p = p->nextp()) assocs.push_back(VN_AS(p, VhdlAssoc));
        size_t position = 0;
        for (AstNode* p = formalsp; p; p = p->nextp(), ++position) {
            AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            VhdlSymbol* const symp = ifp->vhdlSymp();
            if (!symp) continue;
            FileLine* const flp = ifp->fileline();
            AstVar* const varp = newVar(symp, VVarType::VAR, flp);
            addStmt(varp);
            AstNodeVhdlExpr* actualp = nullptr;
            for (size_t i = 0; i < assocs.size(); ++i) {
                const AstVhdlName* const namep = VN_CAST(assocs[i]->formalp(), VhdlName);
                if (namep ? namep->name() == symp->name() : i == position) {
                    actualp = assocs[i]->actualp();
                    break;
                }
            }
            if (!actualp) {
                if (ifp->defaultp()) varp->valuep(lowerExpr(ifp->defaultp()));
                continue;
            }
            AstNodeExpr* const valuep = lowerExpr(actualp);
            if (!valuep) continue;
            if (isGeneric) {
                varp->valuep(valuep);
                continue;
            }
            // An "out" port drives its actual; anything else is driven by it
            AstNodeExpr* const portRefp = new AstVarRef{flp, varp, VAccess::READ};
            if (ifp->mode() == VhdlMode::OUT || ifp->mode() == VhdlMode::BUFFER) {
                addStmt(new AstAlways{new AstAssignW{flp, valuep, portRefp}});
            } else {
                addStmt(new AstAlways{new AstAssignW{flp, portRefp, valuep}});
            }
        }
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

    // Sensitive to everything the block reads, which is what a process with
    // no list -- or with "all" -- means (11.3). The STAR form is the one the
    // rest of Verilator expects to see written out rather than inferred.
    static AstSenItem* comboSenItem(FileLine* flp) {
        return new AstSenItem{flp, VEdgeType::ET_COMBO_STAR, nullptr};
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
            return new AstSenTree{flp, comboSenItem(flp)};
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
        if (!itemsp) return new AstSenTree{flp, comboSenItem(flp)};
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
    // The signal a name ultimately assigns, through slices and selections
    static const VhdlSymbol* rootSymOf(const AstNode* nodep) {
        while (nodep) {
            if (const AstNodeVhdlExpr* const vp = VN_CAST(nodep, NodeVhdlExpr)) {
                if (vp->vhdlSymp()) return vp->vhdlSymp();
            }
            if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
                nodep = callp->prefixp();  // An index or a slice
            } else if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
                nodep = selp->prefixp();
            } else {
                return nullptr;
            }
        }
        return nullptr;
    }

    // Every signal one concurrent statement assigns. A process counts once
    // however many assignments it holds, because a process is one driver.
    static void collectDriven(AstNode* nodep, std::set<const VhdlSymbol*>& out,
                              bool follow = false) {
        if (!nodep) return;
        if (const AstVhdlSigAssign* const asgp = VN_CAST(nodep, VhdlSigAssign)) {
            if (const VhdlSymbol* const symp = rootSymOf(asgp->targetp())) {
                // A driver belongs to the longest static prefix of the name
                // (14.7.2), so assigning one element of an array signal from
                // here and another from there is not two drivers of the
                // signal. Only a whole-signal assignment is counted, and a
                // partial one takes the signal out of resolution altogether.
                if (VN_IS(asgp->targetp(), VhdlName)) {
                    out.insert(symp);
                } else {
                    s_partialDriven.insert(symp);
                }
            }
        }
        collectDriven(nodep->op1p(), out, true);
        collectDriven(nodep->op2p(), out, true);
        collectDriven(nodep->op3p(), out, true);
        collectDriven(nodep->op4p(), out, true);
        // Only within a statement: the statements beside this one are other
        // drivers, and counting them here would make every one of them count
        // the ones after it as well
        if (follow) collectDriven(nodep->nextp(), out, true);
    }

    void countDrivers(AstNode* stmtsp) {
        for (AstNode* p = stmtsp; p; p = p->nextp()) {
            std::set<const VhdlSymbol*> driven;
            collectDriven(p, driven);
            // A block is not a driver of its own: its statements are
            if (const AstVhdlBlock* const blockp = VN_CAST(p, VhdlBlock)) {
                countDrivers(blockp->stmtsp());
                continue;
            }
            for (const VhdlSymbol* const symp : driven) ++m_driverCount[symp];
        }
    }

    // Whether this signal needs resolving, and can be
    // Analysis counted the drivers and worked out what resolves them
    static bool isResolvedSignal(const VhdlSymbol* symp) { return symp->driverCount() > 1; }
    // Signals some driver assigns only a part of. Static because the walk
    // that fills it is, and cleared with the rest of the per-entity state.
    static std::set<const VhdlSymbol*> s_partialDriven;

    // The wiring for a signal with several drivers: one value and one enable
    // per driver, and the signal itself a net each of them drives.
    void makeResolved(VhdlSymbol* symp, AstVar* netp, FileLine* flp) {
        const int count = symp->driverCount();
        netp->varType(VVarType::TRIWIRE);
        // What the signal starts at is decided by its drivers, not by the
        // declaration: a net takes its value from what drives it
        if (netp->valuep()) {
            AstNode* const oldp = netp->valuep();
            oldp->unlinkFrBack();
            VL_DO_DANGLING(oldp->deleteTree(), oldp);
        }
        std::vector<VhdlDriver> drivers;
        for (int i = 0; i < count; ++i) {
            VhdlDriver drv;
            const std::string base
                = "__Vdrv" + std::to_string(i) + "__" + m_prefix + symp->name();
            drv.valp = new AstVar{flp, VVarType::VAR, base + "__val", VFlagChildDType{},
                                  lowerType(symp->typep(), flp)};
            // One bit, whatever the signal's width: a driver of a resolved
            // signal is on or off as a whole in every form real code uses,
            // and a per-bit enable cannot be the condition of a select
            drv.enp = new AstVar{flp, VVarType::VAR, base + "__en", VFlagChildDType{},
                                 new AstBasicDType{flp, VBasicDTypeKwd::LOGIC}};
            addStmt(drv.valp);
            addStmt(drv.enp);
            // Off until the driver says otherwise, which is what a driver of
            // a resolved signal contributes before it has run
            drv.enp->valuep(new AstConst{flp, AstConst::WidthedValue{}, 1, 0});
            drv.weakp = new AstVar{flp, VVarType::VAR, base + "__weak", VFlagChildDType{},
                                   new AstBasicDType{flp, VBasicDTypeKwd::LOGIC}};
            addStmt(drv.weakp);
            drv.weakp->valuep(new AstConst{flp, AstConst::WidthedValue{}, 1, 0});
            drivers.push_back(drv);
        }
        // "Some driver is forcing", which is what decides whether the weak
        // ones are heard at all
        AstNodeExpr* forcingp = nullptr;
        for (const VhdlDriver& drv : drivers) {
            AstNodeExpr* const onep
                = new AstAnd{flp, new AstVarRef{flp, drv.enp, VAccess::READ},
                             new AstNot{flp, new AstVarRef{flp, drv.weakp, VAccess::READ}}};
            forcingp = forcingp ? new AstOr{flp, forcingp, onep} : onep;
        }
        for (const VhdlDriver& drv : drivers) {
            // A forcing driver is always heard; a weak one only while nothing
            // forces. That is the strength ordering of the resolution table
            // (16.8.2): a forcing '0' beats a weak 'H', and two weak drivers
            // decide it between themselves.
            AstNodeExpr* const heardp = new AstAnd{
                flp, new AstVarRef{flp, drv.enp, VAccess::READ},
                new AstOr{flp, new AstNot{flp, new AstVarRef{flp, drv.weakp, VAccess::READ}},
                          new AstNot{flp, forcingp->cloneTreePure(false)}}};
            addStmt(new AstAlways{new AstAssignW{
                flp, new AstVarRef{flp, netp, VAccess::WRITE},
                new AstCond{flp, heardp, new AstVarRef{flp, drv.valp, VAccess::READ},
                            new AstConst{flp, AstConst::StringToParse{}, "\'z"}}}});
        }
        VL_DO_DANGLING(forcingp->deleteTree(), forcingp);
        m_drivers.emplace(symp, drivers);
    }

    // A signal whose subtype names a resolution function of its own. The
    // drivers would be gathered into an array and the function asked what
    // they add up to, which is what the language says happens.
    //
    // Not enabled: it works -- NVC's driver5 passes with it -- but one of the
    // other tests brings the emitted tree down in a way a checked build does
    // not see, and a crash is worse than saying so. The analysis that feeds
    // it is done and correct: the drivers are counted, a signal with several
    // of them and no resolution function is now an error, and the function is
    // given the copy that takes that many values.
    void makeResolvedByFunction(VhdlSymbol* symp, AstVar* netp, FileLine* flp) {
        (void)netp;
        unsup(flp, "signal '" + symp->name() + "' with " + std::to_string(symp->driverCount())
                       + " drivers resolved by a function of its own");
    }

    // What one value contributes to a resolved signal: whether it drives at
    // all, and whether it drives only weakly.
    //
    // This has to read the VHDL expression rather than the lowered one. The
    // nine values collapse on the way down -- 'H' becomes 1 and 'L' becomes 0
    // -- so by the time there is an AstNode the strength has gone. Only the
    // literals carry it, which is exactly where real code puts it.
    enum class VhdlDrive : uint8_t { OFF, WEAK, FORCING };

    static VhdlDrive driveOfLiteral(AstNodeVhdlExpr* nodep) {
        if (!nodep) return VhdlDrive::FORCING;
        const VhdlValue val = V3VhdlEval::eval(nodep);
        const VhdlType* const typep = nodep->vhdlTypep();
        if (val.isStr()) {
            // An aggregate or a string of logic values, as in (others => 'Z')
            bool allZ = !val.toStr().empty();
            bool allWeak = !val.toStr().empty();
            for (const char c : val.toStr()) {
                if (c != 'Z' && c != 'z') allZ = false;
                if (c != 'L' && c != 'H' && c != 'W') allWeak = false;
            }
            if (allZ) return VhdlDrive::OFF;
            if (allWeak) return VhdlDrive::WEAK;
            return VhdlDrive::FORCING;
        }
        if (val.isInt() && typep && enumIsLogic(typep->base())) {
            const std::vector<VhdlSymbol*>& lits = typep->base()->literals();
            const int64_t pos = val.toInt();
            if (pos >= 0 && pos < static_cast<int64_t>(lits.size())) {
                const std::string& name = lits[pos]->name();  // "'H'" and the like
                if (name.size() == 3) {
                    switch (name[1]) {
                    case 'Z':
                    case 'z': return VhdlDrive::OFF;
                    case 'L':
                    case 'l':
                    case 'H':
                    case 'h':
                    case 'W':
                    case 'w': return VhdlDrive::WEAK;
                    default: break;
                    }
                }
            }
        }
        return VhdlDrive::FORCING;
    }

    // The same, over the alternatives of a conditional or selected assignment,
    // so that "value when c else 'Z'" turns the driver on and off with c
    AstNodeExpr* driveFlagOf(AstVhdlSigAssign* nodep, bool wantWeak, FileLine* flp) {
        const auto leafp = [&](AstVhdlWaveform* wfp) -> AstNodeExpr* {
            const VhdlDrive drive
                = driveOfLiteral(wfp && wfp->valuep() ? wfp->valuep() : nullptr);
            const bool set = wantWeak ? drive == VhdlDrive::WEAK : drive != VhdlDrive::OFF;
            return new AstConst{flp, AstConst::WidthedValue{}, 1, set ? 1U : 0U};
        };
        if (nodep->kind() == VhdlAssignKind::SIMPLE) return leafp(nodep->wavesp());
        std::vector<AstVhdlAlternative*> alts;
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            alts.push_back(VN_AS(p, VhdlAlternative));
        }
        AstNodeExpr* outp = nullptr;
        for (size_t i = alts.size(); i-- > 0;) {
            AstNodeExpr* const valuep = leafp(alts[i]->wavesp());
            AstNodeExpr* testp = nullptr;
            if (nodep->kind() == VhdlAssignKind::CONDITIONAL) {
                if (alts[i]->condp()) testp = lowerCondition(alts[i]->condp());
            } else {
                testp = choiceTest(alts[i]->choicesp(), nodep->selectorp());
            }
            if (!testp || !outp) {
                if (testp) VL_DO_DANGLING(testp->deleteTree(), testp);
                if (outp) VL_DO_DANGLING(outp->deleteTree(), outp);
                outp = valuep;  // The trailing else, or the last choice
                continue;
            }
            outp = new AstCond{flp, testp, valuep, outp};
        }
        return outp ? outp : new AstConst{flp, AstConst::WidthedValue{}, 1, 1};
    }

    // An assignment to a signal that has several drivers writes this driver's
    // value and enable rather than the signal
    AstNode* driveResolved(AstVhdlSigAssign* nodep, AstNodeVhdlExpr* targetp,
                           AstNodeExpr* valuep, AstNode* delayp, bool sequential,
                           FileLine* flp) {
        const VhdlSymbol* const symp = rootSymOf(targetp);
        const auto it = m_drivers.find(symp);
        const int idx = m_driverUsed[symp];
        if (it == m_drivers.end() || idx >= static_cast<int>(it->second.size())) {
            VL_DO_DANGLING(valuep->deleteTree(), valuep);
            unsup(flp, "assignment to a resolved signal from here");
            return nullptr;
        }
        const VhdlDriver& drv = it->second[idx];
        // Lowering the target against each of the two says how a slice or an
        // index of the signal maps onto them, without repeating the work here
        VhdlSymbol* const keyp = const_cast<VhdlSymbol*>(symp);
        AstVar* const savedp = m_vars[keyp];
        m_vars[keyp] = drv.valp;
        AstNodeExpr* const valLhsp = lowerExpr(targetp);
        m_vars[keyp] = savedp;
        if (!valLhsp) {
            VL_DO_DANGLING(valuep->deleteTree(), valuep);
            return nullptr;
        }
        // The enable and the weak flag are one bit whatever the signal's
        // width, so they are written directly rather than through the target
        if (!drv.enp) {
            // Resolved by a function of its own: the driver holds only a
            // value, and the function decides what they add up to
            return sequential ? static_cast<AstNode*>(
                       new AstAssignDly{flp, valLhsp, valuep, delayp})
                              : static_cast<AstNode*>(
                                  new AstAssignW{flp, valLhsp, valuep, delayp});
        }
        AstNodeExpr* const enp = driveFlagOf(nodep, false, flp);
        AstNodeExpr* const weakp = driveFlagOf(nodep, true, flp);
        AstNode* outp = nullptr;
        if (sequential) {
            outp = new AstAssignDly{flp, valLhsp, valuep, delayp};
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(new AstAssignDly{
                          flp, new AstVarRef{flp, drv.enp, VAccess::WRITE}, enp}));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(new AstAssignDly{
                          flp, new AstVarRef{flp, drv.weakp, VAccess::WRITE}, weakp}));
        } else {
            outp = new AstAssignW{flp, valLhsp, valuep, delayp};
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(new AstAssignW{
                          flp, new AstVarRef{flp, drv.enp, VAccess::WRITE}, enp}));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(new AstAssignW{
                          flp, new AstVarRef{flp, drv.weakp, VAccess::WRITE}, weakp}));
        }
        return outp;
    }

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
        if (m_drivers.count(rootSymOf(nodep->targetp()))) {
            VL_DO_DANGLING(lhsp->deleteTree(), lhsp);
            if (AstNode* const outp
                = driveResolved(nodep, nodep->targetp(), rhsp, delayp, false, flp)) {
                // Each continuous assignment becomes its own always, so
                // the list has to be taken apart from the front: the head has
                // no back to unlink from
                for (AstNode* p = outp; p;) {
                    AstNode* nextp = p->nextp();
                    if (nextp) nextp->unlinkFrBackWithNext();
                    addStmt(new AstAlways{VN_AS(p, AssignW)});
                    p = nextp;
                }
            }
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
            AstNodeExpr* testp = choiceTest(altp->choicesp(), selp);
            if (!testp) {
                outp = valuep;  // "when others"
                continue;
            }
            if (!outp) {
                // The choices of a selected assignment name every value the
                // selector can take (11.6), so the last of them is what is
                // left when none of the others matched. Testing it as well
                // and falling back on the target's own value would say the
                // target keeps its value when nothing matches -- a latch, and
                // one that reads itself, which is neither what VHDL means nor
                // something the scheduler can order.
                VL_DO_DANGLING(testp->deleteTree(), testp);
                outp = valuep;
                continue;
            }
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

    // A configuration specification says which entity a component instance is
    // to be built from (7.3), which is the same choice a configuration
    // declaration makes, written inside the architecture instead.
    void recordBinding(AstVhdlEntitySpec* specp, AstVhdlBinding* bindingp) {
        if (!specp || !bindingp || !bindingp->unitp()) return;
        std::vector<std::string> parts;
        if (!flattenName(bindingp->unitp(), parts) || parts.empty()) return;
        const std::string entity = parts.back();
        if (specp->all() || specp->others()) {
            // "for all : comp" and "for others : comp" name the component,
            // which the specification's class carries
            if (!specp->entityClass().empty()) m_bindings[specp->entityClass()] = entity;
            return;
        }
        for (AstNode* p = specp->namesp(); p; p = p->nextp()) {
            if (const AstVhdlName* const namep = VN_CAST(p, VhdlName)) {
                m_instanceBindings[namep->name()] = entity;
            }
        }
        if (!specp->entityClass().empty()) m_bindings[specp->entityClass()] = entity;
    }
    std::map<std::string, std::string> m_bindings;  // Component -> entity
    std::map<std::string, std::string> m_instanceBindings;  // Label -> entity

    // An instance binds to the entity of the same name, which is VHDL's
    // default binding rule (7.3.3)
    void lowerInstance(AstVhdlInstance* nodep) {
        FileLine* const flp = nodep->fileline();
        std::vector<std::string> parts;
        if (!flattenName(nodep->unitp(), parts) || parts.empty()) {
            unsup(flp, "instantiated unit name");
            return;
        }
        std::string modName = parts.back();
        // A configuration specification may have said which entity this
        // component or instance is built from (7.3)
        const std::string label = nodep->label();
        const auto instIt = m_instanceBindings.find(label);
        if (!label.empty() && instIt != m_instanceBindings.end()) {
            modName = instIt->second;
        } else {
            const auto compIt = m_bindings.find(modName);
            if (compIt != m_bindings.end()) modName = compIt->second;
        }
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
        const std::string instName
            = label.empty() ? "__Vinst" + std::to_string(++m_processNum) : label;
        addStmt(new AstCell{flp, flp, instName, modName, pinsp, paramsp, nullptr});
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
            AstNode* const jumpp = loopJump(nextp, flp);
            if (!jumpp) return nullptr;
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
        AstNode* delayp = nullptr;
        if (wfp->afterp()) {
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "'after' delay without --timing");
                return nullptr;
            }
            delayp = new AstDelay{flp, lowerExpr(wfp->afterp()), false};
        }
        if (m_drivers.count(rootSymOf(nodep->targetp()))) {
            AstNodeExpr* const valuep = lowerExpr(wfp->valuep());
            if (!valuep) return nullptr;
            return driveResolved(nodep, nodep->targetp(), valuep, delayp, sequential, flp);
        }
        AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
        AstNodeExpr* const rhsp = lowerExpr(wfp->valuep());
        if (!lhsp || !rhsp) return nullptr;
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

    // Where "exit" or "next" goes. Without a label it is the innermost loop,
    // which break and continue already mean. With one it may be any enclosing
    // loop, so the jump leaves a block put around that loop, or around its
    // body for a "next".
    AstNode* loopJump(AstVhdlNext* nodep, FileLine* flp) {
        const std::string& label = nodep->loopLabel();
        if (label.empty() || (!m_loops.empty() && m_loops.back().label == label)) {
            return nodep->isExit() ? static_cast<AstNode*>(new AstBreak{flp})
                                   : static_cast<AstNode*>(new AstContinue{flp});
        }
        for (size_t i = m_loops.size(); i-- > 0;) {
            if (m_loops[i].label != label) continue;
            AstJumpBlock*& blockp = nodep->isExit() ? m_loops[i].exitp : m_loops[i].bodyp;
            if (!blockp) blockp = new AstJumpBlock{flp, nullptr};
            return new AstJumpGo{flp, blockp};
        }
        err(flp, "No enclosing loop is labelled '" + label + "'");
        return nullptr;
    }

    // Lower a loop's body, and put it inside the block a labelled "next"
    // jumps out of if there was one
    AstNode* lowerLoopBody(AstVhdlLoop* nodep) {
        AstNode* const stmtsp = lowerStmts(nodep->stmtsp());
        if (AstJumpBlock* const blockp = m_loops.back().bodyp) {
            blockp->addStmtsp(stmtsp);
            return blockp;
        }
        return stmtsp;
    }

    // Put the loop inside the block a labelled "exit" jumps out of, if there
    // was one
    AstNode* wrapLoopExit(AstNode* loopp) {
        if (AstJumpBlock* const blockp = m_loops.back().exitp) {
            blockp->addStmtsp(loopp);
            return blockp;
        }
        return loopp;
    }

    AstNode* lowerLoop(AstVhdlLoop* nodep) {
        FileLine* const flp = nodep->fileline();
        if (nodep->kind() == VhdlLoopKind::FOR) return lowerForLoop(nodep);
        AstLoop* const loopp = new AstLoop{flp};
        m_loops.emplace_back(VhdlLoopFrame{nodep->label(), nullptr, nullptr});
        if (nodep->kind() == VhdlLoopKind::WHILE) {
            loopp->addStmtsp(new AstLoopTest{flp, loopp, lowerCondition(nodep->condp())});
        }
        loopp->addStmtsp(lowerLoopBody(nodep));
        AstNode* const outp = wrapLoopExit(loopp);
        m_loops.pop_back();
        return outp;
    }

    // A for loop declares its own parameter, which VHDL makes a constant of
    // the range's subtype (10.10)
    AstNode* lowerForLoop(AstVhdlLoop* nodep) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp || !symp->typep()) {
            unsup(flp, "loop over a range that is not locally static");
            return nullptr;
        }
        VhdlType* const rangep = symp->typep();
        // The bounds of a loop need not be static: "for i in 1 to n" with n a
        // parameter is ordinary VHDL (10.10). Take them from the range as
        // written, and fold only what is written as a type mark or a range
        // attribute.
        bool ascending = rangep->ascending();
        AstNodeExpr* initp = nullptr;
        AstNodeExpr* limitp = nullptr;
        if (AstVhdlRange* const rp = VN_CAST(nodep->rangep(), VhdlRange)) {
            if (rp->leftp() && rp->rightp()) {
                initp = lowerExpr(rp->leftp());
                limitp = lowerExpr(rp->rightp());
                ascending = rp->dir() == VhdlDir::TO;
            } else if (AstVhdlAttrName* const attrp = VN_CAST(rp->attrp(), VhdlAttrName)) {
                // "for i in v'range" over an object whose bounds are not
                // static, which is how a width-generic design is written
                const std::string& name = attrp->name();
                if (name == "range" || name == "reverse_range") {
                    int dim = 0;
                    if (attrp->argp()) {
                        const VhdlValue dimv = V3VhdlEval::eval(attrp->argp());
                        if (dimv.isInt()) dim = static_cast<int>(dimv.toInt()) - 1;
                    }
                    bool asc = true;
                    if (dynamicBounds(attrp->prefixp()->vhdlTypep(), dim, flp, initp, limitp,
                                      asc)) {
                        ascending = (name == "range") ? asc : !asc;
                        if (name == "reverse_range") std::swap(initp, limitp);
                    }
                }
            }
        }
        if (!initp || !limitp) {
            if (initp) VL_DO_DANGLING(initp->deleteTree(), initp);
            if (limitp) VL_DO_DANGLING(limitp->deleteTree(), limitp);
            if (!rangep->bounded()) {
                unsup(flp, "loop over a range that is not locally static");
                return nullptr;
            }
            ascending = rangep->ascending();
            initp = lowerValue(VhdlValue::mkInt(rangep->left(), rangep), rangep, flp);
            limitp = lowerValue(VhdlValue::mkInt(rangep->right(), rangep), rangep, flp);
        }
        AstVar* const varp = newVar(symp, VVarType::VAR, flp);
        varp->usedLoopIdx(true);
        addStmt(varp);
        // The discrete range is evaluated once, when the loop is entered
        // (10.10): assigning to something the bound names inside the body
        // must not change how many times the loop runs
        AstNode* preloadp = nullptr;
        if (!VN_IS(limitp, Const)) {
            AstVar* const limitVarp
                = newTempVar("__Vloophi", lowerType(rangep, flp), flp);
            preloadp = new AstAssign{
                flp, new AstVarRef{flp, limitVarp, VAccess::WRITE}, limitp};
            limitp = new AstVarRef{flp, limitVarp, VAccess::READ};
        }
        AstLoop* const loopp = new AstLoop{flp};
        AstNodeExpr* const condp
            = ascending ? static_cast<AstNodeExpr*>(new AstLteS{
                              flp, new AstVarRef{flp, varp, VAccess::READ}, limitp})
                        : static_cast<AstNodeExpr*>(new AstGteS{
                              flp, new AstVarRef{flp, varp, VAccess::READ}, limitp});
        loopp->addStmtsp(new AstLoopTest{flp, loopp, condp});
        m_loops.emplace_back(VhdlLoopFrame{nodep->label(), nullptr, nullptr});
        loopp->addStmtsp(lowerLoopBody(nodep));
        loopp->addContsp(new AstAssign{
            flp, new AstVarRef{flp, varp, VAccess::WRITE},
            ascending ? static_cast<AstNodeExpr*>(new AstAdd{
                            flp, new AstVarRef{flp, varp, VAccess::READ},
                            new AstConst{flp, AstConst::Signed32{}, 1}})
                      : static_cast<AstNodeExpr*>(new AstSub{
                            flp, new AstVarRef{flp, varp, VAccess::READ},
                            new AstConst{flp, AstConst::Signed32{}, 1}})});
        AstNode* const wrappedp = wrapLoopExit(loopp);
        m_loops.pop_back();
        AstNode* outp
            = new AstAssign{flp, new AstVarRef{flp, varp, VAccess::WRITE}, initp};
        if (preloadp) outp = AstNode::addNext(static_cast<AstNode*>(preloadp), outp);
        return AstNode::addNext(outp, wrappedp);
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
        if (symp) {
            std::vector<AstNodeVhdlExpr*> args;
            if (callp) {
                for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
                    AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                    if (asp && asp->actualp()) args.push_back(asp->actualp());
                }
            }
            // std_logic_1164 declares its own WRITE and HWRITE for the logic
            // types, so a textio operation is recognised by what it operates
            // on -- a LINE or a file -- as well as by being predefined
            const bool textio
                = symp->predefined()
                  || (!args.empty() && args[0]->vhdlTypep()
                      && (isLineType(args[0]->vhdlTypep())
                          || args[0]->vhdlTypep()->base()->kind() == VhdlTypeKind::FILE));
            if (textio) {
                if (AstNode* const outp = lowerTextioProc(symp->name(), args, flp)) return outp;
            }
        }
        // A procedure of a protected type runs on the object it is called on
        if (symp) {
            AstVhdlSelected* const selp
                = VN_CAST(callp ? callp->prefixp() : nodep->callp(), VhdlSelected);
            if (AstNodeExpr* const instp
                = selp ? protectedInstance(selp->prefixp()) : nullptr) {
                AstMethodCall* const mcallp
                    = new AstMethodCall{flp, instp, methodName(symp), nullptr};
                addCallArgs(mcallp, callp);
                return new AstStmtExpr{flp, mcallp};
            }
        }
        auto it = symp ? m_funcs.find(symp) : m_funcs.end();
        if (it == m_funcs.end() && symp && emitOnDemand(symp)) it = m_funcs.find(symp);
        if (it == m_funcs.end() || !VN_IS(it->second, Task)) {
            unsup(flp, "procedure call to '"
                           + (symp ? symp->name() : std::string{"<name>"}) + "'");
            return nullptr;
        }
        AstTaskRef* const refp = new AstTaskRef{flp, it->second->name(), nullptr};
        refp->taskp(it->second);
        addCallArgs(refp, callp);
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

    // The procedures of std.textio and the implicit file operations (5.5.2,
    // 16.4). A LINE is a string here, so building one is string concatenation
    // and writing it is a formatted write -- neither needs a runtime object.
    AstNode* lowerTextioProc(const std::string& name, std::vector<AstNodeVhdlExpr*>& args,
                             FileLine* flp) {
        if (name == "file_open") return lowerFileOpen(args, flp);
        if (name == "file_close" && args.size() == 1) {
            AstNodeExpr* const filep = lowerExpr(args[0]);
            return filep ? new AstFClose{flp, filep} : nullptr;
        }
        if (name == "flush" && args.size() == 1) {
            AstNodeExpr* const filep = lowerExpr(args[0]);
            return filep ? new AstFFlush{flp, filep} : nullptr;
        }
        if (name == "readline" && args.size() == 2) {
            // "readline(f, l)" fills the line buffer from the file. $fgets
            // writes into the string it is given and returns how much it
            // read, so the call is the statement; its result is not the line.
            AstNodeExpr* const filep = lowerExpr(args[0]);
            AstNodeExpr* const linep = lowerExpr(args[1]);
            if (!filep || !linep) return nullptr;
            AstNode* outp
                = new AstStmtExpr{flp, new AstFGetS{flp, linep->cloneTree(false), filep}};
            // A LINE holds the line without its end of line (16.4), which
            // $fgets keeps
            AstNodeExpr* const lastp
                = new AstGetcN{flp, linep->cloneTree(false),
                               new AstSub{flp, new AstLenN{flp, linep->cloneTree(false)},
                                          new AstConst{flp, AstConst::Signed32{}, 1}}};
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstIf{
                    flp,
                    new AstEq{flp, lastp, new AstConst{flp, AstConst::WidthedValue{}, 8, '\n'}},
                    new AstAssign{
                        flp, linep->cloneTree(false),
                        new AstSubstrN{flp, linep->cloneTree(false),
                                       new AstConst{flp, AstConst::Signed32{}, 0},
                                       new AstSub{flp, new AstLenN{flp, linep->cloneTree(false)},
                                                  new AstConst{flp, AstConst::Signed32{}, 2}}}}}));
            return outp;
        }
        if (name == "writeline" || name == "tee") {
            if (args.size() != 2) return nullptr;
            AstNodeExpr* const filep = lowerExpr(args[0]);
            AstNodeExpr* const linep = lowerExpr(args[1]);
            if (!filep || !linep) return nullptr;
            // Write the buffer, then empty it, as writeline does (16.4)
            AstNode* const outp = new AstDisplay{flp, VDisplayType::DT_WRITE, "%s\n", filep,
                                                 linep->cloneTree(false)};
            return AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, linep, new AstConst{flp, AstConst::String{}, ""}}));
        }
        if (name == "read" || name == "hread" || name == "oread" || name == "bread") {
            return lowerTextioRead(name, args, flp);
        }
        if (name == "deallocate" && args.size() == 1) {
            AstNodeExpr* const linep = lowerExpr(args[0]);
            if (!linep) return nullptr;
            return new AstAssign{flp, linep, new AstConst{flp, AstConst::String{}, ""}};
        }
        // WRITE and its radix forms append to the line buffer
        std::string radix;
        if (name == "write" || name == "swrite") {
            radix = "";
        } else if (name == "hwrite") {
            radix = "%h";
        } else if (name == "owrite") {
            radix = "%o";
        } else if (name == "bwrite") {
            radix = "%b";
        } else {
            return nullptr;
        }
        if (args.size() < 2) return nullptr;
        AstNodeExpr* const linep = lowerExpr(args[0]);
        AstNodeExpr* const valuep = lowerExpr(args[1]);
        if (!linep || !valuep) return nullptr;
        const std::string format
            = radix.empty() ? formatFor(args[1]->vhdlTypep()) : radix;
        AstNodeExpr* textp = new AstSFormatF{flp, format, false, valuep};
        // VHDL writes hexadecimal in upper case (16.4)
        if (radix == "%h") textp = new AstToUpperN{flp, textp};
        return new AstAssign{flp, linep,
                             new AstConcatN{flp, linep->cloneTree(false), textp}};
    }

    // A variable of the lowerer's own, for holding a value across the
    // statements one VHDL construct expands into
    AstVar* newTempVar(const std::string& name, AstNodeDType* dtypep, FileLine* flp) {
        AstVar* const varp = new AstVar{flp, VVarType::VAR, name + std::to_string(++m_tempNum),
                                        VFlagChildDType{}, dtypep};
        addStmt(varp);
        return varp;
    }
    int m_tempNum = 0;

    // "c == ' ' || c == HT", the test that finds the edges of an item
    static AstNodeExpr* isBlank(AstNodeExpr* charp, FileLine* flp) {
        // getc gives one character, so the comparisons are eight bits wide
        return new AstLogOr{
            flp, new AstEq{flp, charp, new AstConst{flp, AstConst::WidthedValue{}, 8, ' '}},
            new AstEq{flp, charp->cloneTree(false),
                      new AstConst{flp, AstConst::WidthedValue{}, 8, 9}}};
    }

    // READ and its radix forms take the leftmost item off the line and turn
    // it into a value (16.4). A LINE is a string here, so the item is found
    // with the string operations Verilator already has, rather than by
    // walking a list of characters at run time as an event-driven simulator
    // must. The line is shortened by what was read, which is what makes
    // successive READs walk along one line.
    AstNode* lowerTextioRead(const std::string& name, std::vector<AstNodeVhdlExpr*>& args,
                             FileLine* flp) {
        if (args.size() < 2 || args.size() > 3) return nullptr;
        AstNodeExpr* const linep = lowerExpr(args[0]);
        AstNodeExpr* const valuep = lowerExpr(args[1]);
        if (!linep || !valuep) return nullptr;
        VhdlType* const typep = args[1]->vhdlTypep();

        // Which base the item is written in. The radix forms say so; plain
        // READ takes its answer from the type, since a vector is written in
        // binary and a number in decimal.
        AstAtoN::FmtType fmt = AstAtoN::ATOI;
        if (name == "hread") {
            fmt = AstAtoN::ATOHEX;
        } else if (name == "oread") {
            fmt = AstAtoN::ATOOCT;
        } else if (name == "bread") {
            fmt = AstAtoN::ATOBIN;
        } else if (isLogicish(typep) || isBitish(typep)) {
            fmt = AstAtoN::ATOBIN;
        } else if (typep && typep->base()->kind() == VhdlTypeKind::FLOATING) {
            fmt = AstAtoN::ATOREAL;
        } else if (isStringType(typep)) {
            fmt = AstAtoN::ATOI;  // Unused: a string takes the item as it is
        } else if (!isIntegerType(typep) && typep
                   && typep->base()->kind() != VhdlTypeKind::PHYSICAL) {
            unsup(flp, "READ of a " + typep->prettyName());
            return nullptr;
        }

        AstVar* const idxp = newTempVar("__Vtxtpos", new AstBasicDType{flp, VBasicDTypeKwd::INT},
                                        flp);
        AstVar* const tokp = newTempVar("__Vtxtitem",
                                        new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);

        // Leading blanks are not part of the item (16.4)
        AstLoop* const skipp = new AstLoop{flp};
        skipp->addStmtsp(new AstLoopTest{
            flp, skipp, isBlank(new AstGetcN{flp, linep->cloneTree(false),
                                             new AstConst{flp, AstConst::Signed32{}, 0}},
                                flp)});
        skipp->addStmtsp(new AstAssign{
            flp, linep->cloneTree(false),
            new AstSubstrN{flp, linep->cloneTree(false),
                           new AstConst{flp, AstConst::Signed32{}, 1},
                           new AstSub{flp, new AstLenN{flp, linep->cloneTree(false)},
                                      new AstConst{flp, AstConst::Signed32{}, 1}}}});
        AstNode* outp = skipp;

        // The item runs to the next blank, or to the end of the line
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(
                      new AstAssign{flp, new AstVarRef{flp, idxp, VAccess::WRITE},
                                    new AstConst{flp, AstConst::Signed32{}, 0}}));
        AstLoop* const scanp = new AstLoop{flp};
        scanp->addStmtsp(new AstLoopTest{
            flp, scanp,
            new AstLogAnd{
                flp,
                new AstLtS{flp, new AstVarRef{flp, idxp, VAccess::READ},
                           new AstLenN{flp, linep->cloneTree(false)}},
                new AstLogNot{flp, isBlank(new AstGetcN{flp, linep->cloneTree(false),
                                                        new AstVarRef{flp, idxp, VAccess::READ}},
                                           flp)}}});
        scanp->addStmtsp(new AstAssign{
            flp, new AstVarRef{flp, idxp, VAccess::WRITE},
            new AstAdd{flp, new AstVarRef{flp, idxp, VAccess::READ},
                       new AstConst{flp, AstConst::Signed32{}, 1}}});
        outp = AstNode::addNext(outp, static_cast<AstNode*>(scanp));

        // Split the line into the item and what is left of it
        outp = AstNode::addNext(
            outp,
            static_cast<AstNode*>(new AstAssign{
                flp, new AstVarRef{flp, tokp, VAccess::WRITE},
                new AstSubstrN{flp, linep->cloneTree(false),
                               new AstConst{flp, AstConst::Signed32{}, 0},
                               new AstSub{flp, new AstVarRef{flp, idxp, VAccess::READ},
                                          new AstConst{flp, AstConst::Signed32{}, 1}}}}));
        outp = AstNode::addNext(
            outp,
            static_cast<AstNode*>(new AstAssign{
                flp, linep->cloneTree(false),
                new AstSubstrN{flp, linep->cloneTree(false),
                               new AstVarRef{flp, idxp, VAccess::READ},
                               new AstSub{flp, new AstLenN{flp, linep->cloneTree(false)},
                                          new AstConst{flp, AstConst::Signed32{}, 1}}}}));

        // The value itself
        AstNodeExpr* readp = nullptr;
        if (isStringType(typep)) {
            readp = new AstVarRef{flp, tokp, VAccess::READ};
        } else {
            readp = new AstAtoN{flp, new AstVarRef{flp, tokp, VAccess::READ}, fmt};
            // The conversions give an integer, so a vector target is taken
            // from its low bits rather than warned about
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && typep->isConstrainedArray() && !typep->indexTypes().empty()) {
                const int64_t bits = typep->indexTypes()[0]->length();
                if (bits > 0 && bits <= 4096) {
                    readp = sizedTo(readp, static_cast<int>(bits), flp);
                }
            }
        }
        outp = AstNode::addNext(outp,
                                static_cast<AstNode*>(new AstAssign{flp, valuep, readp}));

        // The GOOD form says whether there was anything to read, and does not
        // fail when there was not (16.4)
        if (args.size() == 3) {
            AstNodeExpr* const goodp = lowerExpr(args[2]);
            if (goodp) {
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(new AstAssign{
                              flp, goodp,
                              new AstNeq{flp, new AstVarRef{flp, idxp, VAccess::READ},
                                         new AstConst{flp, AstConst::Signed32{}, 0}}}));
            }
        }
        return outp;
    }

    // BIT and BIT_VECTOR, which textio writes and reads in binary just as it
    // does the nine-value logic types
    static bool isBitish(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        if (basep == V3VhdlStd::bitType()) return true;
        return basep->kind() == VhdlTypeKind::ARRAY && basep->elementp()
               && basep->elementp()->base() == V3VhdlStd::bitType();
    }

    // FILE_OPEN in both its forms (5.5.2). The open kind decides the mode,
    // and the status form reports whether the file was there.
    AstNode* lowerFileOpen(std::vector<AstNodeVhdlExpr*>& args, FileLine* flp) {
        size_t first = 0;
        AstNodeVhdlExpr* statusp = nullptr;
        if (args.size() >= 3 && args[0]->vhdlTypep()
            && args[0]->vhdlTypep()->base()->kind() == VhdlTypeKind::ENUM
            && args[1]->vhdlTypep()
            && args[1]->vhdlTypep()->base()->kind() == VhdlTypeKind::FILE) {
            statusp = args[0];
            first = 1;
        }
        if (args.size() < first + 2) return nullptr;
        AstNodeExpr* const filep = lowerExpr(args[first]);
        AstNodeExpr* const namep = lowerExpr(args[first + 1]);
        if (!filep || !namep) return nullptr;
        // READ_MODE, WRITE_MODE, APPEND_MODE (16.3)
        std::string mode = "r";
        if (args.size() > first + 2) {
            const VhdlValue kind = V3VhdlEval::eval(args[first + 2]);
            if (kind.isInt()) mode = kind.toInt() == 1 ? "w" : (kind.toInt() == 2 ? "a" : "r");
        }
        AstNode* outp = new AstAssign{
            flp, filep,
            new AstFOpen{flp, namep, new AstConst{flp, AstConst::String{}, mode}}};
        if (statusp) {
            // OPEN_OK is zero, NAME_ERROR the second value (16.3)
            AstNodeExpr* const statusRefp = lowerExpr(statusp);
            if (statusRefp) {
                outp = AstNode::addNext(
                    outp,
                    static_cast<AstNode*>(new AstAssign{
                        flp, statusRefp,
                        new AstCond{flp,
                                    new AstNeq{flp, filep->cloneTree(false),
                                               new AstConst{flp, AstConst::Signed32{}, 0}},
                                    new AstConst{flp, AstConst::WidthedValue{}, 2, 0},
                                    new AstConst{flp, AstConst::WidthedValue{}, 2, 1}}}));
            }
        }
        return outp;
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
        // 'IMAGE and TO_STRING both turn a value into text, but not the same
        // text: the image of an enumeration literal is written the way the
        // literal is, so a character literal keeps its apostrophes (16.2.3)
        AstNodeVhdlExpr* valuep = nodep;
        bool isImage = false;
        if (AstVhdlAttrName* const attrp = VN_CAST(nodep, VhdlAttrName)) {
            if (attrp->name() == "image" && attrp->argp()) {
                valuep = attrp->argp();
                isImage = true;
            }
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
        if (AstNodeExpr* const namep = enumImage(exprp, valuep->vhdlTypep(), flOf(valuep))) {
            format += "%s";
            return namep;
        }
        format += formatFor(valuep->vhdlTypep(), isImage);
        return exprp;
    }

    static FileLine* flOf(AstNode* nodep) { return nodep->fileline(); }

    // 'IMAGE of an enumeration value is the literal's own name (16.2), not
    // its position. The literals are known here, so the choice between them
    // is made without a table at run time; nothing else in the design needs
    // the names, so nothing else pays for them.
    static AstNodeExpr* enumImage(AstNodeExpr* exprp, const VhdlType* typep, FileLine* flp) {
        if (!typep) return nullptr;
        const VhdlType* const basep = typep->base();
        if (basep->kind() != VhdlTypeKind::ENUM || enumIsLogic(basep)) return nullptr;
        const std::vector<VhdlSymbol*>& literals = basep->literals();
        if (literals.empty() || literals.size() > 256) return nullptr;
        AstNodeExpr* outp = new AstConst{flp, AstConst::String{}, "?"};
        // Built from the last literal back, so the first is tested first
        for (size_t i = literals.size(); i-- > 0;) {
            outp = new AstCond{
                flp,
                new AstEq{flp, exprp->cloneTree(false),
                          new AstConst{flp, AstConst::Unsized32{},
                                       static_cast<uint32_t>(literals[i]->position())}},
                new AstConst{flp, AstConst::String{}, literals[i]->name()}, outp};
        }
        VL_DO_DANGLING(exprp->deleteTree(), exprp);
        return outp;
    }

    // Text that goes into a format string must not be read as a conversion
    static void appendLiteral(std::string& format, const std::string& text) {
        for (const char c : text) {
            if (c == '%') format += '%';
            format += c;
        }
    }

    // How a value of that type reads
    static std::string formatFor(const VhdlType* typep, bool isImage = false) {
        if (!typep) return "%0d";
        const VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::FLOATING:
        case VhdlTypeKind::UNIVERSAL_REAL: return "%g";
        case VhdlTypeKind::PHYSICAL:
            // A physical value is written with its unit, and TIME in terms of
            // the resolution limit, which here is the femtosecond (16.2.3)
            return isImage ? "%0t fs" : "%0t";
        case VhdlTypeKind::ARRAY:
            // A vector of logic reads in binary, as VHDL prints it
            return basep->elementp() && enumIsLogic(basep->elementp()->base()) ? "%b" : "%0d";
        case VhdlTypeKind::ENUM:
            if (!enumIsLogic(basep)) return "%0d";
            // A logic value is a character literal, and its image keeps the
            // apostrophes the literal is written with
            return isImage ? "'%b'" : "%b";
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
        if (m_funcs.count(symp)) return;  // Its declaration already made one
        if (symp->isForeign()) {
            lowerForeignSubprogram(nodep, symp);
            return;
        }
        if (!nodep->hasBody()) return;  // A declaration alone emits nothing
        // A subprogram with an unconstrained parameter is a template: each
        // call site got a copy with the lengths it supplies, and only those
        // copies are emitted. Emitting the template as well would mean
        // deciding a length it does not have.
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && !typep->isConstrainedArray()) {
                return;
            }
        }
        // An overloaded name needs a distinct one in the emitted code
        const std::string name = uniqueName(symp);
        AstNodeFTask* ftaskp = nullptr;
        if (nodep->isFunction()) {
            AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
            funcp->fvarp(newReturnVar(name, lowerType(returnType(nodep, symp), flp), flp));
            ftaskp = funcp;
        } else {
            ftaskp = new AstTask{flp, name, nullptr};
        }
        // A VHDL subprogram's variables are created anew on each call (4.3),
        // which is what an automatic lifetime means
        ftaskp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        // A subprogram declared in a protected type body is one of its
        // methods, and reaches the object's own variables (5.6.2)
        if (m_protClassp) ftaskp->classMethod(true);
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
            // A parameter of a protected type must be a variable of mode
            // inout (4.2.2.3), but what is passed is the object itself, not a
            // copy of its state -- so the handle goes in and nothing comes
            // back, and the subprogram works on the caller's own object
            if (psymp->typep() && psymp->typep()->base()->kind() == VhdlTypeKind::PROTECTED) {
                varp->direction(VDirection::INPUT);
            }
            varp->declDirection(varp->direction());
            varp->funcLocal(true);
            // A parameter with a default value may be left out of a call
            // (4.2.2.1), which is what an initial value on the port means here
            if (ifp->defaultp()) varp->valuep(lowerExpr(ifp->defaultp()));
            m_vars.emplace(psymp, varp);
            ftaskp->addStmtsp(varp);
        }
        for (AstNode* p = nodep->declsp(); p; p = p->nextp()) lowerDecl(p);
        ftaskp->addStmtsp(lowerStmts(nodep->stmtsp()));
        m_ftaskp = nullptr;  // The task itself belongs to the module
        addStmt(ftaskp);
    }

    // The variable a function's result comes back in. V3LinkDot makes one for
    // Verilog, from the return type the parser left on the node; here the type
    // is already known, so the variable is made outright and must carry the
    // same marks V3LinkDot would have given it.
    static AstVar* newReturnVar(const std::string& name, AstNodeDType* dtypep, FileLine* flp) {
        AstVar* const varp
            = new AstVar{flp, VVarType::PORT, name, VFlagChildDType{}, dtypep};
        varp->direction(VDirection::OUTPUT);
        varp->declDirection(VDirection::OUTPUT);
        varp->funcLocal(true);
        varp->funcReturn(true);
        varp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        varp->trace(false);  // Not user visible
        return varp;
    }

    // A subprogram given the FOREIGN attribute (16.2) is implemented in C. It
    // becomes exactly the node an "import \"DPI-C\"" declaration produces, so
    // V3Task generates the same wrapper, does the same marshalling, and writes
    // the same __Dpi.h entry -- and one C function can serve a VHDL and a
    // SystemVerilog testbench in one model. Verilator links statically, so
    // there is no dlopen and the C++ compiler can inline across the boundary.
    void lowerForeignSubprogram(AstVhdlSubprogDecl* nodep, VhdlSymbol* symp) {
        FileLine* const flp = nodep->fileline();
        if (nodep->hasBody()) {
            flp->v3error("Subprogram '" + symp->name()
                         + "' has the FOREIGN attribute, so it must not also have a body");
            return;
        }
        // The C name is what the FOREIGN string said; the VHDL name may be
        // spelled differently and may be overloaded
        const std::string name = uniqueName(symp);
        AstNodeFTask* ftaskp = nullptr;
        if (nodep->isFunction()) {
            AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
            VhdlType* const retp = symp->returnTypep();
            if (retp && retp->base()->kind() == VhdlTypeKind::ARRAY
                && !retp->isConstrainedArray()) {
                flp->v3error("A foreign function must return a constrained type, as C has no "
                             "way to say how long the result is");
                VL_DO_DANGLING(funcp->deleteTree(), funcp);
                return;
            }
            funcp->fvarp(newReturnVar(name, lowerType(retp, flp), flp));
            ftaskp = funcp;
        } else {
            // Not dpiTask: that makes the C function return the int a
            // SystemVerilog task uses to say it was disabled. A VHDL
            // procedure cannot be disabled, and both GHDL's VHPIDIRECT and a
            // SystemVerilog "function void" give a void C function, which is
            // what lets one C model serve both languages.
            ftaskp = new AstTask{flp, name, nullptr};
        }
        ftaskp->dpiImport(true);
        ftaskp->cname(symp->foreignName());
        ftaskp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        for (AstNode* p = nodep->paramsp(); p; p = p->nextp()) {
            AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            VhdlSymbol* const psymp = ifp->vhdlSymp();
            if (!psymp) continue;
            if (psymp->typep() && psymp->typep()->base()->kind() == VhdlTypeKind::ARRAY
                && !psymp->typep()->isConstrainedArray()) {
                ifp->v3warn(E_UNSUPPORTED, "Unsupported: unconstrained parameter '" + ifp->name()
                                               + "' of a foreign subprogram");
                continue;
            }
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
        m_funcs.emplace(symp, ftaskp);
        addStmt(ftaskp);
    }

    // The subtype a function returns. VHDL lets it be unconstrained -- the
    // length comes from whatever the body returns -- so where the declaration
    // does not say, the return statements do (4.2.1).
    static VhdlType* returnType(AstVhdlSubprogDecl* nodep, const VhdlSymbol* symp) {
        VhdlType* const declaredp = symp->returnTypep();
        if (!declaredp || declaredp->base()->kind() != VhdlTypeKind::ARRAY
            || declaredp->isConstrainedArray()) {
            return declaredp;
        }
        VhdlType* foundp = nullptr;
        findReturnType(nodep->stmtsp(), foundp);
        if (foundp) return foundp;
        // Where the returned expression has no length of its own -- "return v
        // and m", whose operator gives a result as long as its operands --
        // a parameter of the same type does
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (typep && typep->base() == declaredp->base() && typep->isConstrainedArray()) {
                return const_cast<VhdlType*>(typep);
            }
        }
        return declaredp;
    }
    static void findReturnType(AstNode* stmtsp, VhdlType*& foundp) {
        for (AstNode* p = stmtsp; p && !foundp; p = p->nextp()) {
            if (AstVhdlReturn* const retp = VN_CAST(p, VhdlReturn)) {
                if (retp->valuep() && retp->valuep()->vhdlTypep()
                    && retp->valuep()->vhdlTypep()->isConstrainedArray()) {
                    foundp = retp->valuep()->vhdlTypep();
                }
                continue;
            }
            if (AstVhdlIf* const ifp = VN_CAST(p, VhdlIf)) {
                for (AstNode* ap = ifp->altsp(); ap && !foundp; ap = ap->nextp()) {
                    findReturnType(VN_AS(ap, VhdlAlternative)->stmtsp(), foundp);
                }
                continue;
            }
            if (AstVhdlCase* const casep = VN_CAST(p, VhdlCase)) {
                for (AstNode* ap = casep->altsp(); ap && !foundp; ap = ap->nextp()) {
                    findReturnType(VN_AS(ap, VhdlAlternative)->stmtsp(), foundp);
                }
                continue;
            }
            if (AstVhdlLoop* const loopp = VN_CAST(p, VhdlLoop)) {
                findReturnType(loopp->stmtsp(), foundp);
            }
        }
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
        m_names.clear();
        m_funcs.clear();
        m_emitting.clear();
        m_drivers.clear();
        m_driverCount.clear();
        m_driverUsed.clear();
        s_partialDriven.clear();
        // Which signals have more than one driver has to be known before the
        // declarations are lowered, since it decides what they become
        if (archp) countDrivers(archp->stmtsp());
        m_bindings.clear();
        m_instanceBindings.clear();
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
                lowerConcurrentDriver(VN_AS(p, NodeVhdlStmt));
            }
        }
        m_modp->addStmtsp(m_stmtsp);
        m_stmtsp = nullptr;
        m_modp->addStmtsp(m_shadowsp);
        m_shadowsp = nullptr;
        m_shadows.clear();
        m_netlistp->addModulesp(m_modp);
        m_modp = nullptr;
    }
};

std::set<const VhdlSymbol*> VhdlLowerVisitor::s_partialDriven;

//######################################################################
// Design units

void VhdlLowerVisitor::lowerUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    // An entity and an architecture become one module, so pair them up
    // first. VHDL's default binding takes the architecture analyzed last
    // (7.3.3), which is what keeping the newest one gives.
    std::map<std::string, AstVhdlEntity*> entities;
    std::map<std::string, AstVhdlArchitecture*> archs;
    std::map<std::string, std::string> chosen;  // Entity -> architecture a configuration picked
    std::map<std::string, AstVhdlArchitecture*> namedArchs;  // "lib.entity.arch" -> it
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
            namedArchs[lib + "." + parts.back() + "." + archp->name()] = archp;
            continue;
        }
        if (VN_IS(unitp, VhdlPackage) || VN_IS(unitp, VhdlPackageBody)
            || VN_IS(unitp, VhdlContextDecl)) {
            // A package contributes types, constants and subprograms, which
            // are used where they are referenced rather than emitted here
            continue;
        }
        if (AstVhdlConfiguration* const cfgp = VN_CAST(unitp, VhdlConfiguration)) {
            // A configuration says which architecture an entity is to be
            // built from (3.4). Recording that choice is all it takes to
            // honour the common form; the default binding, which takes the
            // architecture analyzed last, applies where none says otherwise.
            std::vector<std::string> parts;
            if (!flattenName(cfgp->entityNamep(), parts) || parts.empty()) continue;
            const std::string arch = topArchitectureOf(cfgp->blockConfigp());
            if (!arch.empty()) chosen[lib + "." + parts.back()] = arch;
            continue;
        }
        unsup(unitp->fileline(), std::string{"design unit "} + unitp->typeName());
    }
    for (const std::string& key : order) {
        AstVhdlArchitecture* archp = nullptr;
        const auto pickIt = chosen.find(key);
        if (pickIt != chosen.end()) {
            const auto namedIt = namedArchs.find(key + "." + pickIt->second);
            if (namedIt != namedArchs.end()) archp = namedIt->second;
        }
        if (!archp) {
            const auto archIt = archs.find(key);
            if (archIt != archs.end()) archp = archIt->second;
        }
        lowerEntity(entities[key], archp);
    }
}

// The architecture a configuration's outermost block configuration names
std::string VhdlLowerVisitor::topArchitectureOf(AstVhdlBlockConfig* nodep) {
    if (!nodep) return "";
    // The outermost block specification of a configuration names the
    // architecture (3.4.2), and the parser records it as an entity
    // specification's single name
    const AstVhdlEntitySpec* const specp = VN_CAST(nodep->specp(), VhdlEntitySpec);
    if (!specp) return "";
    const AstVhdlName* const namep = VN_CAST(specp->namesp(), VhdlName);
    return namep ? namep->name() : "";
}

//######################################################################
// Entry point

bool V3VhdlLower::lower(const std::vector<AstNodeVhdlUnit*>& unitsp, AstNetlist* rootp) {
    VhdlLowerVisitor visitor{rootp};
    visitor.lowerUnits(unitsp);
    return !visitor.errored();
}
