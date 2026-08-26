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

struct AstVhdlExprPair final {
    std::string name;
    AstNodeVhdlExpr* exprp;
};

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
        case VhdlTypeKind::RECORD:
            // The subtype's own elements, not the base type's: a subtype may
            // constrain one of them (5.3.3)
            return lowerRecordType(typep, flp);
        case VhdlTypeKind::FILE:
            // A file object is a descriptor, exactly as Verilog's is
            return new AstBasicDType{flp, VBasicDTypeKwd::INTEGER, VSigning::SIGNED};
        case VhdlTypeKind::ACCESS:
            // An access to STRING is a LINE, which is a string of its own.
            // Other access types wait on the general dynamic-object work.
            if (basep->designatedp() && isStringType(basep->designatedp()->base())) {
                return new AstBasicDType{flp, VBasicDTypeKwd::STRING};
            }
            if (!basep->designatedp()
                || basep->designatedp()->base()->kind() == VhdlTypeKind::INCOMPLETE) {
                unsup(flp, "access type '" + basep->prettyName() + "'");
                return newLogicDType(flp, 1);
            }
            return new AstClassRefDType{flp, accessClass(basep, flp), nullptr};
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
        newp->dtypep(newp->findVoidDType());
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
    void addCallArgs(AstNodeFTaskRef* refp, AstVhdlCall* callp,
                     const VhdlSymbol* symp = nullptr) {
        if (callp) {
            for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
                AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->actualp()) continue;
                const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
                refp->addArgsp(new AstArg{asp->fileline(), namep ? namep->name() : "",
                                          lowerExpr(asp->actualp())});
            }
        }
        addCaptureArgs(refp, symp, refp->fileline());
    }

    // A nested subprogram is passed what it reaches out of its own region for
    void addCaptureArgs(AstNodeFTaskRef* refp, const VhdlSymbol* symp, FileLine* flp) {
        if (!symp) return;
        const auto it = m_captureOf.find(symp->resolved());
        if (it == m_captureOf.end()) return;
        for (VhdlSymbol* const capp : it->second) {
            const auto varIt = m_vars.find(capp);
            if (varIt == m_vars.end() || !varIt->second) continue;
            refp->addArgsp(
                new AstArg{flp, "", new AstVarRef{flp, varIt->second, VAccess::READWRITE}});
        }
    }

    // An access type points at an object made by an allocator and read
    // through ".all" (5.4). Verilator has no pointer to a bare value, but a
    // class reference is one: the class holds the designated value as its
    // only member, "new" makes one, and a null access value is a null handle.
    AstClass* accessClass(VhdlType* basep, FileLine* flp) {
        const auto it = m_accClasses.find(basep);
        if (it != m_accClasses.end()) return it->second;
        AstClass* const classp = new AstClass{
            flp, "__Vacc" + std::to_string(++m_nameNum) + "__" + basep->name(),
            m_modp ? m_modp->libname() : ""};
        m_accClasses.emplace(basep, classp);
        v3Global.setHasClasses();
        m_netlistp->dollarUnitPkgAddp()->addStmtsp(classp);
        VhdlType* const designatedp = basep->designatedp();
        AstVar* const memberp
            = new AstVar{flp, VVarType::MEMBER, "all", VFlagChildDType{},
                         lowerType(designatedp, flp)};
        memberp->funcLocal(false);
        memberp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        classp->addStmtsp(memberp);
        // "new t" gives the designated subtype's default, "new t'(v)" gives v,
        // which is one constructor with a defaulted parameter (9.3.7)
        AstFunc* const newp = new AstFunc{flp, "new", nullptr, nullptr};
        newp->isConstructor(true);
        newp->classMethod(true);
        newp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        newp->dtypep(newp->findVoidDType());
        AstVar* const argp = new AstVar{flp, VVarType::PORT, "__Vv", VFlagChildDType{},
                                        lowerType(designatedp, flp)};
        argp->direction(VDirection::INPUT);
        argp->declDirection(VDirection::INPUT);
        argp->funcLocal(true);
        // The parameter has no default: "new t" with no value passes the
        // designated subtype's default from the call site instead. A default
        // written here would have to be a folded constant, which a record's
        // is not.
        newp->addStmtsp(argp);
        newp->addStmtsp(new AstAssign{flp, new AstVarRef{flp, memberp, VAccess::WRITE},
                                      new AstVarRef{flp, argp, VAccess::READ}});
        classp->addStmtsp(newp);
        return classp;
    }
    std::map<const VhdlType*, AstClass*> m_accClasses;

    // "p.all", and the dereference VHDL leaves implicit when a name of an
    // access value is used where the designated object is meant (8.3)
    static AstNodeExpr* dereference(AstNodeExpr* fromp, FileLine* flp) {
        return new AstMemberSel{flp, fromp, VFlagChildDType{}, "all"};
    }

    // The type an expression has, from the node or from what it denotes
    static const VhdlType* typeOfExpr(const AstNodeVhdlExpr* nodep) {
        if (!nodep) return nullptr;
        if (nodep->vhdlTypep()) return nodep->vhdlTypep();
        return nodep->vhdlSymp() ? nodep->vhdlSymp()->typep() : nullptr;
    }

    // Selecting an element of, or indexing, an access value means the object
    // it points at, with no ".all" written (8.3)
    static AstNodeExpr* derefIfAccess(AstNodeExpr* fromp, const AstNodeVhdlExpr* prefixp,
                                      FileLine* flp) {
        const VhdlType* const typep = typeOfExpr(prefixp);
        if (!fromp || !typep || typep->base()->kind() != VhdlTypeKind::ACCESS) return fromp;
        if (isLineType(typep)) return fromp;  // A LINE is text, not an object
        return dereference(fromp, flp);
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

    // The index a STRING's own range starts at. Verilator's string is indexed
    // from zero; VHDL indexes STRING from its range, which is 1 for the
    // predefined subtype and for every string literal (16.3).
    static int64_t stringLow(const VhdlType* typep) {
        if (!typep) return 1;
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        if (!indexes.empty() && indexes[0] && indexes[0]->bounded()) return indexes[0]->left();
        return 1;
    }
    // An index into a STRING, brought back to where Verilator counts from
    static AstNodeExpr* stringIndex(const VhdlType* typep, AstNodeExpr* indexp, FileLine* flp) {
        if (!typep || !isStringType(typep)) return indexp;
        const int64_t low = stringLow(typep);
        if (low == 0) return indexp;
        return new AstSub{flp, indexp,
                          new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(low)}};
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
                // An array whose index subtype is NATURAL, and which nothing
                // has constrained, would be four billion elements long. That
                // is not a design, it is an unconstrained type that reached
                // here by mistake, and emitting it hangs the back end.
                if (idxp->length() > (1 << 24)) {
                    unsup(flp, "array '" + typep->prettyName() + "' of "
                                   + std::to_string(idxp->length()) + " elements");
                    return dtypep;
                }
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
                       AstNodeExpr*& rightp, bool& ascending, bool* dirKnownp = nullptr) {
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
        if (dirKnownp) *dirKnownp = !rp->dirUnknown();
        return true;
    }

    // "left <= right", which is what makes a range ascending once the bounds
    // are folded. Used where the direction is only known then.
    static AstNodeExpr* ascendingOf(AstNodeExpr* leftp, AstNodeExpr* rightp, FileLine* flp) {
        return new AstLteS{flp, leftp->cloneTree(false), rightp->cloneTree(false)};
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
        AstNodeExpr* diffp = nullptr;
        if (rp->dirUnknown()) {
            // Which way round it runs only folds with the instance
            diffp = new AstCond{
                flp, ascendingOf(leftp, rightp, flp),
                new AstSub{flp, rightp->cloneTree(false), leftp->cloneTree(false)},
                new AstSub{flp, leftp, rightp}};
        } else {
            diffp = rp->dir() == VhdlDir::TO
                        ? static_cast<AstNodeExpr*>(new AstSub{flp, rightp, leftp})
                        : static_cast<AstNodeExpr*>(new AstSub{flp, leftp, rightp});
        }
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

    // Whether every element of a record can live inside a packed structure.
    // One that cannot -- a real, a string, an unpacked array, an access value
    // -- makes the whole record unpacked.
    static bool recordPacks(const VhdlType* typep) {
        for (const VhdlSymbol* const elemp : typep->elements()) {
            const VhdlType* const etypep = elemp->typep();
            if (!etypep) return false;
            switch (etypep->base()->kind()) {
            case VhdlTypeKind::ENUM:
            case VhdlTypeKind::INTEGER:
            case VhdlTypeKind::UNIVERSAL_INTEGER:
            case VhdlTypeKind::PHYSICAL: break;
            case VhdlTypeKind::ARRAY:
                if (isStringType(etypep)) return false;
                if (!etypep->isConstrainedArray()) return false;
                if (!etypep->base()->elementp()) return false;
                if (!elementPacks(etypep->base()->elementp())) return false;
                break;
            case VhdlTypeKind::RECORD:
                if (!recordPacks(etypep)) return false;
                break;
            default: return false;
            }
        }
        return true;
    }

    AstNodeDType* lowerRecordType(VhdlType* typep, FileLine* flp) {
        // Packed where it can be, so that a record of vectors is one machine
        // word; unpacked where an element will not fit in one
        const bool packed = recordPacks(typep);
        if (!packed) return unpackedRecordRef(typep, flp);
        AstStructDType* const structp = new AstStructDType{flp, VSigning::UNSIGNED};
        structp->name(typep->name());
        for (VhdlSymbol* const elemp : typep->elements()) {
            structp->addMembersp(new AstMemberDType{flp, elemp->name(), VFlagChildDType{},
                                                    lowerType(elemp->typep(), flp), nullptr});
        }
        return structp;
    }

    // A record that does not pack becomes a C++ structure, which has to be
    // declared once and named -- so it is a typedef of the module, and every
    // use of the record refers to it by name.
    AstNodeDType* unpackedRecordRef(VhdlType* typep, FileLine* flp) {
        const auto found = m_recNames.find(typep);
        if (found != m_recNames.end()) return new AstRefDType{flp, found->second};
        const std::string name
            = "__Vrec" + std::to_string(++m_nameNum) + "__" + typep->name();
        m_recNames.emplace(typep, name);
        AstStructDType* const structp = new AstStructDType{flp, VSigning::NOSIGN};
        structp->name(name);
        // In the one unit every design has, so that a class built for an
        // access or protected type sees the same declaration a module does
        AstPackage* const unitp = m_netlistp->dollarUnitPkgAddp();
        structp->classOrPackagep(unitp);
        for (VhdlSymbol* const elemp : typep->elements()) {
            structp->addMembersp(new AstMemberDType{flp, elemp->name(), VFlagChildDType{},
                                                    lowerType(elemp->typep(), flp), nullptr});
        }
        unitp->addStmtsp(new AstTypedef{flp, name, nullptr, VFlagChildDType{}, structp});
        return new AstRefDType{flp, name};
    }
    std::map<const VhdlType*, std::string> m_recNames;  // Record -> its typedef

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
        AstVar* everp = nullptr;  // Set once an event has happened at all
        AstVar* transp = nullptr;  // Flips on every event
    };
    std::map<const VhdlSymbol*, VhdlShadow> m_shadows;
    AstNode* m_shadowsp = nullptr;  // The blocks that keep them, for the module

    // What 'ACTIVE, 'LAST_ACTIVE, 'TRANSACTION and 'QUIET need. They ask
    // whether a driver assigned, not whether the value changed, and nothing
    // in Verilator's model records that -- an assignment of the value a
    // signal already holds leaves no trace. So a signal any of them names
    // gets a companion bit that every assignment to it flips, and that bit,
    // whose every write is an event, is what the attributes are read from.
    struct VhdlActive final {
        AstVar* togglep = nullptr;  // Flips on each transaction
        AstVar* prevp = nullptr;  // The toggle, one delta behind
        AstVar* timep = nullptr;  // When the previous transaction happened
        AstVar* everp = nullptr;  // Set once a transaction has happened at all
    };
    std::map<const VhdlSymbol*, VhdlActive> m_actives;
    std::set<const VhdlSymbol*> m_needActive;  // Signals an active attribute names
    // The signal "S delayed by T", one per signal and delay it is asked for
    std::map<std::pair<const VhdlSymbol*, int64_t>, AstVar*> m_delayeds;
    // Whether anything in the design delivers a signal transaction at a time
    // of its own -- an "after" clause or a 'DELAYED. Only then can a process
    // whose timeout expires find a signal being updated in the same cycle,
    // and only then does a timed wait have to stand aside for the update.
    bool m_anyAfter = false;

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
        // A port's default value is what an input takes when the instance
        // leaves it open, and what an output's driver starts at (6.5.6.3).
        // An inout may not carry one in the emitted code.
        if (nodep->defaultp() && nodep->mode() != VhdlMode::INOUT
            && nodep->mode() != VhdlMode::LINKAGE) {
            varp->valuep(lowerExpr(nodep->defaultp()));
        }
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
            // A variable of a subprogram is created anew on each call (4.3),
            // so it belongs to the emitted function rather than the module.
            // A variable of a *process* is the opposite: it is elaborated
            // once and keeps its value from one activation to the next
            // (14.7.3), so it must not be turned into a local of the
            // function the process becomes.
            if (m_ftaskp) {
                varp->funcLocal(true);
            } else if (objp->kind() == VhdlObjKind::VARIABLE) {
                varp->lifetime(VLifetime::STATIC_EXPLICIT);
            }
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
            if (objp->kind() == VhdlObjKind::SIGNAL) {
                if (isLogicish(symp->typep())) {
                    // One driver of a logic signal resolves to itself, so the
                    // wiring is only worth building for several
                    if (isResolvedSignal(symp)) makeResolved(symp, varp, flp);
                } else if (symp->resolverp()) {
                    // A function of its own is run over the drivers however
                    // many there are: with one it may still change the value
                    // it is given (6.5.2)
                    makeResolvedByFunction(symp, varp, flp);
                }
            }
            return;
        }
        if (AstVhdlConfigSpec* const cfgp = VN_CAST(nodep, VhdlConfigSpec)) {
            recordBinding(cfgp->specp(), cfgp->bindingp());
            return;
        }
        if (VN_IS(nodep, VhdlPackageInst)) {
            // The copy analysis made contributes types, constants and
            // subprograms, which are emitted where they are used
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
            // An uninstantiated generic subprogram is a template, never
            // called as it stands (4.4)
            if (subp->genericCount() == 0 || subp->isInstance()) lowerSubprogram(subp);
            return;
        }
        if (AstVhdlSubprogInst* const instp = VN_CAST(nodep, VhdlSubprogInst)) {
            // The instance is the copy analysis made, with its generics bound
            if (instp->copyp()) lowerSubprogram(instp->copyp());
            return;
        }
        unsup(flp, std::string{"declaration "} + nodep->typeName());
    }

    // VHDL initializes every object to the leftmost value of its subtype
    // (6.4.2.2), which for STD_LOGIC is 'U'. Two-state, that is X, and
    // --x-initial gives the randomization that most nearly stands in for it.
    // A packed array's default is its element's default repeated. Written as
    // a concatenation, the way a packed record's default is: the parts are
    // constants, so their widths are known before V3Width runs.
    static AstNodeExpr* packedRepeat(AstNodeExpr* elemp, int64_t count, FileLine* flp) {
        if (count <= 0 || count > 4096) return nullptr;
        AstNodeExpr* outp = elemp;
        for (int64_t i = 1; i < count; ++i) {
            outp = new AstConcat{flp, outp, elemp->cloneTree(false)};
        }
        return outp;
    }

    // The default initial value of an object of this type (6.4.2.3). Null
    // where there is nothing worth writing -- an array of logic starts where
    // Verilator starts it anyway -- unless "mustHave", which is how a
    // composite asks, since one element with no value would leave the whole
    // record or array without one.
    AstNodeExpr* defaultValueFor(VhdlType* typep, FileLine* flp, bool mustHave = false) {
        if (!typep || typep->isError()) return nullptr;
        VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::PROTECTED:
            // A shared variable holds an object of the type, made once, at
            // elaboration (5.6.1)
            return new AstNew{flp};
        case VhdlTypeKind::ACCESS:
            // An access value starts out pointing at nothing (5.4.1); a LINE
            // is text here, so it starts out empty
            if (isLineType(basep)) return new AstConst{flp, AstConst::String{}, ""};
            return new AstConst{flp, AstConst::Null{}};
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
            if (!mustHave && enumIsLogic(basep->elementp()->base())) return nullptr;
            if (isStringType(basep)) {
                // A STRING is Verilator's string, whose length is the number
                // of characters it holds. One declared with a constraint must
                // start out that long, or assigning to an element of it
                // writes nothing at all. Every character starts at
                // CHARACTER'LEFT, which is NUL (6.4.2.3).
                if (!typep->isConstrainedArray()) return nullptr;
                const std::vector<VhdlType*>& sindexes = typep->indexTypes();
                if (sindexes.size() != 1 || !sindexes[0] || !sindexes[0]->bounded()) {
                    return nullptr;
                }
                const int64_t len = sindexes[0]->length();
                if (len <= 0 || len > (1 << 20)) return nullptr;
                return new AstConst{flp, AstConst::String{},
                                    std::string(static_cast<size_t>(len), '\0')};
            }
            AstNodeExpr* const elemp = defaultValueFor(basep->elementp(), flp, true);
            if (!elemp) return nullptr;
            AstNodeDType* const dtypep = lowerType(typep, flp);
            if (VN_IS(dtypep, PackArrayDType)) {
                // Packed, so the whole array is one constant: the element's
                // default repeated over every index, however many dimensions
                // it has
                VL_DO_DANGLING(dtypep->deleteTree(), dtypep);
                int64_t count = 1;
                for (const VhdlType* const idxp : typep->indexTypes()) {
                    if (!idxp || !idxp->bounded()) count = 0;
                    if (count) count *= idxp->length();
                }
                AstNodeExpr* const outp = packedRepeat(elemp, count, flp);
                if (!outp) VL_DO_DANGLING(elemp->deleteTree(), elemp);
                return outp;
            }
            // Unpacked, so a sparse initializer with only a default says
            // "every element" without writing them out. A dimension of its
            // own needs one such initializer per level, each carrying the
            // type of that level.
            std::vector<AstNodeDType*> levels;
            for (AstNodeDType* d = dtypep; VN_IS(d, UnpackArrayDType);
                 d = VN_AS(d, UnpackArrayDType)->childDTypep()) {
                levels.push_back(d);
            }
            if (levels.size() != typep->indexTypes().size()) {
                VL_DO_DANGLING(elemp->deleteTree(), elemp);
                VL_DO_DANGLING(dtypep->deleteTree(), dtypep);
                return nullptr;
            }
            AstNodeExpr* outp = elemp;
            for (size_t i = levels.size(); i-- > 0;) {
                AstNodeDType* const lvlp
                    = (i == 0) ? dtypep : levels[i]->cloneTree(false);
                outp = new AstInitArray{flp, lvlp, outp};
            }
            return outp;
        }
        case VhdlTypeKind::RECORD: {
            // A record's default is each element's default (6.4.2.2), written
            // the way the record itself is: one concatenated constant when it
            // packs, member by member when it does not
            const bool packed = recordPacks(typep);
            AstNodeExpr* outp = nullptr;
            AstNode* itemsp = nullptr;
            for (VhdlSymbol* const elemp : typep->elements()) {
                AstNodeExpr* const onep = defaultValueFor(elemp->typep(), flp, true);
                if (!onep) {
                    if (outp) VL_DO_DANGLING(outp->deleteTree(), outp);
                    if (itemsp) VL_DO_DANGLING(itemsp->deleteTree(), itemsp);
                    return nullptr;  // One element with no default gives none
                }
                if (packed) {
                    outp = outp ? new AstConcat{flp, outp, onep} : onep;
                } else {
                    itemsp = AstNode::addNextNull(
                        itemsp, new AstPatMember{flp, onep, new AstText{flp, elemp->name()},
                                                 nullptr});
                }
            }
            if (packed) return outp;
            return itemsp ? new AstPattern{flp, itemsp} : nullptr;
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
        if (AstVhdlAllocator* const p = VN_CAST(nodep, VhdlAllocator)) return lowerAllocator(p);
        if (AstVhdlLiteral* const p = VN_CAST(nodep, VhdlLiteral)) {
            // A null access value is a null handle (9.3.2)
            if (p->kind() == VhdlLitKind::NULLLIT) return new AstConst{flp, AstConst::Null{}};
        }
        if (AstVhdlExternalName* const p = VN_CAST(nodep, VhdlExternalName)) {
            return lowerExternalName(p, false);
        }
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
        if (attr == "pos") {
            if (argp) return argp;
        } else if (attr == "val") {
            // "T'VAL(n)" is a value of T, so it is as wide as T even though
            // the position it is given is an integer
            if (argp) {
                VhdlType* const totypep = nodep->vhdlTypep();
                if (!totypep) return argp;
                return new AstCast{flp, argp, VFlagChildDType{}, lowerType(totypep, flp)};
            }
        } else if (attr == "succ" || attr == "rightof") {
            if (argp) return new AstAdd{flp, argp, new AstConst{flp, AstConst::Signed32{}, 1}};
        } else if (attr == "pred" || attr == "leftof") {
            if (argp) return new AstSub{flp, argp, new AstConst{flp, AstConst::Signed32{}, 1}};
        }
        // The name attributes are about the name, which is always known here
        // even when the analyzer could not fold them -- the prefix may be a
        // design unit, which denotes no value (16.2.7)
        if (attr == "simple_name" || attr == "instance_name" || attr == "path_name") {
            if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
            std::vector<std::string> parts;
            flattenName(nodep->prefixp(), parts);
            const std::string name = parts.empty() ? std::string{"?"} : parts.back();
            return new AstConst{flp, AstConst::String{},
                                attr == "simple_name" ? name : ":" + name + ":"};
        }
        // The signal attributes ask about what has happened to a signal, not
        // about its type (16.2.2)
        if (attr == "event" || attr == "active" || attr == "last_value"
            || attr == "last_event" || attr == "last_active" || attr == "stable"
            || attr == "quiet" || attr == "transaction" || attr == "delayed") {
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
            bool dirKnown = true;
            if (dynamicBounds(typep, dim, flp, leftp, rightp, ascending, &dirKnown)) {
                if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
                if (!dirKnown) {
                    // The bounds arrive with the instance, and so does which
                    // way round they run, so every answer is written to fold
                    // once they do
                    AstNodeExpr* const ascp = ascendingOf(leftp, rightp, flp);
                    if (attr == "ascending") {
                        VL_DO_DANGLING(leftp->deleteTree(), leftp);
                        VL_DO_DANGLING(rightp->deleteTree(), rightp);
                        return ascp;
                    }
                    if (attr == "left" || attr == "right") {
                        AstNodeExpr* const keepp = attr == "left" ? leftp : rightp;
                        AstNodeExpr* const dropp = attr == "left" ? rightp : leftp;
                        VL_DO_DANGLING(dropp->deleteTree(), dropp);
                        VL_DO_DANGLING(ascp->deleteTree(), ascp);
                        return keepp;
                    }
                    if (attr == "length") {
                        return new AstAdd{
                            flp,
                            new AstCond{flp, ascp, new AstSub{flp, rightp->cloneTree(false),
                                                              leftp->cloneTree(false)},
                                        new AstSub{flp, leftp, rightp}},
                            new AstConst{flp, AstConst::Signed32{}, 1}};
                    }
                    // "high" is whichever end is greater, "low" the other
                    const bool wantRightFirst = attr == "high";
                    return new AstCond{flp, ascp, wantRightFirst ? rightp : leftp,
                                       wantRightFirst ? leftp : rightp};
                }
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
        // "t'image(v)" is the text of the value, which is what a report is
        // built from -- but it is an ordinary expression too, and a function
        // may return it (16.2.3)
        if (attr == "image" && nodep->argp()) {
            AstNodeExpr* const valuep = lowerExpr(nodep->argp());
            if (!valuep) return nullptr;
            const VhdlType* const vtypep = nodep->argp()->vhdlTypep();
            if (AstNodeExpr* const namep = enumImage(valuep, vtypep, flp)) return namep;
            return new AstSFormatF{flp, formatFor(vtypep, true), false, valuep};
        }
        // A user-defined attribute stands for the static expression an
        // attribute specification gave the prefix (7.2)
        if (AstNodeExpr* const userp = lowerUserAttr(nodep, attr)) return userp;
        unsup(flp, "attribute '" + attr);
        return nullptr;
    }

    // The value an attribute specification gave the named prefix, lowered in
    // place of the attribute name
    AstNodeExpr* lowerUserAttr(AstVhdlAttrName* nodep, const std::string& attr) {
        VhdlSymbol* symp = nodep->prefixp() ? nodep->prefixp()->vhdlSymp() : nullptr;
        if (!symp) {
            // The prefix of an attribute name need not analyze as an
            // expression, so it may have been left unresolved
            AstVhdlName* const namep = VN_CAST(nodep->prefixp(), VhdlName);
            if (namep) symp = namep->vhdlSymp();
        }
        if (!symp) return nullptr;
        AstNodeVhdlExpr* const valuep = symp->resolved()->userAttr(attr);
        return valuep ? lowerExpr(valuep) : nullptr;
    }

    // What a signal attribute needs kept about one signal, made on first use.
    // One block, sensitive to the signal, records what VHDL's value history
    // would have held: the value one delta back, the value before that, when
    // the last event was, and a bit that flips on each one. The writes are
    // non-blocking, so a process reading them in the same delta sees what was
    // true before the event, which is what the attributes are defined against.
    const VhdlShadow* shadowFor(AstNodeVhdlExpr* prefixp, FileLine* flp) {
        VhdlSymbol* const symp = prefixp->vhdlSymp();
        // Only a signal of the design has a history to keep. A signal-class
        // *parameter* names whatever signal the call passed, which is not
        // known here.
        if (!symp || symp->kind() != VhdlSymKind::SIGNAL) {
            if (!symp || symp->kind() != VhdlSymKind::PORT) return nullptr;
        }
        if (m_ftaskp && symp->kind() == VhdlSymKind::SIGNAL && m_vars.count(symp)
            && m_vars.at(symp)->isFuncLocal()) {
            return nullptr;
        }
        const auto found = m_shadows.find(symp);
        if (found != m_shadows.end()) return &found->second;
        const auto varIt = m_vars.find(symp);
        if (varIt == m_vars.end()) return nullptr;
        AstVar* const sigp = varIt->second;
        VhdlShadow sh;
        const std::string base = "__Vsig" + std::to_string(++m_nameNum) + "__" + symp->name();
        // These belong to the module: a block assigns them, and a
        // non-blocking assignment may not target an automatic variable
        VL_RESTORER(m_ftaskp);
        m_ftaskp = nullptr;
        AstNodeDType* const valDTypep = lowerType(symp->typep(), flp);
        sh.prevp = newTempVar(base + "__prev", valDTypep, flp);
        sh.lastp = newTempVar(base + "__last", lowerType(symp->typep(), flp), flp);
        sh.timep = newTempVar(base + "__time",
                              new AstBasicDType{flp, VBasicDTypeKwd::LOGIC, VSigning::SIGNED,
                                                VNumRange{63, 0}, -1},
                              flp);
        sh.everp = newTempVar(base + "__ever", newLogicDType(flp, 1), flp);
        sh.transp = newTempVar(base + "__trans", newLogicDType(flp, 1), flp);
        // A signal starts out equal to its own past, so nothing reads as an
        // event before anything has happened. That past is the value the
        // signal itself starts at, which is its own initial value where it
        // was given one, and the type's default where it was not (6.4.2.3).
        AstNodeExpr* const startp
            = sigp->valuep() ? VN_CAST(sigp->valuep()->cloneTree(false), NodeExpr) : nullptr;
        sh.prevp->valuep(startp ? startp : defaultValueFor(symp->typep(), flp));
        sh.lastp->valuep(startp ? startp->cloneTree(false)
                                : defaultValueFor(symp->typep(), flp));
        sh.everp->valuep(new AstConst{flp, AstConst::BitFalse{}});
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
                        flp, new AstVarRef{flp, sh.everp, VAccess::WRITE},
                        new AstConst{flp, AstConst::BitTrue{}}}));
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(new AstAssignDly{
                        flp, new AstVarRef{flp, sh.transp, VAccess::WRITE},
                        new AstNot{flp, new AstVarRef{flp, sh.transp, VAccess::READ}}}));
        AstSenTree* const senp = new AstSenTree{
            flp, senItemFor(new AstVarRef{flp, sigp, VAccess::READ}, isOneBitVar(sigp), flp)};
        AstAlways* const alwaysp = new AstAlways{flp, VAlwaysKwd::ALWAYS, senp, stmtsp};
        m_shadowsp = AstNode::addNextNull(m_shadowsp, static_cast<AstNode*>(alwaysp));
        return &m_shadows.emplace(symp, sh).first->second;
    }

    // Which signals need their transactions recorded. Collected before any
    // statement is lowered, because the assignment that marks a transaction
    // is lowered long before the attribute that reads it comes into view.
    void collectActive(AstNode* nodep) {
        if (!nodep) return;
        nodep->foreachAndNext([this](AstVhdlAttrName* attrp) {
            const std::string& attr = attrp->name();
            if (attr != "active" && attr != "last_active" && attr != "transaction"
                && attr != "quiet") {
                return;
            }
            if (const VhdlSymbol* const symp = rootSymOf(attrp->prefixp())) {
                m_needActive.insert(symp->resolved());
            }
        });
    }

    // The companion bit and its history, for each signal an active attribute
    // named. Made once the declarations are lowered, so that the signal's own
    // variable exists, and before any statement is, so that every assignment
    // can mark itself.
    void makeActives(FileLine* flp) {
        for (const VhdlSymbol* const symp : m_needActive) {
            if (m_actives.count(symp)) continue;
            const auto varIt = m_vars.find(symp);
            if (varIt == m_vars.end()) continue;
            VL_RESTORER(m_ftaskp);
            m_ftaskp = nullptr;
            VhdlActive ac;
            const std::string base = "__Vact" + std::to_string(++m_nameNum) + "__" + symp->name();
            // Every driver of the signal marks the same bit, and a concurrent
            // assignment marks it from two blocks -- once to start and once
            // to react -- so it is written from more than one place by
            // construction, which is not a finding about the design
            FileLine* const mflp = new FileLine{flp};
            mflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            ac.togglep = newTempVar(base + "__trans", newLogicDType(mflp, 1), mflp);
            ac.prevp = newTempVar(base + "__prev", newLogicDType(flp, 1), flp);
            ac.timep = newTempVar(base + "__time",
                                  new AstBasicDType{flp, VBasicDTypeKwd::LOGIC, VSigning::SIGNED,
                                                    VNumRange{63, 0}, -1},
                                  flp);
            ac.everp = newTempVar(base + "__ever", newLogicDType(flp, 1), flp);
            ac.togglep->valuep(new AstConst{flp, AstConst::BitFalse{}});
            ac.prevp->valuep(new AstConst{flp, AstConst::BitFalse{}});
            ac.everp->valuep(new AstConst{flp, AstConst::BitFalse{}});
            AstNode* stmtsp = new AstAssignDly{flp,
                                               new AstVarRef{flp, ac.prevp, VAccess::WRITE},
                                               new AstVarRef{flp, ac.togglep, VAccess::READ}};
            stmtsp = AstNode::addNext(
                stmtsp, static_cast<AstNode*>(new AstAssignDly{
                            flp, new AstVarRef{flp, ac.timep, VAccess::WRITE}, nowOf(flp)}));
            stmtsp = AstNode::addNext(
                stmtsp, static_cast<AstNode*>(new AstAssignDly{
                            flp, new AstVarRef{flp, ac.everp, VAccess::WRITE},
                            new AstConst{flp, AstConst::BitTrue{}}}));
            AstSenTree* const senp = new AstSenTree{
                flp, senItemFor(new AstVarRef{flp, ac.togglep, VAccess::READ}, true, flp)};
            m_shadowsp = AstNode::addNextNull(
                m_shadowsp,
                static_cast<AstNode*>(new AstAlways{flp, VAlwaysKwd::ALWAYS, senp, stmtsp}));
            m_actives.emplace(symp, ac);
        }
    }

    const VhdlActive* activeFor(AstNodeVhdlExpr* prefixp) {
        const VhdlSymbol* const symp = rootSymOf(prefixp);
        if (!symp) return nullptr;
        const auto found = m_actives.find(symp->resolved());
        return found == m_actives.end() ? nullptr : &found->second;
    }

    // The extra assignment that says a driver of this signal has assigned,
    // whatever it assigned. It carries the same delay as the assignment it
    // accompanies, since a transaction happens when the value is delivered.
    AstNode* transactionMark(const AstNode* targetp, AstNode* delayp, bool sequential,
                             const AstNode* readsp, FileLine* flp) {
        if (m_actives.empty()) return nullptr;
        const VhdlSymbol* const symp = rootSymOf(targetp);
        if (!symp) return nullptr;
        const auto found = m_actives.find(symp->resolved());
        if (found == m_actives.end()) return nullptr;
        AstVar* const tp = found->second.togglep;
        // Every driver of the signal marks the same bit, and a concurrent
        // assignment marks it from two blocks -- once to start and once to
        // react -- so it is written from more than one place by construction
        FileLine* const mflp = new FileLine{flp};
        mflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
        mflp->warnOff(V3ErrorCode::COMBDLY, true);
        AstNode* const dp = delayp ? delayp->cloneTree(false) : nullptr;
        AstNode* const flipp
            = new AstAssignDly{mflp, new AstVarRef{mflp, tp, VAccess::WRITE},
                               new AstNot{mflp, new AstVarRef{mflp, tp, VAccess::READ}}, dp};
        if (sequential) return flipp;
        // A concurrent assignment is a process sensitive to what it reads
        // (11.6), so the times it makes a transaction are the times that
        // process runs: once at time zero, and again on every event on a
        // signal it reads
        AstNode* const outp = new AstInitial{mflp, flipp->cloneTree(false)};
        AstSenTree* const senp = readsp ? sensitivityOfExpr(VN_CAST(readsp, NodeExpr), mflp)
                                        : nullptr;
        if (!senp) {
            VL_DO_DANGLING(flipp->deleteTree(), flipp);
            return outp;  // Nothing to wake it: it runs at time zero only
        }
        return AstNode::addNext(
            outp, static_cast<AstNode*>(new AstAlways{mflp, VAlwaysKwd::ALWAYS, senp, flipp}));
    }

    // "s /= s'past value", which is true for exactly the delta in which the
    // signal changed
    AstNodeExpr* eventOf(AstNodeVhdlExpr* prefixp, const VhdlShadow* shp, FileLine* flp) {
        AstNodeExpr* const nowp = lowerExpr(prefixp);
        if (!nowp) return nullptr;
        return new AstNeq{flp, nowp, new AstVarRef{flp, shp->prevp, VAccess::READ}};
    }

    // "now"
    AstNodeExpr* nowOf(FileLine* flp) {
        return new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}};
    }

    // How long ago something last happened, as the attributes of 16.2 define
    // it: nothing at all if it is happening in this cycle, and TIME'HIGH if
    // it has never happened
    AstNodeExpr* sinceOf(AstNodeExpr* happeningp, AstVar* timep, AstVar* everp, FileLine* flp) {
        AstNodeExpr* const sincep = new AstSub{flp, nowOf(flp),
                                               new AstVarRef{flp, timep, VAccess::READ}};
        AstNodeExpr* const everSincep
            = new AstCond{flp, new AstVarRef{flp, everp, VAccess::READ}, sincep,
                          new AstConst{flp, AstConst::Unsized64{},
                                       static_cast<uint64_t>(INT64_MAX)}};
        return new AstCond{flp, happeningp, new AstConst{flp, AstConst::Unsized64{}, 0},
                           everSincep};
    }

    // "no such thing has happened for T", the shape 'STABLE(T) and 'QUIET(T)
    // share. Before the first one has happened at all the answer is yes,
    // however long T is.
    AstNodeExpr* steadyForOf(AstNodeExpr* happeningp, AstVar* timep, AstVar* everp,
                             AstNodeExpr* argp, FileLine* flp) {
        AstNodeExpr* const steadyp = new AstLogNot{flp, happeningp};
        if (!argp) return steadyp;
        AstNodeExpr* const sincep = new AstSub{flp, nowOf(flp),
                                               new AstVarRef{flp, timep, VAccess::READ}};
        AstNodeExpr* const longEnoughp
            = new AstLogOr{flp, new AstLogNot{flp, new AstVarRef{flp, everp, VAccess::READ}},
                           new AstGte{flp, sincep, argp}};
        return new AstLogAnd{flp, steadyp, longEnoughp};
    }

    // "the toggle /= its past value", true for exactly the cycle in which a
    // driver of the signal assigned
    AstNodeExpr* activeOf(const VhdlActive* acp, FileLine* flp) {
        return new AstNeq{flp, new AstVarRef{flp, acp->togglep, VAccess::READ},
                          new AstVarRef{flp, acp->prevp, VAccess::READ}};
    }

    // "S'DELAYED(T)", a signal that is S delayed by T (16.2.4). Delayed by
    // nothing at all is the value one delta back, which is what the event
    // shadow already keeps; delayed by a time is a signal of its own, driven
    // from S through that delay. One per signal and delay, since a design
    // may ask for several.
    AstNodeExpr* delayedOf(AstNodeVhdlExpr* prefixp, const VhdlShadow* shp, AstNodeExpr* argp,
                           FileLine* flp) {
        if (!argp) return new AstVarRef{flp, shp->prevp, VAccess::READ};
        AstConst* const constp = VN_CAST(argp, Const);
        if (!constp) {
            unsup(flp, "'delayed by a time that is not static");
            return nullptr;
        }
        const int64_t ticks = static_cast<int64_t>(constp->toUQuad());
        if (ticks == 0) {
            VL_DO_DANGLING(argp->deleteTree(), argp);
            return new AstVarRef{flp, shp->prevp, VAccess::READ};
        }
        if (!v3Global.opt.timing().isTrue()) {
            unsup(flp, "'delayed by a time without --timing");
            return nullptr;
        }
        VhdlSymbol* const symp = prefixp->vhdlSymp();
        const auto varIt = symp ? m_vars.find(symp->resolved()) : m_vars.end();
        if (varIt == m_vars.end()) return nullptr;
        AstVar* const sigp = varIt->second;
        const auto key = std::make_pair(static_cast<const VhdlSymbol*>(symp->resolved()), ticks);
        const auto found = m_delayeds.find(key);
        if (found != m_delayeds.end()) {
            VL_DO_DANGLING(argp->deleteTree(), argp);
            return new AstVarRef{flp, found->second, VAccess::READ};
        }
        VL_RESTORER(m_ftaskp);
        m_ftaskp = nullptr;
        AstVar* const dlyp = newTempVar("__Vdly" + std::to_string(++m_nameNum) + "__"
                                            + symp->name(),
                                        lowerType(symp->typep(), flp), flp);
        // It starts out equal to S, so that nothing reads a delayed value
        // that S never had (16.2.4)
        if (sigp->valuep()) {
            dlyp->valuep(VN_AS(sigp->valuep()->cloneTree(false), NodeExpr));
        } else if (AstNodeExpr* const defp = defaultValueFor(symp->typep(), flp)) {
            dlyp->valuep(defp);
        }
        AstNode* const bodyp = new AstAssignDly{flp, new AstVarRef{flp, dlyp, VAccess::WRITE},
                                                new AstVarRef{flp, sigp, VAccess::READ},
                                                new AstDelay{flp, argp, false}};
        AstSenTree* const senp = new AstSenTree{
            flp, senItemFor(new AstVarRef{flp, sigp, VAccess::READ}, isOneBitVar(sigp), flp)};
        m_shadowsp = AstNode::addNextNull(
            m_shadowsp,
            static_cast<AstNode*>(new AstAlways{flp, VAlwaysKwd::ALWAYS, senp, bodyp}));
        m_delayeds.emplace(key, dlyp);
        return new AstVarRef{flp, dlyp, VAccess::READ};
    }

    // The signal attributes of 16.2.4, answered from what shadowFor and
    // makeActive keep
    AstNodeExpr* lowerSignalAttr(AstVhdlAttrName* nodep, const std::string& attr,
                                 AstNodeExpr* argp, FileLine* flp) {
        if (attr == "transaction" || attr == "active" || attr == "last_active"
            || attr == "quiet") {
            const VhdlActive* const acp = activeFor(nodep->prefixp());
            if (!acp) {
                // Nothing marks this signal's transactions -- it is a signal
                // no assignment here writes, such as a port driven from
                // outside. Its events are all this side can see, and every
                // event is a transaction, so answer from those.
                const VhdlShadow* const evp = shadowFor(nodep->prefixp(), flp);
                if (!evp) return nullptr;
                if (attr == "transaction") {
                    return new AstVarRef{flp, evp->transp, VAccess::READ};
                }
                AstNodeExpr* const happeningp = eventOf(nodep->prefixp(), evp, flp);
                if (!happeningp) return nullptr;
                if (attr == "active") return happeningp;
                if (attr == "last_active") {
                    return sinceOf(happeningp, evp->timep, evp->everp, flp);
                }
                return steadyForOf(happeningp, evp->timep, evp->everp, argp, flp);
            }
            if (attr == "transaction") {
                return new AstVarRef{flp, acp->togglep, VAccess::READ};
            }
            if (attr == "active") return activeOf(acp, flp);
            if (attr == "last_active") {
                return sinceOf(activeOf(acp, flp), acp->timep, acp->everp, flp);
            }
            return steadyForOf(activeOf(acp, flp), acp->timep, acp->everp, argp, flp);
        }
        const VhdlShadow* const shp = shadowFor(nodep->prefixp(), flp);
        if (!shp) return nullptr;
        if (attr == "delayed") return delayedOf(nodep->prefixp(), shp, argp, flp);
        if (attr == "last_value") {
            // During the delta in which it changed, the value before the
            // change is what prev still holds
            AstNodeExpr* const evp = eventOf(nodep->prefixp(), shp, flp);
            if (!evp) return nullptr;
            return new AstCond{flp, evp, new AstVarRef{flp, shp->prevp, VAccess::READ},
                               new AstVarRef{flp, shp->lastp, VAccess::READ}};
        }
        if (attr == "event") return eventOf(nodep->prefixp(), shp, flp);
        if (attr == "last_event") {
            AstNodeExpr* const evp = eventOf(nodep->prefixp(), shp, flp);
            if (!evp) return nullptr;
            return sinceOf(evp, shp->timep, shp->everp, flp);
        }
        if (attr == "stable") {
            AstNodeExpr* const evp = eventOf(nodep->prefixp(), shp, flp);
            if (!evp) return nullptr;
            return steadyForOf(evp, shp->timep, shp->everp, argp, flp);
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
        // An alias is another name for what it denotes (6.6), so it lowers to
        // that. An alias that renumbers an array -- "alias b : bit_vector(3
        // downto 0) is v(7 downto 4)" -- would need its indices translated,
        // which is not done here.
        if (symp->kind() == VhdlSymKind::ALIAS) {
            AstVhdlAliasDecl* const declp = VN_CAST(symp->declp(), VhdlAliasDecl);
            if (declp && declp->targetp()) return lowerExpr(declp->targetp());
            if (symp->aliasp()) return lowerRef(symp->aliasp(), name, flp);
        }
        const auto bit = m_bindExprs.find(symp);
        if (bit != m_bindExprs.end()) return lowerExpr(bit->second);
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
                addCaptureArgs(refp, symp, flp);
                return refp;
            }
        }
        // An object declared in a package is not emitted with the package --
        // there is no package in the generated code -- so it is emitted into
        // the module that reads it, on first use
        if (symp->isObject() && !symp->predefined()) {
            if (AstVhdlObjectDecl* const objp = VN_CAST(symp->declp(), VhdlObjectDecl)) {
                AstVar* const varp = newVar(symp, VVarType::VAR, flp);
                if (objp->defaultp()) {
                    varp->valuep(lowerExpr(objp->defaultp()));
                } else {
                    varp->valuep(defaultValueFor(symp->typep(), flp));
                }
                if (m_protClassp) {
                    m_protClassp->addStmtsp(varp);
                } else {
                    m_stmtsp = AstNode::addNextNull(m_stmtsp, varp);
                }
                return new AstVarRef{flp, varp, VAccess::READ};
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

    // "new t" or "new t'(v)": an object of the designated subtype, which
    // here is an instance of the class the access type became (9.3.7)
    AstNodeExpr* lowerAllocator(AstVhdlAllocator* nodep) {
        FileLine* const flp = nodep->fileline();
        AstNew* const newp = new AstNew{flp};
        AstNodeExpr* valuep = nullptr;
        if (nodep->valuep()) {
            valuep = lowerExpr(nodep->valuep());
        } else if (nodep->subtypep()) {
            // "new t" gives the designated subtype's default (9.3.7)
            valuep = defaultValueFor(nodep->subtypep()->vhdlTypep(), flp);
        }
        if (!valuep) {
            unsup(flp, "allocator for a subtype with no default value");
            VL_DO_DANGLING(newp->deleteTree(), newp);
            return nullptr;
        }
        newp->addArgsp(new AstArg{flp, "", valuep});
        return newp;
    }

    // How an alias numbers its elements against what it denotes (6.6.2).
    // "alias b : bit_vector(3 downto 0) is v(7 downto 4)" makes b(0) mean
    // v(4), so an index into the alias is shifted onto the target's own
    // range. Returns false when there is nothing to translate.
    static bool aliasMapping(const VhdlSymbol* symp, const AstVhdlAliasDecl* declp,
                             int64_t& fromLeft, int64_t& toLeft, int& step) {
        if (aliasKeepsRange(symp, declp)) return false;
        const VhdlType* const atp = symp->typep();
        const VhdlType* const ttp = declp->targetp()->vhdlTypep();
        if (!atp || !ttp || !atp->isConstrainedArray() || !ttp->isConstrainedArray()) {
            return false;
        }
        if (atp->dimensions() != 1 || ttp->dimensions() != 1) return false;
        const VhdlType* const aidx = atp->indexTypes()[0];
        const VhdlType* const tidx = ttp->indexTypes()[0];
        if (!aidx || !tidx || aidx->length() != tidx->length()) return false;
        fromLeft = aidx->left();
        toLeft = tidx->left();
        step = (aidx->ascending() == tidx->ascending()) ? 1 : -1;
        return true;
    }

    // "toLeft + step * (idxp - fromLeft)", folded where it can be
    static AstNodeExpr* shiftIndex(AstNodeExpr* idxp, int64_t fromLeft, int64_t toLeft, int step,
                                   FileLine* flp) {
        if (const AstConst* const constp = VN_CAST(idxp, Const)) {
            const int64_t k = static_cast<int64_t>(constp->toSInt());
            VL_DO_DANGLING(idxp->deleteTree(), idxp);
            return new AstConst{flp, AstConst::Signed32{},
                                static_cast<int32_t>(toLeft + step * (k - fromLeft))};
        }
        AstNodeExpr* offsetp
            = new AstSub{flp, idxp, new AstConst{flp, AstConst::Signed32{},
                                                 static_cast<int32_t>(fromLeft)}};
        if (step < 0) offsetp = new AstNegate{flp, offsetp};
        return new AstAdd{flp, new AstConst{flp, AstConst::Signed32{},
                                            static_cast<int32_t>(toLeft)},
                          offsetp};
    }

    // Whether an alias indexes the same way what it denotes does
    static bool aliasKeepsRange(const VhdlSymbol* symp, const AstVhdlAliasDecl* declp) {
        const VhdlType* const atp = symp->typep();
        if (!atp || atp->base()->kind() != VhdlTypeKind::ARRAY) return true;
        if (!atp->isConstrainedArray() || atp->dimensions() != 1) return true;
        const VhdlType* const ttp = declp->targetp()->vhdlTypep();
        if (!ttp || !ttp->isConstrainedArray() || ttp->dimensions() != 1) return true;
        return atp->indexTypes()[0]->left() == ttp->indexTypes()[0]->left()
               && atp->indexTypes()[0]->right() == ttp->indexTypes()[0]->right();
    }

    // An index into an alias, put onto the range of what it denotes
    static AstNodeExpr* aliasShift(VhdlSymbol* symp, AstNodeExpr* idxp, FileLine* flp) {
        if (!symp || symp->kind() != VhdlSymKind::ALIAS || !idxp) return idxp;
        const AstVhdlAliasDecl* const declp = VN_CAST(symp->declp(), VhdlAliasDecl);
        if (!declp || !declp->targetp()) return idxp;
        int64_t fromLeft = 0;
        int64_t toLeft = 0;
        int step = 1;
        if (!aliasMapping(symp, declp, fromLeft, toLeft, step)) return idxp;
        return shiftIndex(idxp, fromLeft, toLeft, step, flp);
    }

    // "maximum(v)" or "minimum(v)" over a whole array (5.3.2.4)
    AstNodeExpr* lowerArrayExtreme(AstNodeExpr* arrp, const VhdlType* typep, bool wantMax,
                                   FileLine* flp) {
        const VhdlType* const idxp = indexSubtype(typep, 0);
        if (!typep || typep->dimensions() != 1 || !idxp || !idxp->bounded()) {
            VL_DO_DANGLING(arrp->deleteTree(), arrp);
            unsup(flp, std::string{wantMax ? "maximum" : "minimum"}
                           + " over an array whose length is not fixed here");
            return nullptr;
        }
        const int64_t left = idxp->left();
        const int64_t right = idxp->right();
        const int64_t step = idxp->ascending() ? 1 : -1;
        const int64_t count = (idxp->ascending() ? right - left : left - right) + 1;
        if (count <= 0) {
            // A null array has no elements, so the answer is the element
            // type's own limit -- which needs a value this does not have
            VL_DO_DANGLING(arrp->deleteTree(), arrp);
            unsup(flp, std::string{wantMax ? "maximum" : "minimum"} + " over a null array");
            return nullptr;
        }
        const bool signd = typep->elementp() && isSignedType(typep->elementp());
        AstNodeExpr* bestp = nullptr;
        for (int64_t i = 0; i < count; ++i) {
            AstNodeExpr* const elemp = new AstSelBit{
                flp, arrp->cloneTree(false),
                new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(left + i * step)}};
            if (!bestp) {
                bestp = elemp;
                continue;
            }
            AstNodeExpr* const testp
                = signd ? static_cast<AstNodeExpr*>(new AstGtS{flp, elemp->cloneTree(false),
                                                               bestp->cloneTree(false)})
                        : static_cast<AstNodeExpr*>(new AstGt{flp, elemp->cloneTree(false),
                                                              bestp->cloneTree(false)});
            bestp = wantMax ? new AstCond{flp, testp, elemp, bestp}
                            : new AstCond{flp, testp, bestp, elemp};
        }
        VL_DO_DANGLING(arrp->deleteTree(), arrp);
        return bestp;
    }

    // "\"+\"(a, b)" means the same as "a + b" (4.5.3). An operator has no body
    // to emit, so the call is rebuilt as the operation it names.
    AstNodeExpr* lowerOperatorCall(const VhdlSymbol* symp, AstVhdlCall* nodep, FileLine* flp) {
        static const std::map<std::string, VhdlBinaryOp> BINARY{
            {"and", VhdlBinaryOp::AND},      {"or", VhdlBinaryOp::OR},
            {"nand", VhdlBinaryOp::NAND},    {"nor", VhdlBinaryOp::NOR},
            {"xor", VhdlBinaryOp::XOR},      {"xnor", VhdlBinaryOp::XNOR},
            {"=", VhdlBinaryOp::EQ},         {"/=", VhdlBinaryOp::NE},
            {"<", VhdlBinaryOp::LT},         {"<=", VhdlBinaryOp::LE},
            {">", VhdlBinaryOp::GT},         {">=", VhdlBinaryOp::GE},
            {"?=", VhdlBinaryOp::MATCH_EQ},  {"?/=", VhdlBinaryOp::MATCH_NE},
            {"?<", VhdlBinaryOp::MATCH_LT},  {"?<=", VhdlBinaryOp::MATCH_LE},
            {"?>", VhdlBinaryOp::MATCH_GT},  {"?>=", VhdlBinaryOp::MATCH_GE},
            {"sll", VhdlBinaryOp::SLL},      {"srl", VhdlBinaryOp::SRL},
            {"sla", VhdlBinaryOp::SLA},      {"sra", VhdlBinaryOp::SRA},
            {"rol", VhdlBinaryOp::ROL},      {"ror", VhdlBinaryOp::ROR},
            {"+", VhdlBinaryOp::ADD},        {"-", VhdlBinaryOp::SUB},
            {"&", VhdlBinaryOp::CONCAT},     {"*", VhdlBinaryOp::MUL},
            {"/", VhdlBinaryOp::DIV},        {"mod", VhdlBinaryOp::MOD},
            {"rem", VhdlBinaryOp::REM},      {"**", VhdlBinaryOp::POW}};
        static const std::map<std::string, VhdlUnaryOp> UNARY{
            {"+", VhdlUnaryOp::IDENTITY}, {"-", VhdlUnaryOp::NEGATE},
            {"abs", VhdlUnaryOp::ABS},    {"not", VhdlUnaryOp::NOT}};
        // The name may be an alias -- a generic bound to an operator is one
        const VhdlSymbol* const realp = symp->resolved();
        if (!realp->predefined()) return nullptr;
        std::vector<AstNodeVhdlExpr*> args;
        for (AstNode* p = nodep->argsp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            if (!asp || !asp->actualp()) return nullptr;
            args.push_back(asp->actualp());
        }
        if (args.size() == 2) {
            const auto it = BINARY.find(realp->name());
            if (it == BINARY.end()) return nullptr;
            AstVhdlBinary* const tmpp = new AstVhdlBinary{
                flp, it->second, args[0]->cloneTree(false), args[1]->cloneTree(false)};
            tmpp->vhdlTypep(nodep->vhdlTypep());
            AstNodeExpr* const outp = lowerBinary(tmpp);
            VL_DO_DANGLING(tmpp->deleteTree(), tmpp);
            return outp;
        }
        if (args.size() == 1) {
            const auto it = UNARY.find(realp->name());
            if (it == UNARY.end()) return nullptr;
            AstVhdlUnary* const tmpp
                = new AstVhdlUnary{flp, it->second, args[0]->cloneTree(false)};
            tmpp->vhdlTypep(nodep->vhdlTypep());
            AstNodeExpr* const outp = lowerUnary(tmpp);
            VL_DO_DANGLING(tmpp->deleteTree(), tmpp);
            return outp;
        }
        return nullptr;
    }

    // The left side of an assignment, which may be an external name (8.7)
    AstNodeExpr* lowerTarget(AstNodeVhdlExpr* nodep) {
        if (AstVhdlExternalName* const extp = VN_CAST(nodep, VhdlExternalName)) {
            return lowerExternalName(extp, true);
        }
        return lowerExpr(nodep);
    }

    // "<< signal .top.uut.x : natural >>" names an object somewhere else in
    // the design (8.7). Verilator resolves a dotted name against the
    // hierarchy already, so the path becomes one.
    AstNodeExpr* lowerExternalName(AstVhdlExternalName* nodep, bool write) {
        FileLine* const flp = nodep->fileline();
        if (nodep->package()) {
            unsup(flp, "external name of an object in a package");
            return nullptr;
        }
        if (nodep->upLevels() > 0) {
            unsup(flp, "external name with a relative pathname starting at '^'");
            return nullptr;
        }
        std::vector<std::string> parts;
        for (AstNode* p = nodep->pathp(); p; p = p->nextp()) {
            std::vector<std::string> one;
            if (!flattenName(VN_AS(p, NodeVhdlExpr), one) || one.empty()) {
                unsup(flp, "external name whose pathname is not a list of names");
                return nullptr;
            }
            for (const std::string& part : one) parts.push_back(part);
        }
        if (parts.empty()) {
            unsup(flp, "external name with no pathname");
            return nullptr;
        }
        const std::string name = parts.back();
        std::string dotted;
        // An absolute pathname starts at the root of the design, which is the
        // top module; a relative one starts where it is written
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            if (!dotted.empty()) dotted += ".";
            dotted += parts[i];
        }
        AstVarXRef* const refp = new AstVarXRef{
            flp, name, dotted, write ? VAccess::WRITE : VAccess::READ};
        return refp;
    }

    AstNodeExpr* lowerSelected(AstVhdlSelected* nodep) {
        FileLine* const flp = nodep->fileline();
        // "p.all" reads what an access value points at (8.3)
        if (nodep->isAll()) {
            AstNodeExpr* const fromp = lowerExpr(nodep->prefixp());
            return fromp ? dereference(fromp, flp) : nullptr;
        }
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (symp && symp->kind() == VhdlSymKind::RECORD_ELEMENT) {
            AstNodeExpr* fromp = lowerExpr(nodep->prefixp());
            if (!fromp) return nullptr;
            fromp = derefIfAccess(fromp, nodep->prefixp(), flp);
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

    // An operator a design declares itself is an ordinary function whose name
    // happens to be an operator symbol (4.5.2), so it is called like one
    AstNodeExpr* lowerUserOperator(VhdlSymbol* symp,
                                   const std::vector<AstNodeVhdlExpr*>& args, FileLine* flp) {
        if (!symp) return nullptr;
        auto it = m_funcs.find(symp);
        if (it == m_funcs.end() && emitOnDemand(symp)) it = m_funcs.find(symp);
        if (it == m_funcs.end() || !VN_IS(it->second, Func)) return nullptr;
        AstFuncRef* const refp = new AstFuncRef{flp, it->second->name(), nullptr};
        refp->taskp(it->second);
        for (AstNodeVhdlExpr* const argp : args) {
            AstNodeExpr* const onep = lowerExpr(argp);
            if (!onep) {
                VL_DO_DANGLING(refp->deleteTree(), refp);
                return nullptr;
            }
            refp->addArgsp(new AstArg{flp, "", onep});
        }
        return refp;
    }

    AstNodeExpr* lowerUnary(AstVhdlUnary* nodep) {
        FileLine* const flp = nodep->fileline();
        const VhdlSymbol* const symp = nodep->vhdlSymp();
        VhdlType* const otypep = nodep->lhsp()->vhdlTypep();
        // std_logic_1164 declares "not" for STD_ULOGIC itself, so an operator
        // over a logic type is one of the intrinsics even when it is explicit
        if (symp && !symp->predefined() && !isIeeeOperator(symp, otypep, nullptr)) {
            if (AstNodeExpr* const outp
                = lowerUserOperator(const_cast<VhdlSymbol*>(symp), {nodep->lhsp()}, flp)) {
                return outp;
            }
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
            if (AstNodeExpr* const outp = lowerUserOperator(
                    const_cast<VhdlSymbol*>(symp), {nodep->lhsp(), nodep->rhsp()}, flp)) {
                return outp;
            }
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
        // A physical value scaled by a real is defined to be computed in real
        // arithmetic and converted back (9.2.7), so the conversion is written
        // out rather than left to be inferred and reported
        const auto isRealish = [](const VhdlType* typep) {
            return typep
                   && (typep->base()->kind() == VhdlTypeKind::FLOATING
                       || typep->base()->kind() == VhdlTypeKind::UNIVERSAL_REAL);
        };
        const auto isPhysical = [](const VhdlType* typep) {
            return typep && typep->base()->kind() == VhdlTypeKind::PHYSICAL;
        };
        if ((isPhysical(ltypep) && isRealish(rtypep))
            || (isRealish(ltypep) && isPhysical(rtypep))) {
            if (nodep->op() != VhdlBinaryOp::MUL && nodep->op() != VhdlBinaryOp::DIV) {
                // Nothing else pairs a physical value with a real (9.2.7)
            } else {
            const bool leftIsPhysical = isPhysical(ltypep);
            FileLine* const cflp = new FileLine{flp};
            cflp->warnOff(V3ErrorCode::REALCVT, true);
            cflp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
            AstNodeExpr* const lrealp
                = leftIsPhysical ? new AstIToRD{cflp, lhsp} : lhsp;
            AstNodeExpr* const rrealp
                = leftIsPhysical ? rhsp : new AstIToRD{cflp, rhsp};
            AstNodeExpr* const valuep
                = nodep->op() == VhdlBinaryOp::MUL
                      ? static_cast<AstNodeExpr*>(new AstMulD{cflp, lrealp, rrealp})
                      : static_cast<AstNodeExpr*>(new AstDivD{cflp, lrealp, rrealp});
            AstRToIRoundS* const outp = new AstRToIRoundS{cflp, valuep};
            // A physical value is counted in 64 bits, not in an int
            outp->dtypeSetLogicSized(64, VSigning::SIGNED);
            return outp;
            }
        }
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
        if (lw && rw && lw != rw) {
            // "+" and friends of numeric_std take operands of any two lengths
            // and give a result as long as the longer, each operand resized
            // to it first (16.5). Only arithmetic does: array equality is
            // defined for every array type and is false between lengths that
            // differ, so it must not be made to agree here.
            switch (nodep->op()) {
            case VhdlBinaryOp::ADD:
            case VhdlBinaryOp::SUB:
            case VhdlBinaryOp::MUL:
            case VhdlBinaryOp::DIV:
            case VhdlBinaryOp::MOD:
            case VhdlBinaryOp::REM: {
                const bool signd = isSignedType(ltypep) || isSignedType(rtypep);
                if (lw < rw) {
                    lhsp = sizedTo(signd ? new AstSigned{flp, lhsp} : lhsp, rw, flp);
                } else {
                    rhsp = sizedTo(signd ? new AstSigned{flp, rhsp} : rhsp, lw, flp);
                }
                return;
            }
            default: break;
            }
        }
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
            = (objSymp && (objSymp->isObject() || objSymp->kind() == VhdlSymKind::ALIAS))
              || (!objSymp && prefixTypep
                  && (prefixTypep->base()->kind() == VhdlTypeKind::ARRAY
                      || prefixTypep->base()->kind() == VhdlTypeKind::ACCESS));
        if (indexing) {
            AstNodeExpr* fromp = lowerExpr(nodep->prefixp());
            if (!fromp) return nullptr;
            fromp = derefIfAccess(fromp, nodep->prefixp(), flp);
            if (AstVhdlRange* const rp = VN_CAST(nodep->argsp(), VhdlRange)) {
                if (!rp->leftp() || !rp->rightp()) {
                    unsup(flp, "slice by an attribute range");
                    return nullptr;
                }
                AstNodeExpr* leftp = lowerExpr(rp->leftp());
                AstNodeExpr* rightp = lowerExpr(rp->rightp());
                if (!leftp || !rightp) {
                    VL_DO_DANGLING(fromp->deleteTree(), fromp);
                    return nullptr;
                }
                leftp = aliasShift(objSymp, leftp, flp);
                rightp = aliasShift(objSymp, rightp, flp);
                if (prefixTypep && isStringType(prefixTypep)) {
                    // A slice of a STRING is a substring: Verilator's string
                    // is characters, not bits, so a part select of it means
                    // nothing
                    return new AstSubstrN{flp, fromp, stringIndex(prefixTypep, leftp, flp),
                                          stringIndex(prefixTypep, rightp, flp)};
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
                AstNodeExpr* indexp = lowerExpr(asp->actualp());
                if (!indexp) {
                    VL_DO_DANGLING(outp->deleteTree(), outp);
                    return nullptr;
                }
                indexp = aliasShift(objSymp, indexp, flp);
                indexp = stringIndex(prefixTypep, indexp, flp);
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
        // An alias, or a generic subprogram bound to one, denotes what it was
        // declared for, and that is what is emitted (6.6.1)
        VhdlSymbol* const symp
            = nodep->vhdlSymp() ? nodep->vhdlSymp()->resolved() : nullptr;
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
            addCallArgs(refp, nodep, symp);
            return refp;
        }
        // A predefined operator named rather than written: "\"+\"(a, b)", or
        // reached through a generic subprogram bound to one (4.5.3, 6.5.5)
        if (AstNodeExpr* const outp = lowerOperatorCall(symp, nodep, flp)) return outp;
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
        // An edge test is the shape a clocked process is recognised by, but
        // it may be written anywhere; compiling std_logic_1164's own body for
        // it would ask about the history of a signal-class parameter, which
        // names a different signal at every call (16.8.2)
        if ((name == "rising_edge" || name == "falling_edge") && args.size() == 1) {
            if (const VhdlShadow* const shp = shadowFor(args[0], flp)) {
                AstNodeExpr* const evp = eventOf(args[0], shp, flp);
                AstNodeExpr* const levelp = lowerExpr(args[0]);
                if (evp && levelp) {
                    return new AstLogAnd{
                        flp, evp,
                        name == "rising_edge"
                            ? levelp
                            : static_cast<AstNodeExpr*>(new AstLogNot{flp, levelp})};
                }
                if (evp) VL_DO_DANGLING(evp->deleteTree(), evp);
                if (levelp) VL_DO_DANGLING(levelp->deleteTree(), levelp);
            }
        }
        AstNodeExpr* const firstp = lowerExpr(args[0]);
        if (!firstp) return nullptr;

        if (name == "endfile" && args.size() == 1) {
            AstNodeExpr* const filep = lowerExpr(args[0]);
            return filep ? new AstFEof{flp, filep} : nullptr;
        }
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
            // Over one array, the answer is the largest or smallest element
            // it holds (5.3.2.4). Its length is fixed here, so the comparison
            // is written out rather than looped.
            if (args.size() == 1) {
                return lowerArrayExtreme(firstp, args[0]->vhdlTypep(), name == "maximum", flp);
            }
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
        if (chp->nextp()) return nullptr;
        // One value for every element: written "others", or as one range,
        // which the choices of a legal aggregate must cover exactly -- so a
        // single range choice is the whole of it (9.3.3.3)
        if (!chp->isOthers() && !chp->rangep()) return nullptr;
        const bool packed
            = typep->base()->elementp() && elementPacks(typep->base()->elementp());
        if (packed) {
            if (AstNodeExpr* const countp = boundsLength(typep, flp)) {
                AstNodeExpr* const valuep = lowerExpr(eap->valuep());
                if (!valuep) {
                    VL_DO_DANGLING(countp->deleteTree(), countp);
                    return nullptr;
                }
                return new AstReplicate{flp, valuep, countp};
            }
        }
        // Where the length is not known here at all, the one value stands for
        // every element, and how many that is comes from what it is assigned
        // to -- which is what a default assignment pattern means
        AstNodeExpr* const valuep = lowerExpr(eap->valuep());
        if (!valuep) return nullptr;
        AstPatMember* const memberp = new AstPatMember{flp, valuep, nullptr, nullptr};
        memberp->isDefault(true);
        return new AstPattern{flp, memberp};
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
        const std::vector<VhdlSymbol*>& elems = typep->elements();
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
        // A packed record is one value, so its aggregate is a concatenation
        // with the first element most significant. An unpacked one has to be
        // written member by member, which only an assignment pattern can do.
        const bool packed = recordPacks(typep);
        AstNodeExpr* outp = nullptr;
        AstNode* itemsp = nullptr;
        for (size_t i = 0; i < elems.size(); ++i) {
            AstNodeVhdlExpr* const valuep = byElem[i] ? byElem[i] : othersp;
            if (!valuep) {
                unsup(flp, "record aggregate with element '" + elems[i]->name() + "' unset");
                return nullptr;
            }
            AstNodeExpr* const memberp = lowerExpr(valuep);
            if (!memberp) return nullptr;
            if (packed) {
                outp = outp ? new AstConcat{flp, outp, memberp} : memberp;
            } else {
                itemsp = AstNode::addNextNull(
                    itemsp, new AstPatMember{flp, memberp,
                                             new AstText{flp, elems[i]->name()}, nullptr});
            }
        }
        if (packed) return outp;
        return itemsp ? new AstPattern{flp, itemsp} : nullptr;
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
            VL_RESTORER(m_guardVarp);
            if (blockp->guardp()) lowerGuard(blockp, flp);
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
            addStmt(concurrentProcess(lowerAssert(assertp), assertp->postponed(), flp));
            return;
        }
        if (AstVhdlProcCall* const callp = VN_CAST(nodep, VhdlProcCall)) {
            addStmt(concurrentProcess(lowerStmt(callp), callp->postponed(), flp));
            return;
        }
        unsup(flp, std::string{"concurrent statement "} + nodep->typeName());
    }

    // The process a concurrent statement is. A postponed one runs only once
    // the signals have settled for that time step, after the last delta
    // cycle (11.3), which is what Verilator's postponed region is -- so a
    // postponed assertion sees the values the step ended with, not the ones
    // it passed through.
    AstNode* concurrentProcess(AstNode* bodyp, bool postponed, FileLine* flp) {
        if (postponed) return new AstAlwaysPostponed{flp, bodyp};
        // A concurrent statement runs once at the start and again whenever
        // what it reads changes (11.6). One that reads nothing -- an
        // assertion about a generic, say -- therefore runs exactly once.
        bool reads = false;
        bodyp->foreach([&reads](const AstVarRef* refp) {
            if (refp->access().isReadOrRW()) reads = true;
        });
        if (!reads) return new AstInitial{flp, bodyp};
        return new AstAlways{flp, VAlwaysKwd::ALWAYS, new AstSenTree{flp, comboSenItem(flp)},
                             bodyp};
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
            AstNodeVhdlExpr* actualp = nullptr;
            for (size_t i = 0; i < assocs.size(); ++i) {
                const AstVhdlName* const namep = VN_CAST(assocs[i]->formalp(), VhdlName);
                if (namep ? namep->name() == symp->name() : i == position) {
                    actualp = assocs[i]->actualp();
                    break;
                }
            }
            // "open", written out or left out, means the port takes its own
            // default and is wired to nothing (6.5.7.1)
            if (!actualp || VN_IS(actualp, VhdlOpen)) {
                AstVar* const varp = newVar(symp, VVarType::VAR, flp);
                addStmt(varp);
                if (ifp->defaultp()) varp->valuep(lowerExpr(ifp->defaultp()));
                continue;
            }
            // The actual is looked at before the formal has a variable, so
            // that a formal of the same name cannot stand in for it
            AstNodeExpr* const valuep = lowerExpr(actualp);
            if (!valuep) {
                addStmt(newVar(symp, VVarType::VAR, flp));
                continue;
            }
            // A port mapped to a signal of the design is another name for
            // that signal (11.2): a block is elaborated where it stands, so
            // there is nothing between the two, and a port of mode "inout"
            // or "buffer" has to write the signal itself rather than a copy
            if (!isGeneric) {
                if (AstVarRef* const refp = VN_CAST(valuep, VarRef)) {
                    m_vars.emplace(symp, refp->varp());
                    VL_DO_DANGLING(valuep->deleteTree(), valuep);
                    continue;
                }
            }
            AstVar* const varp = newVar(symp, VVarType::VAR, flp);
            addStmt(varp);
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
            // Every VHDL process runs once when the simulation starts, before
            // any signal has changed (11.3). Saying so in the sensitivity
            // itself, rather than emitting the body a second time, is what
            // keeps it to exactly one run: a change item may fire at time
            // zero of its own accord, when the signal starts at a value the
            // trigger's record of it does not.
            if (sentreep && nodep->hasSensList() && !nodep->sensAll() && nodep->sensp()) {
                allowInitialDriver(nodep->stmtsp(), false);
                if (v3Global.opt.timing().isTrue()) {
                    // A process with a sensitivity list is the same as one
                    // with "wait on <list>" as its last statement (11.3): it
                    // runs once when the simulation starts and then waits.
                    // Written out that way it runs exactly once at time zero,
                    // where an "always" with the same sensitivity may or may
                    // not fire there depending on what the signals start at.
                    bodyp = AstNode::addNextNull(
                        bodyp, static_cast<AstNode*>(
                                   new AstEventControl{flp, sentreep, nullptr}));
                    addStmt(new AstAlways{flp, VAlwaysKwd::ALWAYS, nullptr, bodyp});
                    return;
                }
                // Without --timing there is nothing to suspend on, so the
                // body is emitted a second time to start it
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
        // VHDL runs the process when any listed signal *changes* -- not on an
        // edge of its least significant bit, which is what an edge item means
        // for a vector. Keeping the written list rather
        // than substituting "all" preserves the semantics of an incomplete
        // one, deliberate or not.
        AstSenItem* itemsp = nullptr;
        for (AstNode* p = nodep->sensp(); p; p = p->nextp()) {
            AstNodeVhdlExpr* const namep = VN_AS(p, NodeVhdlExpr);
            AstNodeExpr* const sensp = lowerExpr(namep);
            if (!sensp) continue;
            itemsp = AstNode::addNext(
                itemsp, senItemFor(sensp, isOneBitType(typeOfExpr(namep)), flp));
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
    // The part of an object a name selects, as a key, where every step of the
    // selection is decided now: "r.y", "v(3)", "v(7 downto 4)". False if the
    // name is not that -- an index that is only known while running names a
    // different element at every call, and one copy of the subprogram cannot
    // stand for all of them.
    // The object a selected name is ultimately a part of. Unlike rootSymOf,
    // this walks past the element or index symbol the selection itself
    // denotes and answers with the object that holds it.
    static const VhdlSymbol* baseSymOf(const AstNode* nodep) {
        while (nodep) {
            if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
                nodep = callp->prefixp();
            } else if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
                nodep = selp->prefixp();
            } else if (const AstNodeVhdlExpr* const vp = VN_CAST(nodep, NodeVhdlExpr)) {
                return vp->vhdlSymp() ? vp->vhdlSymp()->resolved() : nullptr;
            } else {
                return nullptr;
            }
        }
        return nullptr;
    }

    static bool staticSelectKey(const AstNodeVhdlExpr* nodep, std::string& out) {
        if (!nodep) return false;
        if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            if (!staticSelectKey(selp->prefixp(), out)) return false;
            out += "." + selp->name();
            return true;
        }
        if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
            if (!staticSelectKey(callp->prefixp(), out)) return false;
            if (const AstVhdlRange* const rp = VN_CAST(callp->argsp(), VhdlRange)) {
                const VhdlValue lv = V3VhdlEval::eval(rp->leftp());
                const VhdlValue rv = V3VhdlEval::eval(rp->rightp());
                if (!lv.isInt() || !rv.isInt()) return false;
                out += "(" + std::to_string(lv.toInt()) + ":" + std::to_string(rv.toInt()) + ")";
                return true;
            }
            const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
            if (!asp || asp->nextp() || !asp->actualp()) return false;
            const VhdlValue iv = V3VhdlEval::eval(asp->actualp());
            if (!iv.isInt()) return false;
            out += "(" + std::to_string(iv.toInt()) + ")";
            return true;
        }
        if (nodep->vhdlSymp()) {
            out += "@" + std::to_string(reinterpret_cast<uintptr_t>(nodep->vhdlSymp()->resolved()));
            return true;
        }
        return false;
    }

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
    // Each driver holds a value of its own; they are gathered into the array
    // the function takes and it says what they add up to. Analysis has
    // already given the function the copy that takes exactly this many
    // values, so the array has a length and is built here rather than sized
    // at run time.
    void makeResolvedByFunction(VhdlSymbol* symp, AstVar* netp, FileLine* flp) {
        const std::string what = "signal '" + symp->name() + "' with "
                                 + std::to_string(symp->driverCount())
                                 + " drivers resolved by a function of its own";
        VhdlSymbol* const resolvep = symp->resolverp();
        if (!resolvep) {
            unsup(flp, what);
            return;
        }
        const int count = symp->driverCount();
        VhdlType* const paramTypep
            = resolvep->paramTypes().empty() ? nullptr : resolvep->paramTypes()[0];
        if (!paramTypep || !paramTypep->isConstrainedArray() || paramTypep->dimensions() != 1
            || paramTypep->dimLength(0) != count) {
            unsup(flp, what);
            return;
        }
        AstNodeFTask* ftaskp = nullptr;
        {
            const auto it = m_funcs.find(resolvep);
            ftaskp = it != m_funcs.end() ? it->second : emitOnDemand(resolvep);
        }
        if (!ftaskp || !VN_IS(ftaskp, Func)) {
            unsup(flp, what);
            return;
        }
        // What the signal starts at is decided by its drivers, as it is for a
        // signal resolved by the logic tables: the declaration's own initial
        // value belongs to each driver instead (14.7.2)
        // The signal keeps the initial value its declaration gives it: until
        // the drivers have been resolved for the first time there is nothing
        // else for it to hold, and a process may read it before then
        AstNodeExpr* initp = VN_CAST(netp->valuep(), NodeExpr);
        // Written by the block that resolves it and by nothing else, so the
        // initial value is not a second driver
        netp->fileline()->warnOff(V3ErrorCode::MULTIDRIVEN, true);
        // One variable per driver, each starting where the declaration says
        // the signal does: a driver contributes a value from the beginning,
        // even before the process that owns it has run (14.7.2)
        std::vector<VhdlDriver> drivers;
        for (int i = 0; i < count; ++i) {
            VhdlDriver drv;
            drv.valp = new AstVar{
                flp, VVarType::VAR,
                "__Vdrv" + std::to_string(i) + "__" + m_prefix + symp->name() + "__val",
                VFlagChildDType{}, lowerType(symp->typep(), flp)};
            addStmt(drv.valp);
            drv.valp->valuep(initp ? initp->cloneTree(false)
                                   : defaultValueFor(symp->typep(), flp));
            drivers.push_back(drv);
        }
        // The array the function is given, of exactly the type its parameter
        // was specialized to, so the two agree without any conversion
        AstVar* const arrp = new AstVar{flp, VVarType::VAR,
                                        "__Vres__" + m_prefix + symp->name(), VFlagChildDType{},
                                        lowerType(paramTypep, flp)};
        addStmt(arrp);
        // Filled one element at a time, in the order the parameter's own
        // index range runs, so that a function which looks at s'left sees the
        // driver it expects. Element by element rather than as one
        // concatenation: the widths are not known until V3Width, and a
        // concatenation has to know them as it is built.
        const VhdlType* const idxp = paramTypep->indexTypes()[0];
        AstNode* bodyp = nullptr;
        for (int i = 0; i < count; ++i) {
            const int64_t index = idxp->ascending() ? idxp->left() + i : idxp->left() - i;
            bodyp = AstNode::addNextNull(
                bodyp,
                static_cast<AstNode*>(new AstAssign{
                    flp,
                    new AstSelBit{flp, new AstVarRef{flp, arrp, VAccess::WRITE},
                                  new AstConst{flp, AstConst::Signed32{},
                                               static_cast<int32_t>(index)}},
                    new AstVarRef{flp, drivers[i].valp, VAccess::READ}}));
        }
        AstFuncRef* const callp = new AstFuncRef{flp, ftaskp->name(), nullptr};
        callp->taskp(ftaskp);
        callp->addArgsp(new AstArg{flp, "", new AstVarRef{flp, arrp, VAccess::READ}});
        bodyp = AstNode::addNextNull(
            bodyp, static_cast<AstNode*>(
                       new AstAssign{flp, new AstVarRef{flp, netp, VAccess::WRITE}, callp}));
        // Whenever any driver changes, which is what the process reads
        addStmt(new AstAlways{flp, VAlwaysKwd::ALWAYS_COMB, nullptr, bodyp});
        m_drivers.emplace(symp, drivers);
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
        // A waveform with a delay, or with more than one element, schedules
        // each element at its own time (11.6, 10.5.2.2). It becomes a process
        // that assigns them, rather than a continuous assignment with a
        // delay: a continuous assignment reaches its target through the
        // delay, where a VHDL driver holds its old value until the time comes
        // -- which is what "rst <= '0' after 22 ns" means.
        if (nodep->kind() == VhdlAssignKind::SIMPLE && nodep->wavesp()
            && (nodep->wavesp()->nextp() || nodep->wavesp()->afterp())
            && !m_drivers.count(rootSymOf(nodep->targetp()))) {
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "waveform with more than one element without --timing");
                return;
            }
            // One process that schedules them all, in order, which is what
            // the concurrent form is defined to be (11.6)
            AstNode* stmtsp = nullptr;
            AstNodeExpr* readsp = nullptr;
            for (AstVhdlWaveform* p = nodep->wavesp(); p;
                 p = VN_AS(p->nextp(), VhdlWaveform)) {
                AstNodeExpr* const onep = lowerExpr(nodep->targetp());
                AstNodeExpr* const valuep = lowerExpr(p->valuep());
                if (!onep || !valuep) return;
                readsp = readsp ? new AstConcat{flp, readsp, valuep->cloneTree(false)}
                                : valuep->cloneTree(false);
                AstNode* const delayp
                    = p->afterp() ? new AstDelay{flp, lowerExpr(p->afterp()), false} : nullptr;
                stmtsp = AstNode::addNextNull(
                    stmtsp, static_cast<AstNode*>(new AstAssignDly{flp, onep, valuep, delayp}));
                stmtsp = AstNode::addNextNull(
                    stmtsp, transactionMark(nodep->targetp(), delayp, true, nullptr, flp));
            }
            // A concurrent statement is a process that runs once at time
            // zero and again whenever what it reads changes (11.6). Verilog's
            // "always" with an event control does not run at time zero, so
            // the statements are emitted twice: once to start, once to react.
            AstSenTree* const senp = readsp ? sensitivityOfExpr(readsp, flp) : nullptr;
            if (readsp) VL_DO_DANGLING(readsp->deleteTree(), readsp);
            if (senp) addStmt(new AstAlways{flp, VAlwaysKwd::ALWAYS, senp,
                                            stmtsp->cloneTree(true)});
            addStmt(new AstInitial{flp, stmtsp});
            return;
        }
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
                addStmt(transactionMark(nodep->targetp(), delayp, false, rhsp, flp));
            }
            return;
        }
        if (nodep->guarded()) {
            // "s <= guarded w" drives only while GUARD is true; when it goes
            // false the driver is turned off, which for a signal with one
            // driver leaves it holding its last value (11.6)
            if (!m_guardVarp) {
                unsup(flp, "guarded assignment outside a guarded block");
                VL_DO_DANGLING(lhsp->deleteTree(), lhsp);
                VL_DO_DANGLING(rhsp->deleteTree(), rhsp);
                return;
            }
            // Holding a value while the guard is false is what the
            // construct is for, and a VHDL signal assignment is always a
            // scheduled update, so neither reading as a latch nor using "<="
            // in combinational logic is a finding here
            FileLine* const gflp = new FileLine{flp};
            gflp->warnOff(V3ErrorCode::LATCH, true);
            gflp->warnOff(V3ErrorCode::COMBDLY, true);
            AstNode* const assignp = new AstAssignDly{gflp, lhsp, rhsp, delayp};
            AstNode* const drivep = AstNode::addNextNull(
                assignp, transactionMark(nodep->targetp(), delayp, true, nullptr, gflp));
            AstNode* const stmtsp
                = new AstIf{gflp, new AstVarRef{gflp, m_guardVarp, VAccess::READ}, drivep};
            addStmt(concurrentProcess(stmtsp, nodep->postponed(), gflp));
            return;
        }
        AstNode* const markp = transactionMark(nodep->targetp(), delayp, false, rhsp, flp);
        addStmt(new AstAlways{new AstAssignW{flp, lhsp, rhsp, delayp}});
        if (markp) addStmt(markp);
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
    std::map<std::string, std::string> m_archOf;  // "lib.entity" -> architecture built
    std::map<std::string, std::string> m_bindings;  // Component -> entity
    std::map<std::string, std::string> m_instanceBindings;  // Label -> entity

    // An instance binds to the entity of the same name, which is VHDL's
    // default binding rule (7.3.3)
    void lowerInstance(AstVhdlInstance* nodep) {
        FileLine* const flp = nodep->fileline();
        std::vector<std::string> parts;
        // "entity work.reg4(behav)" names the architecture as well (7.3.2),
        // which the parser could not tell from a call
        AstNodeVhdlExpr* unitp = nodep->unitp();
        std::string archName;
        if (AstVhdlCall* const callp = VN_CAST(unitp, VhdlCall)) {
            if (AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc)) {
                std::vector<std::string> archParts;
                if (!asp->nextp() && asp->actualp()
                    && flattenName(asp->actualp(), archParts) && !archParts.empty()) {
                    archName = archParts.back();
                }
            }
            unitp = callp->prefixp();
        }
        if (!flattenName(unitp, parts) || parts.empty()) {
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
        // Only one architecture of an entity is built, so naming a different
        // one for this instance would need a module of its own
        if (!archName.empty()) {
            const std::string libname = m_modp ? m_modp->libname() : "work";
            const auto archIt = m_archOf.find(libname + "." + modName);
            if (archIt != m_archOf.end() && archIt->second != archName) {
                unsup(flp, "instance of architecture '" + archName + "' where '"
                               + archIt->second + "' is the one built");
                return;
            }
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
        // "open" written out means the same as leaving the formal out: the
        // port takes its own default (6.5.7.1)
        if (nodep->isOpen() || VN_IS(nodep->actualp(), VhdlOpen)) {
            return new AstPin{flp, pinNum, formal, nullptr};
        }
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
        // "s <= force v" drives the signal regardless of what else does,
        // until "s <= release" hands it back (10.5.2.1, VHDL-2008)
        if (nodep->release()) {
            AstNodeExpr* const lhsp = lowerTarget(nodep->targetp());
            if (!lhsp) return nullptr;
            v3Global.setHasForceableSignals();
            return new AstRelease{flp, lhsp};
        }
        if (nodep->force()) {
            AstVhdlWaveform* const forcep = nodep->wavesp();
            if (!forcep || !forcep->valuep()) return nullptr;
            AstNodeExpr* const lhsp = lowerTarget(nodep->targetp());
            AstNodeExpr* const rhsp = lowerExpr(forcep->valuep());
            if (!lhsp || !rhsp) return nullptr;
            v3Global.setHasForceableSignals();
            return new AstAssignForce{flp, lhsp, rhsp};
        }
        if (nodep->kind() != VhdlAssignKind::SIMPLE) {
            unsup(flp, "conditional signal assignment in a process");
            return nullptr;
        }
        AstVhdlWaveform* const wfp = nodep->wavesp();
        if (!wfp || !wfp->valuep()) return nullptr;
        // "s <= a after t1, b after t2" schedules each element at its own
        // time (10.5.2.2). Each becomes a delayed assignment of its own,
        // which is right as long as the times increase, as the language
        // requires them to.
        if (wfp->nextp()) {
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "waveform with more than one element without --timing");
                return nullptr;
            }
            AstNode* outp = nullptr;
            for (AstVhdlWaveform* p = wfp; p; p = VN_AS(p->nextp(), VhdlWaveform)) {
                AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
                AstNodeExpr* const rhsp = lowerExpr(p->valuep());
                if (!lhsp || !rhsp) return outp;
                AstNode* const delayp
                    = p->afterp() ? new AstDelay{flp, lowerExpr(p->afterp()), false} : nullptr;
                AstNode* const markp
                    = transactionMark(nodep->targetp(), delayp, sequential, rhsp, flp);
                outp = AstNode::addNextNull(
                    outp, sequential ? static_cast<AstNode*>(
                              new AstAssignDly{flp, lhsp, rhsp, delayp})
                                     : static_cast<AstNode*>(
                                           new AstAssignW{flp, lhsp, rhsp, delayp}));
                outp = AstNode::addNextNull(outp, markp);
            }
            return outp;
        }
        AstNode* delayp = nullptr;
        if (wfp->afterp()) {
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "'after' delay without --timing");
                return nullptr;
            }
            delayp = new AstDelay{flp, lowerExpr(wfp->afterp()), false};
        }
        if (AstVhdlAggregate* const aggp = VN_CAST(nodep->targetp(), VhdlAggregate)) {
            if (delayp) VL_DO_DANGLING(delayp->deleteTree(), delayp);
            return lowerAggregateTarget(aggp, wfp->valuep(), true, sequential, flp);
        }
        if (m_drivers.count(rootSymOf(nodep->targetp()))) {
            AstNodeExpr* const valuep = lowerExpr(wfp->valuep());
            if (!valuep) return nullptr;
            AstNode* const markp
                = transactionMark(nodep->targetp(), delayp, sequential, valuep, flp);
            return AstNode::addNextNull(
                driveResolved(nodep, nodep->targetp(), valuep, delayp, sequential, flp), markp);
        }
        AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
        AstNodeExpr* const rhsp = lowerExpr(wfp->valuep());
        if (!lhsp || !rhsp) return nullptr;
        AstNode* const markp = transactionMark(nodep->targetp(), delayp, sequential, rhsp, flp);
        AstNode* const outp
            = sequential ? static_cast<AstNode*>(new AstAssignDly{flp, lhsp, rhsp, delayp})
                         : static_cast<AstNode*>(new AstAssignW{flp, lhsp, rhsp, delayp});
        return AstNode::addNextNull(outp, markp);
    }

    // "(a, b, c) := v" assigns each element of the value to the name in the
    // matching position of the aggregate (10.6.2, 10.5.2.1). The value is
    // read once into a temporary, so that a name on the left cannot change
    // what the right side means part way through. An element of the target
    // may be a whole slice of the value, if it is of the value's own type.
    AstNode* lowerAggregateTarget(AstVhdlAggregate* targetp, AstNodeVhdlExpr* valuep,
                                  bool signalAssign, bool sequential, FileLine* flp) {
        VhdlType* const typep = valuep->vhdlTypep();
        const bool isRecord = typep && typep->base()->kind() == VhdlTypeKind::RECORD;
        if (!typep || (!isRecord && typep->base()->kind() != VhdlTypeKind::ARRAY)
            || (!isRecord && typep->dimensions() != 1)) {
            unsup(flp, "aggregate target of this type");
            return nullptr;
        }
        std::vector<AstVhdlElemAssoc*> assocs;
        for (AstVhdlElemAssoc* p = targetp->elemsp(); p; p = VN_AS(p->nextp(), VhdlElemAssoc)) {
            if (p->choicesp()) {
                unsup(p->fileline(), "aggregate target with a named element");
                return nullptr;
            }
            assocs.push_back(p);
        }
        // Where the value's own range is known, use it; an aggregate that
        // constrains itself starts at its index subtype's leftmost value
        int64_t index = 0;
        int64_t step = 1;
        int64_t length = -1;
        if (!isRecord) {
            if (typep->isConstrainedArray()) {
                const VhdlType* const indexp = typep->indexTypes()[0];
                index = indexp->left();
                step = indexp->ascending() ? 1 : -1;
                length = indexp->length();
            } else {
                const VhdlType* const indexp = typep->base()->indexTypes().empty()
                                                   ? nullptr
                                                   : typep->base()->indexTypes()[0];
                if (indexp) {
                    index = indexp->left();
                    step = indexp->ascending() ? 1 : -1;
                }
                // The value has as many elements as the target names
                int64_t total = 0;
                for (AstVhdlElemAssoc* const ap : assocs) {
                    const int64_t span = elementSpan(ap->valuep(), typep);
                    if (span < 0) {
                        unsup(ap->fileline(), "aggregate target element of an unknown length");
                        return nullptr;
                    }
                    total += span;
                }
                length = total;
            }
        }
        AstNodeExpr* const rhsp = lowerExpr(valuep);
        if (!rhsp) return nullptr;
        AstNodeDType* tempDTypep = nullptr;
        if (isRecord || typep->isConstrainedArray()) {
            tempDTypep = lowerType(typep, flp);
        } else {
            VhdlType* const elemp
                = typep->elementp() ? typep->elementp() : typep->base()->elementp();
            AstNodeDType* const edtp = elemp ? lowerType(elemp, flp) : newLogicDType(flp, 1);
            AstRange* const rp
                = new AstRange{flp, static_cast<int>(index),
                               static_cast<int>(index + (length - 1) * step)};
            tempDTypep = (elemp && elementPacks(elemp))
                             ? static_cast<AstNodeDType*>(
                                   new AstPackArrayDType{flp, VFlagChildDType{}, edtp, rp})
                             : static_cast<AstNodeDType*>(
                                   new AstUnpackArrayDType{flp, VFlagChildDType{}, edtp, rp});
        }
        AstVar* const tempp = newTempVar("__Vaggtgt", tempDTypep, flp);
        AstNode* outp = new AstAssign{flp, new AstVarRef{flp, tempp, VAccess::WRITE}, rhsp};

        if (isRecord) {
            const std::vector<VhdlSymbol*>& elems = typep->base()->elements();
            if (assocs.size() != elems.size()) {
                err(flp, "The aggregate on the left has " + std::to_string(assocs.size())
                             + " element(s), but the record has "
                             + std::to_string(elems.size()));
                return outp;
            }
            for (size_t i = 0; i < assocs.size(); ++i) {
                AstNodeExpr* const onep = lowerExpr(assocs[i]->valuep());
                if (!onep) return outp;
                AstNodeExpr* const fromp = new AstMemberSel{
                    flp, new AstVarRef{flp, tempp, VAccess::READ}, VFlagChildDType{},
                    elems[i]->name()};
                outp = AstNode::addNext(outp,
                                        assignOf(onep, fromp, signalAssign, sequential, flp));
            }
            return outp;
        }

        int64_t used = 0;
        for (size_t i = 0; i < assocs.size(); ++i) {
            AstNodeExpr* const onep = lowerExpr(assocs[i]->valuep());
            if (!onep) return outp;
            int64_t span = elementSpan(assocs[i]->valuep(), typep);
            if (span < 0) {
                unsup(assocs[i]->fileline(), "aggregate target element of an unknown length");
                return outp;
            }
            AstNodeExpr* fromp = nullptr;
            if (span == 1) {
                fromp = new AstSelBit{
                    flp, new AstVarRef{flp, tempp, VAccess::READ},
                    new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(index)}};
            } else {
                const int64_t last = index + (span - 1) * step;
                fromp = new AstSelExtract{
                    flp, new AstVarRef{flp, tempp, VAccess::READ},
                    new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(index)},
                    new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(last)}};
            }
            index += span * step;
            used += span;
            outp = AstNode::addNext(outp, assignOf(onep, fromp, signalAssign, sequential, flp));
        }
        if (length >= 0 && used != length) {
            err(flp, "The aggregate on the left covers " + std::to_string(used)
                         + " element(s), but the value has " + std::to_string(length));
        }
        return outp;
    }

    static AstNode* assignOf(AstNodeExpr* lhsp, AstNodeExpr* rhsp, bool signalAssign,
                             bool sequential, FileLine* flp) {
        if (!signalAssign) return new AstAssign{flp, lhsp, rhsp};
        if (sequential) return new AstAssignDly{flp, lhsp, rhsp};
        return new AstAssignW{flp, lhsp, rhsp};
    }

    // A variable assignment takes effect at once (10.6)
    AstNode* lowerVarAssign(AstVhdlVarAssign* nodep) {
        FileLine* const flp = nodep->fileline();
        if (nodep->kind() != VhdlAssignKind::SIMPLE) {
            unsup(flp, "conditional variable assignment");
            return nullptr;
        }
        if (AstVhdlAggregate* const aggp = VN_CAST(nodep->targetp(), VhdlAggregate)) {
            if (!nodep->valuep()) return nullptr;
            return lowerAggregateTarget(aggp, nodep->valuep(), false, false, flp);
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
        AstNodeExpr* ascp = nullptr;  // Set where the direction only folds later
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
                    bool dirKnown = true;
                    if (dynamicBounds(attrp->prefixp()->vhdlTypep(), dim, flp, initp, limitp,
                                      asc, &dirKnown)) {
                        if (!dirKnown) ascp = ascendingOf(initp, limitp, flp);
                        ascending = (name == "range") ? asc : !asc;
                        if (name == "reverse_range") {
                            std::swap(initp, limitp);
                            if (ascp) ascp = new AstLogNot{flp, ascp};
                        }
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
        // Where the direction arrives with the instance, both ways are
        // written out and the one that applies folds away with it
        AstNodeExpr* const condp
            = ascp ? static_cast<AstNodeExpr*>(new AstCond{
                  flp, ascp->cloneTree(false),
                  new AstLteS{flp, new AstVarRef{flp, varp, VAccess::READ},
                              limitp->cloneTree(false)},
                  new AstGteS{flp, new AstVarRef{flp, varp, VAccess::READ}, limitp}})
                   : ascending ? static_cast<AstNodeExpr*>(new AstLteS{
                         flp, new AstVarRef{flp, varp, VAccess::READ}, limitp})
                               : static_cast<AstNodeExpr*>(new AstGteS{
                                   flp, new AstVarRef{flp, varp, VAccess::READ}, limitp});
        loopp->addStmtsp(new AstLoopTest{flp, loopp, condp});
        m_loops.emplace_back(VhdlLoopFrame{nodep->label(), nullptr, nullptr});
        loopp->addStmtsp(lowerLoopBody(nodep));
        AstNodeExpr* stepp = nullptr;
        if (ascp) {
            stepp = new AstAdd{flp, new AstVarRef{flp, varp, VAccess::READ},
                               new AstCond{flp, ascp,
                                           new AstConst{flp, AstConst::Signed32{}, 1},
                                           new AstConst{flp, AstConst::Signed32{}, -1}}};
        } else if (ascending) {
            stepp = new AstAdd{flp, new AstVarRef{flp, varp, VAccess::READ},
                               new AstConst{flp, AstConst::Signed32{}, 1}};
        } else {
            stepp = new AstSub{flp, new AstVarRef{flp, varp, VAccess::READ},
                               new AstConst{flp, AstConst::Signed32{}, 1}};
        }
        loopp->addContsp(
            new AstAssign{flp, new AstVarRef{flp, varp, VAccess::WRITE}, stepp});
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
        // A procedure with a signal parameter drives the caller's own signal,
        // so it gets a copy bound to the signals this call passes (4.2.2.3)
        if (symp && callp && hasSignalParam(symp)) {
            if (AstNode* const outp = lowerSignalProcCall(symp, callp, flp)) return outp;
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
        addCallArgs(refp, callp, symp);
        (void)namep;
        return new AstStmtExpr{flp, refp};
    }

    // How a sensitivity item watches one expression. VHDL means "whenever
    // this changes". For a single bit that is an edge either way, which is
    // exact even for a bit taken out of a vector; for anything wider it has
    // to be a change, since an edge item would only ever look at the least
    // significant bit.
    static AstSenItem* senItemFor(AstNodeExpr* exprp, bool oneBit, FileLine* flp) {
        return new AstSenItem{flp, oneBit ? VEdgeType::ET_BOTHEDGE : VEdgeType::ET_CHANGED,
                              exprp};
    }

    // Whether a VHDL type is one bit once lowered: BIT, BOOLEAN, STD_ULOGIC
    // and anything else declared the same way
    static bool isOneBitType(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        if (basep->kind() != VhdlTypeKind::ENUM) return false;
        return enumIsLogic(basep) || bitsFor(basep->literals().size()) == 1;
    }

    // Whether a variable is one bit wide, as its emitted type says
    static bool isOneBitVar(const AstVar* varp) {
        const AstBasicDType* const dtypep = VN_CAST(varp->childDTypep(), BasicDType);
        return dtypep && dtypep->width() == 1;
    }

    // The declaration of a subprogram, from its body where it has one
    static AstVhdlSubprogDecl* declOf(VhdlSymbol* symp) {
        if (!symp) return nullptr;
        if (AstVhdlSubprogDecl* const bodyp = symp->bodyp()) return bodyp;
        return VN_CAST(symp->declp(), VhdlSubprogDecl);
    }

    // The first real parameter, past the generics a generic subprogram keeps
    // ahead of them (4.2.1)
    static AstNode* firstParamOf(const AstVhdlSubprogDecl* declp) {
        AstNode* p = declp->paramsp();
        for (int i = 0; i < declp->genericCount() && p; ++i) p = p->nextp();
        return p;
    }

    // Whether any parameter is of the signal class (4.2.2.3)
    static bool hasSignalParam(VhdlSymbol* symp) {
        AstVhdlSubprogDecl* const declp = declOf(symp);
        if (!declp) return false;
        for (AstNode* p = firstParamOf(declp); p; p = p->nextp()) {
            if (VN_AS(p, VhdlIfaceDecl)->kind() == VhdlIfaceKind::SIGNAL) return true;
        }
        return false;
    }

    // Call a procedure whose signal parameters are bound to the actuals
    AstNode* lowerSignalProcCall(VhdlSymbol* symp, AstVhdlCall* callp, FileLine* flp) {
        AstVhdlSubprogDecl* const declp = declOf(symp);
        if (!declp || !declp->hasBody()) return nullptr;
        // The actuals, in formal order
        std::vector<AstVhdlAssoc*> assocs;
        for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
            assocs.push_back(VN_AS(p, VhdlAssoc));
        }
        std::vector<AstVhdlIfaceDecl*> formals;
        for (AstNode* p = firstParamOf(declp); p; p = p->nextp()) {
            formals.push_back(VN_AS(p, VhdlIfaceDecl));
        }
        std::map<const VhdlSymbol*, AstVar*> bind;
        std::map<const VhdlSymbol*, AstNodeVhdlExpr*> exprBind;
        std::map<const VhdlSymbol*, VhdlActive> activeBind;
        std::vector<AstVhdlExprPair> passed;  // Formal index and actual, for the rest
        std::string key = symp->name();
        for (size_t i = 0; i < formals.size(); ++i) {
            AstNodeVhdlExpr* actualp = nullptr;
            for (size_t a = 0; a < assocs.size(); ++a) {
                const AstVhdlName* const namep
                    = assocs[a]->formalp() ? VN_CAST(assocs[a]->formalp(), VhdlName) : nullptr;
                if (namep) {
                    if (namep->name() == formals[i]->name()) actualp = assocs[a]->actualp();
                } else if (a == i) {
                    actualp = assocs[a]->actualp();
                }
            }
            if (formals[i]->kind() != VhdlIfaceKind::SIGNAL) {
                if (actualp) passed.push_back(AstVhdlExprPair{formals[i]->name(), actualp});
                continue;
            }
            // A signal actual is a signal of the design, either named plainly
            // or with a part of it selected in a way that is decided now
            VhdlSymbol* const actualSymp = actualp ? actualp->vhdlSymp() : nullptr;
            const auto varIt
                = actualSymp ? m_vars.find(actualSymp) : m_vars.end();
            if (!actualSymp || varIt == m_vars.end() || VN_IS(actualp, VhdlSelected)
                || VN_IS(actualp, VhdlCall)) {
                // "drive(r.y, 3)" or "set(v(3))": the formal has no variable
                // of its own, so the actual's name is lowered again wherever
                // the body refers to it
                std::string path;
                const VhdlSymbol* const rootp = actualp ? baseSymOf(actualp) : nullptr;
                if (!rootp || !m_vars.count(rootp) || m_drivers.count(rootp)) return nullptr;
                if (!staticSelectKey(actualp, path)) return nullptr;
                exprBind.emplace(formals[i]->vhdlSymp(), actualp);
                key += "|" + path;
                continue;
            }
            if (m_drivers.count(actualSymp)) return nullptr;  // Resolved: needs its drivers
            bind.emplace(formals[i]->vhdlSymp(), varIt->second);
            // An assignment inside the copy is a transaction on the actual,
            // so the formal answers from the actual's transaction bit
            const auto acIt = m_actives.find(actualSymp->resolved());
            if (acIt != m_actives.end()) activeBind.emplace(formals[i]->vhdlSymp(), acIt->second);
            key += "|" + std::to_string(reinterpret_cast<uintptr_t>(varIt->second));
        }
        if (bind.empty() && exprBind.empty()) return nullptr;
        auto copyIt = m_signalCopies.find(key);
        if (copyIt == m_signalCopies.end()) {
            // The formals belong to the one declaration, so building a copy
            // moves their mapping; put back whatever the original left
            std::map<const VhdlSymbol*, AstVar*> saved;
            for (AstVhdlIfaceDecl* const ifp : formals) {
                const auto it = m_vars.find(ifp->vhdlSymp());
                if (it != m_vars.end()) saved.emplace(ifp->vhdlSymp(), it->second);
            }
            AstNodeFTask* taskp = nullptr;
            {
                VL_RESTORER_COPY(m_bindSignals);
                VL_RESTORER(m_ftaskp);
                VL_RESTORER_COPY(m_prefix);
                VL_RESTORER_COPY(m_bindExprs);
                m_bindSignals = bind;
                m_bindExprs = exprBind;
                m_ftaskp = nullptr;
                m_prefix.clear();
                for (const auto& pair : activeBind) m_actives.emplace(pair.first, pair.second);
                taskp = lowerSubprogramInto(declp, uniqueName(symp) + "__Vsig");
                for (const auto& pair : activeBind) m_actives.erase(pair.first);
            }
            for (AstVhdlIfaceDecl* const ifp : formals) {
                const auto it = saved.find(ifp->vhdlSymp());
                if (it == saved.end()) {
                    m_vars.erase(ifp->vhdlSymp());
                } else {
                    m_vars[ifp->vhdlSymp()] = it->second;
                }
            }
            if (!taskp) return nullptr;
            copyIt = m_signalCopies.emplace(key, taskp).first;
        }
        AstTaskRef* const refp = new AstTaskRef{flp, copyIt->second->name(), nullptr};
        refp->taskp(copyIt->second);
        for (const AstVhdlExprPair& pair : passed) {
            AstNodeExpr* const onep = lowerExpr(pair.exprp);
            if (!onep) {
                VL_DO_DANGLING(refp->deleteTree(), refp);
                return nullptr;
            }
            refp->addArgsp(new AstArg{flp, pair.name, onep});
        }
        return new AstStmtExpr{flp, refp};
    }

    // The sensitivity a list of signal names gives
    AstSenTree* sensitivityOf(AstNode* sensp, FileLine* flp) {
        AstSenItem* itemsp = nullptr;
        for (AstNode* p = sensp; p; p = p->nextp()) {
            AstNodeVhdlExpr* const namep = VN_AS(p, NodeVhdlExpr);
            AstNodeExpr* const exprp = lowerExpr(namep);
            if (!exprp) continue;
            itemsp = AstNode::addNext(
                itemsp, senItemFor(exprp, isOneBitType(typeOfExpr(namep)), flp));
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
                itemsp,
                senItemFor(new AstVarRef{flp, varp, VAccess::READ}, isOneBitVar(varp), flp));
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
    // A delta cycle: the process schedules a change of its own and waits for
    // it. The change is a non-blocking assignment, so it lands with all the
    // others, and the process resumes after they have -- which is what a
    // delta cycle is, and what "wait for 0 ns" waits for.
    AstNode* deltaWait(FileLine* flp) {
        if (!m_deltaVarp) {
            m_deltaVarp = new AstVar{flp, VVarType::VAR, "__Vdelta", VFlagChildDType{},
                                     newLogicDType(flp, 1)};
            m_deltaVarp->valuep(new AstConst{flp, AstConst::BitFalse{}});
            m_stmtsp = AstNode::addNextNull(m_stmtsp, m_deltaVarp);
        }
        AstNode* const outp = new AstAssignDly{
            flp, new AstVarRef{flp, m_deltaVarp, VAccess::WRITE},
            new AstNot{flp, new AstVarRef{flp, m_deltaVarp, VAccess::READ}}};
        AstSenTree* const senp = new AstSenTree{
            flp, senItemFor(new AstVarRef{flp, m_deltaVarp, VAccess::READ}, true, flp)};
        return AstNode::addNext(outp,
                                static_cast<AstNode*>(new AstEventControl{flp, senp, nullptr}));
    }
    AstVar* m_deltaVarp = nullptr;  // The module's delta-cycle signal, if used
    AstVar* m_guardVarp = nullptr;  // The GUARD of the enclosing guarded block

    // A guard expression declares a signal GUARD holding its value, which the
    // block's guarded assignments test before they drive (11.2)
    void lowerGuard(AstVhdlBlock* nodep, FileLine* flp) {
        VhdlSymbol* const symp = nodep->guardSymp();
        if (!symp) return;
        AstNodeExpr* const exprp = lowerCondition(nodep->guardp());
        if (!exprp) return;
        AstVar* const varp = new AstVar{flp, VVarType::VAR,
                                        "__Vguard__" + (nodep->name().empty() ? "block"
                                                                              : nodep->name()),
                                        VFlagChildDType{}, newLogicDType(flp, 1)};
        m_stmtsp = AstNode::addNextNull(m_stmtsp, varp);
        m_vars[symp] = varp;
        m_guardVarp = varp;
        addStmt(new AstAlways{new AstAssignW{
            flp, new AstVarRef{flp, varp, VAccess::WRITE}, exprp, nullptr}});
    }

    AstNode* lowerWait(AstVhdlWait* nodep) {
        FileLine* const flp = nodep->fileline();
        if (!v3Global.opt.timing().isTrue()) {
            unsup(flp, "wait statement without --timing");
            return nullptr;
        }
        if (nodep->condp() && nodep->timeoutp()) {
            // "wait until C for T" resumes on whichever comes first, so both
            // are run as branches of a join_any and the loser is dropped
            AstNode* const condp = lowerWaitCond(nodep, flp);
            AstNodeExpr* const timep = lowerExpr(nodep->timeoutp());
            if (!condp || !timep) {
                if (condp) VL_DO_DANGLING(condp->deleteTree(), condp);
                if (timep) VL_DO_DANGLING(timep->deleteTree(), timep);
                return nullptr;
            }
            AstFork* const forkp = new AstFork{flp, VJoinType::JOIN_ANY, ""};
            forkp->addForksp(new AstBegin{flp, "", condp, false});
            forkp->addForksp(
                new AstBegin{flp, "", lowerTimeout(timep, flp), false});
            return AstNode::addNext(static_cast<AstNode*>(forkp), new AstDisableFork{flp});
        }
        if (nodep->condp()) {
            // "wait until C" always suspends, and resumes when an event makes
            // C true (10.2). Verilog's wait(C) returns at once if C is
            // already true, which for "wait until clk = '1'" written while
            // the clock is high would spin without advancing time. So the
            // process is suspended on an event first, and only then waits.
            return lowerWaitCond(nodep, flp);
        }
        if (nodep->timeoutp()) {
            AstNodeExpr* const timep = lowerExpr(nodep->timeoutp());
            return timep ? lowerTimeout(timep, flp) : nullptr;
        }
        if (nodep->sensp()) {
            AstSenTree* const sensp = sensitivityOf(nodep->sensp(), flp);
            return sensp ? new AstEventControl{flp, sensp, nullptr} : nullptr;
        }
        // A bare "wait" suspends for good: the process never resumes, so
        // whatever follows it never runs again
        return new AstWait{flp, new AstConst{flp, AstConst::BitFalse{}}, nullptr};
    }

    // "wait until C" always suspends, and resumes when an event makes C true
    // (10.2). Verilog's wait(C) returns at once if C is already true, which
    // for "wait until clk = '1'" written while the clock is high would spin
    // without advancing time. So the process is suspended on an event first,
    // and only then waits.
    AstNode* lowerWaitCond(AstVhdlWait* nodep, FileLine* flp) {
        AstNodeExpr* const condp = lowerCondition(nodep->condp());
        if (!condp) return nullptr;
        AstSenTree* sensp = sensitivityOf(nodep->sensp(), flp);
        if (!sensp) sensp = sensitivityOfExpr(condp, flp);
        if (!sensp) return new AstWait{flp, condp, nullptr};
        AstNode* const outp = new AstEventControl{flp, sensp, nullptr};
        return AstNode::addNext(outp, static_cast<AstNode*>(new AstWait{flp, condp, nullptr}));
    }

    AstNode* lowerTimeout(AstNodeExpr* timep, FileLine* flp) {
        // "wait for 0 ns" does not stand still: it resumes in the next delta
        // cycle, after the signal assignments already made have taken effect
        // (10.2, 14.7.5). Verilog's "#0" is not that -- it resumes before the
        // non-blocking region, so the process would read the values it was
        // waiting to see change.
        //
        // A timeout that does advance time has the same problem one step
        // further out. A VHDL simulation cycle updates every signal whose
        // transaction is due, and only then resumes the processes whose
        // timeouts have expired (14.7.5.1), so a process that waits a
        // nanosecond sees what an "after 1 ns" assignment delivered at that
        // instant. Verilog resumes "#1ns" in the active region, before the
        // non-blocking region that delivers it, so the process would read the
        // value the signal had a moment ago. Waiting one delta more puts the
        // process back where the language says it runs.
        if (const AstConst* const constp = VN_CAST(timep, Const)) {
            if (constp->isZero()) {
                VL_DO_DANGLING(timep->deleteTree(), timep);
                return deltaWait(flp);
            }
            if (!m_anyAfter) return new AstDelay{flp, timep, false};
            return AstNode::addNext(static_cast<AstNode*>(new AstDelay{flp, timep, false}),
                                    deltaWait(flp));
        }
        // Not known here, so decide at run time
        AstNodeExpr* const isZerop = new AstEq{
            flp, timep->cloneTree(false), new AstConst{flp, AstConst::Unsized64{}, 0}};
        // The zero case is taken by the other branch, so this delay is never
        // zero however little the back end can see of it
        FileLine* const dflp = new FileLine{flp};
        dflp->warnOff(V3ErrorCode::ZERODLY, true);
        AstNode* elsep = new AstDelay{dflp, timep, false};
        if (m_anyAfter) elsep = AstNode::addNext(elsep, deltaWait(flp));
        return new AstIf{flp, isZerop, deltaWait(flp), elsep};
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
            // A LINE is text here, so emptying it is what deallocating it
            // means; any other access value becomes null (5.4.2)
            if (isLineType(args[0]->vhdlTypep())) {
                return new AstAssign{flp, linep, new AstConst{flp, AstConst::String{}, ""}};
            }
            return new AstAssign{flp, linep, new AstConst{flp, AstConst::Null{}}};
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
    // A signal-class parameter names the caller's own signal, not a copy of
    // it (4.2.2.3): an assignment inside the subprogram drives that signal.
    // There is no reference parameter to carry that, so the subprogram is
    // copied for the signals one call site passes, with the formals bound
    // straight to them -- the same answer monomorphization gives everywhere
    // else in this frontend.
    std::map<const VhdlSymbol*, AstVar*> m_bindSignals;
    // A signal-class formal bound to part of a signal rather than to a whole
    // one -- "drive(r.y, 3)" -- which has no variable of its own to stand
    // for it, so the actual's own name is lowered again at each reference
    std::map<const VhdlSymbol*, AstNodeVhdlExpr*> m_bindExprs;
    std::map<std::string, AstNodeFTask*> m_signalCopies;

    void lowerSubprogram(AstVhdlSubprogDecl* nodep) { lowerSubprogramInto(nodep, ""); }

    AstNodeFTask* lowerSubprogramInto(AstVhdlSubprogDecl* nodep, const std::string& copyName) {
        FileLine* const flp = nodep->fileline();
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp) return nullptr;
        if (copyName.empty() && m_funcs.count(symp)) {
            return nullptr;  // Its declaration already made one
        }
        if (symp->isForeign()) {
            lowerForeignSubprogram(nodep, symp);
            return nullptr;
        }
        if (!nodep->hasBody()) return nullptr;  // A declaration alone emits nothing
        // A subprogram with an unconstrained parameter is a template: each
        // call site got a copy with the lengths it supplies, and only those
        // copies are emitted. Emitting the template as well would mean
        // deciding a length it does not have.
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && !typep->isConstrainedArray() && !isStringType(typep)) {
                return nullptr;
            }
        }
        // The same where only the *result* has no length of its own: the
        // copies made for the call sites carry the lengths their bodies
        // build. STRING is the exception: it is text, carried dynamically,
        // so a function returning one needs no length decided here.
        if (nodep->isFunction()) {
            const VhdlType* const retp = returnType(nodep, symp);
            if (retp && retp->base()->kind() == VhdlTypeKind::ARRAY
                && !retp->isConstrainedArray() && !isStringType(retp)) {
                return nullptr;
            }
        }
        // An overloaded name needs a distinct one in the emitted code
        const std::string name = copyName.empty() ? uniqueName(symp) : copyName;
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
        if (copyName.empty()) m_funcs.emplace(symp, ftaskp);
        VL_RESTORER(m_ftaskp);
        VL_RESTORER_COPY(m_prefix);
        m_ftaskp = ftaskp;
        m_prefix.clear();
        for (AstNode* p = firstParamOf(nodep); p; p = p->nextp()) {
            AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            VhdlSymbol* const psymp = ifp->vhdlSymp();
            if (!psymp) continue;
            // A signal parameter bound to the caller's own signal needs no
            // port of its own: the body refers to that signal directly
            const auto boundIt = m_bindSignals.find(psymp);
            if (boundIt != m_bindSignals.end()) {
                m_vars[psymp] = boundIt->second;
                continue;
            }
            // Bound to part of a signal rather than to a whole one, which
            // likewise needs no port: the body names that part directly
            if (m_bindExprs.count(psymp)) continue;
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
            // A copy of a subprogram shares its formals' symbols with the
            // original, so the mapping is overwritten rather than added to;
            // the caller puts back what was there
            m_vars[psymp] = varp;
            ftaskp->addStmtsp(varp);
        }
        // What this subprogram reaches out of its own region for comes in as
        // extra parameters, ahead of nothing and behind its own
        const auto captureIt = m_captureOf.find(symp->resolved());
        std::vector<std::pair<const VhdlSymbol*, AstVar*>> captureVars;
        if (captureIt != m_captureOf.end()) {
            for (VhdlSymbol* const capp : captureIt->second) {
                AstVar* const varp
                    = new AstVar{flp, VVarType::PORT, "__Vcap__" + capp->name(),
                                 VFlagChildDType{}, lowerType(capp->typep(), flp)};
                // Passed by reference whether or not this subprogram writes
                // it: it may hand it on to another that does, and the object
                // it stands for is the enclosing subprogram's own
                varp->direction(VDirection::INOUT);
                varp->declDirection(varp->direction());
                varp->funcLocal(true);
                ftaskp->addStmtsp(varp);
                captureVars.emplace_back(capp, varp);
            }
        }
        // Inside the body the captured name means the parameter
        std::vector<std::pair<VhdlSymbol*, AstVar*>> savedCaps;
        for (const auto& pair : captureVars) {
            VhdlSymbol* const capp = const_cast<VhdlSymbol*>(pair.first);
            savedCaps.emplace_back(capp, m_vars.count(capp) ? m_vars[capp] : nullptr);
            m_vars[capp] = pair.second;
        }
        // A subprogram declared inside this one sees this one's variables
        // (12.1), which a separate C++ function cannot. Each such reference
        // becomes a parameter of the inner subprogram, passed at every call.
        VL_RESTORER_COPY(m_captureOf);
        for (AstNode* p = nodep->declsp(); p; p = p->nextp()) {
            // Planned as each is reached, not all at once: what a nested
            // subprogram can name is what was declared before it, and those
            // variables do not exist until they have been lowered
            if (AstVhdlSubprogDecl* const subp = VN_CAST(p, VhdlSubprogDecl)) {
                planCaptures(subp);
            }
            lowerDecl(p);
        }
        ftaskp->addStmtsp(lowerStmts(nodep->stmtsp()));
        for (const auto& pair : savedCaps) {
            if (pair.second) {
                m_vars[pair.first] = pair.second;
            } else {
                m_vars.erase(pair.first);
            }
        }
        m_ftaskp = nullptr;  // The task itself belongs to the module
        addStmt(ftaskp);
        return ftaskp;
    }

    // What a nested subprogram reaches out of its own region for, in the
    // order its extra parameters take
    std::map<const VhdlSymbol*, std::vector<VhdlSymbol*>> m_captureOf;

    // Work out which of the enclosing subprogram's objects a nested one uses.
    // Only objects: a nested subprogram, type or constant needs nothing
    // passed, and neither does anything the module itself owns.
    void planCaptures(AstVhdlSubprogDecl* nodep) {
        VhdlSymbol* const symp = nodep->vhdlSymp() ? nodep->vhdlSymp()->resolved() : nullptr;
        if (!symp || m_captureOf.count(symp)) return;
        std::vector<VhdlSymbol*> captures;
        std::set<const VhdlSymbol*> seen;
        // What this subprogram declares itself is not captured
        std::set<const VhdlSymbol*> ownp;
        collectOwnSymbols(nodep, ownp);
        nodep->foreach([&](AstNodeVhdlExpr* exprp) {
            VhdlSymbol* const usedp = exprp->vhdlSymp();
            if (!usedp) return;
            VhdlSymbol* const rootp = usedp->resolved();
            if (ownp.count(rootp)) return;
            const auto it = m_vars.find(rootp);
            if (it == m_vars.end() || !it->second || !it->second->isFuncLocal()) return;
            if (!seen.insert(rootp).second) return;
            captures.push_back(rootp);
        });
        m_captureOf.emplace(symp, captures);
    }

    // Everything a subprogram declares: its parameters, its own declarations,
    // and whatever those declare in turn
    static void collectOwnSymbols(AstVhdlSubprogDecl* nodep, std::set<const VhdlSymbol*>& out) {
        nodep->foreach([&](AstNodeVhdlDecl* declp) {
            if (declp->vhdlSymp()) out.insert(declp->vhdlSymp());
        });
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
        for (AstNode* p = firstParamOf(nodep); p; p = p->nextp()) {
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
        m_actives.clear();
        m_needActive.clear();
        m_delayeds.clear();
        collectActive(entp->declsp());
        if (archp) {
            collectActive(archp->declsp());
            collectActive(archp->stmtsp());
        }
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
            // Every signal now has its variable, so the transaction bits that
            // the statements will mark can be made
            makeActives(flp);
            for (AstNode* p = archp->stmtsp(); p; p = p->nextp()) {
                lowerConcurrentDriver(VN_AS(p, NodeVhdlStmt));
            }
        }
        m_modp->addStmtsp(m_stmtsp);
        m_stmtsp = nullptr;
        m_modp->addStmtsp(m_shadowsp);
        m_shadowsp = nullptr;
        m_deltaVarp = nullptr;
        m_shadows.clear();
        m_netlistp->addModulesp(m_modp);
        m_modp = nullptr;
    }
};

std::set<const VhdlSymbol*> VhdlLowerVisitor::s_partialDriven;

//######################################################################
// Design units

void VhdlLowerVisitor::lowerUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    // Whether any signal transaction in the design is delivered at a time of
    // its own. This decides whether a process whose timeout expires has to
    // stand aside for a signal update in the same cycle, and it is a property
    // of the whole design, so it is answered before any of it is lowered.
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        if (m_anyAfter) break;
        unitp->foreach([this](AstNode* nodep) {
            if (const AstVhdlWaveform* const wfp = VN_CAST(nodep, VhdlWaveform)) {
                if (wfp->afterp()) m_anyAfter = true;
            } else if (const AstVhdlAttrName* const attrp = VN_CAST(nodep, VhdlAttrName)) {
                if (attrp->name() == "delayed" && attrp->argp()) m_anyAfter = true;
            }
        });
    }
    // A Verilog source in the same design may schedule an update of its own,
    // and nothing here can see when
    if (!m_anyAfter) {
        for (const VFileLibName& fl : v3Global.opt.vFiles()) {
            if (!v3Global.opt.fileLanguage(fl.filename()).vhdl()) {
                m_anyAfter = true;
                break;
            }
        }
    }
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
            || VN_IS(unitp, VhdlPackageInst) || VN_IS(unitp, VhdlContextDecl)) {
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
        if (archp) m_archOf[key] = archp->name();
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
