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
    // The names those classes have been given. A generic package instantiated
    // twice has two protected types of the one name, and each needs a class
    // of its own in the emitted code (4.9).
    std::set<std::string> m_protClassNames;
    AstClass* m_protClassp = nullptr;  // Protected type being built, if any
    // The body whose declarations belong to that class, and the type whose
    // own declarative region declares its methods (5.6.2). What is declared
    // elsewhere and merely used from inside belongs to the module.
    AstNode* m_protBodyp = nullptr;
    const VhdlType* m_protTypep = nullptr;
    VarMap m_vars;  // Declaration -> the variable it became
    std::map<std::string, AstNodeVhdlUnit*> m_entities;  // "lib.name" -> entity
    std::set<std::string> m_instantiated;  // Modules something else builds one of
    std::set<std::string> m_knownEntities;  // "lib.entity" of every entity there is
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
    // What a configuration declaration chose for one instance: the entity to
    // build it from, which architecture of that entity, and the binding
    // indication itself, which may carry generic and port maps (3.4.3)
    struct CfgBind final {
        std::string entity;  // Empty means the instance is left unbound
        std::string arch;
        AstVhdlBinding* bindingp = nullptr;
    };
    // What it chose for the instances of one block. A block statement or a
    // generate configures its own instances, so these nest as the design does
    struct CfgScope final {
        std::map<std::string, CfgBind> comps;  // Component name -> its binding
        std::map<std::string, CfgBind> insts;  // Instance label -> its binding
        std::map<std::string, CfgScope> blocks;  // Block or generate label -> its own
    };
    std::map<std::string, CfgScope> m_cfgScopes;  // "lib.entity" -> what configures it
    const CfgScope* m_cfgScopep = nullptr;  // The one for the block being lowered
    // "lib.configuration" -> the entity and architecture it names (3.4)
    std::map<std::string, std::pair<std::string, std::string>> m_configOf;
    std::string m_prefix;  // Name prefix for declarations inside a process
    std::string m_blockPrefix;  // Labels of the blocks being lowered, outermost first
    std::map<const VhdlType*, AstNodeFTask*> m_valueFuncs;  // "T'VALUE" readers, per type
    std::set<std::string> m_instNames;  // Instance labels the module has used
    int m_processNum = 0;  // For naming unlabelled processes
    // Set by a statement that advanced the driver index itself
    bool m_countedDrivers = false;
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
    void addTraceRequest() VL_MT_DISABLED;
    void markPackageSignalRefs() VL_MT_DISABLED;
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
    // Whether an integer type's own range needs more than thirty-two bits
    static bool needsWideInt(const VhdlType* basep) {
        if (!basep || !basep->bounded()) return false;
        const int64_t lo = basep->low();
        const int64_t hi = basep->high();
        return lo < -2147483648LL || hi > 2147483647LL;
    }

    AstNodeDType* lowerType(VhdlType* typep, FileLine* flp) {
        if (!typep || typep->isError()) return newLogicDType(flp, 1);
        VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::ENUM:
            if (enumIsLogic(basep)) return newLogicDType(flp, 1);
            return newLogicDType(flp, bitsFor(basep->literals().size()));
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::UNIVERSAL_INTEGER:
            // A declared integer type may need more than the 32 bits the
            // predefined INTEGER has: "type t is range -2**40 to 2**40" is
            // an ordinary integer type of the language (5.2.3)
            if (needsWideInt(basep)) {
                return new AstBasicDType{flp, VBasicDTypeKwd::LONGINT, VSigning::SIGNED};
            }
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
        std::string name = "__Vprot_" + basep->name();
        while (!m_protClassNames.insert(name).second) {
            name = "__Vprot_" + basep->name() + "__V" + std::to_string(++m_nameNum);
        }
        AstClass* const classp
            = new AstClass{flp, name, m_modp ? m_modp->libname() : ""};
        m_protClasses.emplace(basep, classp);
        v3Global.setHasClasses();
        // The class goes where the object of it does. A protected type's
        // methods call what is visible where the type was declared -- the
        // subprograms of the package it is in, which are emitted into the
        // module that uses them -- so the class has to be able to see them.
        if (m_modp) {
            m_modp->addStmtsp(classp);
        } else {
            m_netlistp->dollarUnitPkgAddp()->addStmtsp(classp);
        }
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
        VL_RESTORER(m_protBodyp);
        VL_RESTORER(m_protTypep);
        VL_RESTORER(m_ftaskp);
        VL_RESTORER(m_stmtsp);
        VL_RESTORER_COPY(m_prefix);
        m_protClassp = classp;
        m_protBodyp = bodyp;
        m_protTypep = basep;
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
            size_t pos = 0;
            for (AstNode* p = callp->argsp(); p; p = p->nextp(), ++pos) {
                AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->actualp()) continue;
                const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
                AstNodeExpr* onep = nullptr;
                // A value that carries its own bounds, handed to a parameter
                // whose subtype fixes them, is copied into one of those
                if (symp && isDynValue(asp->actualp())) {
                    const int at = namep ? symp->paramIndex(namep->name())
                                         : static_cast<int>(pos);
                    VhdlType* const formalp
                        = at >= 0 && at < static_cast<int>(symp->paramTypes().size())
                              ? symp->paramTypes()[at]
                              : nullptr;
                    if (formalp && !carriedDynamically(formalp)
                        && formalp->base()->kind() == VhdlTypeKind::ARRAY) {
                        onep = dynAsFixed(asp->actualp(), formalp, asp->fileline());
                    }
                }
                if (!onep) onep = lowerExpr(asp->actualp());
                refp->addArgsp(new AstArg{asp->fileline(), namep ? namep->name() : "", onep});
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
    // What an access to an unconstrained array became: the elements, and
    // the bounds the allocation gave them
    struct DynAccess final {
        AstClass* classp = nullptr;
        AstVar* datap = nullptr;  // The elements, stored from zero up
        AstVar* leftp = nullptr;  // The index the object's own range starts at
        AstVar* dirp = nullptr;  // 1 for "to", -1 for "downto"
    };
    std::map<const VhdlType*, DynAccess> m_dynAccess;

    // The object an allocator makes for an array whose length it decides
    // (5.4.1). Verilator's dynamic array holds the elements and answers for
    // its own size; the left bound and the direction are the two things a
    // VHDL array value keeps that a dynamic array does not, so the object
    // carries them alongside.
    AstClass* dynamicAccessClass(VhdlType* basep, FileLine* flp) {
        AstClass* const classp = new AstClass{
            flp, "__Vdyn" + std::to_string(++m_nameNum) + "__" + basep->name(),
            m_modp ? m_modp->libname() : ""};
        m_accClasses.emplace(basep, classp);
        v3Global.setHasClasses();
        m_netlistp->dollarUnitPkgAddp()->addStmtsp(classp);
        VhdlType* const designatedp = basep->designatedp();
        VhdlType* const elemp = designatedp->base()->elementp();
        AstVar* const datap
            = new AstVar{flp, VVarType::MEMBER, "all", VFlagChildDType{},
                         new AstDynArrayDType{flp, VFlagChildDType{},
                                              elemp ? lowerType(elemp, flp)
                                                    : newLogicDType(flp, 1)}};
        AstVar* const leftp = new AstVar{flp, VVarType::MEMBER, "__Vleft", VFlagChildDType{},
                                         newIntDType(flp)};
        AstVar* const dirp = new AstVar{flp, VVarType::MEMBER, "__Vdir", VFlagChildDType{},
                                        newIntDType(flp)};
        for (AstVar* const varp : {datap, leftp, dirp}) {
            varp->funcLocal(false);
            varp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
            classp->addStmtsp(varp);
        }
        // "new(count, left, dir)". The elements start at the element type's
        // own default, which is where a dynamic array starts them anyway.
        AstFunc* const newp = new AstFunc{flp, "new", nullptr, nullptr};
        newp->isConstructor(true);
        newp->classMethod(true);
        newp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        newp->dtypep(newp->findVoidDType());
        AstVar* const nArgp
            = new AstVar{flp, VVarType::PORT, "__Vn", VFlagChildDType{}, newIntDType(flp)};
        AstVar* const lArgp
            = new AstVar{flp, VVarType::PORT, "__Vl", VFlagChildDType{}, newIntDType(flp)};
        AstVar* const dArgp
            = new AstVar{flp, VVarType::PORT, "__Vd", VFlagChildDType{}, newIntDType(flp)};
        for (AstVar* const argp : {nArgp, lArgp, dArgp}) {
            argp->direction(VDirection::INPUT);
            argp->declDirection(VDirection::INPUT);
            argp->funcLocal(true);
            argp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
            newp->addStmtsp(argp);
        }
        newp->addStmtsp(new AstAssign{
            flp, new AstVarRef{flp, datap, VAccess::WRITE},
            new AstNewDynamic{flp, new AstVarRef{flp, nArgp, VAccess::READ}, nullptr}});
        newp->addStmtsp(new AstAssign{flp, new AstVarRef{flp, leftp, VAccess::WRITE},
                                      new AstVarRef{flp, lArgp, VAccess::READ}});
        newp->addStmtsp(new AstAssign{flp, new AstVarRef{flp, dirp, VAccess::WRITE},
                                      new AstVarRef{flp, dArgp, VAccess::READ}});
        classp->addStmtsp(newp);
        m_dynAccess.emplace(basep, DynAccess{classp, datap, leftp, dirp});
        return classp;
    }

    // The class an access to an unconstrained array became, or null if this
    // type is not one. Building it on demand means an access type that is
    // only ever null costs nothing.
    const DynAccess* dynAccessOf(VhdlType* typep, FileLine* flp) {
        if (!typep) return nullptr;
        VhdlType* const basep = typep->base();
        if (!isDynamicAccess(basep)) return nullptr;
        accessClass(basep, flp);
        const auto it = m_dynAccess.find(basep);
        return it == m_dynAccess.end() ? nullptr : &it->second;
    }

    // What an index, a slice or an array attribute is really on when what is
    // written is an access value, or a dereference of one written out in
    // full. Null if this is not an access to an unconstrained array.
    AstNodeVhdlExpr* dynAccessObj(AstNodeVhdlExpr* nodep, const DynAccess*& dynp) {
        dynp = dynAccessOfExpr(nodep);
        if (dynp) return nodep;
        AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected);
        if (selp && selp->isAll()) {
            dynp = dynAccessOfExpr(selp->prefixp());
            if (dynp) return selp->prefixp();
        }
        return nullptr;
    }

    // The same, for what an expression denotes
    const DynAccess* dynAccessOfExpr(AstNodeVhdlExpr* nodep) {
        if (!nodep) return nullptr;
        VhdlType* typep = nodep->vhdlTypep();
        if (!typep && nodep->vhdlSymp()) typep = nodep->vhdlSymp()->typep();
        return dynAccessOf(typep, nodep->fileline());
    }

    // A 32-bit signed integer, which is what a bound is
    static AstNodeDType* newIntDType(FileLine* flp) {
        return new AstBasicDType{flp, VBasicDTypeKwd::INTEGER, VSigning::SIGNED};
    }

    // An expression that may be evaluated more than once because a bound has
    // to be read from the same object the elements are. A name is; anything
    // that could run a function or change something is not.
    static bool repeatable(const AstNodeExpr* nodep) {
        if (!nodep) return false;
        if (VN_IS(nodep, VarRef) || VN_IS(nodep, Const)) return true;
        if (const AstMemberSel* const memp = VN_CAST(nodep, MemberSel)) {
            return repeatable(memp->fromp());
        }
        if (const AstStructSel* const strp = VN_CAST(nodep, StructSel)) {
            return repeatable(strp->fromp());
        }
        // One element of an array of them -- "reg(i).state.list" -- names the
        // same object each time so long as which element it is cannot change
        // between the reads, which are next to each other in what is emitted
        if (const AstSelBit* const selp = VN_CAST(nodep, SelBit)) {
            return repeatable(selp->fromp()) && selp->bitp()
                   && const_cast<AstNodeExpr*>(selp->bitp())->isPure();
        }
        if (const AstArraySel* const selp = VN_CAST(nodep, ArraySel)) {
            return repeatable(selp->fromp()) && selp->bitp()
                   && const_cast<AstNodeExpr*>(selp->bitp())->isPure();
        }
        return false;
    }

    // The bounds of the array an access value points at, which the object
    // carries because its type does not (5.4.1). Each reads the object
    // again, so the object must be a name.
    AstNodeExpr* dynLeft(const DynAccess* dynp, AstNodeExpr* objp, FileLine* flp) {
        return memberOf(objp->cloneTree(false), dynp->leftp->name(), flp);
    }
    AstNodeExpr* dynAscending(const DynAccess* dynp, AstNodeExpr* objp, FileLine* flp) {
        return new AstGt{flp, memberOf(objp->cloneTree(false), dynp->dirp->name(), flp),
                         new AstConst{flp, AstConst::Signed32{}, 0}};
    }
    AstNodeExpr* dynLength(const DynAccess* dynp, AstNodeExpr* objp, FileLine* flp) {
        return new AstMethodCall{flp, memberOf(objp->cloneTree(false), dynp->datap->name(), flp),
                                 "size", nullptr};
    }
    // "left + dir * (length - 1)", which is the other end of the range
    AstNodeExpr* dynRight(const DynAccess* dynp, AstNodeExpr* objp, FileLine* flp) {
        AstNodeExpr* const spanp
            = new AstSub{flp, dynLength(dynp, objp, flp),
                         new AstConst{flp, AstConst::Signed32{}, 1}};
        return new AstAdd{flp, dynLeft(dynp, objp, flp),
                          new AstMul{flp, memberOf(objp->cloneTree(false), dynp->dirp->name(),
                                                   flp),
                                     spanp}};
    }
    AstNodeExpr* dynLow(const DynAccess* dynp, AstNodeExpr* objp, FileLine* flp) {
        return new AstCond{flp, dynAscending(dynp, objp, flp), dynLeft(dynp, objp, flp),
                           dynRight(dynp, objp, flp)};
    }
    AstNodeExpr* dynHigh(const DynAccess* dynp, AstNodeExpr* objp, FileLine* flp) {
        return new AstCond{flp, dynAscending(dynp, objp, flp), dynRight(dynp, objp, flp),
                           dynLeft(dynp, objp, flp)};
    }
    // The attribute an array's own bounds answer, read from the object
    AstNodeExpr* dynAttr(const DynAccess* dynp, AstNodeExpr* objp,
                         const std::string& attr, FileLine* flp) {
        if (attr == "left") return dynLeft(dynp, objp, flp);
        if (attr == "right") return dynRight(dynp, objp, flp);
        if (attr == "low") return dynLow(dynp, objp, flp);
        if (attr == "high") return dynHigh(dynp, objp, flp);
        if (attr == "ascending") return dynAscending(dynp, objp, flp);
        if (attr == "length") return dynLength(dynp, objp, flp);
        return nullptr;
    }

    // A member of the object an access value points at, by name, the way a
    // dereference is written
    static AstNodeExpr* memberOf(AstNodeExpr* objp, const std::string& name, FileLine* flp) {
        return new AstMemberSel{flp, objp, VFlagChildDType{}, name};
    }

    // "(i - left) * dir", the index into the elements as they are stored,
    // from an index on the object's own range
    AstNodeExpr* dynIndex(const DynAccess* dynp, AstNodeExpr* objp, AstNodeExpr* idxp,
                          FileLine* flp) {
        AstNodeExpr* const offsetp
            = new AstSub{flp, idxp, memberOf(objp, dynp->leftp->name(), flp)};
        return new AstMul{flp, offsetp,
                          memberOf(objp->cloneTree(false), dynp->dirp->name(), flp)};
    }

    // Whether what an access type points at is an array whose length is
    // only fixed when it is allocated. The object then carries its own
    // bounds, the way GHDL and NVC carry them on every array value -- but
    // only here, where there is no other way to know them.
    static bool isDynamicAccess(const VhdlType* basep) {
        if (!basep || basep->kind() != VhdlTypeKind::ACCESS) return false;
        const VhdlType* const designatedp = basep->designatedp();
        if (!designatedp) return false;
        if (isStringType(designatedp)) return false;  // A LINE is text
        const VhdlType* const dbasep = designatedp->base();
        if (dbasep->kind() != VhdlTypeKind::ARRAY) return false;
        return !designatedp->isConstrainedArray() && dbasep->indexTypes().size() == 1;
    }

    AstClass* accessClass(VhdlType* basep, FileLine* flp) {
        const auto it = m_accClasses.find(basep);
        if (it != m_accClasses.end()) return it->second;
        if (isDynamicAccess(basep)) return dynamicAccessClass(basep, flp);
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

    // Whether a declaration is one of the protected body being built, rather
    // than something declared elsewhere that its methods happen to use
    bool inProtectedBody(const VhdlSymbol* symp) const {
        if (!m_protBodyp || !symp) return false;
        // A method is declared in the protected type and again in its body,
        // and a call may name either
        if (m_protTypep && symp->declScopep() && symp->declScopep() == m_protTypep->scopep()) {
            return true;
        }
        const AstNode* declp = symp->bodyp() ? static_cast<const AstNode*>(symp->bodyp())
                                             : symp->declp();
        for (const AstNode* p = declp; p; p = p->backp()) {
            if (p == m_protBodyp) return true;
        }
        return false;
    }

    // The object a method is called on, which the analyzer left on the prefix
    // of the selected name (5.6.2). Null if this is not such a call.
    AstNodeExpr* protectedInstance(AstNodeVhdlExpr* objNamep,
                                   const VhdlType** typepp = nullptr) {
        if (!objNamep) return nullptr;
        VhdlSymbol* const objp = objNamep->vhdlSymp();
        if (!objp || !objp->isObject() || !objp->typep()) return nullptr;
        if (objp->typep()->base()->kind() != VhdlTypeKind::PROTECTED) return nullptr;
        if (typepp) *typepp = objp->typep()->base();
        AstVar* const varp = objectVar(objp, objNamep->fileline());
        return varp ? new AstVarRef{objNamep->fileline(), varp, VAccess::READ} : nullptr;
    }

    // What one of a protected type's methods is called in the emitted code
    std::string methodName(VhdlSymbol* symp, const VhdlType* protTypep = nullptr) {
        const AstNodeFTask* ftaskp = emittedFunc(symp);
        if (!ftaskp) ftaskp = emitMethodOnDemand(symp, protTypep);
        return ftaskp ? ftaskp->name() : uniqueName(symp);
    }

    // A method may be called before the body that defines it has been
    // reached: one method's body may call a subprogram declared elsewhere
    // that calls back into the object. Emit it into its own class now,
    // rather than into whatever is being built at the time (5.6.2).
    AstNodeFTask* emitMethodOnDemand(VhdlSymbol* symp, const VhdlType* protTypep) {
        if (!symp) return nullptr;
        // The type is known where the call names an object of it; a
        // specialized copy of a method is declared in the type's own region,
        // which says which type it belongs to when nothing else does
        if (!protTypep && symp->declScopep()) {
            for (const auto& pair : m_protClasses) {
                if (pair.first->scopep() == symp->declScopep()) protTypep = pair.first;
            }
        }
        if (!protTypep) return nullptr;
        const auto it = m_protClasses.find(protTypep);
        if (it == m_protClasses.end()) return nullptr;
        VL_RESTORER(m_protClassp);
        VL_RESTORER(m_protBodyp);
        VL_RESTORER(m_protTypep);
        m_protClassp = it->second;
        m_protTypep = protTypep;
        m_protBodyp = protTypep->bodyp();
        emitOnDemand(symp);
        return emittedFunc(symp);
    }

    // What a subprogram became. A method is declared in the protected type
    // and again in its body (5.6.2), and a call may name either symbol, so
    // both find the one function that was emitted.
    AstNodeFTask* emittedFunc(VhdlSymbol* symp) {
        if (!symp) return nullptr;
        auto it = m_funcs.find(symp);
        if (it != m_funcs.end()) return it->second;
        if (symp->bodyp() && symp->bodyp()->vhdlSymp()) {
            it = m_funcs.find(symp->bodyp()->vhdlSymp());
            if (it != m_funcs.end()) return it->second;
        }
        if (AstVhdlSubprogDecl* const declp = VN_CAST(symp->declp(), VhdlSubprogDecl)) {
            if (declp->vhdlSymp() && declp->vhdlSymp() != symp) {
                it = m_funcs.find(declp->vhdlSymp());
                if (it != m_funcs.end()) return it->second;
            }
        }
        return nullptr;
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

    // An array indexed by a logic type -- "array (STD_ULOGIC) of X", which is
    // how a design writes a table over the nine values -- is indexed here by
    // the *position* of the value, while a value of such a type is carried as
    // one bit. The bit is turned back into a position at the index.
    AstNodeExpr* logicIndex(const VhdlType* arrayTypep, size_t dim, AstNodeExpr* indexp,
                            FileLine* flp) {
        if (!arrayTypep || !indexp) return indexp;
        const std::vector<VhdlType*>& idxs = arrayTypep->base()->indexTypes();
        if (dim >= idxs.size() || !idxs[dim]) return indexp;
        const VhdlType* const idxp = idxs[dim]->base();
        if (!enumIsLogic(idxp)) return indexp;
        int64_t zero = -1;
        int64_t one = -1;
        for (const VhdlSymbol* const litp : idxp->literals()) {
            if (litp->name() == "'0'") zero = litp->position();
            if (litp->name() == "'1'") one = litp->position();
        }
        if (zero < 0 || one < 0) return indexp;
        // Only '0' and '1' survive the two-state model, so those are the two
        // positions a value can stand for (see the accepted divergences)
        return new AstCond{flp, indexp,
                           new AstConst{flp, AstConst::Signed32{},
                                        static_cast<int32_t>(one)},
                           new AstConst{flp, AstConst::Signed32{},
                                        static_cast<int32_t>(zero)}};
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

    // Whether an operand is a literal with a Z in it. Z is the one metavalue
    // this runtime keeps, so a comparison against it has to be the exact one;
    // 'U', 'X', 'W' and '-' all collapse to Verilog's X on both sides of an
    // ordinary comparison, which is what the accepted divergence says.
    static bool hasMetaConst(const AstNodeExpr* nodep) {
        const AstConst* const constp = VN_CAST(nodep, Const);
        return constp && constp->num().isAnyZ();
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
            // The test asks about the array, not only about the index
            // subtype: an unconstrained bit_vector has index subtype NATURAL,
            // which is perfectly bounded, so asking the index alone lets a
            // type that nothing ever constrained pass for a real array of
            // 2**31 elements and be reported as one
            if (!idxp || !idxp->bounded() || typep->dimOpen(i)) {
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
                // A null array has no elements at all (5.3.2.1). Verilog has
                // no zero-width type, so one element stands for it: nothing
                // may index it, and every value of it is equal to every
                // other, which is what a null value means. Its 'LENGTH still
                // folds to zero, since that comes from the type and not from
                // the width emitted here.
                const int64_t left = idxp->left();
                const int64_t right = idxp->length() <= 0 ? left : idxp->right();
                rangep = new AstRange{flp, static_cast<int>(left),
                                      static_cast<int>(right)};
            }
            // A vector of single bits is what Verilog writes as "logic
            // [7:0]", which is a type with a range rather than an array of
            // one-bit elements. Saying it that way keeps the range as it was
            // written -- including a range that runs upward, which an array
            // of bits cannot carry through to a waveform.
            const AstBasicDType* const elemBasicp = VN_CAST(dtypep, BasicDType);
            if (packed && elemBasicp && elemBasicp->isBitLogic() && !elemBasicp->isRanged()
                && elemBasicp->width() == 1 && VN_IS(rangep->leftp(), Const)
                && VN_IS(rangep->rightp(), Const)) {
                const int left = VN_AS(rangep->leftp(), Const)->toSInt();
                const int right = VN_AS(rangep->rightp(), Const)->toSInt();
                // Only where the range ends at zero, which is what Verilog
                // writes and what its packed types are built for. A vector
                // indexed from somewhere else stays an array of bits.
                if (left == 0 || right == 0) {
                    AstNodeDType* const vectorp
                        = new AstBasicDType{flp, elemBasicp->keyword(), VSigning::NOSIGN,
                                            VNumRange{left, right}, -1};
                    VL_DO_DANGLING(rangep->deleteTree(), rangep);
                    VL_DO_DANGLING(dtypep->deleteTree(), dtypep);
                    dtypep = vectorp;
                    continue;
                }
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

    // What "x'RANGE" runs over: the index subtype of what the attribute's
    // prefix denotes, which is where a range attribute gets its bounds
    static const VhdlType* attrRangeSubtype(const AstNodeVhdl* attrNodep) {
        const AstVhdlAttrName* const attrp = VN_CAST(attrNodep, VhdlAttrName);
        if (!attrp || !attrp->prefixp()) return nullptr;
        const VhdlType* ptypep = attrp->prefixp()->vhdlTypep();
        if (!ptypep && attrp->prefixp()->vhdlSymp()) {
            ptypep = attrp->prefixp()->vhdlSymp()->typep();
        }
        return indexSubtype(ptypep, 0);
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
                // A length that names a generic is a length: the element is
                // still a vector, and the structure is still one word. Asking
                // only whether it folds now would make a record whose element
                // an instance settles unpacked in the entity and packed in
                // the architecture that connects to it.
                if (!etypep->lengthEmittable()) return false;
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

    // Whether an element type can live inside a packed array
    static bool elementPacks(const VhdlType* typep) {
        // A STRING is text here rather than a vector of characters, so an
        // array of them is an array of objects, not of bits (16.3)
        if (isStringType(typep)) return false;
        const VhdlType* const basep = typep->base();
        switch (basep->kind()) {
        case VhdlTypeKind::ENUM: return true;
        case VhdlTypeKind::ARRAY: return basep->elementp() && elementPacks(basep->elementp());
        // An array of records is a packed array of packed structures where
        // every element of the record packs, which is what lets a port of one
        // connect by width rather than by C++ structure type
        case VhdlTypeKind::RECORD: return recordPacks(typep);
        default: return false;
        }
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
        // One structure per record *type*, not per subtype of it: two values
        // are assignment-compatible when they share a base type (5.2), so the
        // C++ representation has to be the same one.
        //
        // Except where the record has an element of an unconstrained array
        // type. Then the base says nothing about how long that element is,
        // and each object takes its length from what constrains it (5.3.3) --
        // so those really are different structures, and are kept apart by
        // subtype as before.
        // A STRING element is not one of them: it is carried as a string,
        // which is the same member however long the value is.
        bool openElement = false;
        for (VhdlSymbol* const elemp : typep->base()->elements()) {
            VhdlType* const etp = elemp->typep();
            if (etp && etp->base()->kind() == VhdlTypeKind::ARRAY && etp->unconstrained()
                && !isStringType(etp)) {
                openElement = true;
            }
        }
        VhdlType* const keyp = openElement ? typep : typep->base();
        // Where an element's length is a generic rather than a number, the
        // structure is not one shape but one per set of generics, and only
        // the module it belongs to is cloned per instance. Declaring it in
        // the shared unit would leave a reference to a module's generic
        // outside that module, where nothing ever gives it a value.
        const bool local = m_modp && dependsOnGeneric(keyp);
        if (local) {
            const auto foundl = m_recLocalNames.find({m_modp, keyp});
            if (foundl != m_recLocalNames.end()) return new AstRefDType{flp, foundl->second};
        } else {
            const auto found = m_recNames.find(keyp);
            if (found != m_recNames.end()) return new AstRefDType{flp, found->second};
        }
        const std::string name
            = "__Vrec" + std::to_string(++m_nameNum) + "__" + keyp->name();
        if (local) {
            m_recLocalNames.emplace(std::make_pair(m_modp, keyp), name);
        } else {
            m_recNames.emplace(keyp, name);
        }
        AstStructDType* const structp = new AstStructDType{flp, VSigning::NOSIGN};
        structp->name(name);
        // In the one unit every design has, so that a class built for an
        // access or protected type sees the same declaration a module does
        AstPackage* const unitp = local ? nullptr : m_netlistp->dollarUnitPkgAddp();
        if (unitp) structp->classOrPackagep(unitp);
        for (VhdlSymbol* const elemp : keyp->elements()) {
            structp->addMembersp(new AstMemberDType{flp, elemp->name(), VFlagChildDType{},
                                                    lowerType(elemp->typep(), flp), nullptr});
        }
        AstTypedef* const tdp = new AstTypedef{flp, name, nullptr, VFlagChildDType{}, structp};
        if (local) {
            m_modp->addStmtsp(tdp);
        } else {
            unitp->addStmtsp(tdp);
        }
        return new AstRefDType{flp, name};
    }

    // Whether any bound inside this type is an expression rather than a
    // number, which for a port constrained by its actual means a generic
    static bool dependsOnGeneric(VhdlType* typep, int depth = 0) {
        if (!typep || depth > 16) return false;
        VhdlType* const basep = typep->base();
        if (basep->kind() == VhdlTypeKind::ARRAY) {
            for (const VhdlType* const idxp : typep->indexTypes()) {
                // A range that did not fold but was written out is one an
                // instance settles -- a bound that names a generic. An index
                // with no range at all is open, and lowering rejects that
                // anyway, so it is not what this is about.
                if (idxp && !idxp->bounded() && idxp->boundsp()) return true;
            }
            VhdlType* const elemp = typep->elementp() ? typep->elementp() : basep->elementp();
            return dependsOnGeneric(elemp, depth + 1);
        }
        if (basep->kind() == VhdlTypeKind::RECORD) {
            for (VhdlSymbol* const elemp : typep->elements()) {
                if (dependsOnGeneric(elemp->typep(), depth + 1)) return true;
            }
        }
        return false;
    }
    // A value of an unconstrained array type that no call site can measure is
    // carried with its bounds beside it: the elements in a dynamic array,
    // which knows its own length, and the left and right the value was made
    // with. A copy per call site -- which is what every length that can be
    // known takes, and so what all synthesizable code takes -- cannot serve
    // here, because the length is not decided until the value is built.
    std::map<const VhdlType*, std::string> m_dynNames;  // Array type -> its carrier
    std::set<const VhdlSymbol*> m_dynSyms;  // Objects held in one

    // Whether a bound is one only running decides. A bound that names a
    // generic or a constant is settled when the instance is elaborated, and
    // the back end folds it per instance -- that is tier one, and it is what
    // all synthesizable code takes. A bound that names a variable, a signal
    // or a subprogram parameter is not settled until the declaration is
    // reached, and an object declared with one has to carry its own length.
    bool boundsOnlyRunningDecides(const AstNode* nodep, int depth = 0) const {
        if (!nodep || depth > 32) return false;
        for (const AstNode* p = nodep; p; p = p->nextp()) {
            // An attribute asks about an object's subtype, not about its
            // value, and a subtype is settled when the object is elaborated
            // -- "signed(1 to y'length)" over a port is a length the back end
            // folds per instance. Unless the object is itself one of these,
            // and then its length is not known until it is handed over.
            if (const AstVhdlAttrName* const attrp = VN_CAST(p, VhdlAttrName)) {
                if (attrp->prefixp() && attrp->prefixp()->vhdlSymp()
                    && m_dynSyms.count(attrp->prefixp()->vhdlSymp()->resolved())) {
                    return true;
                }
                if (boundsOnlyRunningDecides(attrp->argp(), depth + 1)) return true;
                continue;
            }
            if (const AstNodeVhdlExpr* const exprp = VN_CAST(p, NodeVhdlExpr)) {
                if (const VhdlSymbol* const symp = exprp->vhdlSymp()) {
                    switch (symp->resolved()->kind()) {
                    case VhdlSymKind::VARIABLE:
                    case VhdlSymKind::SHARED_VARIABLE:
                    case VhdlSymKind::SIGNAL:
                    case VhdlSymKind::PORT:
                    case VhdlSymKind::PARAMETER: return true;
                    default: break;
                    }
                }
            }
            for (int i = 0; i < 4; ++i) {
                if (boundsOnlyRunningDecides(p->op1p(), depth + 1)) return true;
                if (boundsOnlyRunningDecides(p->op2p(), depth + 1)) return true;
                if (boundsOnlyRunningDecides(p->op3p(), depth + 1)) return true;
                if (boundsOnlyRunningDecides(p->op4p(), depth + 1)) return true;
                break;
            }
        }
        return false;
    }

    // Whether a parameter is one the call only reads. A value carried with
    // its own bounds is passed by value, so a parameter the body writes
    // could not be given back to the caller; those keep the honest error
    // until the carrier is passed by reference.
    bool paramIsRead(const VhdlSymbol* symp, size_t i) const {
        const AstVhdlSubprogDecl* declp = symp->bodyp();
        if (!declp) declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
        if (!declp) return false;
        size_t k = 0;
        for (const AstNode* p = firstParamOf(declp); p; p = p->nextp(), ++k) {
            const AstVhdlIfaceDecl* const ifp = VN_CAST(p, VhdlIfaceDecl);
            if (!ifp) continue;
            if (k == i) return ifp->mode() == VhdlMode::IN;
        }
        return false;
    }

    // Whether this parameter is carried with its own bounds. A foreign
    // subprogram's is never one: C has no way to be handed a carrier, and
    // the honest error about the length is the answer there (16.2).
    bool paramCarried(const VhdlSymbol* symp, size_t i) const {
        if (symp->isForeign()) return false;
        return carriedDynamically(symp->paramTypes()[i]) && paramIsRead(symp, i);
    }

    // Whether an object of this subtype is one of those
    bool lengthOnlyRunningDecides(const VhdlType* typep) const {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return false;
        for (const VhdlType* const idxp : typep->indexTypes()) {
            if (!idxp) continue;
            if (idxp->bounded()) continue;
            // A bound that folds is settled however it was written. Inside a
            // copy made for one call site "q'length" is a number, so a record
            // element as long as it is an ordinary member of an ordinary
            // structure and not one of these at all.
            if (const AstVhdlRange* const rp = VN_CAST(idxp->boundsp(), VhdlRange)) {
                if (rp->leftp() && rp->rightp()
                    && V3VhdlEval::eval(rp->leftp()).isInt()
                    && V3VhdlEval::eval(rp->rightp()).isInt()) {
                    continue;
                }
            }
            if (boundsOnlyRunningDecides(idxp->boundsp())) return true;
        }
        return false;
    }

    // Whether a value of this type has to be carried that way
    bool carriedDynamically(const VhdlType* typep) const {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        if (basep->kind() != VhdlTypeKind::ARRAY) return false;
        if (isStringType(typep)) return false;
        if (typep->lengthEmittable() && !lengthOnlyRunningDecides(typep)) return false;
        // One dimension, and an element that is itself a value of a length
        // this can emit -- an array of unconstrained arrays would need a
        // carrier per element, which is a further step
        if (basep->indexTypes().size() != 1) return false;
        const VhdlType* const elemp = typep->elementp() ? typep->elementp() : basep->elementp();
        if (!elemp) return false;
        if (elemp->base()->kind() == VhdlTypeKind::ARRAY && !elemp->lengthEmittable()) {
            return false;
        }
        return true;
    }

    // The carrier structure for such a type, declared once and named, so that
    // every use of the type refers to the same C++ structure
    AstNodeDType* dynCarrierRef(VhdlType* typep, FileLine* flp) {
        VhdlType* const basep = typep->base();
        VhdlType* const elemp = typep->elementp() ? typep->elementp() : basep->elementp();
        const auto found = m_dynNames.find(basep);
        if (found != m_dynNames.end()) return new AstRefDType{flp, found->second};
        const std::string name = "__Vdyn" + std::to_string(++m_nameNum) + "__" + basep->name();
        m_dynNames.emplace(basep, name);
        AstStructDType* const structp = new AstStructDType{flp, VSigning::NOSIGN};
        structp->name(name);
        AstPackage* const unitp = m_netlistp->dollarUnitPkgAddp();
        if (unitp) structp->classOrPackagep(unitp);
        structp->addMembersp(new AstMemberDType{
            flp, "q", VFlagChildDType{},
            new AstDynArrayDType{flp, VFlagChildDType{}, lowerType(elemp, flp)}, nullptr});
        structp->addMembersp(new AstMemberDType{flp, "lft", VFlagChildDType{},
                                                newIntDType(flp), nullptr});
        structp->addMembersp(new AstMemberDType{flp, "rgt", VFlagChildDType{},
                                                newIntDType(flp), nullptr});
        structp->addMembersp(new AstMemberDType{
            flp, "dsc", VFlagChildDType{}, new AstBasicDType{flp, VBasicDTypeKwd::BIT}, nullptr});
        AstTypedef* const tdp = new AstTypedef{flp, name, nullptr, VFlagChildDType{}, structp};
        if (unitp) unitp->addStmtsp(tdp);
        return new AstRefDType{flp, name};
    }

    // The parts of a carrier
    static AstNodeExpr* dynPart(AstNodeExpr* fromp, const std::string& part, FileLine* flp) {
        return new AstMemberSel{flp, fromp, VFlagChildDType{}, part};
    }
    // Where the element with this index sits in the carrier's array, which is
    // counted from zero however the value's own range runs
    static AstNodeExpr* dynOffset(AstNodeExpr* fromp, AstNodeExpr* indexp, FileLine* flp) {
        return new AstCond{
            flp, dynPart(fromp->cloneTree(false), "dsc", flp),
            new AstSub{flp, dynPart(fromp->cloneTree(false), "lft", flp),
                       indexp->cloneTree(false)},
            new AstSub{flp, indexp, dynPart(fromp->cloneTree(false), "lft", flp)}};
    }
    // The attributes a carrier answers from the bounds it holds
    static AstNodeExpr* dynCarrierLength(AstNodeExpr* fromp, FileLine* flp) {
        return new AstMethodCall{flp, dynPart(fromp, "q", flp), "size", nullptr};
    }
    static AstNodeExpr* dynCarrierAscending(AstNodeExpr* fromp, FileLine* flp) {
        return new AstLogNot{flp, dynPart(fromp, "dsc", flp)};
    }
    static AstNodeExpr* dynCarrierAttr(AstNodeExpr* fromp, const std::string& attr,
                                       FileLine* flp) {
        if (attr == "left") return dynPart(fromp, "lft", flp);
        if (attr == "right") return dynPart(fromp, "rgt", flp);
        if (attr == "length") return dynCarrierLength(fromp, flp);
        if (attr == "ascending") return dynCarrierAscending(fromp, flp);
        if (attr == "low") {
            return new AstCond{flp, dynPart(fromp->cloneTree(false), "dsc", flp),
                               dynPart(fromp->cloneTree(false), "rgt", flp),
                               dynPart(fromp, "lft", flp)};
        }
        if (attr == "high") {
            return new AstCond{flp, dynPart(fromp->cloneTree(false), "dsc", flp),
                               dynPart(fromp->cloneTree(false), "lft", flp),
                               dynPart(fromp, "rgt", flp)};
        }
        return nullptr;
    }

    // Give a carrier the bounds the declaration wrote and room for that many
    // elements. Nothing else knows how long the value is, so this is where
    // the length is settled -- once, when the declaration is elaborated
    // (14.4.2.5), which is what makes a later assignment to a bound harmless.
    AstNode* dynCarrierInit(AstVar* varp, const VhdlType* typep, FileLine* flp) {
        const VhdlType* const idxp
            = typep->indexTypes().empty() ? nullptr : typep->indexTypes()[0];
        AstNodeExpr* leftp = nullptr;
        AstNodeExpr* rightp = nullptr;
        if (idxp && idxp->bounded()) {
            leftp = new AstConst{flp, AstConst::Signed32{},
                                 static_cast<int32_t>(idxp->left())};
            rightp = new AstConst{flp, AstConst::Signed32{},
                                  static_cast<int32_t>(idxp->right())};
        } else if (AstRange* const rangep = boundsRange(idxp, flp)) {
            leftp = rangep->leftp()->unlinkFrBack();
            rightp = rangep->rightp()->unlinkFrBack();
            VL_DO_DANGLING(rangep->deleteTree(), rangep);
        } else {
            return nullptr;
        }
        const auto ref = [&]() { return new AstVarRef{flp, varp, VAccess::WRITE}; };
        AstNode* outp = new AstAssign{flp, dynPart(ref(), "lft", flp), leftp};
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(
                      new AstAssign{flp, dynPart(ref(), "rgt", flp), rightp}));
        outp = AstNode::addNext(
            outp,
            static_cast<AstNode*>(new AstAssign{
                flp, dynPart(ref(), "dsc", flp),
                new AstLtS{flp, dynPart(new AstVarRef{flp, varp, VAccess::READ}, "rgt", flp),
                           dynPart(new AstVarRef{flp, varp, VAccess::READ}, "lft", flp)}}));
        // A null range holds nothing at all (5.3.2.1), so the count is never
        // negative however the bounds were written
        AstNodeExpr* const spanp = new AstSub{
            flp,
            new AstCond{flp, dynPart(new AstVarRef{flp, varp, VAccess::READ}, "dsc", flp),
                        dynPart(new AstVarRef{flp, varp, VAccess::READ}, "lft", flp),
                        dynPart(new AstVarRef{flp, varp, VAccess::READ}, "rgt", flp)},
            new AstCond{flp, dynPart(new AstVarRef{flp, varp, VAccess::READ}, "dsc", flp),
                        dynPart(new AstVarRef{flp, varp, VAccess::READ}, "rgt", flp),
                        dynPart(new AstVarRef{flp, varp, VAccess::READ}, "lft", flp)}};
        AstNodeExpr* const countp
            = new AstCond{flp,
                          new AstLtS{flp, spanp->cloneTree(false),
                                     new AstConst{flp, AstConst::Signed32{}, 0}},
                          new AstConst{flp, AstConst::Signed32{}, 0},
                          new AstAdd{flp, spanp, new AstConst{flp, AstConst::Signed32{}, 1}}};
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(new AstAssign{
                      flp, dynPart(ref(), "q", flp),
                      new AstNewDynamic{flp, countp, nullptr}}));
        return outp;
    }

    // Put a value into a carrier. Another carrier is copied whole; a value
    // whose length is known is copied element by element, since it is stored
    // from zero here and from its own left bound there.
    AstNode* dynCarrierAssign(AstNodeExpr* targetp, AstNodeVhdlExpr* valuep, FileLine* flp) {
        if (isDynValue(valuep)) {
            AstNodeExpr* const srcp = lowerExpr(valuep);
            if (!srcp) {
                VL_DO_DANGLING(targetp->deleteTree(), targetp);
                return nullptr;
            }
            return new AstAssign{flp, targetp, srcp};
        }
        // "(others => v)" fills whatever the value turns out to be long,
        // which is the one aggregate that needs no length to write down
        if (AstNodeExpr* const fillp = othersFillValue(valuep)) {
            AstVar* const kp = newTempVar("__Vdynk", newIntDType(flp), flp);
            const auto k = [&]() { return new AstVarRef{flp, kp, VAccess::READ}; };
            AstNode* outp = new AstAssign{flp, new AstVarRef{flp, kp, VAccess::WRITE},
                                          new AstConst{flp, AstConst::Signed32{}, 0}};
            AstLoop* const loopp = new AstLoop{flp};
            loopp->addStmtsp(new AstLoopTest{
                flp, loopp,
                new AstLtS{flp, k(),
                           dynCarrierLength(targetp->cloneTree(false), flp)}});
            loopp->addStmtsp(new AstAssign{
                flp,
                new AstSelBit{flp, dynPart(targetp->cloneTree(false), "q", flp), k()},
                fillp});
            loopp->addStmtsp(new AstAssign{
                flp, new AstVarRef{flp, kp, VAccess::WRITE},
                new AstAdd{flp, k(), new AstConst{flp, AstConst::Signed32{}, 1}}});
            outp = AstNode::addNext(outp, static_cast<AstNode*>(loopp));
            VL_DO_DANGLING(targetp->deleteTree(), targetp);
            return outp;
        }
        const VhdlType* const vtypep = valuep->vhdlTypep();
        if (!vtypep || !vtypep->isConstrainedArray() || vtypep->indexTypes().empty()
            || !vtypep->indexTypes()[0] || !vtypep->indexTypes()[0]->bounded()) {
            VL_DO_DANGLING(targetp->deleteTree(), targetp);
            unsup(flp, "value of a length only running decides copied from this expression");
            return nullptr;
        }
        AstNodeExpr* const srcp = lowerExpr(valuep);
        if (!srcp) {
            VL_DO_DANGLING(targetp->deleteTree(), targetp);
            return nullptr;
        }
        const VhdlType* const idxp = vtypep->indexTypes()[0];
        const int64_t count = idxp->length();
        if (count < 0 || count > 65536 || !repeatable(srcp)) {
            VL_DO_DANGLING(targetp->deleteTree(), targetp);
            VL_DO_DANGLING(srcp->deleteTree(), srcp);
            unsup(flp, "value of a length only running decides copied from this expression");
            return nullptr;
        }
        AstNode* outp = nullptr;
        const bool asc = idxp->ascending();
        for (int64_t i = 0; i < count; ++i) {
            const int64_t at = asc ? idxp->left() + i : idxp->left() - i;
            outp = AstNode::addNextNull(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    flp,
                    new AstSelBit{flp, dynPart(targetp->cloneTree(false), "q", flp),
                                  new AstConst{flp, AstConst::Signed32{},
                                               static_cast<int32_t>(i)}},
                    new AstSelBit{flp, srcp->cloneTree(false),
                                  new AstConst{flp, AstConst::Signed32{},
                                               static_cast<int32_t>(at)}}}));
        }
        VL_DO_DANGLING(targetp->deleteTree(), targetp);
        VL_DO_DANGLING(srcp->deleteTree(), srcp);
        return outp;
    }

    // The value an aggregate that is nothing but "others => v" gives every
    // element, or null if the aggregate says anything else
    AstNodeExpr* othersFillValue(AstNodeVhdlExpr* nodep) {
        AstVhdlAggregate* const aggp = VN_CAST(nodep, VhdlAggregate);
        if (!aggp) return nullptr;
        AstVhdlElemAssoc* const elemp = aggp->elemsp();
        if (!elemp || elemp->nextp() || !elemp->valuep()) return nullptr;
        AstVhdlChoice* const choicep = elemp->choicesp();
        if (!choicep || choicep->nextp() || !choicep->isOthers()) return nullptr;
        return lowerExpr(elemp->valuep());
    }

    // The result type a function is emitted with, which is the declared one
    // only where nothing else says more
    static VhdlType* emittedReturnType(const VhdlSymbol* symp) {
        AstVhdlSubprogDecl* declp = symp->bodyp();
        if (!declp) declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
        if (!declp) return symp->returnTypep();
        return returnType(declp, symp);
    }

    // Copy a value that carries its own bounds into an object whose subtype
    // fixes them. The elements are copied one at a time, since the value is
    // stored from zero and the object from its own left bound, and the
    // lengths must match -- which only running can check (10.5.2.1).
    AstNode* dynCarrierToFixed(AstNodeVhdlExpr* targetp, AstNodeVhdlExpr* valuep,
                               FileLine* flp) {
        const VhdlType* const ttypep = targetp->vhdlTypep();
        if (!ttypep || !ttypep->isConstrainedArray() || ttypep->indexTypes().empty()
            || !ttypep->indexTypes()[0] || !ttypep->indexTypes()[0]->bounded()) {
            return nullptr;
        }
        const VhdlType* const idxp = ttypep->indexTypes()[0];
        const int64_t count = idxp->length();
        if (count < 0 || count > 65536) return nullptr;
        AstNodeExpr* const srcp = lowerExpr(valuep);
        if (!srcp) return nullptr;
        AstNodeExpr* keepp = srcp;
        AstNode* outp = nullptr;
        if (!repeatable(srcp)) {
            // A call gives its value once, so it is worked out and kept
            AstVar* const tmpp = newTempVar(
                "__Vdynval", dynCarrierRef(const_cast<VhdlType*>(valuep->vhdlTypep()), flp),
                flp);
            outp = new AstAssign{flp, new AstVarRef{flp, tmpp, VAccess::WRITE}, srcp};
            keepp = new AstVarRef{flp, tmpp, VAccess::READ};
        }
        AstNodeExpr* const lhsp = lowerDriveTarget(targetp);
        if (!lhsp) {
            if (outp) VL_DO_DANGLING(outp->deleteTree(), outp);
            VL_DO_DANGLING(keepp->deleteTree(), keepp);
            return nullptr;
        }
        // "assert v'length = n", which is what the LRM asks of the assignment
        outp = AstNode::addNextNull(
            outp,
            static_cast<AstNode*>(new AstIf{
                flp,
                new AstNeq{flp, dynCarrierLength(keepp->cloneTree(false), flp),
                           new AstConst{flp, AstConst::Signed32{},
                                        static_cast<int32_t>(count)}},
                new AstDisplay{flp, VDisplayType::DT_ERROR, "Array lengths do not match",
                               nullptr, nullptr},
                nullptr}));
        const bool asc = idxp->ascending();
        for (int64_t i = 0; i < count; ++i) {
            const int64_t at = asc ? idxp->left() + i : idxp->left() - i;
            outp = AstNode::addNextNull(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    flp,
                    new AstSelBit{flp, lhsp->cloneTree(false),
                                  new AstConst{flp, AstConst::Signed32{},
                                               static_cast<int32_t>(at)}},
                    new AstSelBit{flp, dynPart(keepp->cloneTree(false), "q", flp),
                                  new AstConst{flp, AstConst::Signed32{},
                                               static_cast<int32_t>(i)}}}));
        }
        VL_DO_DANGLING(lhsp->deleteTree(), lhsp);
        VL_DO_DANGLING(keepp->deleteTree(), keepp);
        return outp;
    }

    // Whether an expression is worth a value that carries its own bounds.
    // A conversion between two array types keeps whatever it was given, so
    // it is transparent here, and so is a qualified expression.
    bool isDynValue(const AstNodeVhdlExpr* nodep, int depth = 0) const {
        if (!nodep || depth > 8) return false;
        if (isDynObj(nodep)) return true;
        if (dynResultCall(nodep)) return true;
        if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
            const VhdlSymbol* const prefixp
                = callp->prefixp() ? callp->prefixp()->vhdlSymp() : nullptr;
            if (prefixp && prefixp->resolved()->kind() == VhdlSymKind::TYPE) {
                const AstVhdlAssoc* const argp = VN_CAST(callp->argsp(), VhdlAssoc);
                return argp && argp->actualp() && isDynValue(argp->actualp(), depth + 1);
            }
        }
        if (const AstVhdlQualified* const qp = VN_CAST(nodep, VhdlQualified)) {
            return isDynValue(qp->valuep(), depth + 1);
        }
        return false;
    }

    std::map<const VhdlType*, AstNodeFTask*> m_dynEqFuncs;
    std::map<std::pair<const VhdlType*, int64_t>, AstNodeFTask*> m_dynOfFuncs;

    // "a = b" over two values that carry their own bounds is a length and
    // then an element at a time, which is a loop, so it is a function of its
    // own -- one per type, emitted where it is first wanted.
    AstNodeFTask* dynEqFunc(VhdlType* typep, FileLine* flp) {
        VhdlType* const basep = typep->base();
        const auto it = m_dynEqFuncs.find(basep);
        if (it != m_dynEqFuncs.end()) return it->second;
        if (!m_modp) return nullptr;
        const std::string name
            = "__Vdyneq" + std::to_string(m_dynEqFuncs.size()) + "__" + basep->name();
        AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
        funcp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        funcp->fvarp(newReturnVar(name, new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp));
        AstVar* args[2];
        const char* const argNames[2] = {"a", "b"};
        for (int i = 0; i < 2; ++i) {
            args[i] = new AstVar{flp, VVarType::PORT, argNames[i], VFlagChildDType{},
                                 dynCarrierRef(typep, flp)};
            args[i]->direction(VDirection::INPUT);
            args[i]->declDirection(VDirection::INPUT);
            args[i]->funcLocal(true);
            args[i]->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
            args[i]->trace(false);
            funcp->addStmtsp(args[i]);
        }
        AstVar* const kp = newFuncLocal("k", newIntDType(flp), flp);
        funcp->addStmtsp(kp);
        const auto ret = [&](bool val) -> AstNode* {
            return new AstReturn{flp, val ? new AstConst{flp, AstConst::BitTrue{}}
                                          : new AstConst{flp, AstConst::BitFalse{}}};
        };
        // Two values of the same length are equal when every element is, and
        // values of different lengths never are (9.2.3)
        funcp->addStmtsp(new AstIf{
            flp,
            new AstNeq{flp, dynCarrierLength(rd(args[0], flp), flp),
                       dynCarrierLength(rd(args[1], flp), flp)},
            ret(false), nullptr});
        funcp->addStmtsp(new AstAssign{flp, wr(kp, flp),
                                       new AstConst{flp, AstConst::Signed32{}, 0}});
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, rd(kp, flp), dynCarrierLength(rd(args[0], flp), flp)}});
        loopp->addStmtsp(new AstIf{
            flp,
            new AstNeq{flp,
                       new AstSelBit{flp, dynPart(rd(args[0], flp), "q", flp), rd(kp, flp)},
                       new AstSelBit{flp, dynPart(rd(args[1], flp), "q", flp), rd(kp, flp)}},
            ret(false), nullptr});
        loopp->addStmtsp(new AstAssign{
            flp, wr(kp, flp),
            new AstAdd{flp, rd(kp, flp), new AstConst{flp, AstConst::Signed32{}, 1}}});
        funcp->addStmtsp(loopp);
        funcp->addStmtsp(ret(true));
        m_modp->addStmtsp(funcp);
        m_dynEqFuncs.emplace(basep, funcp);
        return funcp;
    }

    // A value whose length its subtype fixes, made into one that carries its
    // own bounds, so that the two can be compared or joined. One function per
    // type and length, since the argument's own type says how long it is.
    AstNodeFTask* dynOfFixedFunc(VhdlType* carrierTypep, VhdlType* fixedTypep, FileLine* flp) {
        const VhdlType* const idxp = fixedTypep->indexTypes().empty()
                                         ? nullptr
                                         : fixedTypep->indexTypes()[0];
        if (!idxp || !idxp->bounded()) return nullptr;
        const int64_t count = idxp->length();
        if (count < 0 || count > 65536) return nullptr;
        const auto key = std::make_pair(carrierTypep->base(), count);
        const auto it = m_dynOfFuncs.find(key);
        if (it != m_dynOfFuncs.end()) return it->second;
        if (!m_modp) return nullptr;
        const std::string name = "__Vdynof" + std::to_string(m_dynOfFuncs.size()) + "__"
                                 + carrierTypep->base()->name();
        AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
        funcp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        funcp->fvarp(newReturnVar(name, dynCarrierRef(carrierTypep, flp), flp));
        AstVar* const vp = new AstVar{flp, VVarType::PORT, "v", VFlagChildDType{},
                                      lowerType(fixedTypep, flp)};
        vp->direction(VDirection::INPUT);
        vp->declDirection(VDirection::INPUT);
        vp->funcLocal(true);
        vp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        vp->trace(false);
        funcp->addStmtsp(vp);
        const auto out = [&]() {
            return new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE};
        };
        funcp->addStmtsp(new AstAssign{
            flp, dynPart(out(), "lft", flp),
            new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(idxp->left())}});
        funcp->addStmtsp(new AstAssign{
            flp, dynPart(out(), "rgt", flp),
            new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(idxp->right())}});
        funcp->addStmtsp(new AstAssign{
            flp, dynPart(out(), "dsc", flp),
            idxp->ascending() ? static_cast<AstNodeExpr*>(new AstConst{flp, AstConst::BitFalse{}})
                              : new AstConst{flp, AstConst::BitTrue{}}});
        funcp->addStmtsp(new AstAssign{
            flp, dynPart(out(), "q", flp),
            new AstNewDynamic{flp,
                              new AstConst{flp, AstConst::Signed32{},
                                           static_cast<int32_t>(count)},
                              nullptr}});
        AstVar* const kp = newFuncLocal("k", newIntDType(flp), flp);
        funcp->addStmtsp(kp);
        funcp->addStmtsp(new AstAssign{flp, wr(kp, flp),
                                       new AstConst{flp, AstConst::Signed32{}, 0}});
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, rd(kp, flp),
                       new AstConst{flp, AstConst::Signed32{},
                                    static_cast<int32_t>(count)}}});
        AstNodeExpr* const atp = idxp->ascending()
                                     ? static_cast<AstNodeExpr*>(new AstAdd{
                                           flp,
                                           new AstConst{flp, AstConst::Signed32{},
                                                        static_cast<int32_t>(idxp->left())},
                                           rd(kp, flp)})
                                     : new AstSub{flp,
                                                  new AstConst{flp, AstConst::Signed32{},
                                                               static_cast<int32_t>(
                                                                   idxp->left())},
                                                  rd(kp, flp)};
        loopp->addStmtsp(new AstAssign{
            flp, new AstSelBit{flp, dynPart(out(), "q", flp), rd(kp, flp)},
            new AstSelBit{flp, rd(vp, flp), atp}});
        loopp->addStmtsp(new AstAssign{
            flp, wr(kp, flp),
            new AstAdd{flp, rd(kp, flp), new AstConst{flp, AstConst::Signed32{}, 1}}});
        funcp->addStmtsp(loopp);
        m_modp->addStmtsp(funcp);
        m_dynOfFuncs.emplace(key, funcp);
        return funcp;
    }

    // Whatever an expression is worth, as a value that carries its own bounds
    AstNodeExpr* asDynCarrier(AstNodeVhdlExpr* nodep, VhdlType* carrierTypep, FileLine* flp) {
        if (isDynValue(nodep)) return lowerExpr(nodep);
        VhdlType* const typep = nodep->vhdlTypep();
        if (!typep || !typep->isConstrainedArray()) return nullptr;
        AstNodeFTask* const funcp = dynOfFixedFunc(carrierTypep, typep, flp);
        if (!funcp) return nullptr;
        AstNodeExpr* const valuep = lowerExpr(nodep);
        if (!valuep) return nullptr;
        AstFuncRef* const refp
            = new AstFuncRef{flp, funcp->name(), new AstArg{flp, "", valuep}};
        refp->taskp(funcp);
        return refp;
    }

    std::map<std::pair<const VhdlType*, int64_t>, AstNodeFTask*> m_dynToFuncs;

    // A value that carries its own bounds, copied into one whose subtype
    // fixes them. The lengths must match, which is a check only running can
    // make (5.3.2.2), and the elements are copied across because the one is
    // stored from zero and the other from its own left bound.
    AstNodeFTask* dynToFixedFunc(VhdlType* carrierTypep, VhdlType* fixedTypep, FileLine* flp) {
        const VhdlType* const idxp
            = fixedTypep->indexTypes().empty() ? nullptr : fixedTypep->indexTypes()[0];
        if (!idxp || !idxp->bounded()) return nullptr;
        const int64_t count = idxp->length();
        if (count < 0 || count > 65536) return nullptr;
        const auto key = std::make_pair(carrierTypep->base(), count);
        const auto it = m_dynToFuncs.find(key);
        if (it != m_dynToFuncs.end()) return it->second;
        if (!m_modp) return nullptr;
        const std::string name = "__Vdynto" + std::to_string(m_dynToFuncs.size()) + "__"
                                 + carrierTypep->base()->name();
        AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
        funcp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        funcp->fvarp(newReturnVar(name, lowerType(fixedTypep, flp), flp));
        AstVar* const cp = new AstVar{flp, VVarType::PORT, "c", VFlagChildDType{},
                                      dynCarrierRef(carrierTypep, flp)};
        cp->direction(VDirection::INPUT);
        cp->declDirection(VDirection::INPUT);
        cp->funcLocal(true);
        cp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        cp->trace(false);
        funcp->addStmtsp(cp);
        funcp->addStmtsp(new AstIf{
            flp,
            new AstNeq{flp, dynCarrierLength(rd(cp, flp), flp),
                       new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(count)}},
            new AstDisplay{flp, VDisplayType::DT_ERROR, "Array lengths do not match", nullptr,
                           nullptr},
            nullptr});
        const auto out = [&]() {
            return new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE};
        };
        AstVar* const kp = newFuncLocal("k", newIntDType(flp), flp);
        funcp->addStmtsp(kp);
        funcp->addStmtsp(new AstAssign{flp, wr(kp, flp),
                                       new AstConst{flp, AstConst::Signed32{}, 0}});
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, rd(kp, flp),
                       new AstConst{flp, AstConst::Signed32{},
                                    static_cast<int32_t>(count)}}});
        AstNodeExpr* const atp = idxp->ascending()
                                     ? static_cast<AstNodeExpr*>(new AstAdd{
                                           flp,
                                           new AstConst{flp, AstConst::Signed32{},
                                                        static_cast<int32_t>(idxp->left())},
                                           rd(kp, flp)})
                                     : new AstSub{flp,
                                                  new AstConst{flp, AstConst::Signed32{},
                                                               static_cast<int32_t>(
                                                                   idxp->left())},
                                                  rd(kp, flp)};
        loopp->addStmtsp(new AstAssign{
            flp, new AstSelBit{flp, out(), atp},
            new AstSelBit{flp, dynPart(rd(cp, flp), "q", flp), rd(kp, flp)}});
        loopp->addStmtsp(new AstAssign{
            flp, wr(kp, flp),
            new AstAdd{flp, rd(kp, flp), new AstConst{flp, AstConst::Signed32{}, 1}}});
        funcp->addStmtsp(loopp);
        m_modp->addStmtsp(funcp);
        m_dynToFuncs.emplace(key, funcp);
        return funcp;
    }

    // Such a value as one of a subtype that fixes its length
    AstNodeExpr* dynAsFixed(AstNodeVhdlExpr* nodep, VhdlType* fixedTypep, FileLine* flp) {
        VhdlType* const carrierp = nodep->vhdlTypep();
        if (!carrierp) return nullptr;
        AstNodeFTask* const funcp = dynToFixedFunc(carrierp, fixedTypep, flp);
        if (!funcp) return nullptr;
        AstNodeExpr* const valuep = lowerExpr(nodep);
        if (!valuep) return nullptr;
        AstFuncRef* const refp
            = new AstFuncRef{flp, funcp->name(), new AstArg{flp, "", valuep}};
        refp->taskp(VN_AS(funcp, Func));
        return refp;
    }

    std::map<const VhdlType*, AstNodeFTask*> m_dynBitsFuncs;

    // An integer spread over as many bits as a width says, as a value that
    // carries its own bounds: what TO_UNSIGNED and RESIZE give when what
    // they are handed to is one of these. The range runs "n-1 downto 0",
    // which is the subtype numeric_std gives its result (9.3.6).
    AstNodeFTask* dynBitsFunc(VhdlType* typep, bool signd, FileLine* origFlp) {
        // The bits are taken out of a whole integer and put into elements one
        // bit wide, so the widths it mixes are deliberate
        FileLine* const flp = new FileLine{origFlp};
        flp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
        flp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
        VhdlType* const basep = typep->base();
        const auto it = m_dynBitsFuncs.find(basep);
        if (it != m_dynBitsFuncs.end() && !signd) return it->second;
        if (!m_modp) return nullptr;
        const std::string name = "__Vdynbits" + std::to_string(m_dynBitsFuncs.size()) + "__"
                                 + basep->name();
        AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
        funcp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        funcp->fvarp(newReturnVar(name, dynCarrierRef(typep, flp), flp));
        AstVar* const vp = new AstVar{flp, VVarType::PORT, "v", VFlagChildDType{},
                                      new AstBasicDType{flp, VBasicDTypeKwd::INTEGER}};
        AstVar* const np = new AstVar{flp, VVarType::PORT, "n", VFlagChildDType{},
                                      newIntDType(flp)};
        for (AstVar* const argp : {vp, np}) {
            argp->direction(VDirection::INPUT);
            argp->declDirection(VDirection::INPUT);
            argp->funcLocal(true);
            argp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
            argp->trace(false);
            funcp->addStmtsp(argp);
        }
        AstVar* const kp = newFuncLocal("k", newIntDType(flp), flp);
        funcp->addStmtsp(kp);
        const auto out = [&]() {
            return new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE};
        };
        funcp->addStmtsp(new AstAssign{
            flp, dynPart(out(), "lft", flp),
            new AstSub{flp, rd(np, flp), new AstConst{flp, AstConst::Signed32{}, 1}}});
        funcp->addStmtsp(new AstAssign{flp, dynPart(out(), "rgt", flp),
                                       new AstConst{flp, AstConst::Signed32{}, 0}});
        funcp->addStmtsp(new AstAssign{flp, dynPart(out(), "dsc", flp),
                                       new AstConst{flp, AstConst::BitTrue{}}});
        funcp->addStmtsp(new AstAssign{flp, dynPart(out(), "q", flp),
                                       new AstNewDynamic{flp, rd(np, flp), nullptr}});
        funcp->addStmtsp(new AstAssign{flp, wr(kp, flp),
                                       new AstConst{flp, AstConst::Signed32{}, 0}});
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{flp, loopp,
                                         new AstLtS{flp, rd(kp, flp), rd(np, flp)}});
        // The element at k holds the bit k places up, since the value is
        // stored from zero and its own range runs downward
        loopp->addStmtsp(new AstAssign{
            flp, new AstSelBit{flp, dynPart(out(), "q", flp), rd(kp, flp)},
            new AstAnd{flp,
                       new AstShiftR{flp, rd(vp, flp),
                                     new AstSub{flp,
                                                new AstSub{flp, rd(np, flp),
                                                           new AstConst{flp,
                                                                        AstConst::Signed32{},
                                                                        1}},
                                                rd(kp, flp)}},
                       new AstConst{flp, AstConst::Signed32{}, 1}}});
        loopp->addStmtsp(new AstAssign{
            flp, wr(kp, flp),
            new AstAdd{flp, rd(kp, flp), new AstConst{flp, AstConst::Signed32{}, 1}}});
        funcp->addStmtsp(loopp);
        m_modp->addStmtsp(funcp);
        if (!signd) m_dynBitsFuncs.emplace(basep, funcp);
        return funcp;
    }

    std::map<const VhdlType*, AstNodeFTask*> m_dynIntFuncs;

    // The integer a value that carries its own bounds stands for, read with
    // the signedness its type has. Its elements are one bit each and its
    // range runs from its left, so this is the other half of dynBitsFunc.
    AstNodeFTask* dynIntFunc(VhdlType* typep, bool signd, FileLine* origFlp) {
        FileLine* const flp = new FileLine{origFlp};
        flp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
        flp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
        VhdlType* const basep = typep->base();
        const auto it = m_dynIntFuncs.find(basep);
        if (it != m_dynIntFuncs.end() && !signd) return it->second;
        if (!m_modp) return nullptr;
        const std::string name = "__Vdynint" + std::to_string(m_dynIntFuncs.size()) + "__"
                                 + basep->name();
        AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
        funcp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        funcp->fvarp(newReturnVar(name, new AstBasicDType{flp, VBasicDTypeKwd::INTEGER}, flp));
        AstVar* const cp = new AstVar{flp, VVarType::PORT, "c", VFlagChildDType{},
                                      dynCarrierRef(typep, flp)};
        cp->direction(VDirection::INPUT);
        cp->declDirection(VDirection::INPUT);
        cp->funcLocal(true);
        cp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        cp->trace(false);
        funcp->addStmtsp(cp);
        AstVar* const kp = newFuncLocal("k", newIntDType(flp), flp);
        funcp->addStmtsp(kp);
        const auto out = [&]() {
            return new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE};
        };
        const auto outr = [&]() {
            return new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::READ};
        };
        // A negative value is all ones above its own width, which is what
        // taking the sign bit as the starting total comes to (9.3.6)
        funcp->addStmtsp(new AstAssign{
            flp, out(),
            signd ? static_cast<AstNodeExpr*>(new AstNegate{
                        flp, new AstSelBit{flp, dynPart(rd(cp, flp), "q", flp),
                                           new AstConst{flp, AstConst::Signed32{}, 0}}})
                  : new AstConst{flp, AstConst::Signed32{}, 0}});
        funcp->addStmtsp(new AstAssign{
            flp, wr(kp, flp),
            signd ? static_cast<AstNodeExpr*>(new AstConst{flp, AstConst::Signed32{}, 1})
                  : new AstConst{flp, AstConst::Signed32{}, 0}});
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, rd(kp, flp), dynCarrierLength(rd(cp, flp), flp)}});
        loopp->addStmtsp(new AstAssign{
            flp, out(),
            new AstAdd{flp,
                       new AstShiftL{flp, outr(),
                                     new AstConst{flp, AstConst::Signed32{}, 1}},
                       new AstSelBit{flp, dynPart(rd(cp, flp), "q", flp), rd(kp, flp)}}});
        loopp->addStmtsp(new AstAssign{
            flp, wr(kp, flp),
            new AstAdd{flp, rd(kp, flp), new AstConst{flp, AstConst::Signed32{}, 1}}});
        funcp->addStmtsp(loopp);
        m_modp->addStmtsp(funcp);
        if (!signd) m_dynIntFuncs.emplace(basep, funcp);
        return funcp;
    }

    // Whether an expression is a call whose result is carried
    bool dynResultCall(const AstNodeVhdlExpr* nodep) const {
        const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
        if (!callp || !callp->vhdlSymp()) return false;
        const VhdlSymbol* const symp = callp->vhdlSymp()->resolved();
        if (symp->kind() != VhdlSymKind::FUNCTION) return false;
        // The conversions that build a value out of an integer are emitted
        // at the call rather than compiled, so what this call is worth is
        // what says whether it carries its bounds
        const std::string& name = symp->name();
        if (name == "to_unsigned" || name == "to_signed" || name == "resize") {
            return carriedDynamically(callp->vhdlTypep());
        }
        // Otherwise what the function was emitted as, not what the
        // declaration said: a copy made for this site, or a body whose
        // return statement says how long the value is, gives the result a
        // length the declaration had none of, and then it is an ordinary
        // value
        return carriedDynamically(emittedReturnType(symp));
    }

    // Whether an expression names an object held in a carrier, and if it does
    // the carrier itself. Only a plain name can be one: a carrier is made for
    // a parameter or a call's result, and neither is reached through a
    // selection or an index.
    bool isDynObj(const AstNodeVhdlExpr* nodep) const {
        const AstVhdlName* const namep = VN_CAST(nodep, VhdlName);
        return namep && namep->vhdlSymp() && m_dynSyms.count(namep->vhdlSymp()->resolved());
    }

    static AstNodeExpr* dynIndex(AstNodeExpr* fromp, AstNodeExpr* indexp, FileLine* flp) {
        AstNodeExpr* const offp = dynOffset(fromp->cloneTree(false), indexp, flp);
        return new AstSelBit{flp, dynPart(fromp, "q", flp), offp};
    }

    std::map<const VhdlType*, std::string> m_recNames;  // Record -> its typedef
    // Record whose shape a generic decides -> its typedef, per module
    std::map<std::pair<const AstNodeModule*, const VhdlType*>, std::string> m_recLocalNames;

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
        // A value that does not fit in thirty-two bits, or one of a type whose
        // own range does not, is written out as sixty-four (5.2.3)
        const int64_t iv = val.toInt();
        if (iv < -2147483648LL || iv > 2147483647LL || needsWideInt(basep)) {
            V3Number num{flp, 64, 0};
            num.setQuad(static_cast<uint64_t>(iv));
            num.isSigned(true);
            return new AstConst{flp, num};
        }
        return new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(iv)};
    }

    // One bit of a logic type. Z survives into the AST as the Verilog one
    // does, since it is representable. 'U', 'X', 'W' and '-' do not: this
    // runtime has no metavalues, and writing them as Verilog X would leave
    // what they stand for to --x-assign -- which under "unique" makes a
    // table of them a table of random bits, and std_logic_1164 and the
    // Synopsys packages are written as tables over the nine values. They
    // are written as zero, which is what --x-assign gives them by default.
    static AstNodeExpr* newLogicConst(FileLine* flp, LogicVal value) {
        V3Number num{flp, 1, 0};
        switch (value) {
        case LogicVal::ZERO: num.setLong(0); break;
        case LogicVal::ONE: num.setLong(1); break;
        case LogicVal::X: num.setLong(0); break;
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
        return newVarOfType(symp, varType, lowerType(symp->typep(), flp), flp);
    }

    AstVar* newVarOfType(VhdlSymbol* symp, VVarType varType, AstNodeDType* dtypep,
                         FileLine* flp) {
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
        AstVar* const varp = new AstVar{flp, varType, name, VFlagChildDType{}, dtypep};
        // Something the design declared, so it is what a waveform shows. The
        // temporaries the lowering makes for itself are not, and are made
        // elsewhere.
        varp->trace(true);
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
    // Whether a value of this type is one the back end can carry as a
    // parameter. An array whose element does not pack has none: a parameter
    // holds one value, and such an array is a list of structures.
    bool parameterizable(const VhdlType* typep) {
        if (!typep) return true;
        if (typep->base()->kind() != VhdlTypeKind::ARRAY) return true;
        if (isStringType(typep)) return true;
        const VhdlType* const elemp
            = typep->elementp() ? typep->elementp() : typep->base()->elementp();
        return !elemp || elemp->base()->kind() != VhdlTypeKind::RECORD || recordPacks(elemp);
    }

    void lowerInterface(AstVhdlIfaceDecl* nodep, bool isGeneric, int& pinNum) {
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp) return;
        FileLine* const flp = nodep->fileline();
        if (nodep->kind() == VhdlIfaceKind::TYPE
            || nodep->kind() == VhdlIfaceKind::SUBPROGRAM
            || nodep->kind() == VhdlIfaceKind::PACKAGE) {
            // In a copy of the entity made for one instance the generic type
            // is already the type that instance named, so there is nothing
            // for the back end to be passed
            if (nodep->isBound()) return;
            unsup(flp, "generic type, subprogram or package");
            return;
        }
        if (isGeneric) {
            // A generic is a parameter, and a parameter carries a value the
            // back end can fold. A composite that does not pack has no such
            // value: elaboration would have to hand over a structure.
            if (!parameterizable(symp->typep())) {
                unsup(flp, "generic '" + symp->name() + "' of a type that is not one value");
                return;
            }
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
                // One standing for a generic an instantiation supplied is a
                // parameter: its value is settled by elaboration, and a range
                // written in terms of it has to fold with it (4.9)
                // A constant of a subprogram is elaborated afresh on every
                // call and what it is given may name a parameter of that
                // call, so it stays a variable however it is used elsewhere
                varType = objp->isElabConstant() && !m_ftaskp && !m_protClassp ? VVarType::LPARAM
                                                                              : VVarType::VAR;
                break;
            case VhdlObjKind::VARIABLE:
            case VhdlObjKind::SHARED_VARIABLE: varType = VVarType::VAR; break;
            case VhdlObjKind::FILE: varType = VVarType::VAR; break;
            }
            const bool dynDecl = objp->kind() != VhdlObjKind::FILE
                                 && carriedDynamically(symp->typep());
            if (dynDecl) m_dynSyms.insert(symp);
            AstVar* const varp
                = dynDecl ? newVarOfType(symp, varType, dynCarrierRef(symp->typep(), flp), flp)
                          : newVar(symp, varType, flp);
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
            if (objp->kind() == VhdlObjKind::FILE && objp->fileNamep()) {
                // "file f : text is <name>" opens the file where it is
                // declared, for reading unless "open <mode>" says otherwise
                // (6.4.2.5); the open happens when the declaration is
                // elaborated, so it is the first thing the region does
                addStmt(varp);
                AstNodeExpr* const namep = lowerExpr(objp->fileNamep());
                if (namep) {
                    std::string mode = "r";
                    if (objp->fileOpenp()) {
                        const VhdlValue kind = V3VhdlEval::eval(objp->fileOpenp());
                        if (kind.isInt()) {
                            mode = kind.toInt() == 1 ? "w" : (kind.toInt() == 2 ? "a" : "r");
                        }
                    }
                    AstNode* const openp = new AstAssign{
                        flp, new AstVarRef{flp, varp, VAccess::WRITE},
                        new AstFOpen{flp, namep,
                                     new AstConst{flp, AstConst::String{}, mode}}};
                    // A file of a subprogram is opened again on every call.
                    // Anywhere else -- an architecture, a block, a process --
                    // the declaration is elaborated once before the design
                    // runs, so the open belongs in an initial rather than
                    // loose among the module's statements, which is not a
                    // place a statement may stand at all.
                    addStmt(m_ftaskp ? openp : new AstInitial{flp, openp});
                }
                return;
            }
            if (dynDecl) {
                // A carrier is given its bounds and its room where it is
                // declared, since nothing else knows how long it is
                addStmt(varp);
                addStmt(dynCarrierInit(varp, symp->typep(), flp));
                if (objp->defaultp()) {
                    if (AstNode* const setp
                        = dynCarrierAssign(new AstVarRef{flp, varp, VAccess::WRITE},
                                           objp->defaultp(), flp)) {
                        addStmt(setp);
                    }
                }
                return;
            }
            if (objp->defaultp()) {
                varp->valuep(lowerExpr(objp->defaultp()));
            } else if (AstNodeExpr* const spacesp = dynamicStringInit(symp->typep(), flp)) {
                // A STRING as long as something only known while running --
                // "variable r : string(s'range)" over an unconstrained
                // parameter -- has to start out that long, or writing to an
                // element of it writes nothing at all
                varp->valuep(spacesp);
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
            if (subp->genericCount() != 0 && !subp->isInstance()) return;
            // One a copy was made of for every call site is a template too
            if (subp->specialized()) return;
            // One with a signal parameter is a template too, of a different
            // kind: what its formal names, and so what that formal's history
            // attributes answer from, is only known at a call site, so a copy
            // is made there instead (4.2.2.3)
            if (subp->vhdlSymp() && hasSignalParam(subp->vhdlSymp())
                && subp->hasBody()) {
                return;
            }
            lowerSubprogram(subp);
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
    // A STRING whose length is an expression rather than a number, as the
    // value that many characters. Null where the length is not one of the
    // shapes that can be worked out here.
    AstNodeExpr* dynamicStringInit(VhdlType* typep, FileLine* flp) {
        if (!typep || !isStringType(typep) || typep->isConstrainedArray()) return nullptr;
        AstNodeExpr* const lenp = dynamicLength(typep, flp);
        if (!lenp) return nullptr;
        return new AstReplicateN{flp, new AstConst{flp, AstConst::String{}, " "}, lenp};
    }

    // How long an array whose bounds are expressions will be, as an
    // expression: "right - left + 1" the way the range runs, and for a range
    // written as another object's own the length of that object
    AstNodeExpr* dynamicLength(VhdlType* typep, FileLine* flp) {
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        if (indexes.size() != 1 || !indexes[0] || !indexes[0]->boundsp()) return nullptr;
        AstVhdlRange* const rp = VN_CAST(indexes[0]->boundsp(), VhdlRange);
        if (!rp) return nullptr;
        if (rp->leftp() && rp->rightp()) {
            AstNodeExpr* const leftp = lowerExpr(rp->leftp());
            AstNodeExpr* const rightp = leftp ? lowerExpr(rp->rightp()) : nullptr;
            if (!leftp || !rightp) {
                if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
                if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
                return nullptr;
            }
            AstNodeExpr* const spanp = rp->dir() == VhdlDir::TO
                                           ? new AstSub{flp, rightp, leftp}
                                           : new AstSub{flp, leftp, rightp};
            return new AstAdd{flp, spanp, new AstConst{flp, AstConst::Signed32{}, 1}};
        }
        // "s'range" of a string whose own length is only known while running
        AstVhdlAttrName* const attrp
            = rp->attrp() ? VN_CAST(rp->attrp(), VhdlAttrName) : nullptr;
        if (!attrp || !attrp->prefixp()) return nullptr;
        if (attrp->name() != "range" && attrp->name() != "reverse_range") return nullptr;
        const VhdlType* ptypep = attrp->prefixp()->vhdlTypep();
        if (ptypep && ptypep->base()->kind() == VhdlTypeKind::ACCESS
            && ptypep->base()->designatedp()) {
            ptypep = ptypep->base()->designatedp();
        }
        if (!ptypep || !isStringType(ptypep)) return nullptr;
        if (ptypep->isConstrainedArray()) {
            const std::vector<VhdlType*>& pidx = ptypep->indexTypes();
            if (pidx.empty() || !pidx[0] || !pidx[0]->bounded()) return nullptr;
            return new AstConst{flp, AstConst::Signed32{},
                                static_cast<int32_t>(pidx[0]->length())};
        }
        AstNodeExpr* const strp = lowerExpr(attrp->prefixp());
        return strp ? new AstLenN{flp, strp} : nullptr;
    }

    // A discrete range as two ends and a direction. Written "l to r" both
    // ends are there to lower; written "x'range" the range is the one x
    // already has, so it is asked of x's own subtype -- which is the form a
    // design uses precisely where those bounds are settled by an instance
    // rather than by the text (16.2.5).
    bool rangeParts(AstVhdlRange* rp, AstNodeExpr*& leftp, AstNodeExpr*& rightp,
                    bool& ascending, bool& dirKnown) {
        leftp = nullptr;
        rightp = nullptr;
        ascending = true;
        dirKnown = true;
        if (!rp) return false;
        FileLine* const flp = rp->fileline();
        if (rp->leftp() && rp->rightp()) {
            leftp = lowerExpr(rp->leftp());
            rightp = leftp ? lowerExpr(rp->rightp()) : nullptr;
            if (!leftp || !rightp) {
                if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
                if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
                leftp = nullptr;
                rightp = nullptr;
                return false;
            }
            ascending = rp->dir() != VhdlDir::DOWNTO;
            dirKnown = !rp->dirUnknown();
            return true;
        }
        AstVhdlAttrName* const attrp = VN_CAST(rp->attrp(), VhdlAttrName);
        if (!attrp || !attrp->prefixp()) return false;
        const bool reverse = attrp->name() == "reverse_range";
        if (!reverse && attrp->name() != "range") return false;
        const VhdlType* typep = attrp->prefixp()->vhdlTypep();
        if (typep && typep->base()->kind() == VhdlTypeKind::ACCESS
            && typep->base()->designatedp()) {
            typep = typep->base()->designatedp();
        }
        int dim = 0;
        if (AstVhdlLiteral* const litp = VN_CAST(attrp->argp(), VhdlLiteral)) {
            dim = static_cast<int>(litp->intVal()) - 1;
        }
        if (!dynamicBounds(typep, dim, flp, leftp, rightp, ascending, &dirKnown)) {
            // Bounds the text already fixed
            const VhdlType* const idxp = indexSubtype(typep, dim);
            if (!idxp || !idxp->bounded()) return false;
            leftp = new AstConst{flp, AstConst::Signed32{},
                                 static_cast<int32_t>(idxp->left())};
            rightp = new AstConst{flp, AstConst::Signed32{},
                                  static_cast<int32_t>(idxp->right())};
            ascending = idxp->ascending();
            dirKnown = true;
        }
        if (reverse) {
            std::swap(leftp, rightp);
            ascending = !ascending;
        }
        return true;
    }

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
            // Every element takes the element type's default (6.4.2.2). An
            // array of logic is packed and starts as Verilator starts it; an
            // array of anything else is unpacked, and a sparse initializer
            // with only a default says "all elements" without writing them
            // out, whatever the array's size -- including a size that only
            // elaboration fixes, where the range travels with the type.
            // A constraint written for the array replaces the index
            // subtype, whether or not its bounds are numbers yet. Where the
            // index subtype is still the type's own, nothing has said how
            // long the array is and there is no default to write.
            const bool constrained
                = typep->isConstrainedArray()
                  || (typep->indexTypes().size() == 1 && basep->indexTypes().size() == 1
                      && typep->indexTypes()[0] != basep->indexTypes()[0]);
            if (!constrained) return nullptr;
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
            // A vector of single bits comes out as a type with a range
            // rather than an array of them, and is packed just the same
            if (VN_IS(dtypep, PackArrayDType) || VN_IS(dtypep, BasicDType)) {
                // Packed, so the whole array is one constant: the element's
                // default repeated over every index, however many dimensions
                // it has
                VL_DO_DANGLING(dtypep->deleteTree(), dtypep);
                int64_t count = 1;
                for (const VhdlType* const idxp : typep->indexTypes()) {
                    if (!idxp || !idxp->bounded()) count = 0;
                    if (count) count *= idxp->length();
                }
                if (!count && typep->indexTypes().size() == 1) {
                    // A length that only elaboration fixes -- an array whose
                    // bounds name a generic -- is as many of the element's
                    // default as the range is long, which V3Param settles
                    // along with the range itself
                    if (AstNodeExpr* const lenp = boundsLength(typep, flp)) {
                        return new AstReplicate{flp, elemp, lenp};
                    }
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
            // A null access value is a null handle (9.3.2). A LINE is text
            // here rather than a handle, so its null is no text at all.
            if (p->kind() == VhdlLitKind::NULLLIT) {
                if (isLineType(p->vhdlTypep())) {
                    return new AstConst{flp, AstConst::String{}, ""};
                }
                return new AstConst{flp, AstConst::Null{}};
            }
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
            // "T'POS(v)" is an integer, however narrow the value's own type
            if (argp) {
                return new AstCast{flp, argp, VFlagChildDType{},
                                   new AstBasicDType{flp, VBasicDTypeKwd::INTEGER,
                                                     VSigning::SIGNED}};
            }
        } else if (attr == "val") {
            // "T'VAL(n)" is a value of T, so it is as wide as T even though
            // the position it is given is an integer
            if (argp) {
                VhdlType* const totypep = nodep->vhdlTypep();
                if (!totypep) return argp;
                return new AstCast{flp, argp, VFlagChildDType{}, lowerType(totypep, flp)};
            }
        } else if (attr == "succ" || attr == "pred") {
            // The next or previous value of the type, which is as wide as the
            // type is -- the 1 that steps it is a position, not a value of
            // that width, so widening it is not a finding (16.2.3)
            if (argp) {
                FileLine* const sflp = new FileLine{flp};
                sflp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
                sflp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
                AstNodeExpr* const onep = new AstConst{sflp, AstConst::Signed32{}, 1};
                AstNodeExpr* const stepp
                    = attr == "succ" ? static_cast<AstNodeExpr*>(new AstAdd{sflp, argp, onep})
                                     : static_cast<AstNodeExpr*>(new AstSub{sflp, argp, onep});
                // The result is a value of the type, so it is as wide as the
                // type is, not as wide as the position arithmetic
                VhdlType* const totypep = nodep->vhdlTypep();
                if (!totypep) return stepp;
                return new AstCast{sflp, stepp, VFlagChildDType{}, lowerType(totypep, sflp)};
            }
        } else if (attr == "leftof" || attr == "rightof") {
            // A step toward one end of the prefix's range, which is a step
            // down for a range that runs the other way (16.2.3). 'SUCC and
            // 'PRED are about the type's own order and do not turn round.
            if (argp) {
                const VhdlType* ptypep = nodep->prefixp()->vhdlTypep();
                if (!ptypep && nodep->prefixp()->vhdlSymp()) {
                    ptypep = nodep->prefixp()->vhdlSymp()->typep();
                }
                const bool ascending = !ptypep || ptypep->ascending();
                if ((attr == "rightof") == ascending) {
                    return new AstAdd{flp, argp,
                                      new AstConst{flp, AstConst::Signed32{}, 1}};
                }
                return new AstSub{flp, argp, new AstConst{flp, AstConst::Signed32{}, 1}};
            }
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
            const VhdlType* typep = nodep->prefixp()->vhdlTypep();
            // An access value is dereferenced without saying so (8.1)
            if (typep && typep->base()->kind() == VhdlTypeKind::ACCESS
                && typep->base()->designatedp()) {
                typep = typep->base()->designatedp();
            }
            // A STRING whose length is not fixed by its subtype -- what a
            // LINE points at -- carries it at run time, so the bounds are
            // read from the value rather than from the type. Such a value
            // always runs from 1 (16.3).
            // A value whose length only running decides carries its own
            // bounds, so the attribute reads them from the value
            if (isDynObj(nodep->prefixp())) {
                if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
                AstNodeExpr* const carrp = lowerExpr(nodep->prefixp());
                if (!carrp) return nullptr;
                if (AstNodeExpr* const outp = dynCarrierAttr(carrp, attr, flp)) return outp;
                VL_DO_DANGLING(carrp->deleteTree(), carrp);
                unsup(flp, "'" + attr + " of a value whose length only running decides");
                return nullptr;
            }
            const DynAccess* dynp = nullptr;
            if (AstNodeVhdlExpr* const dynObjp = dynAccessObj(nodep->prefixp(), dynp)) {
                if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
                AstNodeExpr* const objp = lowerExpr(dynObjp);
                if (!objp) return nullptr;
                if (!repeatable(objp)) {
                    VL_DO_DANGLING(objp->deleteTree(), objp);
                    unsup(flp, "'" + attr + " of an access value that is not a name");
                    return nullptr;
                }
                AstNodeExpr* const outp = dynAttr(dynp, objp, attr, flp);
                VL_DO_DANGLING(objp->deleteTree(), objp);
                return outp;
            }
            if (typep && isStringType(typep) && !typep->isConstrainedArray()) {
                if (argp) VL_DO_DANGLING(argp->deleteTree(), argp);
                AstNodeExpr* const strp = lowerExpr(nodep->prefixp());
                if (!strp) return nullptr;
                if (attr == "ascending") return new AstConst{flp, AstConst::BitTrue{}};
                if (attr == "left" || attr == "low") {
                    VL_DO_DANGLING(strp->deleteTree(), strp);
                    return new AstConst{flp, AstConst::Signed32{}, 1};
                }
                return new AstLenN{flp, strp};
            }
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
        // "t'value(s)" is the value whose image is s (16.2.3)
        if (attr == "value" && nodep->argp()) {
            const VhdlType* wantp = nodep->vhdlTypep();
            if (!wantp && nodep->prefixp()) wantp = nodep->prefixp()->vhdlTypep();
            if (!wantp && nodep->prefixp() && nodep->prefixp()->vhdlSymp()) {
                wantp = nodep->prefixp()->vhdlSymp()->typep();
            }
            if (AstNodeFTask* const funcp = valueFuncFor(wantp, flp)) {
                AstNodeExpr* const textp = lowerExpr(nodep->argp());
                if (!textp) return nullptr;
                AstFuncRef* const refp = new AstFuncRef{flp, funcp->name(), nullptr};
                refp->taskp(VN_AS(funcp, Func));
                refp->addArgsp(new AstArg{flp, "", textp});
                return refp;
            }
        }
        // A user-defined attribute stands for the static expression an
        // attribute specification gave the prefix (7.2)
        if (AstNodeExpr* const userp = lowerUserAttr(nodep, attr)) return userp;
        unsup(flp, "attribute '" + attr);
        return nullptr;
    }

    // "while (cond) body", which the AST writes as a loop whose first
    // statement tests the condition
    static AstNode* whileLoop(FileLine* flp, AstNodeExpr* condp, AstNode* bodyp) {
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{flp, loopp, condp});
        loopp->addStmtsp(bodyp);
        return loopp;
    }

    // A local variable of a generated helper function
    static AstVar* newFuncLocal(const std::string& name, AstNodeDType* dtypep, FileLine* flp) {
        AstVar* const varp = new AstVar{flp, VVarType::BLOCKTEMP, name, VFlagChildDType{}, dtypep};
        varp->funcLocal(true);
        varp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        varp->trace(false);
        return varp;
    }

    static AstNodeExpr* rd(AstVar* varp, FileLine* flp) {
        return new AstVarRef{flp, varp, VAccess::READ};
    }
    static AstNodeExpr* wr(AstVar* varp, FileLine* flp) {
        return new AstVarRef{flp, varp, VAccess::WRITE};
    }
    static AstNodeExpr* i32(int64_t v, FileLine* flp) {
        return new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(v)};
    }

    // "T'VALUE(s)" is the value whose image is s, with leading and trailing
    // whitespace ignored (16.2.3). One function per type does the reading:
    // the text is not known until it runs, so this is a real parser rather
    // than a fold.
    AstNodeFTask* valueFuncFor(const VhdlType* typep, FileLine* origFlp) {
        if (!typep || !m_modp) return nullptr;
        // The reader works a character at a time and counts in whole
        // integers, so the widths it mixes are deliberate
        FileLine* const flp = new FileLine{origFlp};
        flp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
        flp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
        const VhdlType* const basep = typep->base();
        const VhdlTypeKind kind = basep->kind();
        const bool isEnum = kind == VhdlTypeKind::ENUM;
        const bool isReal = kind == VhdlTypeKind::FLOATING;
        const bool isPhys = kind == VhdlTypeKind::PHYSICAL;
        const bool isInt = kind == VhdlTypeKind::INTEGER
                           || kind == VhdlTypeKind::UNIVERSAL_INTEGER;
        if (!isEnum && !isReal && !isPhys && !isInt) return nullptr;
        if (isEnum && (basep->literals().empty() || basep->literals().size() > 256)) {
            return nullptr;
        }
        const auto it = m_valueFuncs.find(basep);
        if (it != m_valueFuncs.end()) return it->second;

        const std::string name = "__Vvalue" + std::to_string(m_valueFuncs.size()) + "__"
                                 + VString::downcase(basep->prettyName());
        AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
        funcp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        AstNodeDType* const retDTypep
            = isReal ? static_cast<AstNodeDType*>(
                           new AstBasicDType{flp, VBasicDTypeKwd::DOUBLE})
                     : lowerType(const_cast<VhdlType*>(typep), flp);
        funcp->fvarp(newReturnVar(name, retDTypep, flp));
        AstVar* const sp = new AstVar{flp, VVarType::PORT, "s", VFlagChildDType{},
                                      new AstBasicDType{flp, VBasicDTypeKwd::STRING}};
        sp->direction(VDirection::INPUT);
        sp->declDirection(VDirection::INPUT);
        sp->funcLocal(true);
        sp->lifetime(VLifetime::AUTOMATIC_EXPLICIT);
        sp->trace(false);
        funcp->addStmtsp(sp);

        AstNodeDType* const intDTypep = new AstBasicDType{flp, VBasicDTypeKwd::INTEGER};
        AstVar* const lop = newFuncLocal("lo", intDTypep, flp);
        AstVar* const hip = newFuncLocal("hi", intDTypep->cloneTree(false), flp);
        funcp->addStmtsp(lop);
        funcp->addStmtsp(hip);
        AstNode* bodyp = new AstAssign{flp, wr(lop, flp), i32(0, flp)};
        bodyp = AstNode::addNext(
            bodyp, static_cast<AstNode*>(new AstAssign{
                       flp, wr(hip, flp),
                       new AstSub{flp, new AstLenN{flp, rd(sp, flp)}, i32(1, flp)}}));
        // Leading and trailing blanks are not part of the image (16.2.3)
        const auto isBlank = [&](AstNodeExpr* charp) -> AstNodeExpr* {
            return new AstLogOr{
                flp, new AstEq{flp, charp, i32(' ', flp)},
                new AstEq{flp, charp->cloneTree(false), i32('\t', flp)}};
        };
        bodyp = AstNode::addNext(
            bodyp,
            whileLoop(flp,
                      new AstLogAnd{flp, new AstLte{flp, rd(lop, flp), rd(hip, flp)},
                                    isBlank(new AstGetcN{flp, rd(sp, flp), rd(lop, flp)})},
                      new AstAssign{flp, wr(lop, flp),
                                    new AstAdd{flp, rd(lop, flp), i32(1, flp)}}));
        bodyp = AstNode::addNext(
            bodyp,
            whileLoop(flp,
                      new AstLogAnd{flp, new AstLte{flp, rd(lop, flp), rd(hip, flp)},
                                    isBlank(new AstGetcN{flp, rd(sp, flp), rd(hip, flp)})},
                      new AstAssign{flp, wr(hip, flp),
                                    new AstSub{flp, rd(hip, flp), i32(1, flp)}}));

        if (isEnum) {
            AstVar* const tp = newFuncLocal(
                "t", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            funcp->addStmtsp(tp);
            bodyp = AstNode::addNext(
                bodyp, static_cast<AstNode*>(new AstAssign{
                           flp, wr(tp, flp),
                           new AstSubstrN{flp, rd(sp, flp), rd(lop, flp), rd(hip, flp)}}));
            // A basic identifier reads without regard to case; a character
            // literal does not (15.4, 15.6)
            AstVar* const up = newFuncLocal(
                "u", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            funcp->addStmtsp(up);
            bodyp = AstNode::addNext(
                bodyp, static_cast<AstNode*>(new AstAssign{
                           flp, wr(up, flp), new AstToLowerN{flp, rd(tp, flp)}}));
            const std::vector<VhdlSymbol*>& literals = basep->literals();
            AstNodeExpr* outp = i32(0, flp);
            for (size_t i = literals.size(); i-- > 0;) {
                const std::string& litName = literals[i]->name();
                const bool isChar = litName.size() >= 2 && litName.front() == '\'';
                AstNodeExpr* const gotp
                    = isChar ? rd(tp, flp) : rd(up, flp);
                outp = new AstCond{
                    flp,
                    new AstEqN{flp, gotp,
                               new AstConst{flp, AstConst::String{},
                                            isChar ? litName : VString::downcase(litName)}},
                    i32(literals[i]->position(), flp), outp};
            }
            bodyp = AstNode::addNext(
                bodyp, static_cast<AstNode*>(new AstAssign{
                           flp, new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE}, outp}));
            funcp->addStmtsp(bodyp);
            m_modp->addStmtsp(funcp);
            m_valueFuncs.emplace(basep, funcp);
            return funcp;
        }
        if (isReal) {
            bodyp = AstNode::addNext(
                bodyp,
                static_cast<AstNode*>(new AstAssign{
                    flp, new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE},
                    new AstAtoN{flp,
                                new AstSubstrN{flp, rd(sp, flp), rd(lop, flp), rd(hip, flp)},
                                AstAtoN::ATOREAL}}));
            funcp->addStmtsp(bodyp);
            m_modp->addStmtsp(funcp);
            m_valueFuncs.emplace(basep, funcp);
            return funcp;
        }
        // An integer, or the abstract part of a physical literal. An
        // underscore is a separator and no part of the value (15.5.2).
        AstVar* const rp = newFuncLocal("r", intDTypep->cloneTree(false), flp);
        AstVar* const negp = newFuncLocal("neg", newLogicDType(flp, 1), flp);
        AstVar* const ip = newFuncLocal("i", intDTypep->cloneTree(false), flp);
        AstVar* const donep = newFuncLocal("done", newLogicDType(flp, 1), flp);
        AstVar* const cp = newFuncLocal("c", newLogicDType(flp, 8), flp);
        funcp->addStmtsp(rp);
        funcp->addStmtsp(negp);
        funcp->addStmtsp(ip);
        funcp->addStmtsp(donep);
        funcp->addStmtsp(cp);
        bodyp = AstNode::addNext(bodyp, static_cast<AstNode*>(
                                            new AstAssign{flp, wr(rp, flp), i32(0, flp)}));
        bodyp = AstNode::addNext(bodyp, static_cast<AstNode*>(new AstAssign{
                                            flp, wr(negp, flp),
                                            new AstConst{flp, AstConst::BitFalse{}}}));
        bodyp = AstNode::addNext(bodyp, static_cast<AstNode*>(new AstAssign{
                                            flp, wr(donep, flp),
                                            new AstConst{flp, AstConst::BitFalse{}}}));
        bodyp = AstNode::addNext(
            bodyp, static_cast<AstNode*>(new AstAssign{flp, wr(ip, flp), rd(lop, flp)}));
        AstNode* const signp = new AstIf{
            flp,
            new AstLogAnd{flp, new AstLte{flp, rd(ip, flp), rd(hip, flp)},
                          new AstEq{flp, new AstGetcN{flp, rd(sp, flp), rd(ip, flp)},
                                    i32('-', flp)}},
            AstNode::addNext(
                static_cast<AstNode*>(new AstAssign{flp, wr(negp, flp),
                                                    new AstConst{flp, AstConst::BitTrue{}}}),
                static_cast<AstNode*>(new AstAssign{
                    flp, wr(ip, flp), new AstAdd{flp, rd(ip, flp), i32(1, flp)}})),
            new AstIf{flp,
                      new AstLogAnd{
                          flp, new AstLte{flp, rd(ip, flp), rd(hip, flp)},
                          new AstEq{flp, new AstGetcN{flp, rd(sp, flp), rd(ip, flp)},
                                    i32('+', flp)}},
                      new AstAssign{flp, wr(ip, flp),
                                    new AstAdd{flp, rd(ip, flp), i32(1, flp)}}}};
        bodyp = AstNode::addNext(bodyp, signp);
        AstNode* const digitp = new AstIf{
            flp, new AstEq{flp, rd(cp, flp), i32('_', flp)},
            new AstAssign{flp, wr(ip, flp), new AstAdd{flp, rd(ip, flp), i32(1, flp)}},
            new AstIf{
                flp,
                new AstLogAnd{flp, new AstGte{flp, rd(cp, flp), i32('0', flp)},
                              new AstLte{flp, rd(cp, flp), i32('9', flp)}},
                AstNode::addNext(
                    static_cast<AstNode*>(new AstAssign{
                        flp, wr(rp, flp),
                        new AstAdd{flp, new AstMul{flp, rd(rp, flp), i32(10, flp)},
                                   new AstSub{flp, rd(cp, flp), i32('0', flp)}}}),
                    static_cast<AstNode*>(new AstAssign{
                        flp, wr(ip, flp), new AstAdd{flp, rd(ip, flp), i32(1, flp)}})),
                new AstAssign{flp, wr(donep, flp), new AstConst{flp, AstConst::BitTrue{}}}}};
        AstNode* loopBodyp = new AstAssign{flp, wr(cp, flp),
                                           new AstGetcN{flp, rd(sp, flp), rd(ip, flp)}};
        loopBodyp = AstNode::addNext(loopBodyp, digitp);
        bodyp = AstNode::addNext(
            bodyp, whileLoop(flp,
                             new AstLogAnd{flp, new AstLte{flp, rd(ip, flp), rd(hip, flp)},
                                           new AstLogNot{flp, rd(donep, flp)}},
                             loopBodyp));
        AstNodeExpr* valuep = new AstCond{flp, rd(negp, flp),
                                          new AstNegate{flp, rd(rp, flp)}, rd(rp, flp)};
        if (isPhys) {
            // The unit name follows the number, and says what the value is
            // counted in (15.8). It reads without regard to case.
            AstVar* const unitp = newFuncLocal(
                "unit", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            funcp->addStmtsp(unitp);
            bodyp = AstNode::addNext(
                bodyp,
                whileLoop(flp,
                          new AstLogAnd{flp, new AstLte{flp, rd(ip, flp), rd(hip, flp)},
                                        isBlank(new AstGetcN{flp, rd(sp, flp), rd(ip, flp)})},
                          new AstAssign{flp, wr(ip, flp),
                                        new AstAdd{flp, rd(ip, flp), i32(1, flp)}}));
            bodyp = AstNode::addNext(
                bodyp,
                static_cast<AstNode*>(new AstAssign{
                    flp, wr(unitp, flp),
                    new AstToLowerN{flp, new AstSubstrN{flp, rd(sp, flp), rd(ip, flp),
                                                        rd(hip, flp)}}}));
            AstNodeExpr* scalep = i32(1, flp);
            for (const auto& unit : basep->units()) {
                scalep = new AstCond{
                    flp,
                    new AstEqN{flp, rd(unitp, flp),
                               new AstConst{flp, AstConst::String{},
                                            VString::downcase(unit.first)}},
                    i32(unit.second, flp), scalep};
            }
            valuep = new AstMul{flp, valuep, scalep};
        }
        bodyp = AstNode::addNext(
            bodyp, static_cast<AstNode*>(new AstAssign{
                       flp, new AstVarRef{flp, VN_AS(funcp->fvarp(), Var), VAccess::WRITE}, valuep}));
        funcp->addStmtsp(bodyp);
        m_modp->addStmtsp(funcp);
        m_valueFuncs.emplace(basep, funcp);
        return funcp;
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
    // A signal-class formal names whatever signal the call passed. Inside a
    // copy of the body made for one call site, that signal is known, and its
    // history is the one the formal's attributes ask about (4.2.2.3).
    VhdlSymbol* boundSignal(VhdlSymbol* symp) const {
        const auto it = m_bindSymbols.find(symp);
        return it == m_bindSymbols.end() ? symp : it->second;
    }

    const VhdlShadow* shadowFor(AstNodeVhdlExpr* prefixp, FileLine* flp) {
        // "r.f'event" and "v(2)'event" ask about part of a signal, and it is
        // the whole signal whose history is kept -- the part is read out of
        // it afterwards
        VhdlSymbol* const symp
            = boundSignal(const_cast<VhdlSymbol*>(rootSymOf(prefixp)));
        // Only a signal of the design has a history to keep
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
        sh.transp->valuep(new AstConst{flp, AstConst::BitTrue{}});
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

    // A signal something forces is driven from two places: its drivers, and
    // the force that overrides them until it is released (10.5.2.1). The
    // drivers write a variable of their own, so that releasing gives back
    // the value they have been keeping all along, and the signal itself is
    // whichever of the two is in charge.
    struct VhdlForced final {
        AstVar* drvp = nullptr;  // What the ordinary drivers write
        AstVar* valp = nullptr;  // The value the force put there
        AstVar* enp = nullptr;  // Whether the force is in charge
    };
    std::set<const VhdlSymbol*> m_needForce;
    std::map<const VhdlSymbol*, VhdlForced> m_forced;
    std::map<const VhdlSymbol*, AstVar*> m_forceDrv;  // Symbol -> what drivers write

    void collectForced(AstNode* nodep) {
        if (!nodep) return;
        nodep->foreachAndNext([this](AstVhdlSigAssign* asgp) {
            if (!asgp->force() && !asgp->release()) return;
            if (const VhdlSymbol* const symp = rootSymOf(asgp->targetp())) {
                m_needForce.insert(symp->resolved());
            }
        });
    }

    void makeForced(FileLine* flp) {
        for (const VhdlSymbol* const symp : m_needForce) {
            if (m_forced.count(symp)) continue;
            const auto varIt = m_vars.find(symp);
            if (varIt == m_vars.end()) continue;
            AstVar* const sigp = varIt->second;
            VL_RESTORER(m_ftaskp);
            m_ftaskp = nullptr;
            VhdlForced fc;
            const std::string base
                = "__Vfrc" + std::to_string(++m_nameNum) + "__" + symp->name();
            FileLine* const mflp = new FileLine{flp};
            mflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            AstNodeDType* const dtypep = lowerType(symp->typep(), flp);
            fc.drvp = newTempVar(base + "__drv", dtypep, mflp);
            fc.valp = newTempVar(base + "__val", lowerType(symp->typep(), flp), mflp);
            fc.enp = newTempVar(base + "__en", newLogicDType(flp, 1), flp);
            fc.enp->valuep(new AstConst{flp, AstConst::BitFalse{}});
            // The drivers keep what the signal started with, so that a
            // release hands back a value it really had
            // Both start where the signal starts. Saying so matters beyond
            // the value: a variable with no initial value and only one
            // reader may be moved inside the function that reads it, and
            // these two have to keep what they hold from one evaluation to
            // the next.
            if (sigp->valuep()) {
                fc.drvp->valuep(VN_AS(sigp->valuep()->cloneTree(false), NodeExpr));
                fc.valp->valuep(VN_AS(sigp->valuep()->cloneTree(false), NodeExpr));
            } else {
                if (AstNodeExpr* const defp = defaultValueFor(symp->typep(), flp, true)) {
                    fc.drvp->valuep(defp);
                }
                if (AstNodeExpr* const def2p = defaultValueFor(symp->typep(), flp, true)) {
                    fc.valp->valuep(def2p);
                }
            }
            // A process rather than a continuous assignment: the value a
            // force holds and the value the drivers keep are both written
            // from processes, and substituting a continuous assignment into
            // its readers loses which of them is in charge
            AstSenTree* const senp = new AstSenTree{
                mflp, new AstSenItem{mflp, VEdgeType::ET_CHANGED,
                                     new AstVarRef{mflp, fc.enp, VAccess::READ}}};
            senp->addSensesp(new AstSenItem{mflp, VEdgeType::ET_CHANGED,
                                            new AstVarRef{mflp, fc.valp, VAccess::READ}});
            senp->addSensesp(new AstSenItem{mflp, VEdgeType::ET_CHANGED,
                                            new AstVarRef{mflp, fc.drvp, VAccess::READ}});
            AstNode* const bodyp = new AstAssign{
                mflp, new AstVarRef{mflp, sigp, VAccess::WRITE},
                new AstCond{mflp, new AstVarRef{mflp, fc.enp, VAccess::READ},
                            new AstVarRef{mflp, fc.valp, VAccess::READ},
                            new AstVarRef{mflp, fc.drvp, VAccess::READ}}};
            m_shadowsp = AstNode::addNextNull(
                m_shadowsp,
                static_cast<AstNode*>(new AstAlways{mflp, VAlwaysKwd::ALWAYS, senp, bodyp}));
            m_shadowsp = AstNode::addNextNull(
                m_shadowsp, static_cast<AstNode*>(new AstInitial{mflp, bodyp->cloneTree(false)}));
            m_forced.emplace(symp, fc);
            m_forceDrv.emplace(symp, fc.drvp);
        }
    }

    // The left-hand side of an ordinary driver. For a signal something else
    // forces, that is the variable the drivers keep rather than the signal.
    AstNodeExpr* lowerDriveTarget(AstNodeVhdlExpr* targetp) {
        VhdlSymbol* const rootp = const_cast<VhdlSymbol*>(rootSymOf(targetp));
        const auto it = rootp ? m_forceDrv.find(rootp->resolved()) : m_forceDrv.end();
        if (it == m_forceDrv.end()) return lowerExpr(targetp);
        const auto prevIt = m_vars.find(rootp);
        const bool had = prevIt != m_vars.end();
        AstVar* const savedp = had ? prevIt->second : nullptr;
        m_vars[rootp] = it->second;
        AstNodeExpr* const outp = lowerExpr(targetp);
        if (had) {
            m_vars[rootp] = savedp;
        } else {
            m_vars.erase(rootp);
        }
        return outp;
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
            // 16.2.4 leaves the initial value of S'TRANSACTION undefined and
            // calls a description that depends on it erroneous, but nvc
            // 1.22.1 and GHDL 7.0.0-dev agree on '1', and a testbench that
            // reads it before the first transaction should see what they see
            ac.togglep->valuep(new AstConst{flp, AstConst::BitTrue{}});
            ac.prevp->valuep(new AstConst{flp, AstConst::BitTrue{}});
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
        const VhdlSymbol* symp = rootSymOf(prefixp);
        if (!symp) return nullptr;
        // A signal-class formal answers from the signal the call passed
        const auto bindIt = m_bindSymbols.find(symp);
        if (bindIt != m_bindSymbols.end()) symp = bindIt->second;
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
    // The same part of a signal, read out of one of the variables that keep
    // its history. Each scalar subelement of a composite signal is a signal
    // in its own right (14.7.3.4), so "v(2)'event" asks about that element
    // and no other -- which means indexing the history the way the signal
    // itself is indexed.
    AstNodeExpr* shadowPart(AstNodeVhdlExpr* prefixp, AstVar* shadowVarp, FileLine* flp) {
        VhdlSymbol* const rootp = const_cast<VhdlSymbol*>(rootSymOf(prefixp));
        if (!rootp || VN_IS(prefixp, VhdlName)) {
            return new AstVarRef{flp, shadowVarp, VAccess::READ};
        }
        const auto prevIt = m_vars.find(rootp);
        const bool had = prevIt != m_vars.end();
        AstVar* const savedp = had ? prevIt->second : nullptr;
        m_vars[rootp] = shadowVarp;
        AstNodeExpr* const outp = lowerExpr(prefixp);
        if (had) {
            m_vars[rootp] = savedp;
        } else {
            m_vars.erase(rootp);
        }
        return outp ? outp : new AstVarRef{flp, shadowVarp, VAccess::READ};
    }

    AstNodeExpr* eventOf(AstNodeVhdlExpr* prefixp, const VhdlShadow* shp, FileLine* flp) {
        AstNodeExpr* const nowp = lowerExpr(prefixp);
        if (!nowp) return nullptr;
        return new AstNeq{flp, nowp, shadowPart(prefixp, shp->prevp, flp)};
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
        if (!argp) return shadowPart(prefixp, shp->prevp, flp);
        AstConst* const constp = VN_CAST(argp, Const);
        if (!constp) {
            unsup(flp, "'delayed by a time that is not static");
            return nullptr;
        }
        const int64_t ticks = static_cast<int64_t>(constp->toUQuad());
        if (ticks == 0) {
            VL_DO_DANGLING(argp->deleteTree(), argp);
            return shadowPart(prefixp, shp->prevp, flp);
        }
        if (!v3Global.opt.timing().isTrue()) {
            unsup(flp, "'delayed by a time without --timing");
            return nullptr;
        }
        VhdlSymbol* const symp = const_cast<VhdlSymbol*>(rootSymOf(prefixp));
        const auto varIt = symp ? m_vars.find(symp->resolved()) : m_vars.end();
        if (varIt == m_vars.end()) return nullptr;
        AstVar* const sigp = varIt->second;
        const auto key = std::make_pair(static_cast<const VhdlSymbol*>(symp->resolved()), ticks);
        const auto found = m_delayeds.find(key);
        if (found != m_delayeds.end()) {
            VL_DO_DANGLING(argp->deleteTree(), argp);
            return shadowPart(prefixp, found->second, flp);
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
        return shadowPart(prefixp, dlyp, flp);
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
            return new AstCond{flp, evp, shadowPart(nodep->prefixp(), shp->prevp, flp),
                               shadowPart(nodep->prefixp(), shp->lastp, flp)};
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

    // What an object became. An object declared in a package is not emitted
    // with the package -- there is no package in the generated code -- so it
    // is emitted into the module that reads it, on first use. Null if this
    // symbol is not an object of the design's own.
    AstVar* objectVar(VhdlSymbol* symp, FileLine* flp) {
        if (!symp) return nullptr;
        const auto it = m_vars.find(symp);
        if (it != m_vars.end()) return it->second;
        if (!symp->isObject() || symp->predefined()) return nullptr;
        // A deferred constant is declared in the package and given its value
        // in the package body, and it is that second declaration which says
        // what the value is (6.4.2.2)
        AstVhdlObjectDecl* const declp
            = VN_CAST(symp->fullDeclp() ? symp->fullDeclp() : symp->declp(), VhdlObjectDecl);
        if (!declp) return nullptr;
        // The type is lowered first: a protected type becomes a class whose
        // own body may name this same object, and that reference must find
        // the one variable rather than make a second
        AstNodeDType* const dtypep = lowerType(symp->typep(), flp);
        const auto madeIt = m_vars.find(symp);
        if (madeIt != m_vars.end()) {
            VL_DO_DANGLING(dtypep->deleteTree(), dtypep);
            return madeIt->second;
        }
        // A constant standing for a generic an instantiation supplied is a
        // parameter, not a variable: a type declared beside it may be as long
        // as it says, and that range has to fold with it (4.9)
        const VVarType varType
            = declp->isElabConstant() ? VVarType::LPARAM : VVarType::VAR;
        AstVar* const varp = newVarOfType(symp, varType, dtypep, flp);
        if (declp->defaultp()) {
            varp->valuep(lowerExpr(declp->defaultp()));
        } else {
            varp->valuep(defaultValueFor(symp->typep(), flp));
        }
        if (m_protClassp && inProtectedBody(symp)) {
            m_protClassp->addStmtsp(varp);
        } else if (m_protClassp && m_modp) {
            // Declared outside the protected type, so it belongs to the
            // module however it was reached
            m_modp->addStmtsp(varp);
        } else {
            m_stmtsp = AstNode::addNextNull(m_stmtsp, varp);
        }
        return varp;
    }

    // Whether a subprogram may be called with no arguments at all
    static bool allParamsDefaulted(const VhdlSymbol* symp) {
        for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
            if (!symp->paramHasDefault(i)) return false;
        }
        return true;
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
            // Where the target is a plain name, what it denotes is already
            // known and the expression adds nothing -- which matters for an
            // alias of an alias, whose target names no object of its own
            const bool plainName = declp && VN_IS(declp->targetp(), VhdlName);
            if (symp->aliasp() && (plainName || !declp || !declp->targetp())) {
                return lowerRef(symp->aliasp(), name, flp);
            }
            if (declp && declp->targetp()) return lowerExpr(declp->targetp());
        }
        const auto bit = m_bindExprs.find(symp);
        if (bit != m_bindExprs.end()) return lowerExpr(bit->second);
        // One element of an array signal, at the index this call passed
        const auto dit = m_bindDynSigs.find(symp);
        if (dit != m_bindDynSigs.end() && dit->second.arrayp && dit->second.idxp) {
            return new AstSelBit{flp, new AstVarRef{flp, dit->second.arrayp, VAccess::READ},
                                 new AstVarRef{flp, dit->second.idxp, VAccess::READ}};
        }
        const auto it = m_vars.find(symp);
        if (it != m_vars.end()) {
            return new AstVarRef{flp, it->second, VAccess::READ};
        }
        // A signal or a shared variable the design never declared inside a
        // module is one a package declared, and there is one of it
        if ((symp->kind() == VhdlSymKind::SIGNAL
             || symp->kind() == VhdlSymKind::SHARED_VARIABLE)
            && !symp->hasValue()) {
            if (AstVar* const varp = pkgSignalVar(symp, flp)) {
                return new AstVarRef{flp, varp, VAccess::READ};
            }
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
        // A function is called by naming it, with no parentheses, when it
        // needs no argument -- either because it takes none or because every
        // parameter has a default (4.2.2.1, 6.5.7.1)
        if (symp->kind() == VhdlSymKind::FUNCTION && allParamsDefaulted(symp)) {
            if (symp->name() == "now") {
                return new AstTime{flp, m_modp ? m_modp->timeunit() : VTimescale{}};
            }
            // The smallest time this simulation tells apart. Time is counted
            // in femtoseconds here, which is TIME's own primary unit (5.2.4.2)
            if (symp->name() == "resolution_limit" && symp->predefined()) {
                return new AstConst{flp, AstConst::Unsized64{}, 1};
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
        if (AstVar* const varp = objectVar(symp, flp)) {
            return new AstVarRef{flp, varp, VAccess::READ};
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
    // A signal a package declares is one signal for the whole design, not one
    // per module that uses it (6.4.2.3), so it is emitted once into the unit
    // every module can see and every reference finds the same object. VUnit's
    // message passing is built on one of these.
    std::map<const VhdlSymbol*, AstVar*> m_pkgSigs;

    // Whether a package is where the declaration stands. A signal of a module
    // that this has simply not reached yet is not one of these, and giving it
    // an object of its own would leave the module's drives going somewhere
    // nothing reads.
    static bool declaredInPackage(const VhdlSymbol* symp) {
        for (const AstNode* p = symp->declp(); p; p = p->backp()) {
            if (VN_IS(p, VhdlPackage) || VN_IS(p, VhdlPackageBody)) return true;
            if (VN_IS(p, VhdlArchitecture) || VN_IS(p, VhdlEntity)) return false;
        }
        return false;
    }

    AstVar* pkgSignalVar(VhdlSymbol* symp, FileLine* flp) {
        if (!declaredInPackage(symp)) return nullptr;
        const auto found = m_pkgSigs.find(symp);
        if (found != m_pkgSigs.end()) {
            m_vars[symp] = found->second;
            return found->second;
        }
        AstPackage* const unitp = m_netlistp ? m_netlistp->dollarUnitPkgAddp() : nullptr;
        if (!unitp) return nullptr;
        std::string name = symp->name();
        if (const VhdlScope* const scopep = symp->declScopep()) {
            name = scopep->name() + "__" + name;
        }
        AstVar* const varp = new AstVar{flp, VVarType::VAR, "__Vpkg__" + name,
                                        VFlagChildDType{}, lowerType(symp->typep(), flp)};
        varp->lifetime(VLifetime::STATIC_EXPLICIT);
        varp->trace(false);
        AstVhdlObjectDecl* const declp = VN_CAST(symp->declp(), VhdlObjectDecl);
        if (declp && declp->defaultp()) {
            varp->valuep(lowerExpr(declp->defaultp()));
        } else {
            varp->valuep(defaultValueFor(symp->typep(), flp));
        }
        unitp->addStmtsp(varp);
        m_pkgSigs.emplace(symp, varp);
        m_vars[symp] = varp;
        return varp;
    }

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
        if (dynAccessOfExpr(nodep)) return lowerDynAllocator(nodep, flp);
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
        // An access to STRING is a LINE, which is text rather than an object
        // with a handle, so allocating one is just the text it holds
        const VhdlType* const designatedp
            = nodep->valuep() ? nodep->valuep()->vhdlTypep()
                              : (nodep->subtypep() ? nodep->subtypep()->vhdlTypep() : nullptr);
        if (designatedp && isStringType(designatedp)) {
            VL_DO_DANGLING(newp->deleteTree(), newp);
            return valuep;
        }
        newp->addArgsp(new AstArg{flp, "", valuep});
        return newp;
    }

    // "new t(l to r)" for an array whose bounds only the allocation fixes:
    // the object it makes carries them, so they are what it is made with
    // (5.4.1, 9.3.7)
    AstNodeExpr* lowerDynAllocator(AstVhdlAllocator* nodep, FileLine* flp) {
        const DynAccess* const dynp = dynAccessOfExpr(nodep);
        AstVhdlRange* const rp = allocatorRange(nodep);
        if (rp && rp->leftp() && rp->rightp()) {
            AstNodeExpr* const leftp = lowerExpr(rp->leftp());
            AstNodeExpr* const rightp = lowerExpr(rp->rightp());
            if (!leftp || !rightp) {
                if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
                if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
                return nullptr;
            }
            const bool ascending = rp->dir() != VhdlDir::DOWNTO;
            // A null range gives an object with no elements, which
            // "right - left + 1" says by coming out at nothing or less
            AstNodeExpr* const spanp
                = ascending ? new AstSub{flp, rightp, leftp->cloneTree(false)}
                            : new AstSub{flp, leftp->cloneTree(false), rightp};
            return dynNew(new AstAdd{flp, spanp, new AstConst{flp, AstConst::Signed32{}, 1}},
                          leftp, ascending ? 1 : -1, flp);
        }
        // "new s" of a subtype that constrains the array itself
        const VhdlType* const subTypep
            = nodep->subtypep() ? nodep->subtypep()->vhdlTypep() : nullptr;
        if (!nodep->valuep() && dynBoundedArray(subTypep)) {
            const VhdlType* const idxp = subTypep->indexTypes()[0];
            return dynNew(new AstConst{flp, AstConst::Signed32{},
                                       static_cast<int32_t>(idxp->length())},
                          new AstConst{flp, AstConst::Signed32{},
                                       static_cast<int32_t>(idxp->left())},
                          idxp->ascending() ? 1 : -1, flp);
        }
        // "new t'(v)", where the value decides the bounds
        if (nodep->valuep()) return dynAllocFromValue(nodep->valuep(), dynp, flp);
        unsup(flp, "allocator for an unconstrained array without a range");
        return nullptr;
    }

    // The range an allocator gives the array it makes. "new t(l to r)"
    // parses as a name with a range rather than as a subtype indication with
    // a constraint, since the two look alike until the name is known (8.1).
    static AstVhdlRange* allocatorRange(AstVhdlAllocator* nodep) {
        AstVhdlSubtypeInd* const subp = nodep->subtypep();
        if (!subp) return nullptr;
        if (AstVhdlIndexConstraint* const icp
            = VN_CAST(subp->constraintp(), VhdlIndexConstraint)) {
            if (AstVhdlRange* const rangep = VN_CAST(icp->rangesp(), VhdlRange)) return rangep;
        }
        if (AstVhdlCall* const callp = VN_CAST(subp->typeMarkp(), VhdlCall)) {
            return VN_CAST(callp->argsp(), VhdlRange);
        }
        return nullptr;
    }

    // Whether a type is an array of one dimension whose bounds are known
    static bool dynBoundedArray(const VhdlType* typep) {
        return typep && typep->isConstrainedArray() && typep->dimensions() == 1
               && typep->indexTypes()[0] && typep->indexTypes()[0]->bounded();
    }

    // The object an allocator makes, given how many elements it has and
    // where its range starts. Takes ownership of both expressions.
    static AstNodeExpr* dynNew(AstNodeExpr* countp, AstNodeExpr* leftp, int dir,
                               FileLine* flp) {
        AstNew* const newp = new AstNew{flp};
        newp->addArgsp(new AstArg{flp, "", countp});
        newp->addArgsp(new AstArg{flp, "", leftp});
        newp->addArgsp(new AstArg{flp, "", new AstConst{flp, AstConst::Signed32{}, dir}});
        return newp;
    }

    // "new t'(l to r => v)", where the range is worked out while running:
    // every element is the one value, so the object is made that long and
    // filled with it. Null where the value is not of that shape.
    AstNodeExpr* dynAllocFromRun(AstNodeVhdlExpr* valuep, const DynAccess* dynp,
                                 FileLine* flp) {
        AstVhdlAggregate* const aggp = VN_CAST(valuep, VhdlAggregate);
        if (!aggp || !aggp->elemsp() || aggp->elemsp()->nextp()) return nullptr;
        AstVhdlElemAssoc* const eap = aggp->elemsp();
        AstVhdlChoice* const chp = eap->choicesp();
        if (!chp || chp->nextp() || !chp->rangep() || !eap->valuep()) return nullptr;
        AstNodeExpr* leftp = nullptr;
        AstNodeExpr* rightp = nullptr;
        bool ascending = true;
        bool dirKnown = true;
        if (!rangeParts(chp->rangep(), leftp, rightp, ascending, dirKnown) || !dirKnown) {
            if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
            if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
            return nullptr;
        }
        AstNodeExpr* const elemp = lowerExpr(eap->valuep());
        if (!elemp) {
            VL_DO_DANGLING(leftp->deleteTree(), leftp);
            VL_DO_DANGLING(rightp->deleteTree(), rightp);
            return nullptr;
        }
        AstVar* const objVarp = newTempVar(
            "__Vdynobj", new AstClassRefDType{flp, dynp->classp, nullptr}, flp);
        AstVar* const cntVarp = newTempVar("__Vdyncnt", newIntDType(flp), flp);
        AstVar* const idxVarp = newTempVar("__Vdynj", newIntDType(flp), flp);
        idxVarp->usedLoopIdx(true);
        AstNodeExpr* const spanp = ascending
                                       ? new AstSub{flp, rightp, leftp->cloneTree(false)}
                                       : new AstSub{flp, leftp->cloneTree(false), rightp};
        AstNode* stmtsp = new AstAssign{
            flp, new AstVarRef{flp, cntVarp, VAccess::WRITE},
            new AstAdd{flp, spanp, new AstConst{flp, AstConst::Signed32{}, 1}}};
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(new AstAssign{
                        flp, new AstVarRef{flp, objVarp, VAccess::WRITE},
                        dynNew(new AstVarRef{flp, cntVarp, VAccess::READ}, leftp,
                               ascending ? 1 : -1, flp)}));
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(
                        new AstAssign{flp, new AstVarRef{flp, idxVarp, VAccess::WRITE},
                                      new AstConst{flp, AstConst::Signed32{}, 0}}));
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, new AstVarRef{flp, idxVarp, VAccess::READ},
                       new AstVarRef{flp, cntVarp, VAccess::READ}}});
        loopp->addStmtsp(new AstAssign{
            flp,
            new AstSelBit{flp,
                          memberOf(new AstVarRef{flp, objVarp, VAccess::READ},
                                   dynp->datap->name(), flp),
                          new AstVarRef{flp, idxVarp, VAccess::READ}},
            elemp});
        loopp->addContsp(new AstAssign{
            flp, new AstVarRef{flp, idxVarp, VAccess::WRITE},
            new AstAdd{flp, new AstVarRef{flp, idxVarp, VAccess::READ},
                       new AstConst{flp, AstConst::Signed32{}, 1}}});
        stmtsp = AstNode::addNext(stmtsp, static_cast<AstNode*>(loopp));
        return new AstExprStmt{flp, stmtsp, new AstVarRef{flp, objVarp, VAccess::READ}};
    }

    // "new t'(v)" for an array whose bounds only the value gives: the object
    // is made the size of the value, and the value copied into it. The
    // statements that do so travel with the expression, since an allocator
    // may appear anywhere an expression may (9.3.7).
    AstNodeExpr* dynAllocFromValue(AstNodeVhdlExpr* valuep, const DynAccess* dynp,
                                   FileLine* flp) {
        // "t'(v)" is of the type mark, which says nothing about bounds; the
        // value inside it is what has them (9.3.5)
        while (AstVhdlQualified* const qualp = VN_CAST(valuep, VhdlQualified)) {
            if (dynBoundedArray(valuep->vhdlTypep())) break;
            valuep = qualp->valuep();
        }
        VhdlType* const typep = valuep->vhdlTypep();
        if (!dynBoundedArray(typep)) {
            // "new t'(l to r => v)": the aggregate says its own range, and
            // the bounds may only be known while running
            if (AstNodeExpr* const outp = dynAllocFromRun(valuep, dynp, flp)) return outp;
            unsup(flp, "allocator for a value whose bounds are not known");
            return nullptr;
        }
        const VhdlType* const idxTypep = typep->indexTypes()[0];
        const int64_t count = idxTypep->length();
        AstNodeExpr* const srcp = lowerExpr(valuep);
        if (!srcp) return nullptr;
        // The value is written into a temporary of its own type first, since
        // an aggregate or a call result cannot be read one element at a time
        AstVar* const srcVarp
            = newTempVar("__Vdynval", lowerType(typep, flp), flp);
        AstVar* const objVarp = newTempVar(
            "__Vdynobj", new AstClassRefDType{flp, dynp->classp, nullptr}, flp);
        AstVar* const idxVarp = newTempVar("__Vdynj", newIntDType(flp), flp);
        idxVarp->usedLoopIdx(true);
        AstNode* stmtsp
            = new AstAssign{flp, new AstVarRef{flp, srcVarp, VAccess::WRITE}, srcp};
        stmtsp = AstNode::addNext(
            stmtsp,
            static_cast<AstNode*>(new AstAssign{
                flp, new AstVarRef{flp, objVarp, VAccess::WRITE},
                dynNew(new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(count)},
                       new AstConst{flp, AstConst::Signed32{},
                                    static_cast<int32_t>(idxTypep->left())},
                       idxTypep->ascending() ? 1 : -1, flp)}));
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(
                        new AstAssign{flp, new AstVarRef{flp, idxVarp, VAccess::WRITE},
                                      new AstConst{flp, AstConst::Signed32{}, 0}}));
        AstNodeExpr* const srcIdxp
            = new AstConst{flp, AstConst::Signed32{},
                           static_cast<int32_t>(idxTypep->left())};
        AstNodeExpr* const elemIdxp
            = idxTypep->ascending()
                  ? static_cast<AstNodeExpr*>(
                        new AstAdd{flp, srcIdxp, new AstVarRef{flp, idxVarp, VAccess::READ}})
                  : static_cast<AstNodeExpr*>(
                        new AstSub{flp, srcIdxp, new AstVarRef{flp, idxVarp, VAccess::READ}});
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, new AstVarRef{flp, idxVarp, VAccess::READ},
                       new AstConst{flp, AstConst::Signed32{},
                                    static_cast<int32_t>(count)}}});
        loopp->addStmtsp(new AstAssign{
            flp,
            new AstSelBit{flp,
                          memberOf(new AstVarRef{flp, objVarp, VAccess::READ},
                                   dynp->datap->name(), flp),
                          new AstVarRef{flp, idxVarp, VAccess::READ}},
            new AstSelBit{flp, new AstVarRef{flp, srcVarp, VAccess::READ}, elemIdxp}});
        loopp->addContsp(new AstAssign{
            flp, new AstVarRef{flp, idxVarp, VAccess::WRITE},
            new AstAdd{flp, new AstVarRef{flp, idxVarp, VAccess::READ},
                       new AstConst{flp, AstConst::Signed32{}, 1}}});
        stmtsp = AstNode::addNext(stmtsp, static_cast<AstNode*>(loopp));
        return new AstExprStmt{flp, stmtsp, new AstVarRef{flp, objVarp, VAccess::READ}};
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

    // What an alias of a slice is an alias of. An index into such an alias is
    // put onto the numbering of the object the slice comes from, so that is
    // what has to be indexed.
    static AstNodeVhdlExpr* aliasBase(VhdlSymbol* symp, AstNodeVhdlExpr* prefixp) {
        if (!symp || symp->kind() != VhdlSymKind::ALIAS) return prefixp;
        const AstVhdlAliasDecl* const declp = VN_CAST(symp->declp(), VhdlAliasDecl);
        if (!declp || !declp->targetp()) return prefixp;
        AstVhdlCall* const callp = VN_CAST(declp->targetp(), VhdlCall);
        if (!callp || !VN_IS(callp->argsp(), VhdlRange)) return prefixp;
        if (aliasKeepsRange(symp, declp)) return prefixp;
        return callp->prefixp();
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
            // type's own limit at the other end: the least of nothing is the
            // largest value the type has (5.3.2.4)
            VL_DO_DANGLING(arrp->deleteTree(), arrp);
            VhdlType* const elemp = typep->elementp();
            if (!elemp || !elemp->bounded()) {
                unsup(flp, std::string{wantMax ? "maximum" : "minimum"} + " over a null array");
                return nullptr;
            }
            const bool wantHigh = !wantMax;
            if (elemp->base()->kind() == VhdlTypeKind::FLOATING) {
                return lowerValue(
                    VhdlValue::mkReal(wantHigh ? elemp->highReal() : elemp->lowReal(), elemp),
                    elemp, flp);
            }
            return lowerValue(VhdlValue::mkInt(wantHigh ? elemp->high() : elemp->low(), elemp),
                              elemp, flp);
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
            if (!fromp) return nullptr;
            return derefIfAccess(fromp, nodep->prefixp(), flp);
        }
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (symp && symp->kind() == VhdlSymKind::RECORD_ELEMENT) {
            // An element of a formal the call associated one element at a
            // time is the actual signal itself (6.5.7.1)
            if (AstNodeVhdlExpr* const boundp = elementBindOf(nodep)) return lowerExpr(boundp);
            AstNodeExpr* fromp = lowerExpr(nodep->prefixp());
            if (!fromp) return nullptr;
            fromp = derefIfAccess(fromp, nodep->prefixp(), flp);
            return new AstMemberSel{flp, fromp, VFlagChildDType{}, symp->name()};
        }
        if (symp && symp->kind() == VhdlSymKind::FUNCTION) {
            const VhdlType* protTypep = nullptr;
            if (AstNodeExpr* const instp = protectedInstance(nodep->prefixp(), &protTypep)) {
                return new AstMethodCall{flp, instp, methodName(symp, protTypep), nullptr};
            }
        }
        if (symp) return lowerRef(symp, symp->name(), flp);
        // A selected name of something in another design unit
        std::vector<std::string> parts;
        flattenName(nodep, parts);
        unsup(flp, "name '" + (parts.empty() ? std::string{"?"} : parts.back()) + "'");
        return nullptr;
    }

    // Where a selected name reads an element of a formal whose elements were
    // associated one at a time, the actual that element was given (6.5.7.1)
    AstNodeVhdlExpr* elementBindOf(const AstVhdlSelected* nodep) {
        if (m_bindElems.empty() || !nodep->vhdlSymp()) return nullptr;
        const AstVhdlName* const prefixp = VN_CAST(nodep->prefixp(), VhdlName);
        if (!prefixp || !prefixp->vhdlSymp()) return nullptr;
        const auto it = m_bindElems.find(prefixp->vhdlSymp());
        if (it == m_bindElems.end()) return nullptr;
        const auto eit = it->second.find(nodep->vhdlSymp()->name());
        return eit == it->second.end() ? nullptr : eit->second;
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

    // The types the short-circuit rule of 9.2.2 names, and no others: a
    // nine-value logic type is not one of them, and neither is an array
    static bool shortCircuits(const VhdlType* typep) {
        if (!typep) return false;
        const VhdlType* const basep = typep->base();
        return basep == V3VhdlStd::booleanType() || basep == V3VhdlStd::bitType();
    }

    // Whether a type is one value of a logic type -- STD_ULOGIC and the like
    static bool isScalarLogic(const VhdlType* typep) {
        return typep && typep->base()->kind() == VhdlTypeKind::ENUM && enumIsLogic(typep);
    }

    // Whether an expression is written as one of the four literals that this
    // runtime cannot tell apart from X
    static bool metavalueLiteral(const AstNodeVhdlExpr* nodep) {
        std::string name;
        if (const AstVhdlLiteral* const litp = VN_CAST(nodep, VhdlLiteral)) {
            if (litp->kind() != VhdlLitKind::CHARACTER) return false;
            name = litp->text();
        } else if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            if (!namep->isChar()) return false;
            name = namep->name();
        } else {
            return false;
        }
        if (name.size() == 3 && name.front() == '\'' && name.back() == '\'') {
            name = name.substr(1, 1);
        }
        if (name.size() != 1) return false;
        const char c = name[0];
        return c == 'U' || c == 'X' || c == 'W' || c == '-';
    }

    // Whether an expression is written as the '-' of 10.9, the one metavalue
    // a matching case reads as "any value"
    static bool dontCareLiteral(const AstNodeVhdlExpr* nodep) {
        if (const AstVhdlLiteral* const litp = VN_CAST(nodep, VhdlLiteral)) {
            return litp->kind() == VhdlLitKind::CHARACTER && litp->text() == "'-'";
        }
        if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            return namep->isChar() && (namep->name() == "'-'" || namep->name() == "-");
        }
        return false;
    }

    // Whether a type is an array of a logic type, so that a string written
    // for it stands for a vector rather than for text
    static bool isLogicArray(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return false;
        return enumIsLogic(typep->base()->elementp());
    }

    // Whether a case choice names a value this runtime cannot hold, so that
    // it can never be taken. Both a single 'U'/'X'/'W'/'-' and a string with
    // one in it count -- the latter is how a selected assignment over a
    // vector is usually written.
    static bool metavalueChoice(const AstNodeVhdlExpr* nodep) {
        if (metavalueLiteral(nodep)) return true;
        const AstVhdlLiteral* const litp = VN_CAST(nodep, VhdlLiteral);
        if (!litp || litp->kind() != VhdlLitKind::STRING) return false;
        if (!isLogicArray(nodep->vhdlTypep())) return false;
        return litp->text().find_first_of("UuXxWw-") != std::string::npos;
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
        // Comparing one value of a logic type with 'U', 'X', 'W' or '-'
        // never holds: this runtime has no metavalues, and those four map
        // onto the one X that cannot occur (see the accepted divergences).
        // Left as an ordinary comparison it would find X written as zero and
        // match every '0' -- worse than saying no, which is what the mapping
        // means. 'Z' is representable, so it is compared as it stands.
        if (nodep->op() == VhdlBinaryOp::EQ || nodep->op() == VhdlBinaryOp::NE) {
            if (isScalarLogic(ltypep) && isScalarLogic(rtypep)
                && (metavalueLiteral(nodep->lhsp()) || metavalueLiteral(nodep->rhsp()))) {
                if (nodep->op() == VhdlBinaryOp::EQ) {
                    return new AstConst{flp, AstConst::BitFalse{}};
                }
                return new AstConst{flp, AstConst::BitTrue{}};
            }
        }
        // Comparing values that carry their own bounds is a length and then
        // an element at a time, which a function of the type does (9.2.3)
        if ((nodep->op() == VhdlBinaryOp::EQ || nodep->op() == VhdlBinaryOp::NE)
            && (isDynValue(nodep->lhsp()) || isDynValue(nodep->rhsp()))) {
            VhdlType* const carrp
                = carriedDynamically(ltypep) ? ltypep : rtypep;
            AstNodeFTask* const eqp = carrp ? dynEqFunc(carrp, flp) : nullptr;
            AstNodeExpr* const ap = eqp ? asDynCarrier(nodep->lhsp(), carrp, flp) : nullptr;
            AstNodeExpr* const bp = ap ? asDynCarrier(nodep->rhsp(), carrp, flp) : nullptr;
            if (ap && bp) {
                AstFuncRef* const refp = new AstFuncRef{
                    flp, eqp->name(),
                    AstNode::addNext(new AstArg{flp, "", ap}, new AstArg{flp, "", bp})};
                refp->taskp(VN_AS(eqp, Func));
                if (nodep->op() == VhdlBinaryOp::NE) return new AstLogNot{flp, refp};
                return refp;
            }
            if (ap) VL_DO_DANGLING(ap->deleteTree(), ap);
            unsup(flp, "comparison of a value whose length only running decides");
            return nullptr;
        }
        AstNodeExpr* lhsp = lowerExpr(nodep->lhsp());
        AstNodeExpr* rhsp = lowerExpr(nodep->rhsp());
        if (!lhsp || !rhsp) return nullptr;
        matchVectorWidth(nodep, ltypep, rtypep, lhsp, rhsp, flp);
        const bool signd = isSignedType(ltypep) || isSignedType(rtypep);
        // A real literal that no context has converted is still a real. "**"
        // is the operator that shows it, being the only predefined one whose
        // two operands have different types: "2.0 ** n" would otherwise be
        // taken for integer arithmetic over a double.
        const auto isRealValue = [](const VhdlType* typep) {
            return typep
                   && (typep->base()->kind() == VhdlTypeKind::FLOATING
                       || typep->base()->kind() == VhdlTypeKind::UNIVERSAL_REAL);
        };
        const bool real = isRealValue(ltypep) || isRealValue(rtypep);
        // A physical value scaled by a real is defined to be computed in real
        // arithmetic and converted back (9.2.7), so the conversion is written
        // out rather than left to be inferred and reported
        const auto& isRealish = isRealValue;
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
        // On BOOLEAN and BIT, and only there, these four are short-circuit
        // operations: the right operand is evaluated only if the left has not
        // already decided the answer (9.2.2). It is not an optimisation --
        // "i <= v'high and v(i) = '1'" depends on it, and so does any impure
        // function written on the right.
        case VhdlBinaryOp::AND:
            if (shortCircuits(ltypep)) return new AstLogAnd{flp, lhsp, rhsp};
            return new AstAnd{flp, lhsp, rhsp};
        case VhdlBinaryOp::OR:
            if (shortCircuits(ltypep)) return new AstLogOr{flp, lhsp, rhsp};
            return new AstOr{flp, lhsp, rhsp};
        case VhdlBinaryOp::XOR: return new AstXor{flp, lhsp, rhsp};
        case VhdlBinaryOp::NAND:
            if (shortCircuits(ltypep)) {
                return new AstLogNot{flp, new AstLogAnd{flp, lhsp, rhsp}};
            }
            return new AstNot{flp, new AstAnd{flp, lhsp, rhsp}};
        case VhdlBinaryOp::NOR:
            if (shortCircuits(ltypep)) {
                return new AstLogNot{flp, new AstLogOr{flp, lhsp, rhsp}};
            }
            return new AstNot{flp, new AstOr{flp, lhsp, rhsp}};
        case VhdlBinaryOp::XNOR: return new AstNot{flp, new AstXor{flp, lhsp, rhsp}};
        case VhdlBinaryOp::EQ:
        case VhdlBinaryOp::MATCH_EQ:
            if (real) return new AstEqD{flp, lhsp, rhsp};
            // A logic literal with a Z in it is compared exactly: "s = 'Z'"
            // is true when s is 'Z' (9.2.3), where Verilog's "==" answers X.
            if (hasMetaConst(lhsp) || hasMetaConst(rhsp)) {
                return new AstEqCase{flp, lhsp, rhsp};
            }
            return new AstEq{flp, lhsp, rhsp};
        case VhdlBinaryOp::NE:
        case VhdlBinaryOp::MATCH_NE:
            if (real) return new AstNeqD{flp, lhsp, rhsp};
            if (hasMetaConst(lhsp) || hasMetaConst(rhsp)) {
                return new AstNeqCase{flp, lhsp, rhsp};
            }
            return new AstNeq{flp, lhsp, rhsp};
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
        case VhdlBinaryOp::POW: {
            if (real) return new AstPowD{flp, lhsp, rhsp};
            // "**" on INTEGER gives an INTEGER (9.2.8). Saying so here rather
            // than leaving the width to be worked out is what lets one stand
            // in a range, where the width pass reaches it already settled.
            AstNodeExpr* const powp
                = signd ? static_cast<AstNodeExpr*>(new AstPowSS{flp, lhsp, rhsp})
                        : static_cast<AstNodeExpr*>(new AstPow{flp, lhsp, rhsp});
            powp->dtypeSetInteger();
            return powp;
        }
        case VhdlBinaryOp::CONCAT: return new AstConcat{flp, lhsp, rhsp};
        case VhdlBinaryOp::SLL:
        case VhdlBinaryOp::SRL:
        case VhdlBinaryOp::SLA:
        case VhdlBinaryOp::SRA: return lowerShift(nodep, lhsp, rhsp, flp);
        case VhdlBinaryOp::ROL:
        case VhdlBinaryOp::ROR: {
            // 9.2.4 says a rotate moves every element one place and repeats
            // that as many times as the right operand asks, with a negative
            // count going the other way. Repeating past the length comes back
            // round, so the count is folded into range and the rotate is the
            // two shifts it amounts to. Both operands are read into variables
            // first, since each is needed twice and neither may run twice.
            const int width = exprVectorWidth(nodep->lhsp());
            if (width <= 0) {
                unsup(flp, "rotate of a vector whose length is not known here");
                return nullptr;
            }
            return rotateOf(lhsp, rhsp, width, nodep->op() == VhdlBinaryOp::ROL, flp);
        }
        }
        unsup(flp, "binary operator");
        return nullptr;
    }

    // A rotate by a count, written as the two shifts it amounts to (9.2.4)
    AstNodeExpr* rotateOf(AstNodeExpr* lhsp, AstNodeExpr* rhsp, int width, bool leftward,
                          FileLine* flp) {
        {
            AstVar* const valp = newTempVar("__Vrotv", newLogicDType(flp, width), flp);
            AstVar* const cntp = newTempVar("__Vrotn", newIntDType(flp), flp);
            const auto widthConst = [&]() {
                return new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(width)};
            };
            AstNodeExpr* placesp
                = leftward ? rhsp : static_cast<AstNodeExpr*>(new AstNegate{flp, rhsp});
            // ((n rem W) + W) rem W leaves the count in 0 .. W-1 whichever
            // way round it was written
            AstNodeExpr* const foldedp = new AstModDivS{
                flp,
                new AstAdd{flp, new AstModDivS{flp, placesp, widthConst()}, widthConst()},
                widthConst()};
            AstNode* stmtsp
                = new AstAssign{flp, new AstVarRef{flp, valp, VAccess::WRITE}, lhsp};
            stmtsp = AstNode::addNext(
                stmtsp, static_cast<AstNode*>(new AstAssign{
                            flp, new AstVarRef{flp, cntp, VAccess::WRITE}, foldedp}));
            // The part that wraps is written as two shifts rather than one by
            // W - count, because that would be a shift by the whole width
            // when the count is zero
            AstNodeExpr* const wrapp = new AstShiftR{
                flp,
                new AstShiftR{flp, new AstVarRef{flp, valp, VAccess::READ},
                              new AstConst{flp, AstConst::Signed32{}, 1}},
                new AstSub{flp, new AstConst{flp, AstConst::Signed32{}, width - 1},
                           new AstVarRef{flp, cntp, VAccess::READ}}};
            return new AstExprStmt{
                flp, stmtsp,
                new AstOr{flp,
                          new AstShiftL{flp, new AstVarRef{flp, valp, VAccess::READ},
                                        new AstVarRef{flp, cntp, VAccess::READ}},
                          wrapp}};
        }
    }

    // The four shift operators of 9.2.4. What comes in at the vacated end is
    // what tells them apart: "sll" and "srl" bring in the element type's own
    // left bound, "sla" repeats the operand's rightmost element and "sra" its
    // leftmost. A count that is negative shifts the other way instead.
    AstNodeExpr* lowerShift(AstVhdlBinary* nodep, AstNodeExpr* lhsp, AstNodeExpr* rhsp,
                            FileLine* flp) {
        const VhdlBinaryOp op = nodep->op();
        const bool arith = op == VhdlBinaryOp::SLA || op == VhdlBinaryOp::SRA;
        const bool leftward = op == VhdlBinaryOp::SLL || op == VhdlBinaryOp::SLA;
        const int width = exprVectorWidth(nodep->lhsp());
        if (width <= 0) {
            // Nothing here needs the length: the vacated end takes zero, and
            // Verilog's shift already does that
            if (arith) {
                unsup(flp, "'" + std::string{leftward ? "sla" : "sra"}
                               + "' on a vector whose length is not known here");
                return nullptr;
            }
            return leftward ? static_cast<AstNodeExpr*>(new AstShiftL{flp, lhsp, rhsp})
                            : static_cast<AstNodeExpr*>(new AstShiftR{flp, lhsp, rhsp});
        }
        AstVar* const valp = newTempVar("__Vshv", newLogicDType(flp, width), flp);
        AstVar* const cntp = newTempVar("__Vshn", newIntDType(flp), flp);
        const auto val = [&]() { return new AstVarRef{flp, valp, VAccess::READ}; };
        const auto cnt = [&]() { return new AstVarRef{flp, cntp, VAccess::READ}; };
        const auto widthConst = [&]() {
            return new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(width)};
        };
        // All ones, as wide as the operand, for building the mask that says
        // which places the shift vacated
        const auto allOnes = [&]() {
            return new AstReplicate{flp, new AstConst{flp, AstConst::BitTrue{}}, widthConst()};
        };
        // One shifted end of the operand, repeated across the whole width
        const auto edge = [&](bool leftmost) -> AstNodeExpr* {
            AstNodeExpr* const bitp
                = new AstSel{flp, val(), leftmost ? width - 1 : 0, 1};
            return new AstReplicate{flp, bitp, widthConst()};
        };
        // "sla" fills from the right end, "sra" from the left; the mask picks
        // out exactly the places the shift left empty, and is all ones when
        // the count reaches the whole width
        const auto filled = [&](bool toLeft, AstNodeExpr* placesp) -> AstNodeExpr* {
            AstNodeExpr* const shiftedp
                = toLeft ? static_cast<AstNodeExpr*>(new AstShiftL{flp, val(), placesp})
                         : static_cast<AstNodeExpr*>(new AstShiftR{flp, val(), placesp});
            if (!arith) return shiftedp;
            AstNodeExpr* const maskp
                = toLeft ? static_cast<AstNodeExpr*>(new AstNot{
                              flp, new AstShiftL{flp, allOnes(), placesp->cloneTree(false)}})
                         : static_cast<AstNodeExpr*>(new AstShiftL{
                               flp, allOnes(),
                               new AstSub{flp, widthConst(), placesp->cloneTree(false)}});
            return new AstOr{flp, shiftedp, new AstAnd{flp, edge(!toLeft), maskp}};
        };
        AstNode* stmtsp = new AstAssign{flp, new AstVarRef{flp, valp, VAccess::WRITE}, lhsp};
        stmtsp = AstNode::addNext(
            stmtsp, static_cast<AstNode*>(
                        new AstAssign{flp, new AstVarRef{flp, cntp, VAccess::WRITE}, rhsp}));
        AstNodeExpr* const forwardp = filled(leftward, cnt());
        AstNodeExpr* const backwardp = filled(!leftward, new AstNegate{flp, cnt()});
        return new AstExprStmt{
            flp, stmtsp,
            new AstCond{flp,
                        new AstGteS{flp, cnt(), new AstConst{flp, AstConst::Signed32{}, 0}},
                        forwardp, backwardp}};
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
        if (!isLogicish(ltypep) && !isLogicish(rtypep)) return false;
        return inArithmeticPackage(symp);
    }
    // Whether a subprogram was declared in one of the packages whose
    // arithmetic is emitted as Verilator's own. An operator a design writes
    // for itself is compiled instead, however its operands are shaped: it is
    // free to mean something else, and std_logic_unsigned's own unary "-",
    // which returns a value one bit longer than its operand, shows that two
    // operators of the same name over the same type need not agree.
    static bool inArithmeticPackage(const VhdlSymbol* symp) {
        static const std::set<std::string> pkgs{
            "numeric_std",        "numeric_bit",        "numeric_std_unsigned",
            "numeric_bit_unsigned", "std_logic_1164",   "std_logic_arith",
            "std_logic_unsigned", "std_logic_signed",   "std_logic_misc",
        };
        for (const VhdlScope* scopep = symp->declScopep(); scopep;
             scopep = scopep->parentp()) {
            if (pkgs.count(scopep->name())) return true;
        }
        return false;
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
        // An alias of an object is indexed like the object; an alias of a
        // subprogram is a call
        const bool aliasOfSubprogram
            = objSymp && objSymp->kind() == VhdlSymKind::ALIAS
              && (objSymp->resolved()->kind() == VhdlSymKind::FUNCTION
                  || objSymp->resolved()->kind() == VhdlSymKind::PROCEDURE);
        const bool indexing
            = (objSymp && !aliasOfSubprogram
               && (objSymp->isObject() || objSymp->kind() == VhdlSymKind::ALIAS))
              || (!objSymp && prefixTypep
                  && (prefixTypep->base()->kind() == VhdlTypeKind::ARRAY
                      || prefixTypep->base()->kind() == VhdlTypeKind::ACCESS));
        if (indexing) {
            const DynAccess* dynp = nullptr;
            if (AstNodeVhdlExpr* const dynObjp = dynAccessObj(nodep->prefixp(), dynp)) {
                AstNodeExpr* const objp = lowerExpr(dynObjp);
                return objp ? lowerDynIndex(nodep, dynp, objp, flp) : nullptr;
            }
            // An alias of a slice numbers its elements its own way, and
            // aliasShift below puts an index back onto the numbering of what
            // it denotes -- so it is that object that is indexed, not the
            // slice, whose own indices start again from its length (6.6.2)
            AstNodeVhdlExpr* const prefixExprp = aliasBase(objSymp, nodep->prefixp());
            AstNodeExpr* fromp = lowerExpr(prefixExprp);
            if (!fromp) return nullptr;
            fromp = derefIfAccess(fromp, prefixExprp, flp);
            if (AstVhdlRange* const rp = VN_CAST(nodep->argsp(), VhdlRange)) {
                AstNodeExpr* leftp = nullptr;
                AstNodeExpr* rightp = nullptr;
                bool sliceAsc = true;
                bool sliceDirKnown = true;
                if (!rangeParts(rp, leftp, rightp, sliceAsc, sliceDirKnown)) {
                    VL_DO_DANGLING(fromp->deleteTree(), fromp);
                    if (!rp->leftp() || !rp->rightp()) unsup(flp, "slice by an attribute range");
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
            size_t dim = 0;
            for (AstNode* p = nodep->argsp(); p; p = p->nextp(), ++dim) {
                AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->actualp()) continue;
                AstNodeExpr* indexp = lowerExpr(asp->actualp());
                if (!indexp) {
                    VL_DO_DANGLING(outp->deleteTree(), outp);
                    return nullptr;
                }
                indexp = aliasShift(objSymp, indexp, flp);
                indexp = stringIndex(prefixTypep, indexp, flp);
                indexp = logicIndex(prefixTypep, dim, indexp, flp);
                // A value that carries its own bounds is stored from zero,
                // whatever range it was made with
                if (dim == 0 && isDynObj(nodep->prefixp())) {
                    outp = dynIndex(outp, indexp, flp);
                    continue;
                }
                // Such a value read from anything but a name would have to be
                // worked out once and kept, since every part of it -- the
                // elements, the left bound, the direction -- is a separate
                // read
                if (dim == 0 && VN_IS(nodep->prefixp(), VhdlCall)
                    && dynResultCall(nodep->prefixp())) {
                    VL_DO_DANGLING(indexp->deleteTree(), indexp);
                    VL_DO_DANGLING(outp->deleteTree(), outp);
                    unsup(flp, "index of a value whose length only running decides and that is "
                               "not a name");
                    return nullptr;
                }
                outp = new AstSelBit{flp, outp, indexp};
            }
            return outp;
        }
        // Otherwise a function call
        return lowerFuncCall(nodep, flp);
    }

    // Indexing what an access value points at, where the object carries its
    // own bounds: the index is put onto the elements as they are stored.
    // A slice is a value with bounds of its own, which only an assignment
    // between two such objects can carry here, so it is left to the
    // statement that assigns it.
    AstNodeExpr* lowerDynIndex(AstVhdlCall* nodep, const DynAccess* dynp, AstNodeExpr* objp,
                               FileLine* flp) {
        if (VN_IS(nodep->argsp(), VhdlRange)) {
            VL_DO_DANGLING(objp->deleteTree(), objp);
            unsup(flp, "slice of an unconstrained array read through an access value");
            return nullptr;
        }
        if (!repeatable(objp)) {
            VL_DO_DANGLING(objp->deleteTree(), objp);
            unsup(flp, "index of an access value that is not a name");
            return nullptr;
        }
        AstVhdlAssoc* const asp = VN_CAST(nodep->argsp(), VhdlAssoc);
        if (asp && asp->nextp()) {
            VL_DO_DANGLING(objp->deleteTree(), objp);
            unsup(flp, "index of an unconstrained array of more than one dimension");
            return nullptr;
        }
        AstNodeExpr* const indexp = asp && asp->actualp() ? lowerExpr(asp->actualp()) : nullptr;
        if (!indexp) {
            VL_DO_DANGLING(objp->deleteTree(), objp);
            return nullptr;
        }
        return new AstSelBit{flp, memberOf(objp->cloneTree(false), dynp->datap->name(), flp),
                             dynIndex(dynp, objp, indexp, flp)};
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

    // An integer expression as a sum of terms with integer coefficients and
    // a constant. Two slice bounds move together when their term parts are
    // the same, however differently they are written: "8*(i+1)-1" and "8*i"
    // differ by seven, which no comparison of the two expressions shows.
    struct LinearForm final {
        // Opaque subexpression and how many of it. Kept as trees and matched
        // by shape: two writings of the same term are different nodes.
        // Each term: what it is, how many of it, and whether it names a
        // generic. A term left over decides the width, and only a generic
        // folds when the instance is elaborated -- anything else never
        // folds, and a width that never folds is not a part select at all.
        struct Term final {
            AstNodeExpr* exprp = nullptr;
            int64_t coeff = 0;
            bool generic = false;
        };
        std::vector<Term> terms;
        int64_t konst = 0;
        bool ok = true;
        void clear() {
            for (auto& term : terms) {
                if (term.exprp) VL_DO_DANGLING(term.exprp->deleteTree(), term.exprp);
            }
            terms.clear();
        }
    };

    void linearAdd(LinearForm& form, AstNodeVhdlExpr* nodep, int64_t scale, int depth) {
        if (!form.ok || !nodep || depth > 16) {
            form.ok = false;
            return;
        }
        const VhdlValue folded = V3VhdlEval::eval(nodep);
        if (folded.isInt()) {
            form.konst += scale * folded.toInt();
            return;
        }
        if (AstVhdlBinary* const binp = VN_CAST(nodep, VhdlBinary)) {
            if (binp->op() == VhdlBinaryOp::ADD || binp->op() == VhdlBinaryOp::SUB) {
                const int64_t rscale = binp->op() == VhdlBinaryOp::ADD ? scale : -scale;
                linearAdd(form, binp->lhsp(), scale, depth + 1);
                linearAdd(form, binp->rhsp(), rscale, depth + 1);
                return;
            }
            if (binp->op() == VhdlBinaryOp::MUL) {
                const VhdlValue lhs = V3VhdlEval::eval(binp->lhsp());
                const VhdlValue rhs = V3VhdlEval::eval(binp->rhsp());
                if (rhs.isInt()) {
                    linearAdd(form, binp->lhsp(), scale * rhs.toInt(), depth + 1);
                    return;
                }
                if (lhs.isInt()) {
                    linearAdd(form, binp->rhsp(), scale * lhs.toInt(), depth + 1);
                    return;
                }
                // Neither side is a number -- "(i+1)*WORD_BITS", where the
                // width is a generic -- so the product is spread over the
                // sum, which is what makes the two ends of the slice
                // cancel: i*W + W, against i*W
                AstVhdlBinary* const sump = VN_CAST(binp->lhsp(), VhdlBinary);
                AstNodeVhdlExpr* otherp = binp->rhsp();
                if (!sump || (sump->op() != VhdlBinaryOp::ADD
                              && sump->op() != VhdlBinaryOp::SUB)) {
                    AstVhdlBinary* const rsump = VN_CAST(binp->rhsp(), VhdlBinary);
                    if (rsump
                        && (rsump->op() == VhdlBinaryOp::ADD
                            || rsump->op() == VhdlBinaryOp::SUB)) {
                        spreadProduct(form, rsump, binp->lhsp(), scale, depth);
                        return;
                    }
                } else {
                    spreadProduct(form, sump, otherp, scale, depth);
                    return;
                }
            }
        }
        if (AstVhdlUnary* const unp = VN_CAST(nodep, VhdlUnary)) {
            if (unp->op() == VhdlUnaryOp::NEGATE) {
                linearAdd(form, unp->lhsp(), -scale, depth + 1);
                return;
            }
            if (unp->op() == VhdlUnaryOp::IDENTITY) {
                linearAdd(form, unp->lhsp(), scale, depth + 1);
                return;
            }
        }
        // Anything else is a term of its own, matched by what it lowers to
        const VhdlSymbol* const termSymp = rootSymOf(nodep);
        const bool isGeneric = termSymp && termSymp->kind() == VhdlSymKind::GENERIC;
        AstNodeExpr* const exprp = lowerExpr(nodep);
        if (!exprp) {
            form.ok = false;
            return;
        }
        for (auto& term : form.terms) {
            if (term.exprp && term.exprp->sameTree(exprp)) {
                term.coeff += scale;
                VL_DO_DANGLING(exprp->deleteTree(), exprp);
                return;
            }
        }
        form.terms.push_back(LinearForm::Term{exprp, scale, isGeneric});
    }

    // "(a +/- b) * c" spread as "a*c +/- b*c", so that a width written with
    // a generic still cancels between the two ends of a slice
    void spreadProduct(LinearForm& form, AstVhdlBinary* sump, AstNodeVhdlExpr* otherp,
                       int64_t scale, int depth) {
        FileLine* const flp = sump->fileline();
        AstVhdlBinary* const leftp
            = new AstVhdlBinary{flp, VhdlBinaryOp::MUL,
                                sump->lhsp()->cloneTree(false),
                                otherp->cloneTree(false)};
        AstVhdlBinary* const rightp
            = new AstVhdlBinary{flp, VhdlBinaryOp::MUL,
                                sump->rhsp()->cloneTree(false),
                                otherp->cloneTree(false)};
        linearAdd(form, leftp, scale, depth + 1);
        linearAdd(form, rightp, sump->op() == VhdlBinaryOp::ADD ? scale : -scale, depth + 1);
        VL_DO_DANGLING(leftp->deleteTree(), leftp);
        VL_DO_DANGLING(rightp->deleteTree(), rightp);
    }

    // The indexed part select a moving slice becomes, or null if its bounds
    // do not move together
    AstNodeExpr* indexedSlice(AstVhdlRange* rp, AstNodeExpr* fromp, AstNodeExpr* leftp,
                              AstNodeExpr* rightp, FileLine* flp) {
        LinearForm span;
        linearAdd(span, rp->leftp(), 1, 0);
        linearAdd(span, rp->rightp(), -1, 0);
        if (!span.ok) {
            span.clear();
            return nullptr;
        }
        const bool downto = rp->dir() == VhdlDir::DOWNTO;
        const int64_t sign = downto ? 1 : -1;
        // What is left after the two ends cancel is the width. Usually a
        // number; where the design wrote the width as a generic it is an
        // expression, which folds once the instance is elaborated -- and
        // that is soon enough for a part select.
        for (const auto& term : span.terms) {
            if (term.coeff != 0 && !term.generic) {
                span.clear();
                return nullptr;
            }
        }
        AstNodeExpr* widthp = nullptr;
        for (auto& term : span.terms) {
            if (term.coeff == 0 || !term.exprp) continue;
            AstNodeExpr* termp = term.exprp;
            term.exprp = nullptr;
            const int64_t coeff = sign * term.coeff;
            if (coeff != 1) {
                termp = new AstMul{flp, termp,
                                   new AstConst{flp, AstConst::Signed32{},
                                                static_cast<int32_t>(coeff)}};
            }
            widthp = widthp ? new AstAdd{flp, widthp, termp} : termp;
        }
        span.clear();
        const int64_t konst = sign * span.konst + 1;
        if (!widthp) {
            if (konst < 1 || konst > (1 << 20)) return nullptr;
            widthp = new AstConst{flp, AstConst::Unsized32{}, static_cast<uint32_t>(konst)};
        } else if (konst != 0) {
            widthp = new AstAdd{flp, widthp,
                                new AstConst{flp, AstConst::Signed32{},
                                             static_cast<int32_t>(konst)}};
        }
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
        // A value that carries its own bounds keeps them and its elements
        // whatever array type it is called: the two types have the same
        // element and the same index range, which is all a conversion
        // between closely related array types may change (9.3.6)
        const bool dyn = isDynValue(argp);
        AstNodeExpr* const valuep = lowerExpr(argp);
        if (!valuep || !top) return valuep;
        if (dyn && top->base()->kind() == VhdlTypeKind::ARRAY) return valuep;
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
    // Why a subprogram could not be emitted, where the reason is one the
    // reader can act on. A subprogram whose parameter or result has no length
    // of its own is emitted once per call site with the lengths that call
    // supplies; where a call cannot say them, it is that which is missing,
    // not the subprogram.
    std::string whyNotEmitted(VhdlSymbol* symp) {
        if (!symp) return "";
        for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
            const VhdlType* const typep = symp->paramTypes()[i];
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && !typep->lengthEmittable() && !isStringType(typep)
                && !paramCarried(symp, i)) {
                return " whose parameter '" + symp->paramName(i)
                       + "' has no length at this call";
            }
        }
        const VhdlType* const retp = symp->returnTypep();
        if (retp && retp->base()->kind() == VhdlTypeKind::ARRAY
            && !retp->lengthEmittable() && !isStringType(retp)
            && (symp->isForeign() || !carriedDynamically(emittedReturnType(symp)))) {
            return " whose result has no length at this call";
        }
        return "";
    }

    AstNodeFTask* emitOnDemand(VhdlSymbol* symp) {
        if (!symp) return nullptr;
        AstVhdlSubprogDecl* declp = symp->bodyp();
        if (!declp) declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
        if (!declp) return nullptr;
        if (m_emitting.count(symp)) return nullptr;  // A recursive call, mid-emission
        m_emitting.insert(symp);
        {
            VL_RESTORER(m_ftaskp);
            VL_RESTORER(m_protClassp);
            VL_RESTORER_COPY(m_prefix);
            // A subprogram reached for the first time from inside a
            // statement belongs to the module, not to the statement. Left
            // where the call is, one reached from a branch of an if-generate
            // is emitted into that branch and deleted with it when the branch
            // is not the one taken -- and the call in the branch that *is*
            // taken then names a subprogram that is no longer there.
            // A subprogram declared outside a protected type is not one of
            // its methods, however it was first reached, so it and whatever
            // it declares belong to the module too (5.6.2)
            const bool method = m_protClassp && inProtectedBody(symp);
            AstNode* const outerStmtsp = m_stmtsp;
            if (!method) {
                m_protClassp = nullptr;
                m_stmtsp = nullptr;
            }
            m_ftaskp = nullptr;
            m_prefix.clear();
            lowerSubprogram(declp);
            if (!method) {
                if (m_stmtsp && m_modp) m_modp->addStmtsp(m_stmtsp);
                m_stmtsp = outerStmtsp;
            }
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
            const VhdlType* protTypep = nullptr;
            if (AstNodeExpr* const instp = protectedInstance(selp->prefixp(), &protTypep)) {
                AstMethodCall* const callp
                    = new AstMethodCall{flp, instp, methodName(symp, protTypep), nullptr};
                addCallArgs(callp, nodep);
                return callp;
            }
        }
        // A function with a signal parameter needs a copy of its body per
        // call site, so that the formal's 'EVENT and the rest can be answered
        // from the signal the call passed
        if (hasSignalParam(symp)) {
            if (AstNodeExpr* const outp = lowerSignalFuncCall(symp, nodep, flp)) return outp;
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
        unsup(flp, "call to '" + symp->name() + "'" + whyNotEmitted(symp));
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
        // "to_string(v)" is the text of the value: for a scalar the same as
        // T'IMAGE, except that a CHARACTER or a STRING is its own characters
        // with no quotation marks around them (16.3)
        // "justify(v, side, field)" pads a string out to a width, on the
        // left where it is justified RIGHT and on the right where LEFT
        // (16.4). SIDE is declared as (RIGHT, LEFT), so RIGHT is position 0.
        if (name == "justify" && symp->predefined() && !args.empty() && args.size() <= 3) {
            AstNodeExpr* const valuep = lowerExpr(args[0]);
            if (!valuep) return nullptr;
            int64_t field = 0;
            if (args.size() >= 3) {
                const VhdlValue fv = V3VhdlEval::eval(args[2]);
                if (!fv.isInt()) {
                    unsup(flp, "justify to a width that is not static");
                    VL_DO_DANGLING(valuep->deleteTree(), valuep);
                    return nullptr;
                }
                field = fv.toInt();
            }
            bool right = true;
            if (args.size() >= 2) {
                const VhdlValue sv = V3VhdlEval::eval(args[1]);
                if (!sv.isInt()) {
                    unsup(flp, "justify to a side that is not static");
                    VL_DO_DANGLING(valuep->deleteTree(), valuep);
                    return nullptr;
                }
                right = sv.toInt() == 0;
            }
            if (field <= 0) return valuep;
            return new AstSFormatF{flp,
                                   "%" + std::string{right ? "" : "-"} + std::to_string(field)
                                       + "s",
                                   false, valuep};
        }
        // "to_string(t, unit)" writes a time in that unit (16.3). The unit
        // is a time of its own, so the value is scaled by it and the unit's
        // own name follows.
        if (name == "to_string" && args.size() == 2 && symp->predefined()
            && args[0]->vhdlTypep()
            && args[0]->vhdlTypep()->base()->kind() == VhdlTypeKind::PHYSICAL) {
            AstNodeExpr* const valuep = lowerExpr(args[0]);
            const VhdlValue unitv = V3VhdlEval::eval(args[1]);
            if (!valuep) return nullptr;
            if (unitv.isInt() && unitv.toInt() > 0) {
                const std::string suffix = unitNameOf(args[0]->vhdlTypep(), unitv.toInt());
                AstNodeExpr* const scaledp
                    = new AstDivS{flp, valuep,
                                  new AstConst{flp, AstConst::Unsized64{},
                                               static_cast<uint64_t>(unitv.toInt())}};
                return new AstSFormatF{flp, "%0d " + suffix, false, scaledp};
            }
            // The unit is only known while running -- OSVVM reads it from a
            // setting -- so both the scaling and the unit's name are worked
            // out then, the name by asking which unit the value is
            AstNodeExpr* const unitp = lowerExpr(args[1]);
            if (!unitp) {
                VL_DO_DANGLING(valuep->deleteTree(), valuep);
                return nullptr;
            }
            AstNodeExpr* namep = nullptr;
            for (const auto& unit : args[0]->vhdlTypep()->base()->units()) {
                AstNodeExpr* const litp
                    = new AstConst{flp, AstConst::String{}, " " + unit.first};
                if (!namep) {
                    namep = litp;
                    continue;
                }
                namep = new AstCond{
                    flp,
                    new AstEq{flp, unitp->cloneTree(false),
                              new AstConst{flp, AstConst::Unsized64{},
                                           static_cast<uint64_t>(unit.second)}},
                    litp, namep};
            }
            AstNodeExpr* const scaledp = new AstDivS{flp, valuep, unitp};
            AstSFormatF* const outp = new AstSFormatF{flp, "%0d%s", false, scaledp};
            if (namep) outp->addExprsp(namep);
            return outp;
        }
        if (name == "to_string" && args.size() == 1 && symp->predefined()) {
            const VhdlType* const vtypep = args[0]->vhdlTypep();
            AstNodeExpr* const valuep = lowerExpr(args[0]);
            if (!valuep) return nullptr;
            if (vtypep && isStringType(vtypep)) return valuep;
            if (vtypep && vtypep->base() == V3VhdlStd::characterType()->base()) {
                return new AstSFormatF{flp, "%c", false, valuep};
            }
            if (AstNodeExpr* const namep = enumImage(valuep, vtypep, flp)) return namep;
            return new AstSFormatF{flp, formatFor(vtypep, false), false, valuep};
        }
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
            // ENDFILE is true when nothing more can be read (5.5.2), which is
            // not what C's feof reports: that only becomes true once a read
            // has already run off the end, so it would still be false with
            // the position sitting at the end of the last line. The next
            // character is taken and put straight back instead.
            AstVar* const fdp = newTempVar("__Veoffd", newIntDType(flp), flp);
            AstVar* const chp = newTempVar("__Veofc", newIntDType(flp), flp);
            AstVar* const backp = newTempVar("__Veofu", newIntDType(flp), flp);
            AstNode* stmtsp
                = new AstAssign{flp, new AstVarRef{flp, fdp, VAccess::WRITE}, firstp};
            stmtsp = AstNode::addNext(
                stmtsp,
                static_cast<AstNode*>(new AstAssign{
                    flp, new AstVarRef{flp, chp, VAccess::WRITE},
                    new AstFGetC{flp, new AstVarRef{flp, fdp, VAccess::READ}}}));
            stmtsp = AstNode::addNext(
                stmtsp,
                static_cast<AstNode*>(new AstIf{
                    flp,
                    new AstGteS{flp, new AstVarRef{flp, chp, VAccess::READ},
                                new AstConst{flp, AstConst::Signed32{}, 0}},
                    new AstAssign{flp, new AstVarRef{flp, backp, VAccess::WRITE},
                                  new AstFUngetC{flp, new AstVarRef{flp, fdp, VAccess::READ},
                                                 new AstVarRef{flp, chp, VAccess::READ}}},
                    nullptr}));
            return new AstExprStmt{flp, stmtsp,
                                   new AstLtS{flp, new AstVarRef{flp, chp, VAccess::READ},
                                              new AstConst{flp, AstConst::Signed32{}, 0}}};
        }
        if (name == "to_integer") {
            // A value that carries its own bounds is read element by element
            if (isDynValue(args[0])) {
                AstNodeFTask* const intp
                    = dynIntFunc(args[0]->vhdlTypep(),
                                 isSignedType(args[0]->vhdlTypep()), flp);
                if (intp) {
                    AstFuncRef* const refp
                        = new AstFuncRef{flp, intp->name(), new AstArg{flp, "", firstp}};
                    refp->taskp(VN_AS(intp, Func));
                    return refp;
                }
                unsup(flp, "TO_INTEGER of a value whose length only running decides");
                return nullptr;
            }
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
            } else if (resultp && resultp->lengthEmittable()) {
                // A width that names a generic or a port's bounds is not a
                // number here. The result's own subtype says it, and what the
                // value is given to has that subtype, so the width comes from
                // the assignment exactly as it does for RESIZE. Narrowing the
                // integer to it is what the conversion is for.
                FileLine* const cflp = new FileLine{flp};
                cflp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
                cflp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
                flp = cflp;
                sized = firstp;
            } else if (resultp && carriedDynamically(resultp)) {
                // What the value is given to carries its own bounds, so the
                // width the call names is the length it is made with
                AstNodeExpr* const widthp = lowerExpr(args[1]);
                AstNodeFTask* const bitsp
                    = widthp ? dynBitsFunc(resultp, name == "to_signed", flp) : nullptr;
                if (bitsp) {
                    AstFuncRef* const refp = new AstFuncRef{
                        flp, bitsp->name(),
                        AstNode::addNext(new AstArg{flp, "", firstp},
                                         new AstArg{flp, "", widthp})};
                    refp->taskp(VN_AS(bitsp, Func));
                    return refp;
                }
                if (widthp) VL_DO_DANGLING(widthp->deleteTree(), widthp);
                unsup(flp, "conversion to a width that is not locally static");
                return nullptr;
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
        if (name == "rotate_left" || name == "rotate_right") {
            // 16.5 defines these as the rotate of 9.2.4 applied to the
            // vector, so they are the same two shifts
            if (args.size() < 2) return nullptr;
            const int width = exprVectorWidth(args[0]);
            AstNodeExpr* const placesp = lowerExpr(args[1]);
            if (width <= 0 || !placesp) {
                if (placesp) VL_DO_DANGLING(placesp->deleteTree(), placesp);
                unsup(flp, "'" + name + "' of a vector whose length is not known here");
                return nullptr;
            }
            return rotateOf(firstp, placesp, width, name == "rotate_left", flp);
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
        // "to_hstring(v)" and its relatives write the value in that base
        // (16.3). Compiled from the IEEE body they would be a loop over a
        // padded copy, whose padding is a null array when the length is a
        // multiple of the base -- and a null array has no representation in
        // a statically sized model. Written as a conversion they are neither
        // a loop nor a null array.
        if ((name == "to_hstring" || name == "to_ostring" || name == "to_bstring"
             || name == "to_binarystring" || name == "to_hex_string"
             || name == "to_octal_string" || name == "to_binary_string")
            && args.size() == 1
            && (isLogicish(args[0]->vhdlTypep()) || isBitish(args[0]->vhdlTypep()))) {
            const std::string fmt
                = (name == "to_hstring" || name == "to_hex_string")
                      ? "%h"
                      : ((name == "to_ostring" || name == "to_octal_string") ? "%o" : "%b");
            AstNodeExpr* outp = new AstSFormatF{flp, fmt, false, firstp};
            // VHDL writes hexadecimal in upper case
            if (fmt == "%h") outp = new AstToUpperN{flp, outp};
            return outp;
        }
        // The conversions between the logic families keep the same bits.
        // Recognised by what they operate on rather than by name alone: a
        // design may declare a function of the same name over a type of its
        // own, and that one has to be called.
        if ((name == "to_stdlogicvector" || name == "to_stdulogicvector"
             || name == "to_bitvector" || name == "to_bit" || name == "to_stdulogic"
             || name == "to_stdlogic" || name == "to_x01" || name == "to_01"
             || name == "to_ux01")
            && (isLogicish(args[0]->vhdlTypep()) || isBitish(args[0]->vhdlTypep()))) {
            return firstp;
        }
        VL_DO_DANGLING(firstp->deleteTree(), firstp);
        return nullptr;
    }

    // An aggregate builds a value out of its elements (9.3.3). Every choice
    // has already been checked to be static, so the whole value is a
    // concatenation over the target's index range.
    // One element of a STRING aggregate, as text. A STRING is characters
    // here rather than bits, so an aggregate of one is built with the string
    // operations: a packed value would lose any NUL in it on the way (16.3).
    AstNodeExpr* stringElem(AstNodeVhdlExpr* valuep, AstNodeExpr* loweredp, FileLine* flp) {
        const VhdlType* const typep = valuep ? valuep->vhdlTypep() : nullptr;
        if (typep && isStringType(typep)) return loweredp;
        return new AstSFormatF{flp, "%c", false, loweredp};
    }

    // The range a choice denotes, where it is a subtype mark rather than a
    // range written out: "(engrange => x)" covers every index of that
    // subtype (9.3.3.1, 5.3.2.1). False where the choice is not one.
    static bool choiceNamesSubtype(const AstVhdlChoice* chp, int64_t& lo, int64_t& hi) {
        const AstNodeVhdlExpr* const valp = chp->valuep();
        if (!valp) return false;
        const VhdlSymbol* const symp = valp->vhdlSymp();
        if (!symp || symp->kind() != VhdlSymKind::TYPE) return false;
        const VhdlType* const typep = symp->typep();
        if (!typep || !typep->bounded() || typep->base()->kind() == VhdlTypeKind::ARRAY) {
            return false;
        }
        lo = typep->low();
        hi = typep->high();
        return true;
    }

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
        const bool asString = isStringType(typep);
        if (typep->base()->elementp() && elementPacks(typep->base()->elementp())
            && typep->dimensions() == 1 && allPositional(nodep)) {
            AstNodeExpr* outp = nullptr;
            for (AstVhdlElemAssoc* p = nodep->elemsp(); p;
                 p = VN_AS(p->nextp(), VhdlElemAssoc)) {
                AstNodeExpr* elemp = lowerExpr(p->valuep());
                if (!elemp) {
                    if (outp) VL_DO_DANGLING(outp->deleteTree(), outp);
                    return nullptr;
                }
                if (asString) elemp = stringElem(p->valuep(), elemp, flp);
                outp = outp ? (asString ? static_cast<AstNodeExpr*>(
                                   new AstConcatN{flp, outp, elemp})
                                        : static_cast<AstNodeExpr*>(
                                            new AstConcat{flp, outp, elemp}))
                            : elemp;
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
            // A bound that names a generic is a length but not a number
            // here, so how many times the value repeats is the range's own
            // length, worked out where the instance is elaborated
            if (!indexp->bounded()) {
                if (AstNodeExpr* const outp = othersOfDynamicLength(nodep, typep, flp)) {
                    return outp;
                }
            }
            left = indexp->left();
            step = indexp->ascending() ? 1 : -1;
            count = indexp->length();
        } else if (AstNodeExpr* const outp = othersOfDynamicLength(nodep, typep, flp)) {
            return outp;
        } else if (!ownConstraint(nodep, typep, left, step, count)) {
            unsup(flp, "aggregate of this type");
            return nullptr;
        }
        if (count <= 0) {
            // A null array has no elements at all (5.3.2.1), so the aggregate
            // supplies none and nothing may read them. Any value of the
            // target's own shape will do, which is what a default assignment
            // pattern gives.
            AstNodeVhdlExpr* const nullValp
                = nodep->elemsp() ? nodep->elemsp()->valuep() : nullptr;
            AstNodeExpr* const valuep = nullValp ? lowerExpr(nullValp) : nullptr;
            if (!valuep) return new AstConst{flp, AstConst::BitFalse{}};
            AstPatMember* const memberp = new AstPatMember{flp, valuep, nullptr, nullptr};
            memberp->isDefault(true);
            return new AstPattern{flp, memberp};
        }
        if (count > 65536) {
            // One value repeated is one node, however long the range is;
            // it is writing out a hundred thousand of them that is not on
            if (AstNodeExpr* const outp = othersOfDynamicLength(nodep, typep, flp, count)) {
                return outp;
            }
            unsup(flp, "aggregate of this size");
            return nullptr;
        }
        // Which element association gives each index. A value of the
        // aggregate's own type covers a whole run of them rather than one,
        // which VHDL-2008 allows for a choice as well as for a positional
        // association (9.3.3.3); the run is recorded at its first index and
        // the rest are marked as spoken for.
        std::vector<AstNodeVhdlExpr*> byIndex(static_cast<size_t>(count), nullptr);
        std::vector<int64_t> spanAt(static_cast<size_t>(count), 1);
        AstNodeVhdlExpr* othersp = nullptr;
        int64_t positional = 0;
        for (AstNode* p = nodep->elemsp(); p; p = p->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(p, VhdlElemAssoc);
            if (!eap->choicesp()) {
                // A value of the aggregate's own type covers as many indices
                // as it is long, whether it was written positionally or with
                // a choice (9.3.3.3)
                const int64_t slot = positional;
                int64_t span = 1;
                const VhdlType* const vtypep
                    = eap->valuep() ? eap->valuep()->vhdlTypep() : nullptr;
                if (vtypep && vtypep->base() == typep->base()) {
                    if (vtypep->isConstrainedArray() && !vtypep->indexTypes().empty()
                        && vtypep->indexTypes()[0] && vtypep->indexTypes()[0]->bounded()) {
                        span = vtypep->indexTypes()[0]->length();
                    } else {
                        // A string literal carries its length in itself, not
                        // in its type
                        const VhdlValue val = V3VhdlEval::eval(eap->valuep());
                        if (val.isStr()) span = static_cast<int64_t>(val.toStr().size());
                    }
                }
                positional += span;
                if (slot >= 0 && slot < count) {
                    byIndex[static_cast<size_t>(slot)] = eap->valuep();
                    if (span > 1 && slot + span <= count) {
                        spanAt[static_cast<size_t>(slot)] = span;
                        for (int64_t k = 1; k < span; ++k) {
                            spanAt[static_cast<size_t>(slot + k)] = 0;
                        }
                    }
                }
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
                if (choiceNamesSubtype(chp, lo, hi)) {
                    // A subtype mark stands for every index of that subtype
                } else if (AstVhdlRange* const rp = chp->rangep()) {
                    // "v'range => x" names every index of v, which its own
                    // subtype says (16.2.3) rather than an expression
                    if (!rp->leftp() && rp->attrp()) {
                        const VhdlType* const idxp = attrRangeSubtype(rp->attrp());
                        if (!idxp || !idxp->bounded()) {
                            unsup(chp->fileline(), "aggregate choice that is not static");
                            return nullptr;
                        }
                        lo = idxp->low();
                        hi = idxp->high();
                    } else {
                        const VhdlValue a = V3VhdlEval::eval(rp->leftp());
                        const VhdlValue b = V3VhdlEval::eval(rp->rightp());
                        if (!a.isInt() || !b.isInt()) {
                            unsup(chp->fileline(), "aggregate choice that is not static");
                            return nullptr;
                        }
                        lo = std::min(a.toInt(), b.toInt());
                        hi = std::max(a.toInt(), b.toInt());
                    }
                } else {
                    const VhdlValue v = V3VhdlEval::eval(chp->valuep());
                    if (!v.isInt()) {
                        unsup(chp->fileline(), "aggregate choice that is not static");
                        return nullptr;
                    }
                    lo = hi = v.toInt();
                }
                const bool wholeRun
                    = eap->valuep() && eap->valuep()->vhdlTypep()
                      && eap->valuep()->vhdlTypep()->base() == typep->base();
                if (wholeRun) {
                    // "11 downto 0 => t" puts the whole of t across those
                    // indices, so it is one value spanning them all
                    const int64_t first = ((step > 0 ? lo : hi) - left) * step;
                    const int64_t span = hi - lo + 1;
                    if (first >= 0 && first + span <= count) {
                        byIndex[static_cast<size_t>(first)] = eap->valuep();
                        spanAt[static_cast<size_t>(first)] = span;
                        for (int64_t k = 1; k < span; ++k) {
                            spanAt[static_cast<size_t>(first + k)] = 0;
                        }
                    }
                    continue;
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
            if (spanAt[static_cast<size_t>(i)] == 0) continue;  // Covered by a run
            AstNodeVhdlExpr* const valuep
                = byIndex[static_cast<size_t>(i)] ? byIndex[static_cast<size_t>(i)] : othersp;
            if (!valuep) {
                unsup(flp, "aggregate with an element left unset");
                return nullptr;
            }
            if (spanAt[static_cast<size_t>(i)] > 1 && !packed && !asString) {
                unsup(flp, "aggregate with a run of elements given at once");
                return nullptr;
            }
            AstNodeExpr* elemp = lowerExpr(valuep);
            if (!elemp) return nullptr;
            if (asString) {
                elemp = stringElem(valuep, elemp, flp);
                outp = outp ? new AstConcatN{flp, outp, elemp} : elemp;
            } else if (packed) {
                outp = outp ? new AstConcat{flp, outp, elemp} : elemp;
            } else {
                itemsp = AstNode::addNextNull(
                    itemsp, new AstPatMember{flp, elemp, nullptr, nullptr});
            }
        }
        if (packed || asString) return outp;
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
    AstNodeExpr* othersOfDynamicLength(AstVhdlAggregate* nodep, VhdlType* typep, FileLine* flp,
                                       int64_t knownCount = 0) {
        // Where the range has already been folded, the repetition count is
        // that number rather than an expression to work it out again
        AstVhdlElemAssoc* const firstp = nodep->elemsp();
        const auto lengthOf = [&]() -> AstNodeExpr* {
            if (AstNodeExpr* const countp = boundsLength(typep, flp)) return countp;
            if (knownCount > 0) {
                return new AstConst{flp, AstConst::Unsized32{},
                                    static_cast<uint32_t>(knownCount)};
            }
            // An aggregate standing as an operand has no bounds of its own,
            // so a single range choice is what says how long it is (9.3.3.3)
            // -- and that range may name a generic rather than a number
            AstVhdlChoice* const chp = firstp ? firstp->choicesp() : nullptr;
            AstVhdlRange* const rp = chp && !chp->nextp() ? chp->rangep() : nullptr;
            if (rp && rp->leftp() && rp->rightp() && !rp->dirUnknown()) {
                AstNodeExpr* const lop = lowerExpr(rp->leftp());
                AstNodeExpr* const hip = lop ? lowerExpr(rp->rightp()) : nullptr;
                if (!hip) {
                    if (lop) VL_DO_DANGLING(lop->deleteTree(), lop);
                    return nullptr;
                }
                AstNodeExpr* const spanp = rp->dir() == VhdlDir::TO
                                               ? new AstSub{flp, hip, lop}
                                               : new AstSub{flp, lop, hip};
                return new AstAdd{flp, spanp, new AstConst{flp, AstConst::Signed32{}, 1}};
            }
            return nullptr;
        };
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
        if (isStringType(typep)) {
            if (AstNodeExpr* const countp = lengthOf()) {
                AstNodeExpr* const valuep = lowerExpr(eap->valuep());
                if (!valuep) {
                    VL_DO_DANGLING(countp->deleteTree(), countp);
                    return nullptr;
                }
                return new AstReplicateN{flp, stringElem(eap->valuep(), valuep, flp), countp};
            }
            return nullptr;
        }
        if (packed) {
            if (AstNodeExpr* const countp = lengthOf()) {
                AstNodeExpr* const valuep = lowerExpr(eap->valuep());
                if (!valuep) {
                    VL_DO_DANGLING(countp->deleteTree(), countp);
                    return nullptr;
                }
                // How many times the value repeats is the length of a range
                // the design declared, not a number written into a
                // replication, so the warning about a suspect replication
                // count has nothing to say about it
                FileLine* const rflp = new FileLine{flp};
                rflp->warnOff(V3ErrorCode::WIDTHCONCAT, true);
                // A null array has no elements at all (5.3.2.1), so a range
                // that elaboration makes empty repeats the value no times --
                // which is a legal VHDL aggregate, not a suspect replication
                rflp->warnOff(V3ErrorCode::ZEROREPL, true);
                return new AstReplicate{rflp, valuep, countp};
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
        // In a multidimensional aggregate the choices are those of the first
        // dimension, and each element covers the rest (9.3.3.3), so the same
        // reckoning applies -- it is the first index that is being described
        const VhdlType* const indexp = typep->base()->indexTypes().empty()
                                           ? nullptr
                                           : typep->base()->indexTypes()[0];
        if (!indexp || !indexp->bounded()) return false;
        int64_t positional = 0;
        bool named = false;
        bool sawRange = false;
        bool rangeAscending = true;
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
                if (choiceNamesSubtype(chp, lo, hi)) {
                    // A subtype mark stands for every index of that subtype
                } else if (AstVhdlRange* const rp = chp->rangep()) {
                    const VhdlValue a = V3VhdlEval::eval(rp->leftp());
                    const VhdlValue b = V3VhdlEval::eval(rp->rightp());
                    if (!a.isInt() || !b.isInt()) return false;
                    lo = std::min(a.toInt(), b.toInt());
                    hi = std::max(a.toInt(), b.toInt());
                    // A choice written as a range says which way the
                    // aggregate's own index runs (9.3.3.3), whatever the
                    // index subtype does: "(15 downto 12 => s, 11 downto 0
                    // => v)" is sixteen bits with 15 at the left
                    if (!sawRange && !rp->dirUnknown()) {
                        sawRange = true;
                        rangeAscending = rp->dir() == VhdlDir::TO;
                    }
                } else {
                    const VhdlValue v = V3VhdlEval::eval(chp->valuep());
                    if (!v.isInt()) return false;
                    lo = hi = v.toInt();
                }
                lowest = std::min(lowest, lo);
                highest = std::max(highest, hi);
            }
        }
        step = (sawRange ? rangeAscending : indexp->ascending()) ? 1 : -1;
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
        if (VN_IS(nodep, VhdlBlock)) {
            // A block is not a driver of its own: each statement inside it is
            // one, and is counted as it is lowered (11.2)
            lowerConcurrent(nodep);
            return;
        }
        std::set<const VhdlSymbol*> driven;
        collectDriven(nodep, driven);
        std::vector<const VhdlSymbol*> resolved;
        for (const VhdlSymbol* const symp : driven) {
            if (m_drivers.count(symp)) resolved.push_back(symp);
        }
        VL_RESTORER(m_countedDrivers);
        m_countedDrivers = false;
        lowerConcurrent(nodep);
        // A generate written out once per iteration counted its own, since
        // how many there were is what made it write them out
        if (m_countedDrivers) return;
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
            VL_RESTORER_COPY(m_blockPrefix);
            VL_RESTORER(m_cfgScopep);
            m_cfgScopep = cfgScopeIn(blockp->label());
            if (!blockp->label().empty()) m_blockPrefix += blockp->label() + "__";
            if (blockp->guardp()) lowerGuard(blockp, flp);
            if (AstVhdlBlockHeader* const headerp = blockp->headerp()) {
                lowerBlockHeader(headerp);
            }
            for (AstNode* p = blockp->declsp(); p; p = p->nextp()) lowerDecl(p);
            for (AstNode* p = blockp->stmtsp(); p; p = p->nextp()) {
                // A block is transparent to resolution: each statement of it
                // is a driver of its own, as it would be outside the block
                lowerConcurrentDriver(VN_AS(p, NodeVhdlStmt));
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
            // A concurrent procedure call is a process that calls it and then
            // waits on the signal parameters it reads (11.4). Those are not
            // read by the call itself -- the copy made for it names them
            // directly -- so the sensitivity comes from the actuals.
            AstSenTree* const senp
                = callp->postponed() ? nullptr : signalCallSensitivity(callp, flp);
            AstNode* const bodyp = lowerStmt(callp);
            if (senp && bodyp) {
                // The call, then the wait, over and over: one process, not a
                // fresh one on every change. The procedure may itself wait,
                // and then the process is inside it until it returns.
                if (!v3Global.opt.timing().isTrue()) {
                    VL_DO_DANGLING(senp->deleteTree(), senp);
                    addStmt(concurrentProcess(bodyp, callp->postponed(), flp));
                    return;
                }
                AstNode* const loopp = AstNode::addNext(
                    bodyp, static_cast<AstNode*>(new AstEventControl{flp, senp, nullptr}));
                addStmt(new AstAlways{flp, VAlwaysKwd::ALWAYS, nullptr, loopp});
                return;
            }
            if (senp) VL_DO_DANGLING(senp->deleteTree(), senp);
            addStmt(concurrentProcess(bodyp, callp->postponed(), flp));
            return;
        }
        unsup(flp, std::string{"concurrent statement "} + nodep->typeName());
    }

    // What a concurrent procedure call waits on: every signal named in the
    // actual of a parameter of mode "in" or "inout", whatever the class of
    // the formal -- a constant parameter given a signal counts too (11.4).
    // Null where it has none, and the ordinary rule decides instead.
    AstSenTree* signalCallSensitivity(AstVhdlProcCall* nodep, FileLine* flp) {
        AstVhdlCall* const callp = VN_CAST(nodep->callp(), VhdlCall);
        VhdlSymbol* const namedp = callp ? callp->vhdlSymp() : nullptr;
        VhdlSymbol* const symp = namedp ? namedp->resolved() : nullptr;
        if (!symp || !callp) return nullptr;
        AstVhdlSubprogDecl* const declp = declOf(symp);
        if (!declp) return nullptr;
        std::vector<AstVhdlIfaceDecl*> formals;
        for (AstNode* p = firstParamOf(declp); p; p = p->nextp()) {
            formals.push_back(VN_AS(p, VhdlIfaceDecl));
        }
        std::vector<AstVhdlAssoc*> assocs;
        for (AstNode* p = callp->argsp(); p; p = p->nextp()) {
            if (AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc)) assocs.push_back(asp);
        }
        std::vector<AstVar*> varps;
        for (size_t i = 0; i < formals.size(); ++i) {
            if (formals[i]->kind() == VhdlIfaceKind::FILE) continue;
            if (formals[i]->mode() == VhdlMode::OUT) continue;
            AstNodeVhdlExpr* actualp = nullptr;
            for (size_t a = 0; a < assocs.size(); ++a) {
                const AstVhdlName* const namep
                    = assocs[a]->formalp() ? VN_CAST(assocs[a]->formalp(), VhdlName) : nullptr;
                if (namep) {
                    if (namep->name() == formals[i]->name()) actualp = assocs[a]->actualp();
                } else if (!assocs[a]->formalp() && a == i) {
                    actualp = assocs[a]->actualp();
                }
            }
            if (!actualp) continue;
            // Only a signal is waited on. A constant or a variable named
            // there is read once and never changes the process's mind.
            const VhdlSymbol* const rootp = rootSymOf(actualp);
            if (!rootp || rootp->resolved()->kind() != VhdlSymKind::SIGNAL) continue;
            AstNodeExpr* const onep = lowerExpr(actualp);
            if (!onep) continue;
            collectVars(onep, varps);
            VL_DO_DANGLING(onep->deleteTree(), onep);
        }
        AstSenItem* itemsp = nullptr;
        for (AstVar* const varp : varps) {
            itemsp = AstNode::addNext(
                itemsp,
                senItemFor(new AstVarRef{flp, varp, VAccess::READ}, isOneBitVar(varp), flp));
        }
        return itemsp ? new AstSenTree{flp, itemsp} : nullptr;
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
        // What the statement is assigned *to* is not something it reads,
        // even though the reference has not been marked one way or the other
        // yet -- that happens in V3LinkLValue, long after here
        std::set<const AstNode*> writtenp;
        bodyp->foreach([&writtenp](const AstNodeAssign* asgp) {
            for (const AstNode* np = asgp->lhsp(); np;) {
                if (VN_IS(np, VarRef)) {
                    writtenp.insert(np);
                    break;
                }
                if (const AstMemberSel* const memp = VN_CAST(np, MemberSel)) {
                    np = memp->fromp();
                } else if (const AstStructSel* const strp = VN_CAST(np, StructSel)) {
                    np = strp->fromp();
                } else if (const AstSelBit* const bitp = VN_CAST(np, SelBit)) {
                    np = bitp->fromp();
                } else if (const AstSelExtract* const extp = VN_CAST(np, SelExtract)) {
                    np = extp->fromp();
                } else if (const AstSel* const selp = VN_CAST(np, Sel)) {
                    np = selp->fromp();
                } else {
                    break;
                }
            }
        });
        bool reads = false;
        bodyp->foreach([&reads, &writtenp](const AstVarRef* refp) {
            if (!refp->access().isReadOrRW()) return;
            if (writtenp.count(refp)) return;
            // A generic or a generate parameter has its value fixed by
            // elaboration and never changes afterwards, so a statement that
            // reads nothing else still runs exactly once
            const AstVar* const varp = refp->varp();
            if (varp && (varp->isParam() || varp->varType() == VVarType::GENVAR)) return;
            reads = true;
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
    // Whether an expression names something that can be assigned to
    static bool isAssignable(const AstNodeExpr* nodep) {
        if (VN_IS(nodep, VarRef)) return true;
        if (const AstMemberSel* const memp = VN_CAST(nodep, MemberSel)) {
            return isAssignable(memp->fromp());
        }
        if (const AstStructSel* const strp = VN_CAST(nodep, StructSel)) {
            return isAssignable(strp->fromp());
        }
        if (const AstSelBit* const bitp = VN_CAST(nodep, SelBit)) {
            return isAssignable(bitp->fromp());
        }
        if (const AstSelExtract* const extp = VN_CAST(nodep, SelExtract)) {
            return isAssignable(extp->fromp());
        }
        return false;
    }

    // The name a conversion on a formal is applied to, as in "f(p) => a"
    static std::string formalNamed(AstNode* argsp) {
        AstVhdlAssoc* const asp = VN_CAST(argsp, VhdlAssoc);
        if (!asp || asp->nextp()) return "";
        const AstVhdlName* const namep = VN_CAST(asp->actualp(), VhdlName);
        return namep ? namep->name() : "";
    }

    // Whether an actual names a signal, possibly through a conversion.
    // 6.5.7.1 keeps that an association with the signal itself; anything
    // else is an expression, and a port associated with one has a driver of
    // its own and so a delta of its own.
    static bool isSignalActual(const AstNodeVhdlExpr* nodep) {
        if (!nodep) return false;
        if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            const VhdlSymbol* const symp = namep->vhdlSymp();
            return symp
                   && (symp->kind() == VhdlSymKind::SIGNAL
                       || symp->kind() == VhdlSymKind::PORT);
        }
        if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            return isSignalActual(selp->prefixp());
        }
        const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
        if (!callp || !callp->argsp() || callp->argsp()->nextp()) return false;
        const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
        const AstNodeVhdlExpr* const argp
            = asp ? asp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
        return isSignalActual(argp);
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
            AstNodeVhdlExpr* outConvp = nullptr;  // "f(port) => actual" (6.5.7.1)
            bool inertial = false;
            for (size_t i = 0; i < assocs.size(); ++i) {
                const AstVhdlName* const namep = VN_CAST(assocs[i]->formalp(), VhdlName);
                AstVhdlCall* const convp = VN_CAST(assocs[i]->formalp(), VhdlCall);
                const AstVhdlName* const convNamep
                    = convp ? VN_CAST(convp->prefixp(), VhdlName) : nullptr;
                const bool byName = namep && namep->name() == symp->name();
                const bool byConv = convNamep && convNamep->name() != symp->name()
                                    && formalNamed(convp->argsp()) == symp->name();
                if (byName || byConv || (!namep && !convp && i == position)) {
                    actualp = assocs[i]->actualp();
                    inertial = assocs[i]->inertial();
                    if (byConv) outConvp = assocs[i]->formalp();
                    break;
                }
            }
            // A conversion written on the formal converts what the port
            // drives on its way out to the actual (6.5.7.1): the port is a
            // signal of its own, and the actual follows the conversion of it
            if (outConvp) {
                AstNodeExpr* const targetp = lowerExpr(actualp);
                // The actual has to be something that can be assigned. Where
                // it is itself a conversion -- a conversion written on both
                // sides -- there is nothing to drive, and the association is
                // left to be bound the ordinary way.
                if (targetp && !isAssignable(targetp)) {
                    VL_DO_DANGLING(targetp->deleteTree(), targetp);
                    outConvp = nullptr;
                } else {
                    AstVar* const varp = newVar(symp, VVarType::VAR, flp);
                    addStmt(varp);
                    AstNodeExpr* const convValuep = lowerExpr(outConvp);
                    if (!targetp || !convValuep) {
                        if (targetp) VL_DO_DANGLING(targetp->deleteTree(), targetp);
                        if (convValuep) VL_DO_DANGLING(convValuep->deleteTree(), convValuep);
                        continue;
                    }
                    // The conversion is applied to the value as it leaves
                    // the port (6.5.7.1); the delta belongs to the port's own
                    // driver, not to the conversion of what it drives
                    addStmt(new AstAlways{new AstAssignW{flp, targetp, convValuep}});
                    continue;
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
            // Written "inertial", the actual is an expression rather than
            // the signal itself, so the port is a signal of its own with a
            // driver (6.5.7.1) -- which is the whole reason for saying it
            if (!isGeneric && !inertial) {
                if (AstVarRef* const refp = VN_CAST(valuep, VarRef)) {
                    m_vars.emplace(symp, refp->varp());
                    VL_DO_DANGLING(valuep->deleteTree(), valuep);
                    continue;
                }
            }
            // A port the association drives is written from two blocks by
            // construction -- once to start, once to react -- so the finding
            // is about how this is emitted, not about the design
            FileLine* const vflp = new FileLine{flp};
            vflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            AstVar* const varp = newVar(symp, VVarType::VAR, vflp);
            addStmt(varp);
            if (isGeneric) {
                varp->valuep(valuep);
                continue;
            }
            // A conversion applied to a signal is still an association with
            // that signal: the port follows it, converted, with nothing in
            // between (6.5.7.1). Only a real expression gives the port a
            // driver of its own.
            if (!isGeneric && !inertial && isSignalActual(actualp)) {
                AstNodeExpr* const convRefp = new AstVarRef{flp, varp, VAccess::READ};
                if (ifp->mode() == VhdlMode::OUT || ifp->mode() == VhdlMode::BUFFER) {
                    addStmt(new AstAlways{new AstAssignW{flp, valuep, convRefp}});
                } else {
                    addStmt(new AstAlways{new AstAssignW{flp, convRefp, valuep}});
                }
                continue;
            }
            // A port whose actual is an expression rather than a signal is
            // a signal of its own with a driver (6.5.7.1): it holds its
            // default until the association delivers, and every delivery
            // after that arrives a delta later, as any signal assignment
            // does. A continuous assignment would give it the value with no
            // delta at all and no default to start from.
            if (ifp->defaultp()) {
                varp->valuep(lowerExpr(ifp->defaultp()));
            } else if (AstNodeExpr* const defp = defaultValueFor(symp->typep(), flp, true)) {
                // With nothing written, the port starts at its type's own
                // default until the association delivers (6.4.2.3)
                varp->valuep(defp);
            }
            // The association is emitted twice -- once to start, once to
            // react -- as every concurrent assignment is, so it drives from
            // two blocks by construction
            FileLine* const pflp = new FileLine{flp};
            pflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            pflp->warnOff(V3ErrorCode::COMBDLY, true);
            AstNodeExpr* const portRefp = new AstVarRef{pflp, varp, VAccess::READ};
            if (ifp->mode() == VhdlMode::OUT || ifp->mode() == VhdlMode::BUFFER) {
                AstNodeExpr* readsp = portRefp->cloneTree(false);
                emitWaveformProcess(new AstAssignDly{pflp, valuep, portRefp}, readsp, pflp);
            } else {
                AstNodeExpr* readsp = valuep->cloneTree(false);
                emitWaveformProcess(new AstAssignDly{pflp, portRefp, valuep}, readsp, pflp);
            }
        }
    }

    // A process becomes an always block. Which sensitivity it gets is the
    // single most consequential choice in this file: a clocked process must
    // become edge-triggered, or every register would be re-evaluated whenever
    // any of its inputs changed and the speed the project exists for is lost.
    // Whether anything in these statements suspends the process
    static bool hasWait(AstNode* nodep) {
        bool found = false;
        for (AstNode* p = nodep; p && !found; p = p->nextp()) {
            p->foreach([&found](AstNode* subp) {
                if (VN_IS(subp, VhdlWait)) found = true;
            });
        }
        return found;
    }

    void lowerProcess(AstVhdlProcess* nodep) {
        FileLine* const flp = nodep->fileline();
        VL_RESTORER_COPY(m_prefix);
        VL_RESTORER(m_postponed);
        // A postponed process is not recognised as a clocked one: what makes
        // it different is *when* it runs, which an edge-triggered block
        // cannot say (10.3)
        m_postponed = nodep->postponed() && v3Global.opt.timing().isTrue();
        const std::string label
            = nodep->label().empty() ? "__Vprocess" + std::to_string(++m_processNum)
                                     : nodep->label();
        m_prefix = label + "__";
        for (AstNode* p = nodep->declsp(); p; p = p->nextp()) lowerDecl(p);

        bool rising = false;
        AstNodeVhdlExpr* clockp = nullptr;
        AstVhdlIf* const ifp = m_postponed ? nullptr : clockedIf(nodep, clockp, rising);
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
            // "process (all)" is sensitive to every signal it reads (11.3).
            // Which those are is only known once the body is lowered, and
            // saying it as a list rather than as "@*" is what lets the
            // process run once and then wait, as every process does.
            if (nodep->sensAll() && v3Global.opt.timing().isTrue()) {
                if (AstSenTree* const readsp = sensitivityOfBody(bodyp, flp)) {
                    if (sentreep) VL_DO_DANGLING(sentreep->deleteTree(), sentreep);
                    sentreep = readsp;
                }
            }
            // Every VHDL process runs once when the simulation starts, before
            // any signal has changed (11.3). Saying so in the sensitivity
            // itself, rather than emitting the body a second time, is what
            // keeps it to exactly one run: a change item may fire at time
            // zero of its own accord, when the signal starts at a value the
            // trigger's record of it does not.
            // "process (all)" is a process with a sensitivity list like any
            // other (11.3); the list is worked out rather than written, and
            // it runs once at the start and then waits just the same
            if (sentreep && nodep->hasSensList()) {
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
                    if (m_postponed) {
                        bodyp = AstNode::addNextNull(bodyp, postponedWait(flp));
                    }
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
        if (!sentreep && !hasWait(nodep->stmtsp())) {
            // A process with no sensitivity list and no wait in it never
            // suspends, so it has nothing to be sensitive to. Written as a
            // process that runs when the simulation starts it does what such
            // a process can usefully do -- run once, and stop the run -- and
            // is not taken for combinational logic, which reading and writing
            // its own variables would otherwise make it.
            addStmt(new AstInitial{flp, bodyp});
            return;
        }
        // A process whose first statement is a wait suspends before it does
        // anything (10.2). Left as the block's own first statement the back
        // end would read it as what the block is sensitive to, and run the
        // body once at the start -- so it is kept inside a block of its own.
        if (!sentreep && VN_IS(bodyp, EventControl)) {
            bodyp = new AstBegin{flp, "", bodyp, false};
        }
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

    // "s(i)" where i is not static: the array is a signal of its own and the
    // index is an expression the caller can work out. True where the actual
    // is exactly that -- one index of a plain signal name, nothing further.
    static bool dynamicElementOf(AstNodeVhdlExpr* actualp, const VhdlSymbol* rootp,
                                 AstNodeVhdlExpr*& idxp) {
        AstVhdlCall* const callp = VN_CAST(actualp, VhdlCall);
        if (!callp || !callp->prefixp() || !VN_IS(callp->prefixp(), VhdlName)) return false;
        if (callp->prefixp()->vhdlSymp() != rootp) return false;
        AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
        if (!asp || asp->nextp() || asp->formalp() || !asp->actualp()) return false;
        // An array of arrays indexed once still names an array, which is not
        // what this is for
        const VhdlType* const typep = actualp->vhdlTypep();
        if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY) return false;
        idxp = asp->actualp();
        return true;
    }

    // "p(iface.d => sd)" names one element of one formal (6.5.7.1): which
    // formal, and which element of it
    static bool individualFormal(const AstNodeVhdlExpr* formalp, std::string& root,
                                 std::string& elem) {
        const AstVhdlSelected* const selp = VN_CAST(formalp, VhdlSelected);
        if (!selp) return false;
        const AstVhdlName* const prefixp = VN_CAST(selp->prefixp(), VhdlName);
        const AstVhdlName* const suffixp = VN_CAST(selp->suffixp(), VhdlName);
        if (!prefixp || !suffixp) return false;
        root = prefixp->name();
        elem = suffixp->name();
        return true;
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
        // Descend before asking: an index or a selection carries a symbol of
        // its own -- the element it names -- and the object it is part of is
        // the one further in
        while (nodep) {
            if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
                nodep = callp->prefixp();  // An index or a slice
            } else if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
                nodep = selp->prefixp();
            } else if (const AstNodeVhdlExpr* const vp = VN_CAST(nodep, NodeVhdlExpr)) {
                return vp->vhdlSymp();
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
        if (const AstVhdlAssoc* const asp = VN_CAST(nodep, VhdlAssoc)) {
            // An instance's out port is a driver of the signal associated
            // with it, as much as an assignment to that signal is (6.5.6.3)
            if (asp->drives() && asp->actualp()) {
                if (const VhdlSymbol* const symp = rootSymOf(asp->actualp())) {
                    if (VN_IS(asp->actualp(), VhdlName)) {
                        out.insert(symp);
                    } else {
                        s_partialDriven.insert(symp);
                    }
                }
            }
        }
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
                testp = choiceTest(alts[i]->choicesp(), nodep->selectorp(),
                                   nodep->matching());
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

    // A port associated with an expression rather than with a signal is an
    // implicit signal of its own, driven by an implicit concurrent
    // assignment of that expression (6.5.6.3). It therefore holds its default
    // until the expression is first delivered, and every delivery after that
    // arrives a delta later -- which wiring the expression straight to the
    // port would not: the port would follow it with no delta at all.
    AstPin* portExprSignal(AstPin* pinp, AstVhdlAssoc* asp, const std::string& label,
                           FileLine* flp) {
        if (!pinp || !pinp->exprp() || !asp->actualp()) return pinp;
        if (asp->drives() || asp->isOpen() || VN_IS(asp->actualp(), VhdlOpen)) return pinp;
        if (isSignalActual(asp->actualp())) return pinp;
        // Only an expression that is not globally static: one that reads no
        // signal cannot change while the design runs, and such an actual is
        // associated with the port directly (6.5.6.3)
        if (!readsSignal(asp->actualp())) return pinp;
        AstNodeExpr* const exprp = VN_CAST(pinp->exprp(), NodeExpr);
        if (!exprp) return pinp;
        // Written from two blocks by construction -- once to start, once to
        // react -- so the finding is about how this is emitted, not about
        // the design
        FileLine* const pflp = new FileLine{flp};
        pflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
        pflp->warnOff(V3ErrorCode::COMBDLY, true);
        // "The signal has the same subtype as the formal port", which for a
        // concatenation is the only place its length is written down
        VhdlType* typep = nullptr;
        if (const AstVhdlName* const fnamep = VN_CAST(asp->formalp(), VhdlName)) {
            const VhdlSymbol* const psymp = fnamep->vhdlSymp();
            if (psymp && psymp->kind() == VhdlSymKind::PORT) typep = psymp->typep();
        }
        if (typep && (!typep->lengthEmittable() || dependsOnGeneric(typep))) typep = nullptr;
        if (!typep) typep = asp->actualp()->vhdlTypep();
        // Where neither says how long the signal is here -- a port whose
        // bounds this instance settles, most often -- the expression goes to
        // the port as it did, which costs the delta rather than the design
        if (!typep || !typep->lengthEmittable() || dependsOnGeneric(typep)) return pinp;
        AstNodeDType* const dtypep = lowerType(typep, flp);
        if (!dtypep) return pinp;
        AstVar* const tempp = newTempVar(
            "__Vpexpr__" + label + "__" + std::to_string(++m_processNum), dtypep, pflp);
        if (AstNodeExpr* const defp = defaultValueFor(typep, flp, true)) tempp->valuep(defp);
        AstNodeExpr* const valuep = exprp->unlinkFrBack();
        AstNodeExpr* const readsp = valuep->cloneTree(false);
        // Emitted twice -- once to start, once to react -- as every
        // concurrent assignment is, so it drives from two blocks
        emitWaveformProcess(
            new AstAssignDly{pflp, new AstVarRef{pflp, tempp, VAccess::WRITE}, valuep}, readsp,
            pflp);
        pinp->exprp(new AstVarRef{flp, tempp, VAccess::READ});
        return pinp;
    }

    // Whether an expression reads a signal or a port, which is what makes it
    // able to change while the design runs
    static bool readsSignal(AstNodeVhdlExpr* nodep) {
        bool found = false;
        nodep->foreach([&](AstNodeVhdlExpr* refp) {
            const VhdlSymbol* const symp = refp->vhdlSymp();
            if (symp
                && (symp->kind() == VhdlSymKind::SIGNAL || symp->kind() == VhdlSymKind::PORT)) {
                found = true;
            }
        });
        return found;
    }

    // A port of mode out is one driver of the signal it is associated with,
    // so where that signal is resolved the port is wired to this driver's own
    // value rather than to the signal (6.5.6.3, 14.7.2). An instance drives
    // from the moment it is elaborated, so its driver is always on.
    // Between this call and unbindPortDriver, the actual's name means that
    // driver's own value rather than the signal. Returns null, and binds
    // nothing, where the signal is not resolved.
    VhdlSymbol* bindPortDriver(AstVhdlAssoc* asp, FileLine* flp, AstVar*& savedp) {
        const VhdlSymbol* const symp
            = asp->drives() && asp->actualp() ? rootSymOf(asp->actualp()) : nullptr;
        const auto it = symp ? m_drivers.find(symp) : m_drivers.end();
        if (it == m_drivers.end()
            || m_driverUsed[symp] >= static_cast<int>(it->second.size())) {
            return nullptr;
        }
        const VhdlDriver& drv = it->second[m_driverUsed[symp]];
        VhdlSymbol* const keyp = const_cast<VhdlSymbol*>(symp);
        savedp = m_vars[keyp];
        m_vars[keyp] = drv.valp;
        if (drv.enp) {
            // Resolved by the logic tables, where a driver says whether it
            // drives at all; this one always does, and forcingly
            addStmt(new AstInitial{
                flp, new AstAssign{flp, new AstVarRef{flp, drv.enp, VAccess::WRITE},
                                   new AstConst{flp, AstConst::WidthedValue{}, 1, 1}}});
        }
        return keyp;
    }
    void unbindPortDriver(VhdlSymbol* keyp, AstVar* savedp) {
        if (keyp) m_vars[keyp] = savedp;
    }
    AstPin* lowerPortAssoc(AstVhdlAssoc* asp, int pinNum, FileLine* flp) {
        AstVar* savedp = nullptr;
        VhdlSymbol* const keyp = bindPortDriver(asp, flp, savedp);
        AstPin* const pinp = lowerAssoc(asp, pinNum);
        unbindPortDriver(keyp, savedp);
        return pinp;
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
        AstNodeExpr* const valLhsp = lowerDriveTarget(targetp);
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

    // Whether any waveform of a conditional assignment schedules a
    // transaction at a time of its own, or more than one
    static bool anyWaveformDelay(AstVhdlSigAssign* nodep) {
        for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
            AstVhdlWaveform* const wavesp = VN_AS(p, VhdlAlternative)->wavesp();
            if (wavesp && (wavesp->nextp() || wavesp->afterp())) return true;
        }
        return false;
    }

    // The statements one waveform becomes: each element assigned at its own
    // time, in the order written (10.5.2.2). Adds what it reads to readsp,
    // which is what the process is sensitive to.
    AstNode* waveformStmts(AstNodeVhdlExpr* targetp, AstVhdlWaveform* wavesp,
                           AstNodeExpr*& readsp, FileLine* flp) {
        AstNode* stmtsp = nullptr;
        // A transaction happens when each element is delivered, and the bit
        // that records it flips *then* rather than now: a delayed assignment
        // of "not t" reads t as it stands when the assignment is made, so
        // three of them scheduled together would all deliver the same value.
        // The flips are therefore a sequence of waits, after the whole
        // waveform has been scheduled.
        AstNodeExpr* prevDelayp = nullptr;
        AstNode* marksp = nullptr;
        for (AstVhdlWaveform* p = wavesp; p; p = VN_AS(p->nextp(), VhdlWaveform)) {
            AstNodeExpr* const onep = lowerDriveTarget(targetp);
            AstNodeExpr* const valuep = lowerExpr(p->valuep());
            if (!onep || !valuep) return nullptr;
            readsp = readsp ? new AstConcat{flp, readsp, valuep->cloneTree(false)}
                            : valuep->cloneTree(false);
            AstNodeExpr* const afterp = p->afterp() ? lowerExpr(p->afterp()) : nullptr;
            AstNode* const delayp = afterp ? new AstDelay{flp, afterp, false} : nullptr;
            stmtsp = AstNode::addNextNull(
                stmtsp, static_cast<AstNode*>(new AstAssignDly{flp, onep, valuep, delayp}));
            if (AstNode* const flipp = transactionMark(targetp, nullptr, true, nullptr, flp)) {
                // The wait before it is how much later than the last element
                // this one is delivered, since the waveform's times increase
                AstNodeExpr* stepp = afterp ? afterp->cloneTree(false) : nullptr;
                if (stepp && prevDelayp) {
                    stepp = new AstSub{flp, stepp, prevDelayp->cloneTree(false)};
                }
                if (stepp) {
                    marksp = AstNode::addNextNull(
                        marksp, static_cast<AstNode*>(new AstDelay{flp, stepp, false}));
                }
                marksp = AstNode::addNextNull(marksp, flipp);
            }
            if (afterp) prevDelayp = afterp;
        }
        return AstNode::addNextNull(stmtsp, marksp);
    }

    // A concurrent statement is a process that runs once at time zero and
    // again whenever what it reads changes (11.6). Verilog's "always" with an
    // event control does not run at time zero, so the statements are emitted
    // twice: once to start, once to react.
    void emitWaveformProcess(AstNode* stmtsp, AstNodeExpr* readsp, FileLine* flp) {
        AstSenTree* const senp = readsp ? sensitivityOfExpr(readsp, flp) : nullptr;
        if (readsp) VL_DO_DANGLING(readsp->deleteTree(), readsp);
        if (senp) {
            addStmt(new AstAlways{flp, VAlwaysKwd::ALWAYS, senp, stmtsp->cloneTree(true)});
        }
        addStmt(new AstInitial{flp, stmtsp});
    }

    void lowerConcurrentAssign(AstVhdlSigAssign* nodep) {
        FileLine* const flp = nodep->fileline();
        // "(a, b) <= v" written as a concurrent statement is the process it
        // is defined to be (11.6), with the value spread over the names
        if (AstVhdlAggregate* const aggp = VN_CAST(nodep->targetp(), VhdlAggregate)) {
            if (nodep->kind() == VhdlAssignKind::SIMPLE && nodep->wavesp()
                && !nodep->wavesp()->nextp() && !nodep->wavesp()->afterp()) {
                FileLine* const aflp = new FileLine{flp};
                aflp->warnOff(V3ErrorCode::COMBDLY, true);
                aflp->warnOff(V3ErrorCode::LATCH, true);
                AstNode* const stmtsp
                    = lowerAggregateTarget(aggp, nodep->wavesp()->valuep(), true, true, aflp);
                if (stmtsp) addStmt(concurrentProcess(stmtsp, nodep->postponed(), aflp));
                return;
            }
        }
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
            AstNodeExpr* readsp = nullptr;
            AstNode* const stmtsp = waveformStmts(nodep->targetp(), nodep->wavesp(), readsp, flp);
            if (!stmtsp) return;
            emitWaveformProcess(stmtsp, readsp, flp);
            return;
        }
        // The same, where which waveform is scheduled depends on a condition
        if (nodep->kind() == VhdlAssignKind::CONDITIONAL
            && !m_drivers.count(rootSymOf(nodep->targetp())) && anyWaveformDelay(nodep)) {
            if (!v3Global.opt.timing().isTrue()) {
                unsup(flp, "waveform with more than one element without --timing");
                return;
            }
            std::vector<AstVhdlAlternative*> alts;
            for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
                alts.push_back(VN_AS(p, VhdlAlternative));
            }
            AstNode* outp = nullptr;
            AstNodeExpr* readsp = nullptr;
            for (size_t i = alts.size(); i-- > 0;) {
                AstNode* const bodyp
                    = waveformStmts(nodep->targetp(), alts[i]->wavesp(), readsp, flp);
                if (!bodyp) return;
                if (!alts[i]->condp()) {
                    outp = bodyp;  // The trailing "else"
                    continue;
                }
                AstNodeExpr* const condp = lowerCondition(alts[i]->condp());
                if (!condp) return;
                readsp = readsp ? new AstConcat{flp, readsp, condp->cloneTree(false)}
                                : condp->cloneTree(false);
                outp = new AstIf{alts[i]->fileline(), condp, bodyp, outp};
            }
            if (!outp) return;
            emitWaveformProcess(outp, readsp, flp);
            return;
        }
        AstNodeExpr* const lhsp = lowerDriveTarget(nodep->targetp());
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
        // A concurrent signal assignment is the process it is defined to be:
        // it runs whenever what it reads changes, and the assignment
        // *schedules* an update rather than making one (11.6, 10.5.2.2).  It
        // is not a continuous assignment: a continuous assignment lands in
        // the same delta it was computed in, so a process that reads the
        // target without being sensitive to it would see the new value where
        // VHDL gives it the old one.
        FileLine* const aflp = new FileLine{flp};
        aflp->warnOff(V3ErrorCode::COMBDLY, true);
        aflp->warnOff(V3ErrorCode::LATCH, true);
        AstNode* const assignp = new AstAssignDly{aflp, lhsp, rhsp, delayp};
        addStmt(concurrentProcess(assignp, nodep->postponed(), aflp));
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
                    outp = lowerDriveTarget(nodep->targetp());
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
            AstNodeExpr* testp = choiceTest(altp->choicesp(), selp, nodep->matching());
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
    // "select?" matches with "?=", where a '-' in a choice stands for any
    // value (10.5.4). That is Verilog's wildcard equality, and '-' is carried
    // as x here.
    AstNodeExpr* choiceTest(AstNode* choicesp, AstNodeVhdlExpr* selp, bool matching = false) {
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
                onep = matching
                           ? static_cast<AstNodeExpr*>(new AstEqWild{flp, selValp, chValp})
                           : static_cast<AstNodeExpr*>(new AstEq{flp, selValp, chValp});
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
        // "use entity work.sub(test)" parses as a name with a parenthesised
        // suffix, since an architecture name and an index look alike (7.3.2)
        AstNodeVhdlExpr* unitp = bindingp->unitp();
        if (AstVhdlCall* const callp = VN_CAST(unitp, VhdlCall)) unitp = callp->prefixp();
        std::vector<std::string> parts;
        if (!flattenName(unitp, parts) || parts.empty()) return;
        const std::string entity = parts.back();
        // "labels : component_name", where the component name is the last of
        // the names and the ones before it are the instances it applies to
        // (7.3.1). "all" and "others" leave only the component name.
        std::vector<std::string> names;
        for (AstNode* p = specp->namesp(); p; p = p->nextp()) {
            if (const AstVhdlName* const namep = VN_CAST(p, VhdlName)) {
                names.push_back(namep->name());
            }
        }
        std::string component;
        if (specp->entityClass() == "component" && !names.empty()) {
            component = names.back();
            names.pop_back();
        }
        // "all" and "others" speak for every instance of the component;
        // a specification that names instances binds those alone (7.3.1)
        if (!component.empty() && (specp->all() || specp->others())) {
            m_bindings[component] = entity;
            m_bindingMaps[component] = bindingp;
        }
        for (const std::string& label : names) {
            m_instanceBindings[label] = entity;
            m_bindingMaps[label] = bindingp;
        }
    }
    // What a binding indication inside a configuration declaration names:
    // the entity to build the instance from, and which architecture of it
    // (7.3.2.1). "use configuration work.c" names neither directly -- it
    // names a configuration, which made both choices already.
    bool cfgBindOf(AstVhdlBinding* bindingp, const std::string& lib, CfgBind& out) {
        if (!bindingp) return false;
        out.bindingp = bindingp;
        if (bindingp->kind() == "open") return true;  // Deliberately left unbound
        if (!bindingp->unitp()) return false;
        AstNodeVhdlExpr* unitp = bindingp->unitp();
        if (AstVhdlCall* const callp = VN_CAST(unitp, VhdlCall)) unitp = callp->prefixp();
        std::vector<std::string> parts;
        if (!flattenName(unitp, parts) || parts.empty()) return false;
        out.entity = parts.back();
        out.arch = bindingp->archName();
        if (bindingp->kind() != "configuration") return true;
        const std::string cfgLib = parts.size() > 1 ? parts[parts.size() - 2] : lib;
        auto it = m_configOf.find(cfgLib + "." + out.entity);
        if (it == m_configOf.end()) it = m_configOf.find(lib + "." + out.entity);
        if (it == m_configOf.end()) return false;
        out.entity = it->second.first;
        out.arch = it->second.second;
        return true;
    }
    // Which instances of the block a component configuration speaks for
    static void recordCfgBind(AstVhdlEntitySpec* specp, const CfgBind& bind, CfgScope& scope) {
        std::vector<std::string> names;
        for (AstNode* p = specp->namesp(); p; p = p->nextp()) {
            if (const AstVhdlName* const namep = VN_CAST(p, VhdlName)) {
                names.push_back(namep->name());
            }
        }
        if (names.empty()) return;
        const std::string component = names.back();
        names.pop_back();
        if (specp->all() || specp->others()) {
            scope.comps[component] = bind;
            return;
        }
        for (const std::string& label : names) scope.insts[label] = bind;
    }
    // The component configurations inside a configuration declaration say
    // which entity each instance is built from, in the same words a
    // configuration specification uses (3.4.3). What one says reaches the
    // statements of its own block alone, so the blocks and generates nest
    // (3.4.2) -- two blocks may configure the same component name
    // differently, and each instance follows the block it stands in.
    void buildCfgScope(AstVhdlBlockConfig* itemsp, CfgScope& scope, const std::string& lib) {
        for (AstVhdlBlockConfig* p = itemsp; p; p = VN_AS(p->nextp(), VhdlBlockConfig)) {
            AstVhdlEntitySpec* const specp = VN_CAST(p->specp(), VhdlEntitySpec);
            if (!specp) continue;
            if (specp->entityClass() == "component") {
                CfgBind bind;
                if (cfgBindOf(p->bindingp(), lib, bind)) recordCfgBind(specp, bind, scope);
                continue;
            }
            // A block specification names a block or a generate statement.
            // "g(0 to 3)" says which of a generate's iterations it applies
            // to; the iterations are elaborated later, by which time the
            // instances are gone, so all of them follow what it says.
            AstNodeVhdlExpr* namep = specp->namesp();
            if (AstVhdlCall* const callp = VN_CAST(namep, VhdlCall)) namep = callp->prefixp();
            std::vector<std::string> parts;
            if (!namep || !flattenName(namep, parts) || parts.empty()) continue;
            buildCfgScope(p->itemsp(), scope.blocks[parts.back()], lib);
        }
    }
    // What a configuration declaration says about one block inside the one
    // being lowered. Nothing said outside that block reaches into it (3.4.2).
    const CfgScope* cfgScopeIn(const std::string& label) const {
        if (!m_cfgScopep || label.empty()) return nullptr;
        const auto it = m_cfgScopep->blocks.find(label);
        return it == m_cfgScopep->blocks.end() ? nullptr : &it->second;
    }
    // Every entity and architecture some configuration asks for
    static void cfgScopeArchs(const CfgScope& scope,
                              std::vector<std::pair<std::string, std::string>>& out) {
        for (const auto& pair : scope.comps) {
            if (!pair.second.arch.empty()) out.emplace_back(pair.second.entity, pair.second.arch);
        }
        for (const auto& pair : scope.insts) {
            if (!pair.second.arch.empty()) out.emplace_back(pair.second.entity, pair.second.arch);
        }
        for (const auto& pair : scope.blocks) cfgScopeArchs(pair.second, out);
    }

    std::map<std::string, std::string> m_archOf;  // "lib.entity" -> architecture built
    std::set<std::string> m_altArchMods;  // "lib.module" built from another architecture
    std::map<std::string, std::string> m_bindings;  // Component -> entity
    std::map<std::string, std::string> m_instanceBindings;  // Label -> entity
    // The binding indication itself, which may carry generic and port maps
    // of its own for the instance it binds (7.3.2)
    std::map<std::string, AstVhdlBinding*> m_bindingMaps;

    // Whether a pin of that name is already among these
    static bool pinNamed(AstPin* pinsp, const std::string& name) {
        if (name.empty()) return false;
        for (AstPin* p = pinsp; p; p = VN_AS(p->nextp(), Pin)) {
            if (p->name() == name) return true;
        }
        return false;
    }

    // The names an instance wrote for itself when it said which architecture
    // it wants, as in "entity work.sub(one)" (7.3.2). The parser could not
    // tell that shape from a call, so the architecture arrives as an argument.
    static bool instanceNamesArch(const AstVhdlInstance* nodep, std::string& entName,
                                  std::string& archName) {
        const AstVhdlCall* const callp = VN_CAST(nodep->unitp(), VhdlCall);
        if (!callp) return false;
        const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
        if (!asp || asp->nextp() || !asp->actualp()) return false;
        std::vector<std::string> archParts;
        if (!flattenName(asp->actualp(), archParts) || archParts.empty()) return false;
        std::vector<std::string> parts;
        if (!flattenName(callp->prefixp(), parts) || parts.empty()) return false;
        entName = parts.back();
        archName = archParts.back();
        return true;
    }

    // The module an entity built from an architecture other than the design's
    // own choice goes by, so that both can stand in the netlist at once
    static std::string altArchName(const std::string& entName, const std::string& archName) {
        return entName + "__a" + archName;
    }

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
        // A configuration declaration says the same from outside the
        // architecture, and says it of this block alone (3.4.3)
        const CfgBind* cfgBindp = nullptr;
        if (m_cfgScopep) {
            const auto cInstIt = m_cfgScopep->insts.find(label);
            if (!label.empty() && cInstIt != m_cfgScopep->insts.end()) {
                cfgBindp = &cInstIt->second;
            } else {
                const auto cCompIt = m_cfgScopep->comps.find(modName);
                if (cCompIt != m_cfgScopep->comps.end()) cfgBindp = &cCompIt->second;
            }
        }
        if (cfgBindp) {
            if (cfgBindp->entity.empty()) {
                // "use open" leaves the instance deliberately unbound, so
                // nothing is built for it (7.3.2)
                const VhdlSymbol* const openSymp
                    = nodep->unitp() ? nodep->unitp()->vhdlSymp() : nullptr;
                const AstVhdlComponentDecl* const openDeclp
                    = openSymp && openSymp->kind() == VhdlSymKind::COMPONENT
                          ? VN_CAST(openSymp->declp(), VhdlComponentDecl)
                          : nullptr;
                if (openDeclp) lowerUnboundInstance(nodep, openDeclp, flp);
                return;
            }
            modName = cfgBindp->entity;
            if (!cfgBindp->arch.empty()) archName = cfgBindp->arch;
        } else {
            const auto instIt = m_instanceBindings.find(label);
            if (!label.empty() && instIt != m_instanceBindings.end()) {
                modName = instIt->second;
            } else {
                const auto compIt = m_bindings.find(modName);
                if (compIt != m_bindings.end()) modName = compIt->second;
            }
        }
        // Where the entity had a generic type, this instance is of the copy
        // made for the types it named (6.5.5)
        if (!nodep->specModule().empty()) modName = nodep->specModule();
        // An entity may have several architectures, of which one is the
        // design's own choice; an instance that names another is of the
        // module built from that one instead (7.3.2)
        if (!archName.empty()) {
            const std::string libname = m_modp ? m_modp->libname() : "work";
            const auto archIt = m_archOf.find(libname + "." + modName);
            if (archIt != m_archOf.end() && archIt->second != archName) {
                const std::string altName = altArchName(modName, archName);
                if (!m_altArchMods.count(libname + "." + altName)) {
                    unsup(flp, "instance of architecture '" + archName + "' where '"
                                   + archIt->second + "' is the one built");
                    return;
                }
                modName = altName;
            }
        }
        // A binding indication may say which of the entity's ports each of
        // the component's becomes (7.3.2.2). The two usually go by the same
        // name, and then there is nothing to do; where they do not, the
        // instance wrote the component's names and the entity wants its own.
        const AstVhdlBinding* bindingp = nullptr;
        if (cfgBindp) {
            bindingp = cfgBindp->bindingp;
        } else {
            const auto bindIt = m_bindingMaps.find(
                !label.empty() && m_instanceBindings.count(label) ? label : parts.back());
            if (bindIt != m_bindingMaps.end()) bindingp = bindIt->second;
        }
        std::map<std::string, std::vector<std::string>> renamed;  // Component port -> entity's
        bool anyRenamed = false;
        if (bindingp && bindingp->portMapp()) {
            for (AstNode* p = bindingp->portMapp(); p; p = p->nextp()) {
                const AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->formalp() || !asp->actualp()) continue;
                std::vector<std::string> fparts;
                std::vector<std::string> aparts;
                if (!flattenName(asp->formalp(), fparts) || fparts.empty()) continue;
                if (asp->isOpen() || VN_IS(asp->actualp(), VhdlOpen)) continue;
                if (!flattenName(asp->actualp(), aparts) || aparts.empty()) {
                    unsup(flp, "binding indication whose actual is not a port name");
                    return;
                }
                renamed[aparts.back()].push_back(fparts.back());
                if (aparts.back() != fparts.back()) anyRenamed = true;
            }
        }
        if (!anyRenamed) renamed.clear();
        AstPin* pinsp = nullptr;
        int pinNum = 0;
        // "f(p) => a" converts what the port drives on its way out to the
        // actual (6.5.7.1): the port is a signal of its own here, and the
        // actual follows the conversion of it.
        std::set<const AstVhdlAssoc*> converted;
        for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            AstVhdlCall* const convp = VN_CAST(asp->formalp(), VhdlCall);
            if (!convp || !asp->actualp() || asp->isOpen()) continue;
            const AstVhdlName* const convNamep = VN_CAST(convp->prefixp(), VhdlName);
            if (!convNamep) continue;
            const AstVhdlAssoc* const argp = VN_CAST(convp->argsp(), VhdlAssoc);
            const AstNodeVhdlExpr* const argValp
                = argp ? argp->actualp() : VN_CAST(convp->argsp(), NodeVhdlExpr);
            const AstVhdlName* const portNamep
                = convp->argsp() && !convp->argsp()->nextp() ? VN_CAST(argValp, VhdlName)
                                                             : nullptr;
            VhdlSymbol* const portSymp = portNamep ? portNamep->vhdlSymp() : nullptr;
            if (!portSymp || portSymp->kind() != VhdlSymKind::PORT) continue;
            AstNodeExpr* const targetp = lowerExpr(asp->actualp());
            if (!targetp || !isAssignable(targetp)) {
                if (targetp) VL_DO_DANGLING(targetp->deleteTree(), targetp);
                continue;
            }
            AstNodeDType* dtypep = nullptr;
            {
                VL_RESTORER_COPY(m_bindExprs);
                for (const auto& pair : genericStandIns(nodep, portSymp->typep())) {
                    m_bindExprs[pair.first] = pair.second;
                }
                dtypep = lowerType(portSymp->typep(), flp);
            }
            AstVar* const tempp
                = newTempVar("__Vconv__" + label + "__" + portSymp->name(), dtypep, flp);
            // The signal standing in for the port starts where the port
            // does, so the conversion of it is right before anything runs
            tempp->valuep(defaultValueFor(portSymp->typep(), flp));
            // The conversion reads the port's own signal, so the name it
            // applies to stands for that signal while it is lowered
            m_vars.emplace(portSymp, tempp);
            AstNodeExpr* const convValuep = lowerExpr(asp->formalp());
            m_vars.erase(portSymp);
            if (!convValuep) {
                VL_DO_DANGLING(targetp->deleteTree(), targetp);
                continue;
            }
            FileLine* const cflp = new FileLine{flp};
            cflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            // A port association is transparent: the actual holds the
            // conversion of what the port drives from the start, not one
            // delta later (6.5.7.1) -- so the conversion is applied once as
            // the design elaborates as well as whenever the port changes
            addStmt(new AstInitial{
                cflp, new AstAssign{cflp, targetp->cloneTree(false),
                                    convValuep->cloneTree(false)}});
            addStmt(new AstAlways{new AstAssignW{cflp, targetp, convValuep}});
            pinsp = AstNode::addNextNull(
                pinsp, new AstPin{flp, ++pinNum, portSymp->name(),
                                  new AstVarRef{flp, tempp, VAccess::WRITE}});
            converted.insert(asp);
        }
        // A formal may name part of a port rather than the whole of it, and
        // several such associations together cover it (6.5.7.1). One signal
        // of the port's own type stands for the port, each part of it is
        // wired to its actual, and the port itself is associated once.
        std::map<const VhdlSymbol*, AstVar*> partial;
        size_t portPosition = 0;  // Which port a positional association names
        for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (converted.count(asp)) continue;
            AstNodeVhdlExpr* const rootp = individualRoot(asp->formalp());
            AstVhdlName* const namep = VN_CAST(rootp, VhdlName);
            VhdlSymbol* const symp = namep ? namep->vhdlSymp() : nullptr;
            if (!symp || symp->kind() != VhdlSymKind::PORT) continue;
            if (partial.count(symp)) continue;
            // A port whose bounds an instance settles is written in terms of
            // the entity's own generics, which mean nothing here. The formal
            // names a part of it, and that part's subtype is written in terms
            // this architecture can see, so it is the one to stand in.
            VhdlType* standTypep = symp->typep();
            // Only where the formal names a slice of the port: "p.a => x"
            // names one element of a record, and the signal standing in for
            // the port is the whole record, not that element
            if (VN_IS(asp->formalp(), VhdlCall) && dependsOnGeneric(standTypep)
                && asp->formalp()->vhdlTypep()
                && asp->formalp()->vhdlTypep()->base() == standTypep->base()
                && !dependsOnGeneric(asp->formalp()->vhdlTypep())) {
                standTypep = asp->formalp()->vhdlTypep();
            }
            AstNodeDType* standDtypep = nullptr;
            {
                VL_RESTORER_COPY(m_bindExprs);
                for (const auto& pair : genericStandIns(nodep, standTypep)) {
                    m_bindExprs[pair.first] = pair.second;
                }
                standDtypep = lowerType(standTypep, flp);
            }
            AstVar* const tempp = newTempVar(
                "__Vpart__" + label + "__" + symp->name(), standDtypep, flp);
            partial.emplace(symp, tempp);
            // Which way the signal standing in for the port travels. An
            // input read as though it were also driven here is a loop, not
            // a connection.
            const AstVhdlIfaceDecl* const pdeclp = VN_CAST(symp->declp(), VhdlIfaceDecl);
            const VhdlMode pmode = pdeclp ? pdeclp->mode() : VhdlMode::IN;
            const VAccess pinAccess = pmode == VhdlMode::IN      ? VAccess::READ
                                      : pmode == VhdlMode::INOUT ? VAccess::READWRITE
                                                                 : VAccess::WRITE;
            pinsp = AstNode::addNextNull(
                pinsp, new AstPin{flp, ++pinNum, symp->name(),
                                  new AstVarRef{flp, tempp, pinAccess}});
        }
        for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (converted.count(asp)) continue;
            AstNodeVhdlExpr* const rootp = individualRoot(asp->formalp());
            AstVhdlName* const namep = VN_CAST(rootp, VhdlName);
            VhdlSymbol* const symp = namep ? namep->vhdlSymp() : nullptr;
            const auto partIt = symp ? partial.find(symp) : partial.end();
            if (partIt == partial.end()) {
                if (!renamed.empty()) {
                    // Which of the component's ports this association names,
                    // by name where it was written and by position otherwise
                    std::string compPort;
                    if (asp->formalp()) {
                        std::vector<std::string> fparts;
                        if (flattenName(asp->formalp(), fparts) && !fparts.empty()) {
                            compPort = fparts.back();
                        }
                    } else if (portPosition < nodep->formalPorts().size()) {
                        compPort = nodep->formalPorts()[portPosition];
                    }
                    ++portPosition;
                    const auto renIt = renamed.find(compPort);
                    if (renIt != renamed.end()) {
                        AstVar* savedp = nullptr;
                        VhdlSymbol* const keyp = bindPortDriver(asp, flp, savedp);
                        // One of the component's ports may feed more than one
                        // of the entity's, so each gets a pin of its own
                        for (const std::string& entPort : renIt->second) {
                            AstNodeExpr* const actp
                                = asp->isOpen() || VN_IS(asp->actualp(), VhdlOpen)
                                      ? nullptr
                                      : lowerExpr(asp->actualp());
                            pinsp = AstNode::addNextNull(
                                pinsp, new AstPin{asp->fileline(), ++pinNum, entPort, actp});
                        }
                        unbindPortDriver(keyp, savedp);
                        continue;
                    }
                }
                AstPin* const pinp
                    = portExprSignal(lowerPortAssoc(asp, ++pinNum, flp), asp, label, flp);
                if (pinp) pinsp = AstNode::addNext(pinsp, pinp);
                continue;
            }
            if (!asp->actualp() || asp->isOpen() || VN_IS(asp->actualp(), VhdlOpen)) continue;
            // The part of the standing signal the formal names, reached by
            // lowering the formal with its root bound to that signal
            AstNodeExpr* partp = nullptr;
            {
                // The formal names a port of the instantiated unit, which has
                // no variable here; for the length of this one expression it
                // names the signal standing in for it
                const auto prevIt = m_vars.find(symp);
                const bool had = prevIt != m_vars.end();
                AstVar* const savedp = had ? prevIt->second : nullptr;
                m_vars[symp] = partIt->second;
                partp = lowerExpr(asp->formalp());
                if (had) {
                    m_vars[symp] = savedp;
                } else {
                    m_vars.erase(symp);
                }
            }
            AstNodeExpr* const actp = lowerExpr(asp->actualp());
            if (!partp || !actp) {
                if (partp) VL_DO_DANGLING(partp->deleteTree(), partp);
                if (actp) VL_DO_DANGLING(actp->deleteTree(), actp);
                continue;
            }
            const AstVhdlIfaceDecl* const declp
                = VN_CAST(symp->declp(), VhdlIfaceDecl);
            const bool outward = declp
                                 && (declp->mode() == VhdlMode::OUT
                                     || declp->mode() == VhdlMode::BUFFER);
            FileLine* const cflp = new FileLine{flp};
            cflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            addStmt(new AstAlways{outward ? new AstAssignW{cflp, actp, partp}
                                          : new AstAssignW{cflp, partp, actp}});
        }
        AstPin* paramsp = nullptr;
        pinNum = 0;
        size_t genPosition = 0;  // Which generic a positional association names
        for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            AstPin* const pinp = lowerAssoc(asp, ++pinNum);
            if (!pinp) continue;
            // A generic carrying another's bounds stands ahead of it in the
            // clause but has no place in it, so a positional association is
            // given the name of the generic it actually reaches (6.5.7.1)
            if (!asp->formalp()) {
                if (genPosition < nodep->formalGenerics().size()) {
                    pinp->name(nodep->formalGenerics()[genPosition]);
                }
                ++genPosition;
            }
            paramsp = AstNode::addNext(paramsp, pinp);
        }
        // A binding indication may give the generics and the ports itself,
        // which is how a configuration supplies what the instance left out
        // (7.3.2). What the instance wrote comes first and wins.
        if (bindingp) {
            for (AstNode* p = bindingp->genericMapp(); p; p = p->nextp()) {
                // A binding indication may be what chooses a generic type,
                // and then the entity has to be copied for it just as an
                // instance's own generic map would have it copied (6.5.5)
                const AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                const AstNodeVhdlExpr* const actp = asp ? asp->actualp() : nullptr;
                const VhdlSymbol* const asymp = actp ? actp->vhdlSymp() : nullptr;
                if (asymp
                    && (asymp->kind() == VhdlSymKind::TYPE
                        || asymp->kind() == VhdlSymKind::PACKAGE)) {
                    unsup(flp, "binding indication that chooses a generic type or package");
                    return;
                }
                AstPin* const pinp = lowerAssoc(VN_AS(p, VhdlAssoc), ++pinNum);
                if (pinp && !pinNamed(paramsp, pinp->name())) {
                    paramsp = AstNode::addNext(paramsp, pinp);
                } else if (pinp) {
                    VL_DO_DANGLING(pinp->deleteTree(), pinp);
                }
            }
        }
        // A component with no entity of its name is unbound: the instance is
        // not elaborated at all, and each actual associated with a formal
        // that has a default takes that default (7.3.3, 12.4.3). Refusing it
        // would stop a design that deliberately leaves a component unbound.
        {
            const VhdlSymbol* const compSymp
                = nodep->unitp() ? nodep->unitp()->vhdlSymp() : nullptr;
            const AstVhdlComponentDecl* const declp
                = compSymp && compSymp->kind() == VhdlSymKind::COMPONENT
                      ? VN_CAST(compSymp->declp(), VhdlComponentDecl)
                      : nullptr;
            const std::string entLib = m_modp ? m_modp->libname() : "work";
            if (declp && !m_knownEntities.count(entLib + "." + modName)) {
                flp->v3warn(VHDLUNBOUND,
                            "Component instance '"
                                + (label.empty() ? modName : label)
                                + "' has no entity bound to it: no entity '" + modName
                                + "' was analyzed");
                lowerUnboundInstance(nodep, declp, flp);
                return;
            }
        }
        // A component's generic may have a default the entity's has not, and
        // an instance that says nothing about it takes the component's
        // (6.5.6.2). The module is built from the entity, so that default
        // has to travel with the instance.
        {
            const VhdlSymbol* const compSymp
                = nodep->unitp() ? nodep->unitp()->vhdlSymp() : nullptr;
            const AstVhdlComponentDecl* const compp
                = compSymp && compSymp->kind() == VhdlSymKind::COMPONENT
                      ? VN_CAST(compSymp->declp(), VhdlComponentDecl)
                      : nullptr;
            if (compp) {
                for (AstNode* p = compp->genericsp(); p; p = p->nextp()) {
                    const AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
                    if (!ifp->defaultp() || pinNamed(paramsp, ifp->name())) continue;
                    AstNodeExpr* const valuep = lowerExpr(ifp->defaultp());
                    if (!valuep) continue;
                    paramsp = AstNode::addNextNull(
                        paramsp, new AstPin{flp, ++pinNum, ifp->name(), valuep});
                }
            }
        }
        std::string instName
            = label.empty() ? "__Vinst" + std::to_string(++m_processNum) : label;
        // A block is a declarative region of its own (11.2), so two blocks
        // may each label an instance the same; the module they flatten into
        // needs one name for each of them
        if (!m_instNames.insert(instName).second) {
            std::string alt = m_blockPrefix + instName;
            int n = 0;
            while (!m_instNames.insert(alt).second) {
                alt = m_blockPrefix + instName + "__" + std::to_string(++n);
            }
            instName = alt;
        }
        m_instantiated.insert(modName);
        addStmt(new AstCell{flp, flp, instName, modName, pinsp, paramsp, nullptr});
    }

    // An instance of a component no entity is bound to. Nothing is
    // elaborated for it; what an "out" formal would have driven takes the
    // default that formal was declared with (7.3.3, 12.4.3).
    void lowerUnboundInstance(AstVhdlInstance* nodep, const AstVhdlComponentDecl* declp,
                              FileLine* flp) {
        std::vector<AstVhdlIfaceDecl*> ports;
        for (AstNode* p = declp->portsp(); p; p = p->nextp()) {
            ports.push_back(VN_AS(p, VhdlIfaceDecl));
        }
        size_t position = 0;
        for (AstNode* p = nodep->portMapp(); p; p = p->nextp(), ++position) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (!asp->actualp() || asp->isOpen() || VN_IS(asp->actualp(), VhdlOpen)) continue;
            const AstVhdlIfaceDecl* formalp = nullptr;
            if (const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName)) {
                for (AstVhdlIfaceDecl* const ifp : ports) {
                    if (ifp->name() == namep->name()) formalp = ifp;
                }
            } else if (!asp->formalp() && position < ports.size()) {
                formalp = ports[position];
            }
            if (!formalp || !formalp->defaultp()) continue;
            if (formalp->mode() != VhdlMode::OUT && formalp->mode() != VhdlMode::BUFFER
                && formalp->mode() != VhdlMode::INOUT) {
                continue;
            }
            AstNodeExpr* const targetp = lowerDriveTarget(asp->actualp());
            AstNodeExpr* const valuep = targetp ? lowerExpr(formalp->defaultp()) : nullptr;
            if (!targetp || !valuep) {
                if (targetp) VL_DO_DANGLING(targetp->deleteTree(), targetp);
                continue;
            }
            // Nothing else drives it, so the value it starts at is the
            // value it keeps
            addStmt(new AstInitial{flp, new AstAssign{flp, targetp, valuep}});
        }
    }

    // A port's bounds are written in the entity's own generics, which name
    // nothing in the architecture that instantiates it. This instance says
    // what each of them is, so those expressions -- written in terms this
    // architecture can see -- stand in for them while the port's type is
    // built here.
    std::map<const VhdlSymbol*, AstNodeVhdlExpr*> genericStandIns(AstVhdlInstance* nodep,
                                                                  VhdlType* typep) {
        std::map<const VhdlSymbol*, AstNodeVhdlExpr*> out;
        std::map<int, VhdlSymbol*> wanted;  // Position -> the generic there
        std::function<void(VhdlType*, int)> collect = [&](VhdlType* tp, int depth) {
            if (!tp || depth > 8) return;
            for (VhdlType* const idxp : tp->indexTypes()) {
                if (!idxp || !idxp->boundsp()) continue;
                idxp->boundsp()->foreach([&](AstVhdlName* namep) {
                    VhdlSymbol* const gsymp = namep->vhdlSymp();
                    if (gsymp && gsymp->kind() == VhdlSymKind::GENERIC) {
                        wanted.emplace(gsymp->position(), gsymp);
                    }
                });
            }
            VhdlType* const basep = tp->base();
            collect(tp->elementp() ? tp->elementp() : basep->elementp(), depth + 1);
            for (VhdlSymbol* const ep : tp->elements()) collect(ep->typep(), depth + 1);
        };
        collect(typep, 0);
        if (wanted.empty()) return out;
        int position = 0;
        for (AstNode* p = nodep->genericMapp(); p; p = p->nextp(), ++position) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (!asp->actualp() || VN_IS(asp->actualp(), VhdlOpen)) continue;
            if (AstVhdlName* const fnamep = VN_CAST(asp->formalp(), VhdlName)) {
                if (VhdlSymbol* const gsymp = fnamep->vhdlSymp()) {
                    out.emplace(gsymp, asp->actualp());
                    continue;
                }
                for (const auto& pair : wanted) {
                    if (pair.second->name() == fnamep->name()) {
                        out.emplace(pair.second, asp->actualp());
                    }
                }
                continue;
            }
            if (asp->formalp()) continue;
            const auto it = wanted.find(position);
            if (it != wanted.end()) out.emplace(it->second, asp->actualp());
        }
        return out;
    }

    // The leftmost name of a formal that names part of a port, or null where
    // the formal is a whole port, absent, or a conversion applied to one
    static AstNodeVhdlExpr* individualRoot(AstNodeVhdlExpr* formalp) {
        if (!formalp || VN_IS(formalp, VhdlName)) return nullptr;
        AstNodeVhdlExpr* rootp = formalp;
        while (rootp) {
            if (AstVhdlSelected* const selp = VN_CAST(rootp, VhdlSelected)) {
                rootp = selp->prefixp();
            } else if (AstVhdlCall* const callp = VN_CAST(rootp, VhdlCall)) {
                rootp = callp->prefixp();
            } else {
                break;
            }
        }
        return rootp;
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
    // Whether the statements of a generate drive a signal that is resolved
    bool drivesResolved(AstVhdlGenerate* nodep) {
        std::set<const VhdlSymbol*> driven;
        collectDriven(nodep, driven);
        for (const VhdlSymbol* const dsymp : driven) {
            if (m_drivers.count(dsymp)) return true;
        }
        return false;
    }

    // A for generate written out once per iteration, with its parameter a
    // constant in each copy. Each copy is a driver of whatever it drives,
    // which is why the loop cannot be left for the back end to unroll.
    void unrollGenerate(AstVhdlGenerate* nodep, VhdlSymbol* symp, VhdlType* rangep,
                        const std::string& label, FileLine* flp) {
        std::set<const VhdlSymbol*> driven;
        collectDriven(nodep, driven);
        std::vector<const VhdlSymbol*> resolved;
        for (const VhdlSymbol* const dsymp : driven) {
            if (m_drivers.count(dsymp)) resolved.push_back(dsymp);
        }
        m_countedDrivers = true;
        const int64_t left = rangep->left();
        const int64_t right = rangep->right();
        const bool up = rangep->ascending();
        const int64_t count = up ? right - left + 1 : left - right + 1;
        for (int64_t k = 0; k < count; ++k) {
            const int64_t value = up ? left + k : left - k;
            // The name a copy goes by. A negative index is spelled with an
            // 'n', since the back end's names have no sign in them.
            const std::string suffix = value < 0 ? "n" + std::to_string(-value)
                                                 : std::to_string(value);
            AstVar* const paramp
                = new AstVar{flp, VVarType::LPARAM, "__Vgenv__" + symp->name() + "__" + suffix,
                             VFlagChildDType{}, new AstBasicDType{flp, VBasicDTypeKwd::INTEGER}};
            paramp->valuep(new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(value)});
            const auto prevIt = m_vars.find(symp);
            const bool had = prevIt != m_vars.end();
            AstVar* const savedp = had ? prevIt->second : nullptr;
            m_vars[symp] = paramp;
            AstNode* const itemsp = lowerGenerateBody(nodep->altsp());
            if (had) {
                m_vars[symp] = savedp;
            } else {
                m_vars.erase(symp);
            }
            AstGenBlock* const blockp
                = new AstGenBlock{flp, label + "__" + suffix, nullptr, false};
            blockp->addItemsp(paramp);
            if (itemsp) blockp->addItemsp(itemsp);
            addStmt(blockp);
            for (const VhdlSymbol* const dsymp : resolved) ++m_driverUsed[dsymp];
        }
    }

    void lowerGenerate(AstVhdlGenerate* nodep) {
        FileLine* const flp = nodep->fileline();
        const std::string label = nodep->label().empty()
                                      ? "__Vgen" + std::to_string(++m_processNum)
                                      : nodep->label();
        VL_RESTORER(m_cfgScopep);
        m_cfgScopep = cfgScopeIn(nodep->label());
        if (nodep->kind() == VhdlLoopKind::FOR) {
            VhdlSymbol* const symp = nodep->vhdlSymp();
            VhdlType* const rangep = symp ? symp->typep() : nullptr;
            // Each iteration of a generate that drives a resolved signal is
            // one driver of it (11.8, 14.7.2), and the drivers are separate
            // objects, so the body is written out once per iteration rather
            // than left as a loop for the back end to unroll
            if (symp && rangep && rangep->bounded() && drivesResolved(nodep)) {
                unrollGenerate(nodep, symp, rangep, label, flp);
                return;
            }
            AstNodeExpr* fromp = nullptr;
            AstNodeExpr* limitp = nullptr;
            bool ascending = true;
            if (rangep && !rangep->bounded() && rangep->boundsp()) {
                // A range that names a generic is fixed once the instance is
                // elaborated: the bounds go to the back end as expressions
                bool dirKnown = true;
                if (!dynamicBounds(rangep, 0, flp, fromp, limitp, ascending, &dirKnown)
                    || !dirKnown) {
                    if (fromp) VL_DO_DANGLING(fromp->deleteTree(), fromp);
                    if (limitp) VL_DO_DANGLING(limitp->deleteTree(), limitp);
                    fromp = nullptr;
                    limitp = nullptr;
                }
            }
            if (!fromp && (!symp || !rangep || !rangep->bounded())) {
                unsup(flp, "generate over a range that is not locally static");
                return;
            }
            AstVar* const genvarp
                = new AstVar{flp, VVarType::GENVAR, m_prefix + symp->name(), VFlagChildDType{},
                             new AstBasicDType{flp, VBasicDTypeKwd::INTEGER}};
            m_vars.emplace(symp, genvarp);
            if (!fromp) {
                ascending = rangep->ascending();
                fromp = lowerValue(VhdlValue::mkInt(rangep->left(), rangep), rangep, flp);
                limitp = lowerValue(VhdlValue::mkInt(rangep->right(), rangep), rangep, flp);
            }
            AstNode* const initp = new AstAssign{
                flp, new AstVarRef{flp, genvarp, VAccess::WRITE}, fromp};
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
            // A case generate elaborates the one alternative whose choices
            // cover the selector (11.8). Written out, each alternative's
            // choices are the test that picks it, so this is the same shape
            // as an if generate with "others" standing for the final else.
            std::vector<AstVhdlGenerateBody*> calts;
            for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
                calts.push_back(VN_AS(p, VhdlGenerateBody));
            }
            AstNode* coutp = nullptr;
            for (size_t i = calts.size(); i-- > 0;) {
                AstNode* const itemsp = lowerGenerateBody(calts[i]);
                const std::string altLabel
                    = calts[i]->name().empty() ? label : label + "__" + calts[i]->name();
                AstNode* const blockp = new AstGenBlock{flp, altLabel, itemsp, false};
                AstNodeExpr* const testp = choiceTest(calts[i]->choicesp(), nodep->exprp());
                if (!testp) {
                    coutp = blockp;  // "when others"
                    continue;
                }
                coutp = new AstGenIf{calts[i]->fileline(), testp, blockp, coutp};
            }
            addStmt(coutp);
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
        if (AstVhdlWait* const waitp = VN_CAST(nodep, VhdlWait)) {
            AstNode* const outp = lowerWait(waitp);
            // A postponed process resumes when its wait says, but runs at the
            // end of the last delta of that time step (10.3)
            if (outp && m_postponed) {
                return AstNode::addNext(outp, postponedWait(waitp->fileline()));
            }
            return outp;
        }
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
        if (nodep->release() || nodep->force()) {
            // 10.5.2.1 makes both of these signal assignments, so they take
            // effect at the end of the delta like any other. Which of the
            // force and the drivers is in charge is a signal of its own.
            VhdlSymbol* const rootp = const_cast<VhdlSymbol*>(rootSymOf(nodep->targetp()));
            const auto it = rootp ? m_forced.find(rootp->resolved()) : m_forced.end();
            if (it == m_forced.end()) {
                // An external name reaches a signal somewhere else in the
                // design, which has no driver pair here. Verilog's own force
                // is what is left: it overrides the value, though a release
                // then keeps it rather than handing back what the drivers
                // hold.
                AstNodeExpr* const extLhsp = lowerTarget(nodep->targetp());
                if (!extLhsp) return nullptr;
                v3Global.setHasForceableSignals();
                if (nodep->release()) return new AstRelease{flp, extLhsp};
                AstVhdlWaveform* const extWfp = nodep->wavesp();
                AstNodeExpr* const extRhsp
                    = extWfp && extWfp->valuep() ? lowerExpr(extWfp->valuep()) : nullptr;
                if (!extRhsp) {
                    VL_DO_DANGLING(extLhsp->deleteTree(), extLhsp);
                    return nullptr;
                }
                return new AstAssignForce{flp, extLhsp, extRhsp};
            }
            const VhdlForced& fc = it->second;
            FileLine* const mflp = new FileLine{flp};
            mflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            mflp->warnOff(V3ErrorCode::COMBDLY, true);
            if (nodep->release()) {
                AstNode* outp
                    = new AstAssignDly{mflp, new AstVarRef{mflp, fc.enp, VAccess::WRITE},
                                       new AstConst{mflp, AstConst::BitFalse{}}};
                // Handing the signal back is a transaction on it (10.5.2.1)
                return AstNode::addNextNull(
                    outp, transactionMark(nodep->targetp(), nullptr, true, nullptr, mflp));
            }
            AstVhdlWaveform* const forcep = nodep->wavesp();
            if (!forcep || !forcep->valuep()) return nullptr;
            AstNodeExpr* const rhsp = lowerExpr(forcep->valuep());
            if (!rhsp) return nullptr;
            // A force may name part of the signal, and then only that part
            // is forced; what is not named keeps coming from the drivers,
            // which the enable bit cannot express -- so the value the force
            // holds starts from what the drivers have.
            AstNode* outp = new AstAssign{mflp, new AstVarRef{mflp, fc.valp, VAccess::WRITE},
                                          new AstVarRef{mflp, fc.drvp, VAccess::READ}};
            const auto prevIt = m_vars.find(rootp);
            const bool had = prevIt != m_vars.end();
            AstVar* const savedp = had ? prevIt->second : nullptr;
            m_vars[rootp] = fc.valp;
            // Not lowerDriveTarget: what a force writes is the value the
            // force holds, not what the drivers keep
            AstNodeExpr* const lhsp = lowerExpr(nodep->targetp());
            if (had) {
                m_vars[rootp] = savedp;
            } else {
                m_vars.erase(rootp);
            }
            if (!lhsp) {
                VL_DO_DANGLING(outp->deleteTree(), outp);
                VL_DO_DANGLING(rhsp->deleteTree(), rhsp);
                return nullptr;
            }
            outp = AstNode::addNext(outp,
                                    static_cast<AstNode*>(new AstAssign{mflp, lhsp, rhsp}));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssignDly{mflp, new AstVarRef{mflp, fc.enp, VAccess::WRITE},
                                           new AstConst{mflp, AstConst::BitTrue{}}}));
            // Driving the signal is a transaction on it, whatever it was
            // driven with (10.5.2.1)
            outp = AstNode::addNextNull(
                outp, transactionMark(nodep->targetp(), nullptr, true, nullptr, mflp));
            return outp;
        }
        // Inside a process the conditional and selected forms are defined
        // as the if and case statements they stand for (10.5.3, 10.5.4), so
        // each alternative's waveform is assigned by the branch that picks
        // it. Where nothing is picked and no "else" was written, no
        // assignment happens at all -- unlike the concurrent forms, which
        // hold the signal by assigning it to itself.
        if (nodep->kind() == VhdlAssignKind::CONDITIONAL) {
            std::vector<AstVhdlAlternative*> alts;
            for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
                alts.push_back(VN_AS(p, VhdlAlternative));
            }
            AstNode* outp = nullptr;
            for (size_t i = alts.size(); i-- > 0;) {
                AstNode* const thenp = lowerSeqWaveform(nodep, alts[i]->wavesp(), sequential);
                if (!alts[i]->condp()) {
                    outp = thenp;  // The trailing "else"
                    continue;
                }
                outp = new AstIf{alts[i]->fileline(), lowerCondition(alts[i]->condp()), thenp,
                                 outp};
            }
            return outp;
        }
        if (nodep->kind() == VhdlAssignKind::SELECTED) {
            AstNodeVhdlExpr* const selp = nodep->selectorp();
            if (!selp) return nullptr;
            std::vector<AstVhdlAlternative*> alts;
            for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
                alts.push_back(VN_AS(p, VhdlAlternative));
            }
            AstNode* outp = nullptr;
            for (size_t i = alts.size(); i-- > 0;) {
                AstNode* const thenp = lowerSeqWaveform(nodep, alts[i]->wavesp(), sequential);
                AstNodeExpr* const testp
                    = choiceTest(alts[i]->choicesp(), selp, nodep->matching());
                if (!testp) {
                    outp = thenp;  // "when others"
                    continue;
                }
                outp = new AstIf{alts[i]->fileline(), testp, thenp, outp};
            }
            return outp;
        }
        return lowerSeqWaveform(nodep, nodep->wavesp(), sequential);
    }

    // The assignment one waveform stands for, which is what each branch of a
    // conditional or selected signal assignment reduces to
    AstNode* lowerSeqWaveform(AstVhdlSigAssign* nodep, AstVhdlWaveform* wfp, bool sequential) {
        FileLine* const flp = nodep->fileline();
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
                AstNodeExpr* const lhsp = lowerDriveTarget(nodep->targetp());
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
        AstNodeExpr* const lhsp = lowerDriveTarget(nodep->targetp());
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
        AstNodeExpr* const rhsp = lowerExpr(valuep);
        if (!rhsp) return nullptr;
        AstNodeDType* tempDTypep = nullptr;
        if (isRecord || typep->isConstrainedArray()) {
            tempDTypep = lowerType(typep, flp);
        } else {
            // The value has as many elements as the target names, starting at
            // the index subtype's own leftmost value
            const VhdlType* const indexp
                = typep->base()->indexTypes().empty() ? nullptr : typep->base()->indexTypes()[0];
            const int64_t index = indexp ? indexp->left() : 0;
            const int64_t step = (indexp && !indexp->ascending()) ? -1 : 1;
            int64_t length = 0;
            for (AstVhdlElemAssoc* p = targetp->elemsp(); p;
                 p = VN_AS(p->nextp(), VhdlElemAssoc)) {
                const int64_t span = elementSpan(p->valuep(), typep);
                if (span < 0) {
                    unsup(p->fileline(), "aggregate target element of an unknown length");
                    VL_DO_DANGLING(rhsp->deleteTree(), rhsp);
                    return nullptr;
                }
                length += span;
            }
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
        AstNode* const outp = new AstAssign{flp, new AstVarRef{flp, tempp, VAccess::WRITE}, rhsp};
        return AstNode::addNextNull(
            outp, spreadAggregate(targetp, new AstVarRef{flp, tempp, VAccess::READ}, typep,
                                  signalAssign, sequential, flp));
    }

    // Each name the aggregate lists takes the part of "srcp" that stands in
    // its place. An element may itself be an aggregate, and then the part it
    // is given is spread over the names inside it in turn (10.6.2).
    // Takes ownership of srcp.
    AstNode* spreadAggregate(AstVhdlAggregate* targetp, AstNodeExpr* srcp, VhdlType* typep,
                             bool signalAssign, bool sequential, FileLine* flp) {
        const bool isRecord = typep && typep->base()->kind() == VhdlTypeKind::RECORD;
        if (!typep || (!isRecord && typep->base()->kind() != VhdlTypeKind::ARRAY)
            || (!isRecord && typep->dimensions() != 1)) {
            unsup(flp, "aggregate target of this type");
            VL_DO_DANGLING(srcp->deleteTree(), srcp);
            return nullptr;
        }
        std::vector<AstVhdlElemAssoc*> assocs;
        for (AstVhdlElemAssoc* p = targetp->elemsp(); p; p = VN_AS(p->nextp(), VhdlElemAssoc)) {
            assocs.push_back(p);
        }
        AstNode* outp = nullptr;
        if (isRecord) {
            const std::vector<VhdlSymbol*>& elems = typep->base()->elements();
            // A record aggregate may name its elements instead of listing
            // them in order, and may name several at once (9.3.3.2)
            std::vector<AstVhdlElemAssoc*> byPos{elems.size(), nullptr};
            if (!recordTargetOrder(assocs, elems, byPos, flp)) {
                VL_DO_DANGLING(srcp->deleteTree(), srcp);
                return nullptr;
            }
            for (size_t i = 0; i < elems.size(); ++i) {
                AstNodeExpr* const fromp
                    = new AstMemberSel{flp, srcp->cloneTree(false), VFlagChildDType{},
                                       elems[i]->name()};
                outp = AstNode::addNextNull(outp,
                                            oneTarget(byPos[i]->valuep(), fromp,
                                                      elems[i]->typep(), signalAssign,
                                                      sequential, flp));
            }
            VL_DO_DANGLING(srcp->deleteTree(), srcp);
            return outp;
        }
        // Where the value's own range is known, use it; an aggregate that
        // constrains itself starts at its index subtype's leftmost value
        int64_t index = 0;
        int64_t step = 1;
        int64_t length = -1;
        const VhdlType* indexp = nullptr;
        if (typep->isConstrainedArray()) {
            indexp = typep->indexTypes()[0];
            length = indexp->length();
        } else {
            indexp = typep->base()->indexTypes().empty() ? nullptr
                                                         : typep->base()->indexTypes()[0];
        }
        if (indexp) {
            index = indexp->left();
            step = indexp->ascending() ? 1 : -1;
        }
        VhdlType* const elemp
            = typep->elementp() ? typep->elementp() : typep->base()->elementp();
        const bool isString = isStringType(typep);
        int64_t used = 0;
        for (size_t i = 0; i < assocs.size(); ++i) {
            // "(1 => a, 4 to 8 => k)" says where in the value each name
            // takes its part from, rather than leaving it to the order
            int64_t span = -1;
            if (assocs[i]->choicesp()) {
                if (!targetChoiceSpan(assocs[i], step, index, span)) {
                    VL_DO_DANGLING(srcp->deleteTree(), srcp);
                    return outp;
                }
            } else {
                span = elementSpan(assocs[i]->valuep(), typep);
            }
            if (span < 0) {
                unsup(assocs[i]->fileline(), "aggregate target element of an unknown length");
                VL_DO_DANGLING(srcp->deleteTree(), srcp);
                return outp;
            }
            AstNodeExpr* fromp = nullptr;
            const int64_t last = index + (span - 1) * step;
            AstNodeExpr* const firstIdxp
                = new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(index)};
            AstNodeExpr* const lastIdxp
                = new AstConst{flp, AstConst::Signed32{}, static_cast<int32_t>(last)};
            if (span == 1 && !VN_IS(assocs[i]->valuep(), VhdlAggregate)) {
                VL_DO_DANGLING(lastIdxp->deleteTree(), lastIdxp);
                fromp = new AstSelBit{flp, srcp->cloneTree(false),
                                      stringIndex(typep, firstIdxp, flp)};
            } else if (isString) {
                // A run of characters is a substring: Verilator's string is
                // characters, not bits, so a part select of it means nothing
                fromp = new AstSubstrN{flp, srcp->cloneTree(false),
                                       stringIndex(typep, firstIdxp, flp),
                                       stringIndex(typep, lastIdxp, flp)};
            } else {
                fromp = new AstSelExtract{flp, srcp->cloneTree(false), firstIdxp, lastIdxp};
            }
            index += span * step;
            used += span;
            // A name that stands for one element takes the element's type; a
            // name that stands for a run of them takes the array's own
            outp = AstNode::addNextNull(
                outp, oneTarget(assocs[i]->valuep(), fromp, span == 1 ? elemp : typep,
                                signalAssign, sequential, flp));
        }
        VL_DO_DANGLING(srcp->deleteTree(), srcp);
        if (length >= 0 && used != length) {
            err(flp, "The aggregate on the left covers " + std::to_string(used)
                         + " element(s), but the value has " + std::to_string(length));
        }
        return outp;
    }

    // One name of an aggregate target, which may be an aggregate itself
    AstNode* oneTarget(AstNodeVhdlExpr* namep, AstNodeExpr* fromp, VhdlType* typep,
                       bool signalAssign, bool sequential, FileLine* flp) {
        if (AstVhdlAggregate* const aggp = VN_CAST(namep, VhdlAggregate)) {
            return spreadAggregate(aggp, fromp, typep, signalAssign, sequential, flp);
        }
        AstNodeExpr* const onep = lowerExpr(namep);
        if (!onep) {
            VL_DO_DANGLING(fromp->deleteTree(), fromp);
            return nullptr;
        }
        return assignOf(onep, fromp, signalAssign, sequential, flp);
    }

    // Which element of the record each association of the target names
    bool recordTargetOrder(const std::vector<AstVhdlElemAssoc*>& assocs,
                           const std::vector<VhdlSymbol*>& elems,
                           std::vector<AstVhdlElemAssoc*>& byPos, FileLine* flp) {
        size_t pos = 0;
        for (AstVhdlElemAssoc* const ap : assocs) {
            if (!ap->choicesp()) {
                if (pos >= elems.size()) break;
                byPos[pos++] = ap;
                continue;
            }
            for (AstVhdlChoice* cp = ap->choicesp(); cp; cp = VN_AS(cp->nextp(), VhdlChoice)) {
                const AstVhdlName* const namep = VN_CAST(cp->exprp(), VhdlName);
                if (!namep) {
                    unsup(ap->fileline(), "aggregate target with a named element");
                    return false;
                }
                bool matched = false;
                for (size_t i = 0; i < elems.size(); ++i) {
                    if (elems[i]->name() != namep->name()) continue;
                    byPos[i] = ap;
                    matched = true;
                    break;
                }
                if (!matched) {
                    unsup(ap->fileline(), "aggregate target with a named element");
                    return false;
                }
            }
        }
        for (size_t i = 0; i < elems.size(); ++i) {
            if (byPos[i]) continue;
            err(flp, "The aggregate on the left has no element for '" + elems[i]->name() + "'");
            return false;
        }
        return true;
    }

    // "1 => a" and "4 to 8 => k" say where in the value the name takes its
    // part from, and how much of it. Both have to be static.
    bool targetChoiceSpan(AstVhdlElemAssoc* assocp, int64_t step, int64_t& index,
                          int64_t& span) {
        if (assocp->choicesp()->nextp()) {
            unsup(assocp->fileline(), "aggregate target with a named element");
            return false;
        }
        AstVhdlChoice* const choicep = assocp->choicesp();
        if (choicep->isOthers()) {
            unsup(assocp->fileline(), "aggregate target with a named element");
            return false;
        }
        if (AstVhdlRange* const rangep = choicep->rangep()) {
            const VhdlValue lo = V3VhdlEval::eval(rangep->leftp());
            const VhdlValue hi = V3VhdlEval::eval(rangep->rightp());
            const bool up = rangep->dir() == VhdlDir::TO;
            if (!lo.isInt() || !hi.isInt() || rangep->dirUnknown()) {
                unsup(assocp->fileline(), "aggregate target with a named element");
                return false;
            }
            if ((step < 0) == up) {
                // The choice runs the other way from the value's own index
                unsup(assocp->fileline(), "aggregate target with a named element");
                return false;
            }
            index = lo.toInt();
            span = (up ? hi.toInt() - lo.toInt() : lo.toInt() - hi.toInt()) + 1;
            if (span < 0) span = 0;
            return true;
        }
        AstNodeVhdlExpr* const atp = choicep->valuep();
        const VhdlValue at = atp ? V3VhdlEval::eval(atp) : VhdlValue{};
        if (!at.isInt()) {
            unsup(assocp->fileline(), "aggregate target with a named element");
            return false;
        }
        index = at.toInt();
        span = 1;
        return true;
    }

    static AstNode* assignOf(AstNodeExpr* lhsp, AstNodeExpr* rhsp, bool signalAssign,
                             bool sequential, FileLine* flp) {
        if (!signalAssign) return new AstAssign{flp, lhsp, rhsp};
        if (sequential) return new AstAssignDly{flp, lhsp, rhsp};
        return new AstAssignW{flp, lhsp, rhsp};
    }

    // "s(1 to 5) := t" and "s(3) := c" write part of a STRING. Verilator's
    // string is characters rather than bits, so a part select of it means
    // nothing: the whole string is rebuilt around the part that changed.
    // Returns null where the target is not part of a STRING.
    static bool isStringPart(const AstNodeVhdlExpr* targetp) {
        const AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall);
        if (!callp || !callp->prefixp()) return false;
        const VhdlType* const strTypep = callp->prefixp()->vhdlTypep();
        return strTypep && isStringType(strTypep);
    }

    AstNode* lowerStringPartAssign(AstNodeVhdlExpr* targetp, AstNodeExpr* valuep,
                                   bool signalAssign, bool sequential, FileLine* flp) {
        if (!isStringPart(targetp)) return nullptr;
        AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall);
        const VhdlType* const strTypep = callp->prefixp()->vhdlTypep();
        AstNodeExpr* const wholep = lowerExpr(callp->prefixp());
        if (!wholep) return nullptr;
        AstNodeExpr* outp = nullptr;
        if (AstVhdlRange* const rp = VN_CAST(callp->argsp(), VhdlRange)) {
            AstNodeExpr* leftp = nullptr;
            AstNodeExpr* rightp = nullptr;
            bool ascending = true;
            bool dirKnown = true;
            if (!rangeParts(rp, leftp, rightp, ascending, dirKnown) || !leftp || !rightp) {
                VL_DO_DANGLING(wholep->deleteTree(), wholep);
                return nullptr;
            }
            AstNodeExpr* const lop = stringIndex(strTypep, leftp, flp);
            AstNodeExpr* const hip = stringIndex(strTypep, rightp, flp);
            AstNodeExpr* const headp
                = new AstSubstrN{flp, wholep->cloneTree(false),
                                 new AstConst{flp, AstConst::Signed32{}, 0},
                                 new AstSub{flp, lop->cloneTree(false),
                                            new AstConst{flp, AstConst::Signed32{}, 1}}};
            AstNodeExpr* const tailp
                = new AstSubstrN{flp, wholep->cloneTree(false),
                                 new AstAdd{flp, hip->cloneTree(false),
                                            new AstConst{flp, AstConst::Signed32{}, 1}},
                                 new AstSub{flp, new AstLenN{flp, wholep->cloneTree(false)},
                                            new AstConst{flp, AstConst::Signed32{}, 1}}};
            VL_DO_DANGLING(lop->deleteTree(), lop);
            VL_DO_DANGLING(hip->deleteTree(), hip);
            outp = new AstConcatN{flp, new AstConcatN{flp, headp, valuep}, tailp};
        } else {
            AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
            AstNodeVhdlExpr* const idxValp
                = asp ? asp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
            if (!idxValp || (callp->argsp() && callp->argsp()->nextp())) {
                VL_DO_DANGLING(wholep->deleteTree(), wholep);
                return nullptr;
            }
            AstNodeExpr* const idxp = lowerExpr(idxValp);
            if (!idxp) {
                VL_DO_DANGLING(wholep->deleteTree(), wholep);
                return nullptr;
            }
            outp = new AstPutcN{flp, wholep->cloneTree(false),
                                stringIndex(strTypep, idxp, flp), valuep};
        }
        AstNodeExpr* const lhsp = lowerDriveTarget(callp->prefixp());
        VL_DO_DANGLING(wholep->deleteTree(), wholep);
        if (!lhsp) {
            VL_DO_DANGLING(outp->deleteTree(), outp);
            return nullptr;
        }
        if (!signalAssign) return new AstAssign{flp, lhsp, outp};
        if (sequential) return new AstAssignDly{flp, lhsp, outp};
        return new AstAssignW{flp, lhsp, outp};
    }

    // A variable assignment takes effect at once (10.6)
    AstNode* lowerVarAssign(AstVhdlVarAssign* nodep) {
        FileLine* const flp = nodep->fileline();
        // "v := a when c else b" and "with s select v := ..." are defined as
        // the if and case statements they stand for (10.6.3, 10.6.4), so each
        // alternative's value is assigned by the branch that picks it
        if (nodep->kind() != VhdlAssignKind::SIMPLE) {
            std::vector<AstVhdlAlternative*> alts;
            for (AstNode* p = nodep->altsp(); p; p = p->nextp()) {
                alts.push_back(VN_AS(p, VhdlAlternative));
            }
            AstNode* outp = nullptr;
            for (size_t i = alts.size(); i-- > 0;) {
                AstVhdlWaveform* const wfp = alts[i]->wavesp();
                AstNode* const thenp = wfp ? lowerVarValue(nodep, wfp->valuep(), flp) : nullptr;
                AstNodeExpr* testp = nullptr;
                if (nodep->kind() == VhdlAssignKind::CONDITIONAL) {
                    if (alts[i]->condp()) testp = lowerCondition(alts[i]->condp());
                } else if (nodep->selectorp()) {
                    testp = choiceTest(alts[i]->choicesp(), nodep->selectorp(),
                                       nodep->matching());
                }
                if (!testp) {
                    outp = thenp;  // The trailing "else", or "when others"
                    continue;
                }
                outp = new AstIf{alts[i]->fileline(), testp, thenp, outp};
            }
            return outp;
        }
        return lowerVarValue(nodep, nodep->valuep(), flp);
    }

    // The assignment one value stands for, which is what each branch of a
    // conditional or selected variable assignment reduces to
    AstNode* lowerVarValue(AstVhdlVarAssign* nodep, AstNodeVhdlExpr* valuep, FileLine* flp) {
        if (AstVhdlAggregate* const aggp = VN_CAST(nodep->targetp(), VhdlAggregate)) {
            if (!valuep) return nullptr;
            return lowerAggregateTarget(aggp, valuep, false, false, flp);
        }
        if (valuep && isStringPart(nodep->targetp())) {
            AstNodeExpr* const strValp = lowerExpr(valuep);
            if (strValp) {
                if (AstNode* const outp = lowerStringPartAssign(nodep->targetp(), strValp,
                                                                false, false, flp)) {
                    return outp;
                }
                VL_DO_DANGLING(strValp->deleteTree(), strValp);
            }
        }
        if (valuep && isDynTarget(nodep->targetp())) {
            return lowerDynCopy(nodep->targetp(), valuep, flp);
        }
        // A value that carries its own bounds put into an object that has
        // its own: the elements are copied across, and the lengths have to
        // match, which is a check only running can make (10.5.2.1)
        if (valuep && isDynValue(valuep) && !isDynObj(nodep->targetp())) {
            if (AstNode* const outp = dynCarrierToFixed(nodep->targetp(), valuep, flp)) {
                return outp;
            }
        }
        // The other way about: an object that carries its bounds given a
        // value that has its own
        if (valuep && isDynObj(nodep->targetp()) && !isDynValue(valuep)) {
            AstNodeExpr* const tgtp = lowerDriveTarget(nodep->targetp());
            if (tgtp) return dynCarrierAssign(tgtp, valuep, flp);
        }
        AstNodeExpr* const lhsp = lowerDriveTarget(nodep->targetp());
        AstNodeExpr* const rhsp = valuep ? lowerExpr(valuep) : nullptr;
        if (!lhsp || !rhsp) return nullptr;
        return new AstAssign{flp, lhsp, rhsp};
    }

    // Whether an assignment writes a whole array, or a slice of one, that an
    // access value points at. Writing one element is an ordinary assignment;
    // writing many is a copy, since how many there are is only known while
    // running.
    bool isDynTarget(AstNodeVhdlExpr* targetp) {
        const DynAccess* dynp = nullptr;
        if (AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall)) {
            if (!VN_IS(callp->argsp(), VhdlRange)) return false;
            return dynAccessObj(callp->prefixp(), dynp) != nullptr;
        }
        AstVhdlSelected* const selp = VN_CAST(targetp, VhdlSelected);
        return selp && selp->isAll() && dynAccessObj(selp, dynp) != nullptr;
    }

    // "p.all := v" and "p(a to b) := v", where what p points at carries its
    // own bounds: the elements are copied one at a time, since how many
    // there are is only known while running. Slices of one same object that
    // overlap are copied in order rather than through a temporary, which the
    // LRM would have them go through (10.5.2.2).
    AstNode* lowerDynCopy(AstNodeVhdlExpr* targetp, AstNodeVhdlExpr* valuep, FileLine* flp) {
        const DynAccess* dynp = nullptr;
        AstVhdlRange* rangep = nullptr;
        AstNodeVhdlExpr* objNodep = nullptr;
        if (AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall)) {
            rangep = VN_CAST(callp->argsp(), VhdlRange);
            objNodep = dynAccessObj(callp->prefixp(), dynp);
        } else {
            objNodep = dynAccessObj(VN_AS(targetp, VhdlSelected), dynp);
        }
        AstNodeExpr* const dstp = objNodep ? lowerExpr(objNodep) : nullptr;
        if (!dstp) return nullptr;
        if (!repeatable(dstp)) {
            VL_DO_DANGLING(dstp->deleteTree(), dstp);
            unsup(flp, "assignment to an access value that is not a name");
            return nullptr;
        }
        AstNodeExpr* countp = nullptr;
        AstNodeExpr* basep = nullptr;
        if (rangep) {
            if (!rangep->leftp() || !rangep->rightp()) {
                VL_DO_DANGLING(dstp->deleteTree(), dstp);
                unsup(flp, "slice by an attribute range");
                return nullptr;
            }
            AstNodeExpr* const leftp = lowerExpr(rangep->leftp());
            AstNodeExpr* const rightp = lowerExpr(rangep->rightp());
            if (!leftp || !rightp) {
                VL_DO_DANGLING(dstp->deleteTree(), dstp);
                if (leftp) VL_DO_DANGLING(leftp->deleteTree(), leftp);
                if (rightp) VL_DO_DANGLING(rightp->deleteTree(), rightp);
                return nullptr;
            }
            const bool ascending = rangep->dir() != VhdlDir::DOWNTO;
            AstNodeExpr* const spanp
                = ascending ? new AstSub{flp, rightp, leftp->cloneTree(false)}
                            : new AstSub{flp, leftp->cloneTree(false), rightp};
            countp = new AstAdd{flp, spanp, new AstConst{flp, AstConst::Signed32{}, 1}};
            basep = dynIndex(dynp, dstp->cloneTree(false), leftp, flp);
        } else {
            countp = dynLength(dynp, dstp, flp);
            basep = new AstConst{flp, AstConst::Signed32{}, 0};
        }
        AstVar* const countVarp = newTempVar("__Vdyncnt", newIntDType(flp), flp);
        AstVar* const baseVarp = newTempVar("__Vdynbase", newIntDType(flp), flp);
        AstVar* const idxVarp = newTempVar("__Vdyni", newIntDType(flp), flp);
        idxVarp->usedLoopIdx(true);
        AstNodeExpr* const elemp
            = dynSourceElem(valuep, new AstVarRef{flp, idxVarp, VAccess::READ}, flp);
        if (!elemp) {
            VL_DO_DANGLING(dstp->deleteTree(), dstp);
            VL_DO_DANGLING(countp->deleteTree(), countp);
            VL_DO_DANGLING(basep->deleteTree(), basep);
            return nullptr;
        }
        AstNodeExpr* const dstElemp = new AstSelBit{
            flp, memberOf(dstp, dynp->datap->name(), flp),
            new AstAdd{flp, new AstVarRef{flp, baseVarp, VAccess::READ},
                       new AstVarRef{flp, idxVarp, VAccess::READ}}};
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, new AstVarRef{flp, idxVarp, VAccess::READ},
                       new AstVarRef{flp, countVarp, VAccess::READ}}});
        loopp->addStmtsp(new AstAssign{flp, dstElemp, elemp});
        loopp->addContsp(new AstAssign{
            flp, new AstVarRef{flp, idxVarp, VAccess::WRITE},
            new AstAdd{flp, new AstVarRef{flp, idxVarp, VAccess::READ},
                       new AstConst{flp, AstConst::Signed32{}, 1}}});
        AstNode* outp
            = new AstAssign{flp, new AstVarRef{flp, countVarp, VAccess::WRITE}, countp};
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(
                      new AstAssign{flp, new AstVarRef{flp, baseVarp, VAccess::WRITE}, basep}));
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(
                      new AstAssign{flp, new AstVarRef{flp, idxVarp, VAccess::WRITE},
                                    new AstConst{flp, AstConst::Signed32{}, 0}}));
        return AstNode::addNext(outp, static_cast<AstNode*>(loopp));
    }

    // One element of the value being copied into an array an access value
    // points at, at the offset the loop has reached. Takes ownership of the
    // offset expression.
    AstNodeExpr* dynSourceElem(AstNodeVhdlExpr* valuep, AstNodeExpr* offsetp, FileLine* flp) {
        const DynAccess* dynp = nullptr;
        AstVhdlRange* rangep = nullptr;
        AstNodeVhdlExpr* objNodep = nullptr;
        if (AstVhdlCall* const callp = VN_CAST(valuep, VhdlCall)) {
            if ((rangep = VN_CAST(callp->argsp(), VhdlRange))) {
                objNodep = dynAccessObj(callp->prefixp(), dynp);
            }
        }
        if (!objNodep && !rangep) objNodep = dynAccessObj(valuep, dynp);
        if (objNodep) {
            AstNodeExpr* const srcp = lowerExpr(objNodep);
            if (!srcp) {
                VL_DO_DANGLING(offsetp->deleteTree(), offsetp);
                return nullptr;
            }
            if (!repeatable(srcp)) {
                VL_DO_DANGLING(srcp->deleteTree(), srcp);
                VL_DO_DANGLING(offsetp->deleteTree(), offsetp);
                unsup(flp, "copy from an access value that is not a name");
                return nullptr;
            }
            AstNodeExpr* startp = nullptr;
            if (rangep && rangep->leftp()) {
                AstNodeExpr* const leftp = lowerExpr(rangep->leftp());
                if (!leftp) {
                    VL_DO_DANGLING(srcp->deleteTree(), srcp);
                    VL_DO_DANGLING(offsetp->deleteTree(), offsetp);
                    return nullptr;
                }
                startp = dynIndex(dynp, srcp->cloneTree(false), leftp, flp);
            }
            AstNodeExpr* const idxp
                = startp ? static_cast<AstNodeExpr*>(new AstAdd{flp, startp, offsetp}) : offsetp;
            return new AstSelBit{flp, memberOf(srcp, dynp->datap->name(), flp), idxp};
        }
        // A value whose own bounds are known, indexed on its own range. A
        // slice keeps the index values of what it is a slice of (8.5), so it
        // is the object itself that is indexed, not the slice of it.
        const VhdlType* const typep = valuep->vhdlTypep();
        AstNodeVhdlExpr* wholep = valuep;
        if (AstVhdlCall* const callp = VN_CAST(valuep, VhdlCall)) {
            if (VN_IS(callp->argsp(), VhdlRange)) wholep = callp->prefixp();
        }
        if (typep && typep->isConstrainedArray() && typep->dimensions() == 1
            && typep->indexTypes()[0] && typep->indexTypes()[0]->bounded()) {
            AstNodeExpr* const srcp = lowerExpr(wholep);
            if (!srcp) {
                VL_DO_DANGLING(offsetp->deleteTree(), offsetp);
                return nullptr;
            }
            if (repeatable(srcp)) {
                const VhdlType* const idxTypep = typep->indexTypes()[0];
                AstNodeExpr* const leftp
                    = new AstConst{flp, AstConst::Signed32{},
                                   static_cast<int32_t>(idxTypep->left())};
                AstNodeExpr* const idxp
                    = idxTypep->ascending()
                          ? static_cast<AstNodeExpr*>(new AstAdd{flp, leftp, offsetp})
                          : static_cast<AstNodeExpr*>(new AstSub{flp, leftp, offsetp});
                return new AstSelBit{flp, srcp, idxp};
            }
            VL_DO_DANGLING(srcp->deleteTree(), srcp);
        }
        VL_DO_DANGLING(offsetp->deleteTree(), offsetp);
        unsup(flp, "value copied into an unconstrained array through an access value");
        return nullptr;
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
        // A choice may name 'Z', or '-' in a matching case, and those reach
        // the emitted case as the Verilog values they stand for. Verilator
        // reads such a constant as a mistake for casex or casez, which is
        // sound advice for Verilog but not here: VHDL has only the one
        // spelling of each, and the analyzer has already checked the choices
        // against the expression's subtype (10.9).
        const auto quietXZ = [&](AstNodeExpr* exprp) {
            AstConst* const constp = VN_CAST(exprp, Const);
            if (!constp || !constp->num().isAnyXZ()) return exprp;
            FileLine* const qflp = new FileLine{constp->fileline()};
            qflp->warnOff(V3ErrorCode::CASEWITHX, true);
            constp->fileline(qflp);
            return exprp;
        };
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
                    // A choice naming a value this runtime cannot hold can
                    // never be taken, and left in it would match the zero the
                    // value is written as -- so it is dropped, which is the
                    // answer the equality operator gives for the same
                    // comparison. A matching case is the exception: there a
                    // '-' is the "any value" of 10.9, which is what Verilog's
                    // casex reads an X as, so it is passed on as one.
                    if (nodep->matching() && dontCareLiteral(chp->valuep())
                        && isScalarLogic(chp->valuep()->vhdlTypep())) {
                        V3Number num{chp->fileline(), 1, 0};
                        num.setAllBitsX();
                        condsp = AstNode::addNextNull(
                            condsp, quietXZ(new AstConst{chp->fileline(), num}));
                        continue;
                    }
                    if (!nodep->matching() && metavalueChoice(chp->valuep())) continue;
                    if (AstNodeExpr* const cvp = lowerExpr(chp->valuep())) {
                        condsp = AstNode::addNextNull(condsp, quietXZ(cvp));
                    }
                }
            }
            AstNode* const stmtsp = lowerStmts(altp->stmtsp());
            if (!others && !condsp) {
                // Every choice named a value this runtime cannot hold, so
                // nothing reaches these statements
                if (stmtsp) VL_DO_DANGLING(stmtsp->deleteTree(), stmtsp);
                continue;
            }
            itemsp = AstNode::addNextNull(
                itemsp, new AstCaseItem{flp, others ? nullptr : condsp, stmtsp});
        }
        // "case?" matches with "?=", where a '-' in a choice stands for any
        // value (10.9). That is what Verilog's casex does with an x, and '-'
        // is carried as x here.
        FileLine* cflp = flp;
        if (nodep->matching()) {
            // Verilog's casez is the one to prefer there, but VHDL has only
            // this one spelling and it is what the source wrote
            cflp = new FileLine{flp};
            cflp->warnOff(V3ErrorCode::CASEX, true);
        }
        AstCase* const casep = new AstCase{
            cflp, nodep->matching() ? VCaseType::CT_CASEX : VCaseType::CT_CASE, exprp, itemsp};
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
        const DynAccess* dynLoopp = nullptr;
        if (AstVhdlRange* const rp = VN_CAST(nodep->rangep(), VhdlRange)) {
            if (rp->leftp() && rp->rightp()) {
                initp = lowerExpr(rp->leftp());
                limitp = lowerExpr(rp->rightp());
                ascending = rp->dir() == VhdlDir::TO;
            } else if (AstVhdlAttrName* const attrp = VN_CAST(rp->attrp(), VhdlAttrName)) {
                // "for i in v'range" over an object whose bounds are not
                // static, which is how a width-generic design is written
                const std::string& name = attrp->name();
                // A STRING whose length its subtype does not fix -- what a
                // LINE points at, or an unconstrained parameter given one --
                // runs from 1 to however long the value is
                const VhdlType* strTypep
                    = attrp->prefixp() ? attrp->prefixp()->vhdlTypep() : nullptr;
                if (strTypep && strTypep->base()->kind() == VhdlTypeKind::ACCESS
                    && strTypep->base()->designatedp()) {
                    strTypep = strTypep->base()->designatedp();
                }
                if ((name == "range" || name == "reverse_range") && strTypep
                    && isStringType(strTypep) && !strTypep->isConstrainedArray()) {
                    if (AstNodeExpr* const strp = lowerExpr(attrp->prefixp())) {
                        initp = new AstConst{flp, AstConst::Signed32{}, 1};
                        limitp = new AstLenN{flp, strp};
                        ascending = name == "range";
                        if (!ascending) std::swap(initp, limitp);
                    }
                } else if ((name == "range" || name == "reverse_range")
                           && dynAccessObj(attrp->prefixp(), dynLoopp)) {
                    // The array an access value points at carries its own
                    // bounds, so the loop reads them from the object
                    AstNodeExpr* const objp = lowerExpr(
                        dynAccessObj(attrp->prefixp(), dynLoopp));
                    if (objp && repeatable(objp)) {
                        initp = dynLeft(dynLoopp, objp, flp);
                        limitp = dynRight(dynLoopp, objp, flp);
                        ascp = dynAscending(dynLoopp, objp, flp);
                        ascending = true;
                        if (name == "reverse_range") {
                            std::swap(initp, limitp);
                            ascp = new AstLogNot{flp, ascp};
                        }
                    } else {
                        unsup(flp, "loop over the range of an access value that is not a name");
                    }
                    if (objp) VL_DO_DANGLING(objp->deleteTree(), objp);
                } else if ((name == "range" || name == "reverse_range")
                           && isDynObj(attrp->prefixp())) {
                    AstNodeExpr* const carrp = lowerExpr(attrp->prefixp());
                    if (carrp && repeatable(carrp)) {
                        initp = dynCarrierAttr(carrp->cloneTree(false), "left", flp);
                        limitp = dynCarrierAttr(carrp->cloneTree(false), "right", flp);
                        ascp = dynCarrierAscending(carrp->cloneTree(false), flp);
                        ascending = true;
                        if (name == "reverse_range") {
                            std::swap(initp, limitp);
                            ascp = new AstLogNot{flp, ascp};
                        }
                    } else {
                        unsup(flp, "loop over the range of a value that is not a name");
                    }
                    if (carrp) VL_DO_DANGLING(carrp->deleteTree(), carrp);
                } else if (name == "range" || name == "reverse_range") {
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
        // A loop over an enumeration is the one that cannot be written as a
        // test before each iteration: the parameter is kept in just enough
        // bits for the type's literals, so stepping past the last one wraps
        // round to the first and "n <= right" is true again. Such a loop ends
        // by recognising its last iteration instead. Every other loop keeps
        // the ordinary shape, which is what Verilator's unroller looks for.
        const bool endTest = rangep->base()->kind() == VhdlTypeKind::ENUM;
        // Whether the range runs at all, which is asked once, before the
        // first iteration: a null range runs none (10.10). Where the
        // direction arrives with the instance, both ways are written out and
        // the one that applies folds away with it.
        AstNodeExpr* const donep
            = endTest ? new AstNeq{flp, new AstVarRef{flp, varp, VAccess::READ},
                                   limitp->cloneTree(false)}
                      : nullptr;
        AstNodeExpr* enterp = nullptr;
        bool runsAtAll = true;
        if (endTest && !ascp && VN_IS(initp, Const) && VN_IS(limitp, Const)) {
            // Both bounds are numbers, so whether the range is null is known
            // here; asking again while running would be a comparison whose
            // answer never changes
            const int64_t left = static_cast<int64_t>(VN_AS(initp, Const)->toSInt());
            const int64_t right = static_cast<int64_t>(VN_AS(limitp, Const)->toSInt());
            runsAtAll = ascending ? left <= right : left >= right;
            VL_DO_DANGLING(limitp->deleteTree(), limitp);
        } else {
            enterp = ascp ? static_cast<AstNodeExpr*>(new AstCond{
                         flp, ascp->cloneTree(false),
                         new AstLteS{flp, new AstVarRef{flp, varp, VAccess::READ},
                                     limitp->cloneTree(false)},
                         new AstGteS{flp, new AstVarRef{flp, varp, VAccess::READ}, limitp}})
                          : ascending
                                ? static_cast<AstNodeExpr*>(new AstLteS{
                                      flp, new AstVarRef{flp, varp, VAccess::READ}, limitp})
                                : static_cast<AstNodeExpr*>(new AstGteS{
                                      flp, new AstVarRef{flp, varp, VAccess::READ}, limitp});
        }
        if (!endTest) loopp->addStmtsp(new AstLoopTest{flp, loopp, enterp});
        m_loops.emplace_back(VhdlLoopFrame{nodep->label(), nullptr, nullptr});
        loopp->addStmtsp(lowerLoopBody(nodep));
        if (endTest) loopp->addStmtsp(new AstLoopTest{flp, loopp, donep});
        AstNodeExpr* stepp = nullptr;
        if (ascp) {
            stepp = new AstAdd{flp, new AstVarRef{flp, varp, VAccess::READ},
                               new AstCond{flp, ascp,
                                           new AstConst{flp, AstConst::Signed32{}, 1},
                                           new AstConst{flp, AstConst::Signed32{}, -1}}};
        } else if (ascending) {
            // The step is as wide as the parameter, which is wide enough:
            // it never has to hold a value the range does not
            stepp = new AstAdd{flp, new AstVarRef{flp, varp, VAccess::READ},
                               new AstConst{flp, AstConst::Unsized32{}, 1}};
        } else {
            stepp = new AstSub{flp, new AstVarRef{flp, varp, VAccess::READ},
                               new AstConst{flp, AstConst::Unsized32{}, 1}};
        }
        loopp->addContsp(
            new AstAssign{flp, new AstVarRef{flp, varp, VAccess::WRITE}, stepp});
        AstNode* const wrappedp = wrapLoopExit(loopp);
        m_loops.pop_back();
        AstNode* outp
            = new AstAssign{flp, new AstVarRef{flp, varp, VAccess::WRITE}, initp};
        if (preloadp) outp = AstNode::addNext(static_cast<AstNode*>(preloadp), outp);
        if (!runsAtAll) {
            VL_DO_DANGLING(wrappedp->deleteTree(), wrappedp);
            return outp;
        }
        if (!endTest || !enterp) return AstNode::addNext(outp, wrappedp);
        return AstNode::addNext(outp, new AstIf{flp, enterp, wrappedp, nullptr});
    }


    AstNode* lowerProcCall(AstVhdlProcCall* nodep) {
        FileLine* const flp = nodep->fileline();
        AstVhdlCall* const callp = VN_CAST(nodep->callp(), VhdlCall);
        AstNodeVhdlExpr* const namep = callp ? callp->prefixp() : nodep->callp();
        // An alias denotes the procedure it was declared for (6.6.1)
        VhdlSymbol* const namedp = callp ? callp->vhdlSymp() : nodep->callp()->vhdlSymp();
        VhdlSymbol* const symp = namedp ? namedp->resolved() : nullptr;
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
            const VhdlType* protTypep = nullptr;
            if (AstNodeExpr* const instp
                = selp ? protectedInstance(selp->prefixp(), &protTypep) : nullptr) {
                AstMethodCall* const mcallp
                    = new AstMethodCall{flp, instp, methodName(symp, protTypep), nullptr};
                addCallArgs(mcallp, callp);
                return new AstStmtExpr{flp, mcallp};
            }
        }
        // A procedure with a signal parameter drives the caller's own signal,
        // so it gets a copy bound to the signals this call passes (4.2.2.3)
        if (symp && callp && hasSignalParam(symp)) {
            if (AstNode* const outp = lowerSignalProcCall(symp, callp, flp)) return outp;
            // Passing such a signal by value would compile and quietly throw
            // the drives away, which is worse than saying so
            if (drivesSignalParam(symp)) {
                unsup(flp, "call to '" + symp->name()
                               + "', whose signal parameter is not a signal this call can name");
                return nullptr;
            }
        }
        auto it = symp ? m_funcs.find(symp) : m_funcs.end();
        if (it == m_funcs.end() && symp && emitOnDemand(symp)) it = m_funcs.find(symp);
        if (it == m_funcs.end() || !VN_IS(it->second, Task)) {
            unsup(flp, "procedure call to '"
                           + (symp ? symp->name() : std::string{"<name>"}) + "'"
                           + whyNotEmitted(symp));
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

    // Whether any parameter is a signal the subprogram may drive. Such a
    // formal has to *be* the caller's own signal: handing over its value
    // instead would run the body against a copy and throw the drives away.
    static bool drivesSignalParam(VhdlSymbol* symp) {
        AstVhdlSubprogDecl* const declp = declOf(symp);
        if (!declp) return false;
        for (AstNode* p = firstParamOf(declp); p; p = p->nextp()) {
            const AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            if (ifp->kind() != VhdlIfaceKind::SIGNAL) continue;
            if (ifp->mode() != VhdlMode::IN) return true;
        }
        return false;
    }

    // A copy of a subprogram's body made for one call site, with each
    // signal-class formal bound to the signal the call passed (4.2.2.3): an
    // assignment inside it drives that signal, and the formal's history
    // attributes answer from that signal's record. The actuals that are not
    // signals are left in `passed` for the caller to hand over as usual.
    AstNodeFTask* signalCopyFor(VhdlSymbol* symp, AstVhdlCall* callp,
                                std::vector<AstVhdlExprPair>& passed) {
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
        std::map<const VhdlSymbol*, VhdlSymbol*> symBind;
        std::map<const VhdlSymbol*, std::map<std::string, AstNodeVhdlExpr*>> elemBind;
        std::map<const VhdlSymbol*, DynSigBind> dynBind;
        std::string key = symp->name();
        for (size_t i = 0; i < formals.size(); ++i) {
            AstNodeVhdlExpr* actualp = nullptr;
            // The elements of this formal that the call named one at a time
            std::map<std::string, AstNodeVhdlExpr*> parts;
            for (size_t a = 0; a < assocs.size(); ++a) {
                const AstVhdlName* const namep
                    = assocs[a]->formalp() ? VN_CAST(assocs[a]->formalp(), VhdlName) : nullptr;
                if (namep) {
                    if (namep->name() == formals[i]->name()) actualp = assocs[a]->actualp();
                    continue;
                }
                if (assocs[a]->formalp()) {
                    std::string root;
                    std::string elem;
                    if (individualFormal(assocs[a]->formalp(), root, elem)
                        && root == formals[i]->name()) {
                        parts.emplace(elem, assocs[a]->actualp());
                    }
                    continue;
                }
                if (a == i) actualp = assocs[a]->actualp();
            }
            if (!parts.empty()) {
                // Each element is a signal of the design in its own right, so
                // the formal gets no object and every reference to one of its
                // elements lowers to that element's actual
                if (formals[i]->kind() != VhdlIfaceKind::SIGNAL) return nullptr;
                std::map<std::string, AstNodeVhdlExpr*> resolved;
                for (const auto& pair : parts) {
                    AstNodeVhdlExpr* const partp = pair.second;
                    const VhdlSymbol* const rootp = partp ? baseSymOf(partp) : nullptr;
                    std::string path;
                    if (!rootp || !m_vars.count(rootp) || m_drivers.count(rootp)) return nullptr;
                    if (!staticSelectKey(partp, path)) return nullptr;
                    resolved.emplace(pair.first, partp);
                    key += "|" + pair.first + "=" + path;
                }
                elemBind.emplace(formals[i]->vhdlSymp(), resolved);
                continue;
            }
            if (formals[i]->kind() != VhdlIfaceKind::SIGNAL) {
                if (actualp) passed.push_back(AstVhdlExprPair{formals[i]->name(), actualp});
                continue;
            }
            // An alias is another name for what it denotes (6.6), so a
            // signal parameter given one is given the object itself
            for (int depth = 0; actualp && depth < 32; ++depth) {
                const VhdlSymbol* const asymp = actualp->vhdlSymp();
                if (!asymp || asymp->kind() != VhdlSymKind::ALIAS) break;
                AstVhdlAliasDecl* const adeclp = VN_CAST(asymp->declp(), VhdlAliasDecl);
                if (!adeclp || !adeclp->targetp()) break;
                actualp = adeclp->targetp();
            }
            // An actual that is itself a formal whose elements were named one
            // at a time carries those elements on into this copy
            if (actualp) {
                const VhdlSymbol* const passedp = actualp->vhdlSymp();
                const auto elIt = passedp ? m_bindElems.find(passedp) : m_bindElems.end();
                if (elIt != m_bindElems.end()) {
                    elemBind.emplace(formals[i]->vhdlSymp(), elIt->second);
                    for (const auto& pair : elIt->second) {
                        std::string path;
                        if (!staticSelectKey(pair.second, path)) return nullptr;
                        key += "|" + pair.first + "=" + path;
                    }
                    continue;
                }
            }
            // A signal actual is a signal of the design, either named plainly
            // or with a part of it selected in a way that is decided now
            VhdlSymbol* const actualSymp = actualp ? actualp->vhdlSymp() : nullptr;
            // A signal a package declares has no variable of this module's
            // own; there is one of it, in the unit every module sees
            if (actualSymp && actualSymp->kind() == VhdlSymKind::SIGNAL
                && !m_vars.count(actualSymp)) {
                pkgSignalVar(actualSymp, actualp->fileline());
            }
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
                if (!staticSelectKey(actualp, path)) {
                    // One element of an array signal, at an index the call
                    // works out: the copy takes the index as a parameter
                    AstNodeVhdlExpr* idxExprp = nullptr;
                    if (!dynamicElementOf(actualp, rootp, idxExprp)) return nullptr;
                    DynSigBind dyn;
                    dyn.arrayp = m_vars[rootp];
                    dyn.argName = "__Vsigidx__" + formals[i]->name();
                    dynBind.emplace(formals[i]->vhdlSymp(), dyn);
                    passed.push_back(AstVhdlExprPair{dyn.argName, idxExprp});
                    key += "|dyn" + std::to_string(reinterpret_cast<uintptr_t>(dyn.arrayp));
                    continue;
                }
                exprBind.emplace(formals[i]->vhdlSymp(), actualp);
                key += "|" + path;
                continue;
            }
            if (m_drivers.count(actualSymp)) return nullptr;  // Resolved: needs its drivers
            bind.emplace(formals[i]->vhdlSymp(), varIt->second);
            // Which signal the formal names, so that inside the copy its
            // history attributes -- 'EVENT, 'LAST_VALUE and the rest -- are
            // answered from the actual's record rather than from nothing
            symBind.emplace(formals[i]->vhdlSymp(), actualSymp->resolved());
            // An assignment inside the copy is a transaction on the actual,
            // so the formal answers from the actual's transaction bit
            const auto acIt = m_actives.find(actualSymp->resolved());
            if (acIt != m_actives.end()) activeBind.emplace(formals[i]->vhdlSymp(), acIt->second);
            key += "|" + std::to_string(reinterpret_cast<uintptr_t>(varIt->second));
        }
        if (bind.empty() && exprBind.empty() && elemBind.empty() && dynBind.empty()) {
            return nullptr;
        }
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
                VL_RESTORER_COPY(m_bindSymbols);
                VL_RESTORER_COPY(m_bindElems);
                VL_RESTORER_COPY(m_bindDynSigs);
                m_bindSignals = bind;
                m_bindExprs = exprBind;
                m_bindSymbols = symBind;
                m_bindElems = elemBind;
                m_bindDynSigs = dynBind;
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
        return copyIt->second;
    }

    // The arguments a call to such a copy still has to hand over
    bool addPassedArgs(AstNodeFTaskRef* refp, const std::vector<AstVhdlExprPair>& passed,
                       FileLine* flp) {
        for (const AstVhdlExprPair& pair : passed) {
            AstNodeExpr* const onep = lowerExpr(pair.exprp);
            if (!onep) return false;
            refp->addArgsp(new AstArg{flp, pair.name, onep});
        }
        return true;
    }

    // Call a procedure whose signal parameters are bound to the actuals
    AstNode* lowerSignalProcCall(VhdlSymbol* symp, AstVhdlCall* callp, FileLine* flp) {
        std::vector<AstVhdlExprPair> passed;
        AstNodeFTask* const taskp = signalCopyFor(symp, callp, passed);
        if (!taskp) return nullptr;
        AstTaskRef* const refp = new AstTaskRef{flp, taskp->name(), nullptr};
        refp->taskp(taskp);
        if (!addPassedArgs(refp, passed, flp)) {
            VL_DO_DANGLING(refp->deleteTree(), refp);
            return nullptr;
        }
        return new AstStmtExpr{flp, refp};
    }

    // The same for a function: "rising_edge(s)" written out rather than
    // recognised, or any function of the design that takes a signal
    AstNodeExpr* lowerSignalFuncCall(VhdlSymbol* symp, AstVhdlCall* callp, FileLine* flp) {
        std::vector<AstVhdlExprPair> passed;
        AstNodeFTask* const taskp = signalCopyFor(symp, callp, passed);
        if (!taskp || !VN_IS(taskp, Func)) return nullptr;
        AstFuncRef* const refp = new AstFuncRef{flp, taskp->name(), nullptr};
        refp->taskp(taskp);
        if (!addPassedArgs(refp, passed, flp)) {
            VL_DO_DANGLING(refp->deleteTree(), refp);
            return nullptr;
        }
        return refp;
    }

    // The sensitivity a list of signal names gives
    AstSenTree* sensitivityOf(AstNode* sensp, FileLine* flp) {
        AstSenItem* itemsp = nullptr;
        for (AstNode* p = sensp; p; p = p->nextp()) {
            AstNodeVhdlExpr* const namep = VN_AS(p, NodeVhdlExpr);
            AstNodeExpr* const exprp = lowerExpr(namep);
            if (!exprp) continue;
            itemsp = AstNode::addNext(
                itemsp, senItemFor(shadowSensitivity(exprp, namep, flp),
                                   isOneBitType(typeOfExpr(namep)), flp));
        }
        return itemsp ? new AstSenTree{flp, itemsp} : nullptr;
    }

    // "wait on s(2 downto 1)" and "process (r.f)" wake on an event on that
    // part of the signal and on no other (10.2, 11.3). The back end is
    // sensitive to whole variables, so a signal of the module follows the
    // part and the process waits on that instead -- which changes exactly
    // when the part does. A whole signal needs none of this.
    AstNodeExpr* shadowSensitivity(AstNodeExpr* exprp, AstNodeVhdlExpr* namep, FileLine* flp) {
        if (VN_IS(exprp, VarRef) || !m_modp) return exprp;
        const VhdlType* const typep = typeOfExpr(namep);
        if (!typep) return exprp;
        AstNodeDType* const dtypep = lowerType(const_cast<VhdlType*>(typep), flp);
        if (!dtypep) return exprp;
        AstVar* const tempp
            = new AstVar{flp, VVarType::VAR, "__Vsens" + std::to_string(++m_tempNum),
                         VFlagChildDType{}, dtypep};
        m_modp->addStmtsp(tempp);
        FileLine* const cflp = new FileLine{flp};
        cflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
        m_modp->addStmtsp(new AstAlways{new AstAssignW{
            cflp, new AstVarRef{cflp, tempp, VAccess::WRITE}, exprp}});
        return new AstVarRef{flp, tempp, VAccess::READ};
    }

    // Every signal a lowered body reads. A signal it only writes is not one
    // of them: a process that woke on what it had just assigned would run
    // again for every assignment it makes.
    AstSenTree* sensitivityOfBody(AstNode* bodyp, FileLine* flp) {
        if (!bodyp) return nullptr;
        std::set<const AstVar*> signals;
        for (const auto& pair : m_vars) {
            if (!pair.first) continue;
            if (pair.first->kind() == VhdlSymKind::SIGNAL
                || pair.first->kind() == VhdlSymKind::PORT) {
                signals.insert(pair.second);
            }
        }
        std::vector<AstVar*> reads;
        // A statement that reads only one element of a signal is sensitive to
        // that element and to nothing else (11.6). Where every read of a
        // signal names the same part of it, and by a fixed index, that part
        // is what the statement waits on.
        std::map<const AstVar*, AstNodeExpr*> partOf;  // Null once two parts differ
        bodyp->foreachAndNext([&](AstVarRef* refp) {
            if (!refp->access().isReadOrRW()) return;
            // What an assignment writes it does not thereby read: a process
            // woken by its own assignment runs again for every one it makes
            if (const AstNodeAssign* const asgp = VN_CAST(refp->backp(), NodeAssign)) {
                if (asgp->lhsp() == refp) return;
            }
            AstVar* const varp = refp->varp();
            if (!varp || !signals.count(varp)) return;
            AstNodeExpr* const partp = staticPartRead(refp);
            const auto it = partOf.find(varp);
            if (it == partOf.end()) {
                partOf.emplace(varp, partp);
            } else if (it->second && (!partp || !it->second->sameTree(partp))) {
                it->second = nullptr;  // Two different parts: the whole of it
            }
            if (std::find(reads.begin(), reads.end(), varp) == reads.end()) {
                reads.push_back(varp);
            }
        });
        AstSenItem* itemsp = nullptr;
        for (AstVar* const varp : reads) {
            AstNodeExpr* const partp = partOf[varp];
            AstNodeExpr* sensp = nullptr;
            bool oneBit = isOneBitVar(varp);
            if (partp) {
                sensp = shadowOfPart(partp->cloneTree(false), flp);
                oneBit = VN_IS(partp, SelBit) ? isOneBitVar(varp) : false;
            }
            if (!sensp) sensp = new AstVarRef{flp, varp, VAccess::READ};
            itemsp = AstNode::addNext(itemsp, senItemFor(sensp, oneBit, flp));
        }
        return itemsp ? new AstSenTree{flp, itemsp} : nullptr;
    }

    // The part of a signal a reference names, where a fixed index says which
    // -- "v(1)" or "v(3 downto 1)". Null where the whole signal is read, or
    // where which part is read is not decided until it runs.
    static AstNodeExpr* staticPartRead(AstVarRef* refp) {
        AstNode* upp = refp->backp();
        AstNodeExpr* outp = nullptr;
        while (upp) {
            if (const AstSelBit* const bitp = VN_CAST(upp, SelBit)) {
                if (bitp->fromp() != (outp ? outp : static_cast<AstNodeExpr*>(refp))) break;
                if (!VN_IS(bitp->bitp(), Const)) return nullptr;
                outp = VN_AS(upp, NodeExpr);
            } else if (const AstSelExtract* const extp = VN_CAST(upp, SelExtract)) {
                if (extp->fromp() != (outp ? outp : static_cast<AstNodeExpr*>(refp))) break;
                if (!VN_IS(extp->leftp(), Const) || !VN_IS(extp->rightp(), Const)) {
                    return nullptr;
                }
                outp = VN_AS(upp, NodeExpr);
            } else {
                break;
            }
            upp = upp->backp();
        }
        return outp;
    }

    // A signal of the module that follows part of another, so that what a
    // statement waits on changes exactly when the part it reads does.
    // Takes ownership of partp.
    AstNodeExpr* shadowOfPart(AstNodeExpr* partp, FileLine* flp) {
        if (!m_modp) {
            VL_DO_DANGLING(partp->deleteTree(), partp);
            return nullptr;
        }
        // As wide as the part it follows, which its fixed indices say
        int bits = 1;
        if (const AstSelExtract* const extp = VN_CAST(partp, SelExtract)) {
            const int left = VN_AS(extp->leftp(), Const)->toSInt();
            const int right = VN_AS(extp->rightp(), Const)->toSInt();
            bits = (left > right ? left - right : right - left) + 1;
        }
        AstVar* const tempp
            = new AstVar{flp, VVarType::VAR, "__Vsens" + std::to_string(++m_tempNum),
                         VFlagChildDType{}, newLogicDType(flp, bits)};
        m_modp->addStmtsp(tempp);
        FileLine* const cflp = new FileLine{flp};
        cflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
        cflp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
        cflp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
        m_modp->addStmtsp(new AstAlways{new AstAssignW{
            cflp, new AstVarRef{cflp, tempp, VAccess::WRITE}, partp}});
        return new AstVarRef{flp, tempp, VAccess::READ};
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
    // A postponed process runs at the end of the last delta of a time step
    // (10.3, 14.7.5.3). Verilator has a postponed region of its own -- what
    // $strobe is written in -- so a bit toggled there says when that moment
    // has come, and a process that has just resumed waits for it.
    AstNode* postponedWait(FileLine* flp) {
        if (!m_postVarp) {
            m_postVarp = new AstVar{flp, VVarType::VAR, "__Vpostponed", VFlagChildDType{},
                                    newLogicDType(flp, 1)};
            m_postVarp->valuep(new AstConst{flp, AstConst::BitFalse{}});
            m_stmtsp = AstNode::addNextNull(m_stmtsp, m_postVarp);
            FileLine* const pflp = new FileLine{flp};
            pflp->warnOff(V3ErrorCode::MULTIDRIVEN, true);
            m_shadowsp = AstNode::addNextNull(
                m_shadowsp,
                static_cast<AstNode*>(new AstAlwaysPostponed{
                    pflp, new AstAssign{pflp, new AstVarRef{pflp, m_postVarp, VAccess::WRITE},
                                        new AstNot{pflp, new AstVarRef{pflp, m_postVarp,
                                                                       VAccess::READ}}}}));
        }
        AstSenTree* const senp = new AstSenTree{
            flp, senItemFor(new AstVarRef{flp, m_postVarp, VAccess::READ}, true, flp)};
        return new AstEventControl{flp, senp, nullptr};
    }
    AstVar* m_postVarp = nullptr;  // The module's postponed-region bit, if used
    bool m_postponed = false;  // Lowering the body of a postponed process

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
        if (nodep->sensp() && nodep->timeoutp()) {
            // "wait on S for T" resumes on whichever comes first (10.2), the
            // same shape as a condition with a timeout
            AstSenTree* const sensp = sensitivityOf(nodep->sensp(), flp);
            AstNodeExpr* const timep = lowerExpr(nodep->timeoutp());
            if (!sensp || !timep) {
                if (sensp) VL_DO_DANGLING(sensp->deleteTree(), sensp);
                if (timep) VL_DO_DANGLING(timep->deleteTree(), timep);
                return nullptr;
            }
            AstFork* const forkp = new AstFork{flp, VJoinType::JOIN_ANY, ""};
            forkp->addForksp(new AstBegin{
                flp, "", new AstEventControl{flp, sensp, nullptr}, false});
            forkp->addForksp(new AstBegin{flp, "", lowerTimeout(timep, flp), false});
            return AstNode::addNext(static_cast<AstNode*>(forkp), new AstDisableFork{flp});
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
        // "read" and "write" are declared implicitly for every file type
        // (5.5.2), and they are not the ones std.textio declares: the first
        // argument there is a LINE, here it is the file itself
        if ((name == "read" || name == "write") && !args.empty()
            && args[0]->vhdlTypep()
            && args[0]->vhdlTypep()->base()->kind() == VhdlTypeKind::FILE) {
            return lowerFileReadWrite(name == "read", args, flp);
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
    // Whether a character may appear in a basic identifier (15.4), which is
    // what bounds the name of an enumeration literal on a line
    static AstNodeExpr* isIdentChar(AstNodeExpr* charp, FileLine* flp) {
        const auto between = [&](uint32_t lo, uint32_t hi) -> AstNodeExpr* {
            return new AstLogAnd{
                flp,
                new AstGteS{flp, charp->cloneTree(false),
                            new AstConst{flp, AstConst::WidthedValue{}, 8, lo}},
                new AstLteS{flp, charp->cloneTree(false),
                            new AstConst{flp, AstConst::WidthedValue{}, 8, hi}}};
        };
        AstNodeExpr* outp = between('a', 'z');
        outp = new AstLogOr{flp, outp, between('A', 'Z')};
        outp = new AstLogOr{flp, outp, between('0', '9')};
        outp = new AstLogOr{flp, outp,
                            new AstEq{flp, charp->cloneTree(false),
                                      new AstConst{flp, AstConst::WidthedValue{}, 8, '_'}}};
        VL_DO_DANGLING(charp->deleteTree(), charp);
        return outp;
    }

    // Whether a character is one this radix can read. A logic type spells
    // its own nine values, in either case; BIT has only two (16.4).
    static AstNodeExpr* isRadixChar(AstNodeExpr* charp, const std::string& name, bool bitOnly,
                                    FileLine* flp) {
        std::string ok;
        if (name == "hread") {
            ok = "0123456789abcdefABCDEF";
        } else if (name == "oread") {
            ok = "01234567";
        } else {
            ok = bitOnly ? "01" : "01uUxXzZwWlLhH-";
        }
        AstNodeExpr* outp = nullptr;
        for (const char c : ok) {
            AstNodeExpr* const eqp
                = new AstEq{flp, charp->cloneTree(false),
                            new AstConst{flp, AstConst::WidthedValue{}, 8,
                                         static_cast<uint32_t>(static_cast<uint8_t>(c))}};
            outp = outp ? static_cast<AstNodeExpr*>(new AstLogOr{flp, outp, eqp}) : eqp;
        }
        VL_DO_DANGLING(charp->deleteTree(), charp);
        return outp;
    }

    static AstNodeExpr* isChar(AstNodeExpr* charp, char c, FileLine* flp) {
        return new AstEq{flp, charp,
                         new AstConst{flp, AstConst::WidthedValue{}, 8,
                                      static_cast<uint32_t>(static_cast<uint8_t>(c))}};
    }

    static AstNodeExpr* isDigitChar(AstNodeExpr* charp, FileLine* flp) {
        AstNodeExpr* const outp = new AstLogAnd{
            flp,
            new AstGteS{flp, charp->cloneTree(false),
                        new AstConst{flp, AstConst::WidthedValue{}, 8, uint32_t{'0'}}},
            new AstLteS{flp, charp->cloneTree(false),
                        new AstConst{flp, AstConst::WidthedValue{}, 8, uint32_t{'9'}}}};
        VL_DO_DANGLING(charp->deleteTree(), charp);
        return outp;
    }

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
    // One element of an array a file holds, indexed the way the array is.
    // The base is already lowered, so an aggregate written out in the call
    // has been put in a variable and this indexes that.
    AstNode* lowerFileElement(bool reading, AstNodeExpr* filep, AstNodeExpr* basep,
                              VhdlType* valTypep, int64_t which, FileLine* flp) {
        const VhdlType* const idxp = valTypep->indexTypes()[0];
        const int64_t index = idxp->ascending() ? idxp->left() + which : idxp->left() - which;
        AstNodeExpr* const elemp
            = new AstSelBit{flp, basep->cloneTree(false),
                            new AstConst{flp, AstConst::Signed32{},
                                         static_cast<int32_t>(index)}};
        VhdlType* const elemTypep
            = valTypep->elementp() ? valTypep->elementp() : valTypep->base()->elementp();
        return lowerFileScalar(reading, filep->cloneTree(false), elemp, elemTypep, flp);
    }

    // One value in a file: a CHARACTER is one byte, and anything else that
    // fits in a machine word is eight bytes, least significant first. The
    // LRM leaves the representation to the implementation and asks only that
    // a design reading its own file back gets what it wrote (5.5.2).
    AstNode* lowerFileScalar(bool reading, AstNodeExpr* filep, AstNodeExpr* valuep,
                             const VhdlType* valTypep, FileLine* flp) {
        const bool isChar = valTypep && V3VhdlStd::characterType()
                            && valTypep->base() == V3VhdlStd::characterType()->base();
        if (isChar) {
            if (!reading) {
                return new AstDisplay{flp, VDisplayType::DT_WRITE, "%c", filep, valuep};
            }
            AstNode* const outp = new AstAssign{flp, valuep, new AstFGetC{flp, filep}};
            return outp;
        }
        static constexpr int BYTES = 8;
        FileLine* const wflp = new FileLine{flp};
        wflp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
        wflp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
        AstVar* const accp = newTempVar(
            "__Vfacc",
            new AstBasicDType{wflp, VBasicDTypeKwd::LOGIC, VSigning::UNSIGNED, VNumRange{63, 0},
                              -1},
            wflp);
        AstNode* outp = nullptr;
        if (!reading) {
            outp = new AstAssign{wflp, new AstVarRef{wflp, accp, VAccess::WRITE}, valuep};
            for (int k = 0; k < BYTES; ++k) {
                outp = AstNode::addNext(
                    outp,
                    static_cast<AstNode*>(new AstDisplay{
                        wflp, VDisplayType::DT_WRITE, "%c", filep->cloneTree(false),
                        new AstAnd{wflp,
                                   new AstShiftR{wflp, new AstVarRef{wflp, accp, VAccess::READ},
                                                 new AstConst{wflp, AstConst::Signed32{}, 8 * k}},
                                   new AstConst{wflp, AstConst::Unsized64{}, 255}}}));
            }
            VL_DO_DANGLING(filep->deleteTree(), filep);
            return outp;
        }
        AstVar* const chp = newTempVar("__Vfrdc", newIntDType(wflp), wflp);
        outp = new AstAssign{wflp, new AstVarRef{wflp, accp, VAccess::WRITE},
                             new AstConst{wflp, AstConst::Unsized64{}, 0}};
        for (int k = 0; k < BYTES; ++k) {
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{wflp, new AstVarRef{wflp, chp, VAccess::WRITE},
                                        new AstFGetC{wflp, filep->cloneTree(false)}}));
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    wflp, new AstVarRef{wflp, accp, VAccess::WRITE},
                    new AstOr{wflp, new AstVarRef{wflp, accp, VAccess::READ},
                              new AstShiftL{wflp,
                                            new AstAnd{wflp,
                                                       new AstVarRef{wflp, chp, VAccess::READ},
                                                       new AstConst{wflp, AstConst::Unsized64{},
                                                                    255}},
                                            new AstConst{wflp, AstConst::Signed32{}, 8 * k}}}}));
        }
        VL_DO_DANGLING(filep->deleteTree(), filep);
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(new AstAssign{
                      wflp, valuep, new AstVarRef{wflp, accp, VAccess::READ}}));
        return outp;
    }

    // The READ and WRITE a file type declares for itself (5.5.2). What the
    // file holds is the values themselves: a file of CHARACTER is bytes, a
    // file of STRING the characters one after another, and an array whose
    // elements do not pack goes element by element.
    AstNode* lowerFileReadWrite(bool reading, std::vector<AstNodeVhdlExpr*>& args,
                                FileLine* flp) {
        if (args.size() < 2 || args.size() > 3) return nullptr;
        VhdlType* const valTypep = args[1]->vhdlTypep();
        const bool isStr = isStringType(valTypep);
        AstNodeExpr* const filep = lowerExpr(args[0]);
        if (!filep) return nullptr;
        if (isStr) {
            AstNodeExpr* const valuep = lowerExpr(args[1]);
            if (!valuep) return nullptr;
            if (!reading) {
                return new AstDisplay{flp, VDisplayType::DT_WRITE, "%s", filep, valuep};
            }
            // "read(f, s, len)" fills as much of s as the file still holds
            // and says how much that was. One character at a time, since
            // $fgets would stop at an end of line and this is not text.
            AstVar* const idxp = newTempVar("__Vfrdi", newIntDType(flp), flp);
            AstVar* const chp = newTempVar("__Vfrdc", newIntDType(flp), flp);
            AstVar* const accp = newTempVar(
                "__Vfrds", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            AstNodeExpr* wantp = nullptr;
            if (const int len = vectorWidth(valTypep)) {
                wantp = new AstConst{flp, AstConst::Signed32{}, len};
            } else {
                wantp = new AstLenN{flp, valuep->cloneTree(false)};
            }
            AstLoop* const loopp = new AstLoop{flp};
            loopp->addStmtsp(new AstLoopTest{
                flp, loopp, new AstLtS{flp, new AstVarRef{flp, idxp, VAccess::READ}, wantp}});
            loopp->addStmtsp(new AstAssign{flp, new AstVarRef{flp, chp, VAccess::WRITE},
                                           new AstFGetC{flp, filep->cloneTree(false)}});
            loopp->addStmtsp(new AstIf{
                flp,
                new AstLtS{flp, new AstVarRef{flp, chp, VAccess::READ},
                           new AstConst{flp, AstConst::Signed32{}, 0}},
                new AstBreak{flp}, nullptr});
            loopp->addStmtsp(new AstAssign{
                flp, new AstVarRef{flp, accp, VAccess::WRITE},
                new AstConcatN{flp, new AstVarRef{flp, accp, VAccess::READ},
                               new AstSFormatF{flp, "%c", false,
                                               new AstVarRef{flp, chp, VAccess::READ}}}});
            loopp->addStmtsp(new AstAssign{
                flp, new AstVarRef{flp, idxp, VAccess::WRITE},
                new AstAdd{flp, new AstVarRef{flp, idxp, VAccess::READ},
                           new AstConst{flp, AstConst::Signed32{}, 1}}});
            VL_DO_DANGLING(filep->deleteTree(), filep);
            AstNode* outp = new AstAssign{flp, new AstVarRef{flp, idxp, VAccess::WRITE},
                                          new AstConst{flp, AstConst::Signed32{}, 0}};
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, new AstVarRef{flp, accp, VAccess::WRITE},
                                        new AstConst{flp, AstConst::String{}, ""}}));
            outp = AstNode::addNext(outp, static_cast<AstNode*>(loopp));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, valuep, new AstVarRef{flp, accp, VAccess::READ}}));
            if (args.size() == 3) {
                AstNodeExpr* const outLenp = lowerExpr(args[2]);
                if (!outLenp) return nullptr;
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(new AstAssign{
                              flp, outLenp, new AstVarRef{flp, idxp, VAccess::READ}}));
            }
            return outp;
        }
        const bool unpackedArray = valTypep
                                   && valTypep->base()->kind() == VhdlTypeKind::ARRAY
                                   && !elementPacks(valTypep->base()->elementp());
        if (unpackedArray) {
            if (!valTypep->isConstrainedArray() || valTypep->dimensions() != 1
                || valTypep->dimLength(0) <= 0 || valTypep->dimLength(0) > 4096) {
                VL_DO_DANGLING(filep->deleteTree(), filep);
                unsup(flp, std::string{reading ? "READ" : "WRITE"} + " of a file of this type");
                return nullptr;
            }
            const int64_t count = valTypep->dimLength(0);
            AstNodeExpr* basep = lowerExpr(args[1]);
            if (!basep) return nullptr;
            AstNode* outp = nullptr;
            // A value that is not a name cannot be indexed one element at a
            // time, so it is held in a variable of its own first
            if (!reading && !repeatable(basep)) {
                AstVar* const holdp = newTempVar("__Vfval", lowerType(valTypep, flp), flp);
                outp = new AstAssign{flp, new AstVarRef{flp, holdp, VAccess::WRITE}, basep};
                basep = new AstVarRef{flp, holdp, VAccess::READ};
            }
            AstVar* cntp = nullptr;
            if (reading) {
                // "read(f, v, len)" fills as much of v as the file still
                // holds and says how many elements that was
                cntp = newTempVar("__Vfcnt", newIntDType(flp), flp);
                outp = AstNode::addNextNull(
                    outp, static_cast<AstNode*>(
                              new AstAssign{flp, new AstVarRef{flp, cntp, VAccess::WRITE},
                                            new AstConst{flp, AstConst::Signed32{}, 0}}));
            }
            for (int64_t i = 0; i < count; ++i) {
                AstNode* partp = lowerFileElement(reading, filep, basep, valTypep, i, flp);
                if (!partp) return nullptr;
                if (reading) {
                    partp = AstNode::addNext(
                        partp, static_cast<AstNode*>(new AstAssign{
                                   flp, new AstVarRef{flp, cntp, VAccess::WRITE},
                                   new AstAdd{flp, new AstVarRef{flp, cntp, VAccess::READ},
                                              new AstConst{flp, AstConst::Signed32{}, 1}}}));
                    partp = new AstIf{
                        flp,
                        new AstLogNot{flp, new AstFEof{flp, filep->cloneTree(false)}}, partp,
                        nullptr};
                }
                outp = AstNode::addNextNull(outp, partp);
            }
            VL_DO_DANGLING(basep->deleteTree(), basep);
            VL_DO_DANGLING(filep->deleteTree(), filep);
            if (reading && args.size() == 3) {
                AstNodeExpr* const outLenp = lowerExpr(args[2]);
                if (!outLenp) return nullptr;
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(new AstAssign{
                              flp, outLenp, new AstVarRef{flp, cntp, VAccess::READ}}));
            }
            return outp;
        }
        if (valTypep && valTypep->base()->kind() == VhdlTypeKind::RECORD) {
            VL_DO_DANGLING(filep->deleteTree(), filep);
            unsup(flp, std::string{reading ? "READ" : "WRITE"} + " of a file of this type");
            return nullptr;
        }
        AstNodeExpr* const valuep = lowerExpr(args[1]);
        if (!valuep) return nullptr;
        return lowerFileScalar(reading, filep, valuep, valTypep, flp);
    }

    // Advances posp over an abstract literal (15.5): an optional sign, then
    // digits and underscores, an optional point with more digits, and an
    // optional exponent. Reading stops at the first character that cannot
    // continue one, which is what leaves "5.152,61.4" with its comma on the
    // line -- a whitespace-delimited item would have swallowed the lot.
    // fracp is set when a point was seen, since such a value is scaled as a
    // real rather than counted in whole units.
    AstNode* scanNumber(AstNodeExpr* linep, AstVar* posp, AstVar* begp, AstVar* fracp,
                        AstVar* digp, bool allowPoint, FileLine* flp) {
        AstVar* const expp = newTempVar("__Vtxtexp", new AstBasicDType{flp, VBasicDTypeKwd::BIT},
                                        flp);
        AstVar* const jexp = newTempVar("__Vtxtjex", new AstBasicDType{flp, VBasicDTypeKwd::BIT},
                                        flp);
        const auto pos = [&]() { return new AstVarRef{flp, posp, VAccess::READ}; };
        const auto ch = [&]() { return new AstGetcN{flp, linep->cloneTree(false), pos()}; };
        const auto rd = [&](AstVar* varp) { return new AstVarRef{flp, varp, VAccess::READ}; };
        const auto set = [&](AstVar* varp, bool val) -> AstNode* {
            return new AstAssign{flp, new AstVarRef{flp, varp, VAccess::WRITE},
                                 val ? new AstConst{flp, AstConst::BitTrue{}}
                                     : new AstConst{flp, AstConst::BitFalse{}}};
        };
        const auto stopIf = [&](AstNodeExpr* condp) -> AstNode* {
            return new AstIf{flp, condp, new AstBreak{flp}, nullptr};
        };
        AstNode* outp = set(fracp, false);
        outp = AstNode::addNext(outp, set(expp, false));
        outp = AstNode::addNext(outp, set(digp, false));
        outp = AstNode::addNext(outp, set(jexp, false));
        outp = AstNode::addNext(
            outp, static_cast<AstNode*>(
                      new AstAssign{flp, new AstVarRef{flp, begp, VAccess::WRITE}, pos()}));
        AstLoop* const loopp = new AstLoop{flp};
        loopp->addStmtsp(new AstLoopTest{
            flp, loopp,
            new AstLtS{flp, pos(), new AstLenN{flp, linep->cloneTree(false)}}});
        // A digit is always part of it; the rest depend on what came before
        AstNode* const digitp
            = AstNode::addNext(set(digp, true), set(jexp, false));
        AstNode* const underp = AstNode::addNext(
            static_cast<AstNode*>(stopIf(new AstLogNot{flp, rd(digp)})), set(jexp, false));
        AstNode* pointp = stopIf(new AstLogOr{
            flp, new AstLogOr{flp, rd(fracp), rd(expp)}, new AstLogNot{flp, rd(digp)}});
        pointp = AstNode::addNext(pointp, set(fracp, true));
        pointp = AstNode::addNext(pointp, set(digp, false));
        pointp = AstNode::addNext(pointp, set(jexp, false));
        AstNode* markp = stopIf(new AstLogOr{flp, rd(expp), new AstLogNot{flp, rd(digp)}});
        markp = AstNode::addNext(markp, set(expp, true));
        markp = AstNode::addNext(markp, set(digp, false));
        markp = AstNode::addNext(markp, set(jexp, true));
        AstNode* signp = stopIf(new AstLogNot{
            flp, new AstLogOr{flp, rd(jexp), new AstEq{flp, pos(), rd(begp)}}});
        signp = AstNode::addNext(signp, set(jexp, false));
        AstNode* const elsep = new AstBreak{flp};
        AstNode* chainp = new AstIf{
            flp,
            new AstLogOr{flp, isChar(ch(), '+', flp), isChar(ch(), '-', flp)}, signp, elsep};
        chainp = new AstIf{
            flp, new AstLogOr{flp, isChar(ch(), 'e', flp), isChar(ch(), 'E', flp)}, markp,
            chainp};
        if (allowPoint) {
            chainp = new AstIf{flp, isChar(ch(), '.', flp), pointp, chainp};
        } else {
            VL_DO_DANGLING(pointp->deleteTree(), pointp);
        }
        chainp = new AstIf{flp, isChar(ch(), '_', flp), underp, chainp};
        chainp = new AstIf{flp, isDigitChar(ch(), flp), digitp, chainp};
        loopp->addStmtsp(chainp);
        loopp->addStmtsp(new AstAssign{
            flp, new AstVarRef{flp, posp, VAccess::WRITE},
            new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}}});
        outp = AstNode::addNext(outp, static_cast<AstNode*>(loopp));
        VL_DO_DANGLING(linep->deleteTree(), linep);
        return outp;
    }

    AstNode* lowerTextioRead(const std::string& name, std::vector<AstNodeVhdlExpr*>& args,
                             FileLine* origFlp) {
        if (args.size() < 2 || args.size() > 3) return nullptr;
        // The reader works a character at a time and counts in whole
        // integers, so the widths it mixes are deliberate
        FileLine* const flp = new FileLine{origFlp};
        flp->warnOff(V3ErrorCode::WIDTHEXPAND, true);
        flp->warnOff(V3ErrorCode::WIDTHTRUNC, true);
        AstNodeExpr* const linep = lowerExpr(args[0]);
        AstNodeExpr* const valuep = lowerExpr(args[1]);
        if (!linep || !valuep) return nullptr;
        VhdlType* const typep = args[1]->vhdlTypep();

        // Which base the item is written in. The radix forms say so; plain
        // READ takes its answer from the type, since a vector is written in
        // binary and a number in decimal.
        // READ of a CHARACTER takes exactly one character, whatever it is,
        // and says so through GOOD when there is one (16.4)
        if (name == "read" && typep && V3VhdlStd::characterType()
            && typep->base() == V3VhdlStd::characterType()->base()) {
            AstNode* thenp = new AstAssign{
                flp, valuep,
                new AstGetcN{flp, linep->cloneTree(false),
                             new AstConst{flp, AstConst::Signed32{}, 0}}};
            thenp = AstNode::addNext(
                thenp,
                static_cast<AstNode*>(new AstAssign{
                    flp, linep->cloneTree(false),
                    new AstSubstrN{flp, linep->cloneTree(false),
                                   new AstConst{flp, AstConst::Signed32{}, 1},
                                   new AstSub{flp, new AstLenN{flp, linep->cloneTree(false)},
                                              new AstConst{flp, AstConst::Signed32{}, 1}}}}));
            AstNode* elsep = nullptr;
            if (args.size() == 3) {
                AstNodeExpr* const goodp = lowerExpr(args[2]);
                if (!goodp) {
                    VL_DO_DANGLING(thenp->deleteTree(), thenp);
                    VL_DO_DANGLING(linep->deleteTree(), linep);
                    return nullptr;
                }
                thenp = AstNode::addNext(
                    thenp, static_cast<AstNode*>(
                               new AstAssign{flp, goodp->cloneTree(false),
                                             new AstConst{flp, AstConst::BitTrue{}}}));
                elsep = new AstAssign{flp, goodp, new AstConst{flp, AstConst::BitFalse{}}};
            }
            AstNodeExpr* const anyp
                = new AstGtS{flp, new AstLenN{flp, linep}, new AstConst{flp, AstConst::Signed32{}, 0}};
            return new AstIf{flp, anyp, thenp, elsep};
        }

        // READ of a physical type takes an abstract literal and then the
        // name of a unit -- "15 sec" -- and the value is counted in the
        // primary unit (16.4, 5.2.4). As for an enumeration, the line is
        // left alone unless both parts were there.
        if (name == "read" && typep && typep->base()->kind() == VhdlTypeKind::PHYSICAL
            && !typep->base()->units().empty()) {
            AstVar* const posp = newTempVar(
                "__Vtxtpos", new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
            AstVar* const begp = newTempVar(
                "__Vtxtbeg", new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
            AstVar* const nump = newTempVar(
                "__Vtxtnum", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            AstVar* const unitp = newTempVar(
                "__Vtxtunit", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            AstVar* const fracp = newTempVar(
                "__Vtxtfrac", new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
            AstVar* const okp = newTempVar(
                "__Vtxtok", new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
            AstVar* const scalep = newTempVar(
                "__Vtxtscale", new AstBasicDType{flp, VBasicDTypeKwd::LONGINT}, flp);
            const auto pos = [&]() { return new AstVarRef{flp, posp, VAccess::READ}; };
            const auto len = [&]() { return new AstLenN{flp, linep->cloneTree(false)}; };
            const auto setPos = [&](AstNodeExpr* valp) {
                return new AstAssign{flp, new AstVarRef{flp, posp, VAccess::WRITE}, valp};
            };
            const auto bump = [&]() {
                return setPos(new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}});
            };
            const auto skipBlanks = [&]() {
                AstLoop* const loopp = new AstLoop{flp};
                loopp->addStmtsp(new AstLoopTest{
                    flp, loopp,
                    new AstLogAnd{flp, new AstLtS{flp, pos(), len()},
                                  isBlank(new AstGetcN{flp, linep->cloneTree(false), pos()},
                                          flp)}});
                loopp->addStmtsp(bump());
                return loopp;
            };
            AstNode* outp = setPos(new AstConst{flp, AstConst::Signed32{}, 0});
            outp = AstNode::addNext(outp, static_cast<AstNode*>(skipBlanks()));
            AstVar* const digp = newTempVar(
                "__Vtxtdig", new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
            outp = AstNode::addNext(
                outp, scanNumber(linep->cloneTree(false), posp, begp, fracp, digp, true, flp));
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    flp, new AstVarRef{flp, nump, VAccess::WRITE},
                    new AstSubstrN{flp, linep->cloneTree(false),
                                   new AstVarRef{flp, begp, VAccess::READ},
                                   new AstSub{flp, pos(),
                                              new AstConst{flp, AstConst::Signed32{}, 1}}}}));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, new AstVarRef{flp, okp, VAccess::WRITE},
                                        new AstVarRef{flp, digp, VAccess::READ}}));
            outp = AstNode::addNext(outp, static_cast<AstNode*>(skipBlanks()));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, new AstVarRef{flp, begp, VAccess::WRITE}, pos()}));
            AstLoop* const uscanp = new AstLoop{flp};
            uscanp->addStmtsp(new AstLoopTest{
                flp, uscanp,
                new AstLogAnd{
                    flp, new AstLtS{flp, pos(), len()},
                    isIdentChar(new AstGetcN{flp, linep->cloneTree(false), pos()}, flp)}});
            uscanp->addStmtsp(bump());
            outp = AstNode::addNext(outp, static_cast<AstNode*>(uscanp));
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    flp, new AstVarRef{flp, unitp, VAccess::WRITE},
                    new AstToLowerN{
                        flp, new AstSubstrN{flp, linep->cloneTree(false),
                                            new AstVarRef{flp, begp, VAccess::READ},
                                            new AstSub{flp, pos(),
                                                       new AstConst{flp, AstConst::Signed32{},
                                                                    1}}}}}));
            // A unit name that is not one of this type's is not a value
            AstNode* unitsp = new AstAssign{
                flp, new AstVarRef{flp, scalep, VAccess::WRITE},
                new AstConst{flp, AstConst::Signed32{}, 0}};
            for (const auto& unit : typep->base()->units()) {
                V3Number num{flp, 64, 0};
                num.setQuad(static_cast<uint64_t>(unit.second));
                num.isSigned(true);
                unitsp = AstNode::addNext(
                    unitsp,
                    static_cast<AstNode*>(new AstIf{
                        flp,
                        new AstEqN{flp, new AstVarRef{flp, unitp, VAccess::READ},
                                   new AstConst{flp, AstConst::String{}, unit.first}},
                        new AstAssign{flp, new AstVarRef{flp, scalep, VAccess::WRITE},
                                      new AstConst{flp, num}},
                        nullptr}));
            }
            outp = AstNode::addNext(outp, unitsp);
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    flp, new AstVarRef{flp, okp, VAccess::WRITE},
                    new AstLogAnd{flp, new AstVarRef{flp, okp, VAccess::READ},
                                  new AstNeq{flp, new AstVarRef{flp, scalep, VAccess::READ},
                                             new AstConst{flp, AstConst::Signed32{}, 0}}}}));
            // A literal written without a point keeps every bit of its value;
            // one written with a point is scaled as a real and rounded
            AstNodeExpr* const wholep = new AstMul{
                flp, new AstAtoN{flp, new AstVarRef{flp, nump, VAccess::READ}, AstAtoN::ATOI},
                new AstVarRef{flp, scalep, VAccess::READ}};
            AstNodeExpr* const partp = new AstRToIRoundS{
                flp,
                new AstMulD{
                    flp, new AstAtoN{flp, new AstVarRef{flp, nump, VAccess::READ},
                                     AstAtoN::ATOREAL},
                    new AstIToRD{flp, new AstVarRef{flp, scalep, VAccess::READ}}}};
            AstNode* takep = new AstAssign{
                flp, valuep,
                new AstCond{flp, new AstVarRef{flp, fracp, VAccess::READ}, partp, wholep}};
            takep = AstNode::addNext(
                takep,
                static_cast<AstNode*>(new AstAssign{
                    flp, linep->cloneTree(false),
                    new AstSubstrN{flp, linep->cloneTree(false), pos(),
                                   new AstSub{flp, len(),
                                              new AstConst{flp, AstConst::Signed32{}, 1}}}}));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(new AstIf{
                          flp, new AstVarRef{flp, okp, VAccess::READ}, takep, nullptr}));
            if (args.size() == 3) {
                if (AstNodeExpr* const goodp = lowerExpr(args[2])) {
                    outp = AstNode::addNext(
                        outp, static_cast<AstNode*>(
                                  new AstAssign{flp, goodp,
                                                new AstVarRef{flp, okp, VAccess::READ}}));
                }
            }
            VL_DO_DANGLING(linep->deleteTree(), linep);
            return outp;
        }

        // READ of an enumeration takes the name of a literal, which is how
        // textio reads a BOOLEAN -- "true" or "false", in either case (16.4).
        // The line is left alone unless a literal matched, so the GOOD form
        // can be asked again at the same place.
        if (name == "read" && typep && typep->base()->kind() == VhdlTypeKind::ENUM
            && !isLogicish(typep) && !isBitish(typep)) {
            AstVar* const posp = newTempVar(
                "__Vtxtpos", new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
            AstVar* const begp = newTempVar(
                "__Vtxtbeg", new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
            AstVar* const tokp = newTempVar(
                "__Vtxtitem", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
            AstVar* const okp = newTempVar(
                "__Vtxtok", new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
            const auto pos = [&]() { return new AstVarRef{flp, posp, VAccess::READ}; };
            const auto len = [&]() { return new AstLenN{flp, linep->cloneTree(false)}; };
            AstNode* outp = new AstAssign{flp, new AstVarRef{flp, posp, VAccess::WRITE},
                                          new AstConst{flp, AstConst::Signed32{}, 0}};
            // Leading blanks are not part of the item (16.4)
            AstLoop* const skipp = new AstLoop{flp};
            skipp->addStmtsp(new AstLoopTest{
                flp, skipp,
                new AstLogAnd{flp, new AstLtS{flp, pos(), len()},
                              isBlank(new AstGetcN{flp, linep->cloneTree(false), pos()}, flp)}});
            skipp->addStmtsp(new AstAssign{
                flp, new AstVarRef{flp, posp, VAccess::WRITE},
                new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}}});
            outp = AstNode::addNext(outp, static_cast<AstNode*>(skipp));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, new AstVarRef{flp, begp, VAccess::WRITE}, pos()}));
            // The name runs to the first character that cannot be in one
            AstLoop* const scanp = new AstLoop{flp};
            scanp->addStmtsp(new AstLoopTest{
                flp, scanp,
                new AstLogAnd{
                    flp, new AstLtS{flp, pos(), len()},
                    isIdentChar(new AstGetcN{flp, linep->cloneTree(false), pos()}, flp)}});
            scanp->addStmtsp(new AstAssign{
                flp, new AstVarRef{flp, posp, VAccess::WRITE},
                new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}}});
            outp = AstNode::addNext(outp, static_cast<AstNode*>(scanp));
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstAssign{
                    flp, new AstVarRef{flp, tokp, VAccess::WRITE},
                    new AstToLowerN{
                        flp, new AstSubstrN{flp, linep->cloneTree(false),
                                            new AstVarRef{flp, begp, VAccess::READ},
                                            new AstSub{flp, pos(),
                                                       new AstConst{flp, AstConst::Signed32{},
                                                                    1}}}}}));
            outp = AstNode::addNext(
                outp, static_cast<AstNode*>(
                          new AstAssign{flp, new AstVarRef{flp, okp, VAccess::WRITE},
                                        new AstConst{flp, AstConst::BitFalse{}}}));
            // An identifier names a literal; a character literal cannot be
            // written as one, so those are not offered
            const std::vector<VhdlSymbol*>& lits = typep->base()->literals();
            const int bits = bitsFor(lits.size());
            for (size_t i = 0; i < lits.size(); ++i) {
                const std::string& litName = lits[i]->name();
                if (litName.size() == 3 && litName.front() == '\'') continue;
                AstNode* hitp = new AstAssign{
                    flp, valuep->cloneTree(false),
                    new AstConst{flp, AstConst::WidthedValue{}, bits,
                                 static_cast<uint32_t>(i)}};
                hitp = AstNode::addNext(
                    hitp, static_cast<AstNode*>(
                              new AstAssign{flp, new AstVarRef{flp, okp, VAccess::WRITE},
                                            new AstConst{flp, AstConst::BitTrue{}}}));
                outp = AstNode::addNext(
                    outp,
                    static_cast<AstNode*>(new AstIf{
                        flp,
                        new AstEqN{flp, new AstVarRef{flp, tokp, VAccess::READ},
                                   new AstConst{flp, AstConst::String{}, litName}},
                        hitp, nullptr}));
            }
            // Only a name that was read is taken off the line (16.4)
            outp = AstNode::addNext(
                outp,
                static_cast<AstNode*>(new AstIf{
                    flp, new AstVarRef{flp, okp, VAccess::READ},
                    new AstAssign{flp, linep->cloneTree(false),
                                  new AstSubstrN{flp, linep->cloneTree(false), pos(),
                                                 new AstSub{flp, len(),
                                                            new AstConst{flp,
                                                                         AstConst::Signed32{},
                                                                         1}}}},
                    nullptr}));
            if (args.size() == 3) {
                if (AstNodeExpr* const goodp = lowerExpr(args[2])) {
                    outp = AstNode::addNext(
                        outp, static_cast<AstNode*>(
                                  new AstAssign{flp, goodp,
                                                new AstVarRef{flp, okp, VAccess::READ}}));
                }
            }
            VL_DO_DANGLING(valuep->deleteTree(), valuep);
            VL_DO_DANGLING(linep->deleteTree(), linep);
            return outp;
        }

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

        // READ of a STRING takes as many characters as the value is long,
        // whatever they are -- blanks included, and no leading blank
        // skipped. GOOD says whether the line had that many (16.4).
        if (name == "read" && isStringType(typep)) {
            AstNodeExpr* wantp = nullptr;
            if (typep->isConstrainedArray() && !typep->indexTypes().empty()
                && typep->indexTypes()[0] && typep->indexTypes()[0]->bounded()) {
                wantp = new AstConst{flp, AstConst::Signed32{},
                                     static_cast<int32_t>(typep->indexTypes()[0]->length())};
            } else {
                wantp = new AstLenN{flp, valuep->cloneTree(false)};
            }
            AstNode* thenp = new AstAssign{
                flp, valuep,
                new AstSubstrN{flp, linep->cloneTree(false),
                               new AstConst{flp, AstConst::Signed32{}, 0},
                               new AstSub{flp, wantp->cloneTree(false),
                                          new AstConst{flp, AstConst::Signed32{}, 1}}}};
            thenp = AstNode::addNext(
                thenp,
                static_cast<AstNode*>(new AstAssign{
                    flp, linep->cloneTree(false),
                    new AstSubstrN{flp, linep->cloneTree(false), wantp->cloneTree(false),
                                   new AstSub{flp, new AstLenN{flp, linep->cloneTree(false)},
                                              new AstConst{flp, AstConst::Signed32{}, 1}}}}));
            AstNode* elsep = nullptr;
            if (args.size() == 3) {
                AstNodeExpr* const goodp = lowerExpr(args[2]);
                if (!goodp) {
                    VL_DO_DANGLING(thenp->deleteTree(), thenp);
                    VL_DO_DANGLING(linep->deleteTree(), linep);
                    VL_DO_DANGLING(wantp->deleteTree(), wantp);
                    return nullptr;
                }
                thenp = AstNode::addNext(
                    thenp, static_cast<AstNode*>(
                               new AstAssign{flp, goodp->cloneTree(false),
                                             new AstConst{flp, AstConst::BitTrue{}}}));
                elsep = new AstAssign{flp, goodp, new AstConst{flp, AstConst::BitFalse{}}};
            }
            AstNodeExpr* const enoughp
                = new AstGteS{flp, new AstLenN{flp, linep}, wantp};
            return new AstIf{flp, enoughp, thenp, elsep};
        }

        // READ of a value of a logic type takes one character per bit -- one
        // hexadecimal digit per four under HREAD -- skipping blanks before
        // each, which is what reading a BIT_VECTOR one BIT at a time comes
        // to (16.4). A whitespace-delimited item is the wrong unit here:
        // "1 1 0 0 0 0 1 1" is eight bits, and "11111" read into four leaves
        // the fifth character on the line.
        if ((isLogicish(typep) || isBitish(typep))
            && (typep->base()->kind() != VhdlTypeKind::ARRAY
                || (typep->isConstrainedArray() && !typep->indexTypes().empty()
                    && typep->indexTypes()[0] && typep->indexTypes()[0]->bounded()))) {
            const bool scalar = typep->base()->kind() != VhdlTypeKind::ARRAY;
            const int64_t bits = scalar ? 1 : typep->indexTypes()[0]->length();
            const int64_t per = name == "hread" ? 4 : (name == "oread" ? 3 : 1);
            const int64_t chars = (bits + per - 1) / per;
            if (bits > 0 && chars > 0 && chars <= 4096) {
                AstVar* const posp = newTempVar(
                    "__Vtxtpos", new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
                AstVar* const cntp = newTempVar(
                    "__Vtxtcnt", new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
                AstVar* const accp = newTempVar(
                    "__Vtxtitem", new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
                AstVar* const okp = newTempVar(
                    "__Vtxtok", new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
                const auto pos = [&]() { return new AstVarRef{flp, posp, VAccess::READ}; };
                const auto len = [&]() { return new AstLenN{flp, linep->cloneTree(false)}; };
                const auto setOk = [&](bool val) {
                    return new AstAssign{flp, new AstVarRef{flp, okp, VAccess::WRITE},
                                         val ? new AstConst{flp, AstConst::BitTrue{}}
                                             : new AstConst{flp, AstConst::BitFalse{}}};
                };
                AstNode* outp = new AstAssign{flp, new AstVarRef{flp, posp, VAccess::WRITE},
                                              new AstConst{flp, AstConst::Signed32{}, 0}};
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(
                              new AstAssign{flp, new AstVarRef{flp, cntp, VAccess::WRITE},
                                            new AstConst{flp, AstConst::Signed32{}, 0}}));
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(
                              new AstAssign{flp, new AstVarRef{flp, accp, VAccess::WRITE},
                                            new AstConst{flp, AstConst::String{}, ""}}));
                outp = AstNode::addNext(outp, static_cast<AstNode*>(setOk(true)));
                AstLoop* const loopp = new AstLoop{flp};
                loopp->addStmtsp(new AstLoopTest{
                    flp, loopp,
                    new AstLtS{flp, new AstVarRef{flp, cntp, VAccess::READ},
                               new AstConst{flp, AstConst::Signed32{},
                                            static_cast<int32_t>(chars)}}});
                AstLoop* const skipp = new AstLoop{flp};
                skipp->addStmtsp(new AstLoopTest{
                    flp, skipp,
                    new AstLogAnd{flp, new AstLtS{flp, pos(), len()},
                                  isBlank(new AstGetcN{flp, linep->cloneTree(false), pos()},
                                          flp)}});
                skipp->addStmtsp(new AstAssign{
                    flp, new AstVarRef{flp, posp, VAccess::WRITE},
                    new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}}});
                loopp->addStmtsp(skipp);
                // The line may run out, or hold a character this radix has no
                // meaning for; either way there is no value and GOOD says so
                AstNode* const stopp = AstNode::addNext(
                    static_cast<AstNode*>(setOk(false)), new AstBreak{flp});
                loopp->addStmtsp(new AstIf{flp, new AstGteS{flp, pos(), len()}, stopp, nullptr});
                loopp->addStmtsp(new AstIf{
                    flp,
                    new AstLogNot{flp,
                                  isRadixChar(new AstGetcN{flp, linep->cloneTree(false), pos()},
                                              name, isBitish(typep), flp)},
                    AstNode::addNext(static_cast<AstNode*>(setOk(false)), new AstBreak{flp}),
                    nullptr});
                loopp->addStmtsp(new AstAssign{
                    flp, new AstVarRef{flp, accp, VAccess::WRITE},
                    new AstConcatN{flp, new AstVarRef{flp, accp, VAccess::READ},
                                   new AstSFormatF{flp, "%c", false,
                                                   new AstGetcN{flp, linep->cloneTree(false),
                                                                pos()}}}});
                loopp->addStmtsp(new AstAssign{
                    flp, new AstVarRef{flp, posp, VAccess::WRITE},
                    new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}}});
                loopp->addStmtsp(new AstAssign{
                    flp, new AstVarRef{flp, cntp, VAccess::WRITE},
                    new AstAdd{flp, new AstVarRef{flp, cntp, VAccess::READ},
                               new AstConst{flp, AstConst::Signed32{}, 1}}});
                outp = AstNode::addNext(outp, static_cast<AstNode*>(loopp));
                AstNode* takep = new AstAssign{
                    flp, valuep,
                    sizedTo(new AstAtoN{flp, new AstVarRef{flp, accp, VAccess::READ}, fmt},
                            static_cast<int>(bits), flp)};
                takep = AstNode::addNext(
                    takep,
                    static_cast<AstNode*>(new AstAssign{
                        flp, linep->cloneTree(false),
                        new AstSubstrN{flp, linep->cloneTree(false), pos(),
                                       new AstSub{flp, len(),
                                                  new AstConst{flp, AstConst::Signed32{}, 1}}}}));
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(new AstIf{
                              flp, new AstVarRef{flp, okp, VAccess::READ}, takep, nullptr}));
                if (args.size() == 3) {
                    if (AstNodeExpr* const goodp = lowerExpr(args[2])) {
                        outp = AstNode::addNext(
                            outp, static_cast<AstNode*>(
                                      new AstAssign{flp, goodp,
                                                    new AstVarRef{flp, okp, VAccess::READ}}));
                    }
                }
                VL_DO_DANGLING(linep->deleteTree(), linep);
                return outp;
            }
        }

        // What is left is a number: an integer, a real, or the count of a
        // physical value. It runs to the first character that cannot be part
        // of one, and the line keeps what it did not read (16.4).
        AstVar* const posp = newTempVar("__Vtxtpos",
                                        new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
        AstVar* const begp = newTempVar("__Vtxtbeg",
                                        new AstBasicDType{flp, VBasicDTypeKwd::INT}, flp);
        AstVar* const fracp = newTempVar("__Vtxtfrac",
                                         new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
        AstVar* const digp = newTempVar("__Vtxtdig",
                                        new AstBasicDType{flp, VBasicDTypeKwd::BIT}, flp);
        AstVar* const tokp = newTempVar("__Vtxtitem",
                                        new AstBasicDType{flp, VBasicDTypeKwd::STRING}, flp);
        const auto pos = [&]() { return new AstVarRef{flp, posp, VAccess::READ}; };
        const auto len = [&]() { return new AstLenN{flp, linep->cloneTree(false)}; };
        AstNode* outp = new AstAssign{flp, new AstVarRef{flp, posp, VAccess::WRITE},
                                      new AstConst{flp, AstConst::Signed32{}, 0}};
        // Leading blanks are not part of the item (16.4)
        AstLoop* const skipp = new AstLoop{flp};
        skipp->addStmtsp(new AstLoopTest{
            flp, skipp,
            new AstLogAnd{flp, new AstLtS{flp, pos(), len()},
                          isBlank(new AstGetcN{flp, linep->cloneTree(false), pos()}, flp)}});
        skipp->addStmtsp(new AstAssign{
            flp, new AstVarRef{flp, posp, VAccess::WRITE},
            new AstAdd{flp, pos(), new AstConst{flp, AstConst::Signed32{}, 1}}});
        outp = AstNode::addNext(outp, static_cast<AstNode*>(skipp));
        outp = AstNode::addNext(
            outp, scanNumber(linep->cloneTree(false), posp, begp, fracp, digp,
                             fmt == AstAtoN::ATOREAL, flp));
        outp = AstNode::addNext(
            outp,
            static_cast<AstNode*>(new AstAssign{
                flp, new AstVarRef{flp, tokp, VAccess::WRITE},
                new AstSubstrN{flp, linep->cloneTree(false),
                               new AstVarRef{flp, begp, VAccess::READ},
                               new AstSub{flp, pos(),
                                          new AstConst{flp, AstConst::Signed32{}, 1}}}}));
        AstNode* takep = new AstAssign{
            flp, valuep, new AstAtoN{flp, new AstVarRef{flp, tokp, VAccess::READ}, fmt}};
        takep = AstNode::addNext(
            takep,
            static_cast<AstNode*>(new AstAssign{
                flp, linep->cloneTree(false),
                new AstSubstrN{flp, linep->cloneTree(false), pos(),
                               new AstSub{flp, len(),
                                          new AstConst{flp, AstConst::Signed32{}, 1}}}}));
        AstNodeExpr* const anyp = new AstLogAnd{
            flp,
            new AstGtS{flp, new AstLenN{flp, new AstVarRef{flp, tokp, VAccess::READ}},
                       new AstConst{flp, AstConst::Signed32{}, 0}},
            new AstVarRef{flp, digp, VAccess::READ}};
        outp = AstNode::addNext(outp,
                                static_cast<AstNode*>(new AstIf{flp, anyp->cloneTree(false),
                                                                takep, nullptr}));
        // The GOOD form says whether there was anything to read, and does not
        // fail when there was not (16.4)
        if (args.size() == 3) {
            if (AstNodeExpr* const goodp = lowerExpr(args[2])) {
                outp = AstNode::addNext(
                    outp, static_cast<AstNode*>(new AstAssign{flp, goodp, anyp}));
            } else {
                VL_DO_DANGLING(anyp->deleteTree(), anyp);
            }
        } else {
            VL_DO_DANGLING(anyp->deleteTree(), anyp);
        }
        VL_DO_DANGLING(linep->deleteTree(), linep);
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
            // FILE_OPEN_STATUS is (OPEN_OK, STATUS_ERROR, NAME_ERROR,
            // MODE_ERROR), so a file that would not open is 2 (16.3)
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
                                    new AstConst{flp, AstConst::WidthedValue{}, 2, 2}}}));
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
    // The name of the unit of a physical type whose scale is that many
    // primary units, or the primary unit's name if none matches
    static std::string unitNameOf(const VhdlType* typep, int64_t scale) {
        if (!typep) return "";
        const VhdlType* const basep = typep->base();
        std::string best;
        int64_t bestScale = 0;
        for (const auto& unit : basep->units()) {
            if (unit.second != scale) continue;
            return unit.first;
        }
        for (const auto& unit : basep->units()) {
            if (bestScale == 0 || unit.second < bestScale) {
                bestScale = unit.second;
                best = unit.first;
            }
        }
        return best;
    }

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
    // Which signal each signal-class formal names in this copy of the body
    std::map<const VhdlSymbol*, VhdlSymbol*> m_bindSymbols;
    // A signal-class formal bound to part of a signal rather than to a whole
    // one -- "drive(r.y, 3)" -- which has no variable of its own to stand
    // for it, so the actual's own name is lowered again at each reference
    std::map<const VhdlSymbol*, AstNodeVhdlExpr*> m_bindExprs;
    // A signal-class formal of a record type whose elements the call named
    // one at a time -- "p(iface.d => sd, iface.v => sv)" -- so the formal
    // stands for no one object and each element is a signal of its own
    // (6.5.7.1). Each reference to an element lowers to the actual's name.
    std::map<const VhdlSymbol*, std::map<std::string, AstNodeVhdlExpr*>> m_bindElems;
    // A signal-class formal bound to one element of an array signal whose
    // index only the call knows -- "set_bit(s(i))" inside a loop. The copy
    // takes the index as a parameter of its own and indexes the array with
    // it, since neither the element nor the index can be named in the copy.
    struct DynSigBind final {
        AstVar* arrayp = nullptr;  // The signal the element is part of
        AstVar* idxp = nullptr;  // The copy's own parameter holding the index
        std::string argName;  // What the caller passes it as
    };
    std::map<const VhdlSymbol*, DynSigBind> m_bindDynSigs;
    std::map<std::string, AstNodeFTask*> m_signalCopies;

    // Whether every element of a record has a length the back end can fold
    static bool recordLengthEmittable(const VhdlType* typep) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::RECORD) return true;
        for (const VhdlSymbol* const elemp : typep->elements()) {
            const VhdlType* const etp = elemp ? elemp->typep() : nullptr;
            if (!etp) continue;
            if (etp->base()->kind() == VhdlTypeKind::ARRAY && !etp->lengthEmittable()
                && !isStringType(etp)) {
                return false;
            }
            if (etp->base()->kind() == VhdlTypeKind::RECORD && !recordLengthEmittable(etp)) {
                return false;
            }
        }
        return true;
    }

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
        for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
            const VhdlType* const typep = symp->paramTypes()[i];
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && !typep->lengthEmittable() && !isStringType(typep)
                && !paramCarried(symp, i)) {
                return nullptr;
            }
            // A record whose element has no length of its own is a template
            // in the same way: the call is what says how long it is (5.3.3)
            if (typep && typep->base()->kind() == VhdlTypeKind::RECORD
                && !recordLengthEmittable(typep)) {
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
                && !retp->lengthEmittable() && !isStringType(retp)
                && !carriedDynamically(retp)) {
                return nullptr;
            }
            if (retp && retp->base()->kind() == VhdlTypeKind::RECORD
                && !recordLengthEmittable(retp)) {
                return nullptr;
            }
        }
        // An overloaded name needs a distinct one in the emitted code
        const std::string name = copyName.empty() ? uniqueName(symp) : copyName;
        AstNodeFTask* ftaskp = nullptr;
        if (nodep->isFunction()) {
            AstFunc* const funcp = new AstFunc{flp, name, nullptr, nullptr};
            VhdlType* const rettp = returnType(nodep, symp);
            funcp->fvarp(newReturnVar(name,
                                      carriedDynamically(rettp) ? dynCarrierRef(rettp, flp)
                                                                : lowerType(rettp, flp),
                                      flp));
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
            // A formal whose elements were associated one at a time stands
            // for no one object, and each of its elements names its own
            // actual wherever the body reads or drives it (6.5.7.1)
            if (m_bindElems.count(psymp)) continue;
            // Bound to one element of an array signal at an index only the
            // call knows: the index comes in as a parameter, and the body
            // indexes the array with it wherever it names the formal
            const auto dynIt = m_bindDynSigs.find(psymp);
            if (dynIt != m_bindDynSigs.end()) {
                AstVar* const idxp
                    = new AstVar{flp, VVarType::PORT, dynIt->second.argName, VFlagChildDType{},
                                 newIntDType(flp)};
                idxp->direction(VDirection::INPUT);
                idxp->declDirection(VDirection::INPUT);
                idxp->funcLocal(true);
                dynIt->second.idxp = idxp;
                ftaskp->addStmtsp(idxp);
                continue;
            }
            const bool dyn
                = ifp->mode() == VhdlMode::IN && carriedDynamically(psymp->typep());
            if (dyn) m_dynSyms.insert(psymp);
            AstVar* const varp
                = new AstVar{flp, VVarType::PORT, ifp->name(), VFlagChildDType{},
                             dyn ? dynCarrierRef(psymp->typep(), flp)
                                 : lowerType(psymp->typep(), flp)};
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
    void lowerEntity(AstVhdlEntity* entp, AstVhdlArchitecture* archp,
                     const std::string& asName = "") {
        FileLine* const flp = entp->fileline();
        const std::string libname = entp->libname().empty() ? "work" : entp->libname();
        m_modp = new AstModule{flp, asName.empty() ? entp->name() : asName, libname};
        // VHDL counts time in its primary unit, which for TIME is the
        // femtosecond (16.3), so that is the module's time unit, and the
        // design's precision must be at least as fine. Verilator keeps the
        // finest of the two, so a mixed-language design does not lose VHDL's
        // resolution.
        m_modp->timeunit(VTimescale::TS_1FS);
        m_netlistp->timeprecisionMerge(flp, VTimescale{VTimescale::TS_1FS});
        m_vars.clear();
        m_names.clear();
        m_instNames.clear();
        m_blockPrefix.clear();
        m_valueFuncs.clear();
        // The helpers a carrier needs belong to the module they were emitted
        // into, so the next one starts without them
        m_dynEqFuncs.clear();
        m_dynOfFuncs.clear();
        m_dynToFuncs.clear();
        m_dynBitsFuncs.clear();
        m_dynIntFuncs.clear();
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
            collectForced(archp->declsp());
            collectForced(archp->stmtsp());
        }
        m_bindings.clear();
        m_instanceBindings.clear();
        m_bindingMaps.clear();
        {
            // What a configuration declaration said about this entity
            const auto scopeIt = m_cfgScopes.find(libname + "." + entp->name());
            m_cfgScopep = scopeIt == m_cfgScopes.end() ? nullptr : &scopeIt->second;
        }
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
        // An entity may have a statement part of its own, whose statements
        // are passive: an assertion that holds for every instance of it,
        // whatever architecture is bound (3.2.4)
        std::vector<AstNodeVhdlStmt*> entStmtsp;
        for (AstNode* p = entp->declsp(); p; p = p->nextp()) {
            if (AstNodeVhdlStmt* const stmtp = VN_CAST(p, NodeVhdlStmt)) {
                entStmtsp.push_back(stmtp);
                continue;
            }
            lowerDecl(p);
        }
        if (archp) {
            for (AstNode* p = archp->declsp(); p; p = p->nextp()) lowerDecl(p);
        }
        // Every signal now has its variable, so the transaction bits that
        // the statements will mark can be made
        makeActives(flp);
        makeForced(flp);
        for (AstNodeVhdlStmt* const stmtp : entStmtsp) lowerConcurrentDriver(stmtp);
        if (archp) {
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

// Whether a generic type, subprogram or package of this entity is still
// waiting for an instance to say what it is
static bool hasUnboundGeneric(const AstVhdlEntity* entp) {
    if (!entp) return false;
    for (AstNode* p = entp->genericsp(); p; p = p->nextp()) {
        const AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        const bool isKind = ifp->kind() == VhdlIfaceKind::TYPE
                            || ifp->kind() == VhdlIfaceKind::SUBPROGRAM
                            || ifp->kind() == VhdlIfaceKind::PACKAGE;
        if (isKind && !ifp->isBound()) return true;
    }
    return false;
}

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
    // A binding indication may name a configuration, and a configuration may
    // be written before the one that names it, so what each of them chose is
    // read off first (3.4)
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        AstVhdlConfiguration* const cfgp = VN_CAST(unitp, VhdlConfiguration);
        if (!cfgp) continue;
        std::vector<std::string> parts;
        if (!flattenName(cfgp->entityNamep(), parts) || parts.empty()) continue;
        const std::string lib = unitp->libname().empty() ? "work" : unitp->libname();
        m_configOf[lib + "." + cfgp->name()]
            = {parts.back(), topArchitectureOf(cfgp->blockConfigp())};
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
            // built from (3.4), and which entity each component instance in
            // it is to be built from (3.4.3).
            std::vector<std::string> parts;
            if (!flattenName(cfgp->entityNamep(), parts) || parts.empty()) continue;
            const std::string arch = topArchitectureOf(cfgp->blockConfigp());
            if (!arch.empty()) chosen[lib + "." + parts.back()] = arch;
            // A configuration may be named as the design's top unit, and
            // what is elaborated is the entity it configures (3.4)
            if (!v3Global.opt.topModule().empty()
                && VString::downcase(v3Global.opt.topModule()) == cfgp->name()) {
                v3Global.opt.topModule(parts.back());
            }
            // The outermost block configuration is the architecture's; what
            // it holds configures the architecture's own statements, and the
            // blocks and generates inside it configure theirs
            if (cfgp->blockConfigp()) {
                buildCfgScope(cfgp->blockConfigp()->itemsp(), m_cfgScopes[lib + "." + parts.back()],
                              lib);
            }
            continue;
        }
        unsup(unitp->fileline(), std::string{"design unit "} + unitp->typeName());
    }
    // Which architecture each entity is built from has to be settled before
    // any of them is lowered, since an instance asks for it by name
    std::map<std::string, AstVhdlArchitecture*> builtArch;
    for (const auto& pair : entities) m_knownEntities.insert(pair.first);
    // A module the other frontend read is a design unit as much as an entity
    // is: a VHDL architecture may instantiate a Verilog module through a
    // component of the same name, and that component is bound
    for (AstNodeModule* modp = m_netlistp->modulesp(); modp;
         modp = VN_AS(modp->nextp(), NodeModule)) {
        m_knownEntities.insert(VString::downcase(modp->libname().empty() ? "work"
                                                                        : modp->libname())
                               + "." + VString::downcase(modp->name()));
        m_knownEntities.insert("work." + VString::downcase(modp->name()));
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
        builtArch[key] = archp;
        if (archp) m_archOf[key] = archp->name();
    }
    // An entity may have several architectures, and an instance may name the
    // one it wants (7.3.2). Only one of them is the design's own choice, so
    // find which others some instance asks for; each of those becomes a
    // module of its own, so that both stand in the netlist at once.
    std::map<std::string, std::set<std::string>> alsoBuild;  // "lib.entity" -> architectures
    for (const auto& pair : m_cfgScopes) {
        const std::string lib = pair.first.substr(0, pair.first.find('.'));
        std::vector<std::pair<std::string, std::string>> wanted;
        cfgScopeArchs(pair.second, wanted);
        for (const auto& want : wanted) {
            const std::string key = lib + "." + want.first;
            const auto builtIt = builtArch.find(key);
            if (builtIt == builtArch.end() || !builtIt->second) continue;
            if (builtIt->second->name() == want.second) continue;
            if (!namedArchs.count(key + "." + want.second)) continue;
            if (hasUnboundGeneric(entities[key])) continue;
            alsoBuild[key].insert(want.second);
            m_altArchMods.insert(lib + "." + altArchName(want.first, want.second));
        }
    }
    for (const auto& pair : namedArchs) {
        const std::string lib = pair.second->libname().empty() ? "work" : pair.second->libname();
        pair.second->foreach([&](AstVhdlInstance* instp) {
            std::string entName;
            std::string archName;
            if (!instanceNamesArch(instp, entName, archName)) return;
            const std::string key = lib + "." + entName;
            const auto builtIt = builtArch.find(key);
            if (builtIt == builtArch.end() || !builtIt->second) return;
            if (builtIt->second->name() == archName) return;
            if (!namedArchs.count(key + "." + archName)) return;
            if (hasUnboundGeneric(entities[key])) return;
            alsoBuild[key].insert(archName);
            m_altArchMods.insert(lib + "." + altArchName(entName, archName));
        });
    }
    for (const std::string& key : order) {
        // An entity whose generic type nothing settled is not a design: it is
        // the template the copies were made from, and each instance of it is
        // an instance of one of those copies instead (6.5.5)
        if (hasUnboundGeneric(entities[key])) continue;
        lowerEntity(entities[key], builtArch[key]);
        const auto alsoIt = alsoBuild.find(key);
        if (alsoIt == alsoBuild.end()) continue;
        for (const std::string& wanted : alsoIt->second) {
            lowerEntity(entities[key], namedArchs[key + "." + wanted],
                        altArchName(entities[key]->name(), wanted));
        }
    }
    markPackageSignalRefs();
    if (v3Global.opt.trace()) addTraceRequest();
}

// A reference to a signal a package declares has to say which package it is
// in, or V3Scope cannot find the one object all the modules share.
void VhdlLowerVisitor::markPackageSignalRefs() {
    if (m_pkgSigs.empty() || !m_netlistp) return;
    std::set<const AstVar*> varps;
    for (const auto& pair : m_pkgSigs) varps.insert(pair.second);
    AstPackage* const unitp = m_netlistp->dollarUnitPkgAddp();
    m_netlistp->foreach([&](AstVarRef* refp) {
        if (varps.count(refp->varp())) refp->classOrPackagep(unitp);
    });
}

// VHDL has no $dumpfile: a design cannot ask for a waveform from inside
// itself, the way a Verilog testbench does, so the tool asks on its behalf
// when it was told to trace. The request goes in the design's own top, and
// names the file after the model, which is what a VHDL simulator's own
// waveform option would have done.
void VhdlLowerVisitor::addTraceRequest() {
    AstNodeModule* topp = nullptr;
    const std::string wanted = VString::downcase(v3Global.opt.topModule());
    for (AstNodeModule* modp = m_netlistp->modulesp(); modp;
         modp = VN_AS(modp->nextp(), NodeModule)) {
        if (!VN_IS(modp, Module)) continue;
        if (!modp->fileline()->language().vhdl()) continue;
        if (!wanted.empty()) {
            if (VString::downcase(modp->name()) == wanted) topp = modp;
        } else if (!m_instantiated.count(modp->name())) {
            // Nothing else builds one of it, so it is the top -- and if more
            // than one is, the last of them is what V3LinkLevel picks too
            topp = modp;
        }
    }
    if (!topp) return;
    FileLine* const flp = topp->fileline();
    const std::string suffix = v3Global.opt.traceEnabledFst()
                                   ? ".fst"
                                   : (v3Global.opt.traceEnabledSaif() ? ".saif" : ".vcd");
    // Beside the model's other output, which is where it can be found again
    const std::string name = v3Global.opt.makeDir() + "/" + v3Global.opt.prefix() + suffix;
    AstNode* stmtsp
        = new AstDumpCtl{flp, VDumpCtlType::FILE, new AstConst{flp, AstConst::String{}, name}};
    stmtsp = AstNode::addNext(
        stmtsp, static_cast<AstNode*>(new AstDumpCtl{
                    flp, VDumpCtlType::VARS, new AstConst{flp, AstConst::Signed32{}, 0}}));
    topp->addStmtsp(new AstInitial{flp, stmtsp});
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
