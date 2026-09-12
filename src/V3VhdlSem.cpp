// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL semantic analysis
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
// Walks the design units in analysis order and gives the parse tree meaning:
//
//   * every declarative region becomes a VhdlScope, nested as clause 12
//     describes, with std.standard implicitly visible at the outside;
//   * every declaration becomes a VhdlSymbol in the region that declares it;
//   * every type definition becomes a VhdlType, and every type mark resolves
//     to one;
//   * every name resolves to the declaration it denotes, and every expression
//     to a type, with overloaded names decided by 12.5.
//
// Errors are reported where they are found and analysis carries on, so that
// one bad declaration does not hide everything after it.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlSem.h"

#include "V3Global.h"
#include "V3VhdlEval.h"
#include "V3VhdlStd.h"
#include "V3VhdlSym.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

// The node a node is a child of. backp() is the previous sibling for any
// node but the first of a list, so walking it upward passes through every
// declaration written earlier in the same region before reaching the region.
static const AstNode* parentOf(const AstNode* nodep) {
    while (nodep->backp() && nodep->backp()->nextp() == nodep) nodep = nodep->backp();
    return nodep->backp();
}

// The design unit a node is written in
static const AstNodeVhdlUnit* unitOf(const AstNode* nodep) {
    for (const AstNode* upp = nodep; upp; upp = parentOf(upp)) {
        if (const AstNodeVhdlUnit* const unitp = VN_CAST(upp, NodeVhdlUnit)) return unitp;
    }
    return nullptr;
}

// Of these four an implicit signal is made, one per prefix, to hold what the
// attribute answers about. There is nowhere in a subprogram for such a signal
// to live -- a subprogram is not a region that a simulation cycle updates --
// so reading one of a signal parameter is an error (16.2.2, 4.2.2.3).
static bool makesImplicitSignal(const std::string& attr) {
    return attr == "stable" || attr == "quiet" || attr == "delayed" || attr == "transaction";
}

//######################################################################

namespace {

// Lowercase, since VHDL basic identifiers are case insensitive. The scanner
// has already folded them; this is for names built here.
std::string lower(const std::string& in) {
    std::string out = in;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

}  // namespace

//######################################################################
// The analyzer

class VhdlSpecVisitor;

// The part of a formal an association element names -- a record element by
// its name, an array element as its index in parentheses. Defined below,
// beside the binding that produces them.
std::string vhdlIndexPartName(int64_t at);
bool vhdlIsIndexPart(const std::string& part);
int64_t vhdlIndexPartAt(const std::string& part);

class VhdlSemVisitor final : public VNVisitor {
    // STATE
    VhdlScope* m_scopep = nullptr;  // Region being filled
    VhdlScope* m_rootp = nullptr;  // Outermost region, holding library names
    std::map<std::string, VhdlScope*> m_libs;  // Library name -> its region
    std::map<std::string, VhdlScope*> m_unitScopes;  // "lib.unit" -> its region
    std::map<std::string, AstNodeVhdlUnit*> m_primaryUnits;  // "lib.unit" -> the unit
    // "lib.context" -> the context declaration, whose items a context
    // reference applies where it is written (13.3)
    std::map<std::string, AstVhdlContextDecl*> m_contextDecls;
    int m_contextDepth = 0;  // Guards a context that references itself
    std::string m_libname;  // Library of the unit being analyzed
    VhdlType* m_returnTypep = nullptr;  // Result type of the enclosing function
    // Values a call site fixed, for the copy of a subprogram being made now
    std::vector<VhdlValue> m_specValues;
    // A package body continues the package declaration's declarative region
    // (12.1), so what it declares completes an earlier declaration rather
    // than making a second one
    bool m_inPackageBody = false;
    // Names whose own declaration is being analyzed. A declaration's scope
    // begins where it does, so within it the name denotes nothing yet --
    // not the outer object it is about to hide (12.2, 12.3)
    std::set<std::string> m_declaring;
    bool nameBeingDeclared(const AstVhdlName* namep) {
        if (m_inSpecialized || namep->isChar() || !m_declaring.count(namep->name())) {
            return false;
        }
        err(namep->fileline(), "'" + namep->prettyName()
                                   + "' is being declared here, and a name is not visible"
                                     " within its own declaration");
        return true;
    }
    // Package declarations whose body this analysis has seen, and so whose
    // subprograms are known to have been given bodies or not (4.8)
    std::set<const AstVhdlPackage*> m_pkgsWithBody;
    int m_ifacePos = 0;  // Position of the interface element being declared
    bool m_errored = false;
    // An entity with a generic type is one entity per type the design hands
    // it. What the copies are, so that the same type asks for one copy only.
    struct VhdlEntitySpec final {
        std::string key;  // "lib.entity" of the original
        std::vector<VhdlType*> types;  // What its generic types were bound to
        std::vector<VhdlScope*> packages;  // And its generic packages
        std::vector<VhdlSymbol*> subprograms;  // And its generic subprograms
        std::string name;  // Name the copy was given
        VhdlScope* scopep = nullptr;  // The copy's own region
    };
    std::vector<VhdlEntitySpec> m_entitySpecs;
    // The types a copy's generic types stand for, while the copy is analyzed
    std::map<std::string, VhdlType*> m_boundGenTypes;
    std::set<std::string> m_pkgGenTypeNames;  // Generic types of the package copy in hand
    // And the regions its generic packages stand for
    std::map<std::string, VhdlScope*> m_boundGenPackages;
    // And the subprograms its generic subprograms stand for
    std::map<std::string, VhdlSymbol*> m_boundGenSubs;
    std::map<std::string, AstVhdlArchitecture*> m_archUnits;  // "lib.entity" -> its body
    std::map<std::string, AstVhdlArchitecture*> m_archByKey;  // "lib.entity(arch)" -> it
    // While an architecture's own declarations are analyzed: its region, and
    // its entity's, which it continues (12.1)
    VhdlScope* m_archScopep = nullptr;
    VhdlScope* m_archEntityScopep = nullptr;
    const AstNodeVhdlUnit* m_curUnitp = nullptr;  // The design unit being analyzed
    // Incomplete types an access type designates (5.4.2)
    std::set<const VhdlType*> m_designatedIncomplete;
    // "region attribute class" of each attribute specification that said
    // "others" or "all", after which none may follow (7.2)
    std::set<std::string> m_attrSpecsClosed;
    // Which entity a configuration specification binds a component or an
    // instance label to (7.3.1). An entity with a generic type is copied for
    // the instance, so which entity it is has to be known here.
    std::map<std::string, std::string> m_specBindings;  // Component or label -> entity
    // Constants synthesized for a package generic whose value is not static,
    // waiting to be added to the instantiation's declarations
    std::vector<AstNodeVhdl*> m_pendingPkgDecls;
    // Whether the generics being bound are a package instantiation's, which
    // is the only place a synthesized constant has declarations to live in
    bool m_inPkgInst = false;
    int m_specUnitCount = 0;  // Numbers the copies
    int m_specDepth = 0;  // A copy may instantiate another; guards a cycle

    VL_UNCOPYABLE(VhdlSemVisitor);

    // METHODS - scopes and libraries

    VhdlScope* libScope(const std::string& name) {
        const std::string lib = lower(name.empty() ? "work" : name);
        // STD is Verilator's own, holding STANDARD and TEXTIO
        if (lib == "std") return V3VhdlStd::stdLibScope();
        const auto it = m_libs.find(lib);
        if (it != m_libs.end()) return it->second;
        VhdlScope* const scopep = VhdlSymTab::newScope(nullptr, lib);
        m_libs.emplace(lib, scopep);
        return scopep;
    }

    // Push a nested region for the duration of a construct
    VhdlScope* pushScope(const std::string& name) {
        m_scopep = VhdlSymTab::newScope(m_scopep, name);
        return m_scopep;
    }
    void popScope() { m_scopep = m_scopep->parentp(); }

    void err(FileLine* flp, const std::string& msg) {
        flp->v3error(msg);
        m_errored = true;
    }

    // A declarative part admits only the items its own clause lists. A
    // process may not declare a signal or a component, and neither may a
    // subprogram: what those regions are for is code, and an object that
    // outlives one call or a binding that belongs to a structure has nowhere
    // to live there (11.3, 4.3).
    void rejectDecls(AstNode* declsp, const std::string& where) {
        for (AstNode* dp = declsp; dp; dp = dp->nextp()) {
            if (const AstVhdlObjectDecl* const odp = VN_CAST(dp, VhdlObjectDecl)) {
                if (odp->kind() == VhdlObjKind::SIGNAL) {
                    err(dp->fileline(),
                        "A signal cannot be declared in " + where + ": '" + odp->name() + "'");
                }
            } else if (VN_IS(dp, VhdlComponentDecl)) {
                err(dp->fileline(), "A component cannot be declared in " + where + ": '"
                                        + VN_AS(dp, NodeVhdlDecl)->name() + "'");
            } else if (VN_IS(dp, VhdlConfigSpec) || VN_IS(dp, VhdlDisconnectSpec)) {
                // Both are about the structure of a design, which a process
                // or a subprogram has none of (11.3, 4.3)
                err(dp->fileline(),
                    std::string{VN_IS(dp, VhdlConfigSpec) ? "A configuration" : "A disconnection"}
                        + " specification cannot be written in " + where);
            }
            if (const AstVhdlObjectDecl* const odp = VN_CAST(dp, VhdlObjectDecl)) {
                if (odp->kind() == VhdlObjKind::SHARED_VARIABLE) {
                    err(dp->fileline(), "A shared variable cannot be declared in " + where
                                            + ", where every variable belongs to it alone: '"
                                            + odp->name() + "'");
                }
            }
        }
    }

    // Outside a process or a subprogram a variable is visible to every
    // process at once, so it has to be declared shared for that to be what
    // the design meant (6.4.2.4)
    void rejectUnsharedVariables(AstNode* declsp, const std::string& where) {
        for (AstNode* dp = declsp; dp; dp = dp->nextp()) {
            const AstVhdlObjectDecl* const odp = VN_CAST(dp, VhdlObjectDecl);
            if (!odp || odp->kind() != VhdlObjKind::VARIABLE) continue;
            err(dp->fileline(), "A variable declared in " + where + " must be a shared variable: '"
                                    + odp->name() + "'");
        }
    }

    // A copy of a package made for one instantiation, while it is analyzed.
    // Substituting a generic type may make two declarations of a name the
    // same profile -- "f(e : t_element)" and "f(n : integer)" where the
    // instantiation binds t_element to INTEGER. The template told them
    // apart, and the copy is that template with the actuals written in
    // (4.9), so neither is a redeclaration of the other and every call in it
    // means what it meant there.
    bool m_inPackageInstance = false;
    // Which subprogram each call of the template resolved to, keyed by the
    // copy's own node
    std::map<const AstNode*, const VhdlSymbol*> m_templateSyms;
    // The pass that gives each call to an unconstrained subprogram its copy
    VhdlSpecVisitor* m_specp = nullptr;

    // Declare 'symp', complaining if it illegally hides something declared in
    // the same region (12.3)
    void declare(VhdlSymbol* symp, FileLine* flp) {
        // An architecture continues its entity's declarative region (12.1),
        // so it cannot declare again what the entity declares
        if (m_archEntityScopep && m_scopep == m_archScopep && !m_inSpecialized
            && !m_inPackageInstance) {
            std::vector<VhdlSymbol*> prior;
            m_archEntityScopep->lookupLocal(symp->name(), prior);
            for (const VhdlSymbol* const priorp : prior) {
                if (priorp->predefined()) continue;
                if (priorp->isOverloadable() && symp->isOverloadable()) continue;
                err(flp, "Declaration of '" + symp->name() + "' conflicts with the "
                             + priorp->kindName()
                             + " of the same name its entity declares, whose region an"
                               " architecture continues");
                break;
            }
        }
        VhdlSymbol* const hiddenp = m_scopep->insert(symp);
        if (!hiddenp || hiddenp->predefined()) return;
        if (m_inPackageInstance) return;
        err(flp, "Declaration of '" + symp->name() + "' conflicts with the "
                     + hiddenp->kindName() + " of the same name declared in this region");
    }

    // METHODS - names

    // Every declaration a simple name denotes here
    void lookup(const std::string& name, std::vector<VhdlSymbol*>& out) const {
        if (m_scopep) m_scopep->lookup(name, out);
    }

    // Resolve a name that must denote exactly one non-overloadable thing
    VhdlSymbol* lookupOne(AstNodeVhdlExpr* namep, VhdlSymKind wantKind, const char* what) {
        std::vector<std::string> parts;
        if (!flatten(namep, parts) || parts.empty()) return nullptr;
        VhdlScope* scopep = m_scopep;
        // A selected name walks into a library or package first
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            VhdlScope* const nextp = scopep ? scopep->findNamed(parts[i]) : nullptr;
            if (!nextp) {
                err(namep->fileline(), "Cannot find '" + parts[i] + "' in this context");
                return nullptr;
            }
            scopep = nextp;
        }
        std::vector<VhdlSymbol*> found;
        if (parts.size() == 1) {
            lookup(parts[0], found);
            if (conflictingPotentials(found) && !m_inSpecialized) {
                err(namep->fileline(), "'" + parts[0]
                                           + "' is declared by more than one package a use"
                                             " clause makes visible here, so neither is visible");
                return nullptr;
            }
        } else if (scopep) {
            scopep->lookupLocal(parts.back(), found);
        }
        for (VhdlSymbol* const symp : found) {
            if (symp->kind() == wantKind) return symp;
            // An alias denotes what it was declared for, and may stand
            // wherever that could (6.6.1)
            if (symp->aliasp() && symp->aliasp()->kind() == wantKind) return symp->aliasp();
        }
        if (found.empty()) {
            err(namep->fileline(), std::string{"Cannot find "} + what + " '" + parts.back() + "'");
        } else {
            err(namep->fileline(), "'" + parts.back() + "' is a " + found[0]->kindName()
                                       + ", but a " + what + " is required here");
        }
        return nullptr;
    }

    // The type a name denotes, or null, without saying anything if it does
    // not denote one
    VhdlSymbol* lookupTypeQuiet(const AstNodeVhdlExpr* namep) {
        std::vector<std::string> parts;
        if (!flatten(namep, parts) || parts.empty()) return nullptr;
        VhdlScope* scopep = m_scopep;
        for (size_t i = 0; i + 1 < parts.size() && scopep; ++i) {
            scopep = scopep->findNamed(parts[i]);
        }
        if (!scopep) return nullptr;
        std::vector<VhdlSymbol*> found;
        if (parts.size() == 1) {
            lookup(parts[0], found);
        } else {
            scopep->lookupLocal(parts.back(), found);
        }
        for (VhdlSymbol* const symp : found) {
            if (symp->kind() == VhdlSymKind::TYPE && symp->typep()) return symp;
            if (symp->aliasp() && symp->aliasp()->kind() == VhdlSymKind::TYPE) {
                return symp->aliasp();
            }
        }
        return nullptr;
    }

    // A protected type's methods are named through an object of that type
    // (5.6.2): "c.increment(4)". Given the prefix that stands before the
    // method name, return the object it denotes and the region holding the
    // methods, or null if the prefix is not such an object.
    VhdlSymbol* protectedObject(const AstNodeVhdlExpr* prefixp, VhdlScope*& methScopep) {
        std::vector<std::string> parts;
        if (!flatten(prefixp, parts) || parts.empty()) return nullptr;
        std::vector<VhdlSymbol*> found;
        if (parts.size() == 1) {
            lookup(parts[0], found);
        } else {
            VhdlScope* scopep = m_scopep;
            for (size_t i = 0; i + 1 < parts.size() && scopep; ++i) {
                scopep = scopep->findNamed(parts[i]);
            }
            if (!scopep) return nullptr;
            scopep->lookupLocal(parts.back(), found);
        }
        for (VhdlSymbol* const symp : found) {
            // An alias is another name for the object, and a method is
            // reached through it just as well (6.6.1)
            VhdlSymbol* const realp = symp->resolved();
            if (!realp->isObject()) continue;
            VhdlType* typep = realp->typep();
            if (typep && typep->kind() == VhdlTypeKind::ACCESS) typep = typep->designatedp();
            if (!typep || typep->base()->kind() != VhdlTypeKind::PROTECTED) continue;
            if (!typep->base()->scopep()) continue;
            methScopep = typep->base()->scopep();
            return realp;
        }
        return nullptr;
    }

    // Like flatten(), but ".all" is kept as a final component, as a use
    // clause allows and a type mark does not
    static bool flattenUse(const AstNodeVhdlExpr* nodep, std::vector<std::string>& out) {
        if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            out.push_back(namep->name());
            return true;
        }
        if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            if (!flattenUse(selp->prefixp(), out)) return false;
            if (!selp->suffixp()) {
                out.push_back("all");
                return true;
            }
            return flattenUse(selp->suffixp(), out);
        }
        return false;
    }

    // Flatten a selected name into its identifier components
    static bool flatten(const AstNodeVhdlExpr* nodep, std::vector<std::string>& out) {
        if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            out.push_back(namep->name());
            return true;
        }
        if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            if (!flatten(selp->prefixp(), out)) return false;
            if (!selp->suffixp()) return false;  // ".all" is not part of a type mark
            return flatten(selp->suffixp(), out);
        }
        return false;
    }

    // METHODS - ranges and static bounds (5.2.1, 9.4)

    // What analyzing a discrete range found. The bounds are values rather
    // than expressions because a type is not fully known until they have been
    // folded: an index range decides an array's length, which in turn decides
    // whether V3VhdlLower can emit a packed array at all.
    struct RangeInfo final {
        VhdlType* typep = nullptr;  // Type of the values in the range
        VhdlValue left;
        VhdlValue right;
        bool ascending = true;
        bool unbounded = false;  // Written "<>", so deliberately unconstrained
        bool valid = false;  // There was a range to analyze
    };

    // A discrete range in any of its spellings: "left to right", a type mark
    // standing for its own range, a subtype indication, "t range <>", or a
    // range attribute name.
    // The condition of an if, a loop, an exit, a next, an assertion or a
    // conditional assignment: BOOLEAN, or converted to one by "??" (9.2.9).
    // Only the expression itself is a condition -- the operands of "a = b"
    // are not -- so the node is remembered rather than a flag set.
    VhdlType* analyzeCondition(AstNodeVhdlExpr* condp) {
        VL_RESTORER(m_condExprp);
        m_condExprp = condp;
        VhdlType* const typep = analyzeExpr(condp, V3VhdlStd::booleanType());
        checkCondition(condp, typep);
        return typep;
    }

    // A condition is of type BOOLEAN, or of a type the condition operator is
    // defined for, which is the conversion the language would otherwise have
    // written out (9.2.9, and note 1 of 12.5). Anything else is not a
    // condition, however readily it might convert to one in another language.
    void checkCondition(AstNodeVhdlExpr* condp, VhdlType* typep) {
        if (!condp || !typep || typep->isError()) return;
        if (m_inSpecialized) return;  // The template was checked already
        VhdlType* const boolp = V3VhdlStd::booleanType();
        if (boolp && typep->base() == boolp->base()) return;
        // The condition operator arrived in VHDL-2008 (9.2.9). Before it, a
        // condition is of type BOOLEAN and nothing else, however many
        // "??" a design of its own declares
        if (condp->fileline()->language() >= V3LangCode::L1076_2008
            && hasConditionOperator(typep)) {
            return;
        }
        err(condp->fileline(),
            condp->fileline()->language() >= V3LangCode::L1076_2008
                ? "Condition must be of type BOOLEAN, or of a type the condition"
                  " operator is defined for, but is "
                      + typep->name()
                : "Condition must be of type BOOLEAN, but is " + typep->name());
    }

    RangeInfo analyzeRange(AstNode* rangep, VhdlType* wantp) {
        RangeInfo info;
        if (!rangep) return info;
        if (VN_IS(rangep, VhdlBox)) {
            info.typep = wantp;
            info.unbounded = true;
            info.valid = true;
            return info;
        }
        if (AstVhdlSubtypeInd* const sip = VN_CAST(rangep, VhdlSubtypeInd)) {
            info.typep = resolveSubtype(sip);
            info.unbounded = VN_IS(sip->constraintp(), VhdlBox);
            info.valid = true;
            takeBoundsFrom(info, info.typep);
            return info;
        }
        // "v'range" written where a range is expected reaches here as the
        // attribute name itself, since a parenthesized name is a call until
        // something says otherwise (16.2.4)
        AstNodeVhdlExpr* bareAttrp = nullptr;
        if (AstVhdlAttrName* const attrp = VN_CAST(rangep, VhdlAttrName)) {
            if (attrp->name() == "range" || attrp->name() == "reverse_range") {
                bareAttrp = attrp;
            }
        }
        AstVhdlRange* const rp = VN_CAST(rangep, VhdlRange);
        if (!rp && !bareAttrp) return info;
        info.valid = true;
        if (AstNodeVhdlExpr* const namep = bareAttrp ? bareAttrp : rp->attrp()) {
            // A type mark used as a range, or prefix'RANGE (16.2.4)
            bool reversed = false;
            VhdlType* const typep = rangeOfName(namep, reversed);
            if (!typep) return info;
            info.typep = typep;
            takeBoundsFrom(info, typep);
            if (reversed) {
                std::swap(info.left, info.right);
                info.ascending = !info.ascending;
            }
            return info;
        }
        // "left to right": both bounds are of one type, which the context may
        // already fix. Where it does not, the two bounds decide it between
        // them -- "NOTE to t_alert_level'right" is a range of t_alert_level
        // even though NOTE alone names a literal of two types (5.2.1).
        VhdlType* hintp = wantp;
        if (!hintp || hintp->isError()) hintp = commonBoundType(rp->leftp(), rp->rightp());
        // The generic that says which way an open dimension runs is named
        // here like any other, so it is resolved here like any other
        if (rp->dirp()) analyzeExpr(rp->dirp(), V3VhdlStd::booleanType());
        VhdlType* leftp = analyzeExpr(rp->leftp(), hintp);
        VhdlType* rightp
            = analyzeExpr(rp->rightp(), hintp ? hintp : (leftp && !leftp->isUniversal() ? leftp
                                                                                       : nullptr));
        checkRangeBounds(rp, wantp, leftp, rightp);
        if (!wantp || wantp->isError()) wantp = hintp;
        VhdlType* typep = wantp;
        if (!typep || typep->isError()) typep = (rightp && !rightp->isUniversal()) ? rightp : leftp;
        // A range of universal integers indexes with INTEGER (5.3.2.1)
        if (typep && typep->kind() == VhdlTypeKind::UNIVERSAL_INTEGER) {
            typep = V3VhdlStd::integerType();
        } else if (typep && typep->kind() == VhdlTypeKind::UNIVERSAL_REAL) {
            typep = V3VhdlStd::realType();
        }
        info.typep = typep;
        info.ascending = rp->dir() == VhdlDir::TO;
        info.left = evalChecked(rp->leftp());
        info.right = evalChecked(rp->rightp());
        if (info.left.known() && !info.left.typep()) info.left.typep(typep);
        if (info.right.known() && !info.right.typep()) info.right.typep(typep);
        return info;
    }

    // Both bounds of a range are of one type, and of the one the context
    // gives where it gives one (5.2.1): "bit_vector(0 to 1)" of an array
    // indexed by BIT, or "mvl1'low to mvl2'high", names no range at all.
    // Numeric bounds of mixed kinds are left to the rules of the constructs
    // that take them, which say more precisely what is wrong.
    void checkRangeBounds(AstVhdlRange* rp, VhdlType* wantp, VhdlType* leftp, VhdlType* rightp) {
        // A range made here rather than written -- the bounds an open generic
        // or port is given by its instance -- runs a way no text said
        if (m_inSpecialized || rp->dirUnknown()) return;
        // A bound is a value, which a type mark is not
        for (AstNodeVhdlExpr* const boundp : {rp->leftp(), rp->rightp()}) {
            const VhdlSymbol* const symp
                = VN_IS(boundp, VhdlName) && boundp->vhdlSymp() ? boundp->vhdlSymp() : nullptr;
            if (symp && symp->resolved()->kind() == VhdlSymKind::TYPE) {
                err(boundp->fileline(),
                    "A bound of a range is a value, but '" + symp->name() + "' is a type");
                return;
            }
        }
        if (!leftp || !rightp || leftp->isError() || rightp->isError()) return;
        if (wantp && !wantp->isError() && !wantp->isGenericFormal()) {
            for (AstNodeVhdlExpr* const boundp : {rp->leftp(), rp->rightp()}) {
                if (valueFitsType(boundp, wantp)) continue;
                err(boundp->fileline(), "A bound of this range must be of type "
                                            + wantp->prettyName() + ", but is of type "
                                            + boundp->vhdlTypep()->prettyName());
                return;
            }
            return;
        }
        const VhdlTypeKind lk = leftp->base()->kind();
        const VhdlTypeKind rk = rightp->base()->kind();
        const bool lenum = lk == VhdlTypeKind::ENUM;
        const bool renum = rk == VhdlTypeKind::ENUM;
        if ((lenum || renum) && leftp->base() != rightp->base()) {
            err(rp->fileline(), "The bounds of a range are of one type, but these are of types "
                                    + leftp->prettyName() + " and " + rightp->prettyName());
        }
    }

    // The one type both bounds of a range can have, where they decide it
    // between themselves. Null when they do not settle it, which leaves the
    // ordinary left-then-right rule to try.
    VhdlType* commonBoundType(AstNodeVhdlExpr* leftp, AstNodeVhdlExpr* rightp) {
        if (!leftp || !rightp) return nullptr;
        CandSet lc;
        CandSet rc;
        candidateTypes(leftp, lc);
        candidateTypes(rightp, rc);
        if (lc.types.empty() || rc.types.empty()) return nullptr;
        VhdlType* foundp = nullptr;
        for (VhdlType* const ltp : lc.types) {
            if (!ltp || ltp->isUniversal()) continue;
            for (VhdlType* const rtp : rc.types) {
                if (!rtp) continue;
                if (rtp->base() != ltp->base() && !rtp->isUniversal()) continue;
                if (foundp && foundp->base() != ltp->base()) return nullptr;  // Still ambiguous
                foundp = ltp;
            }
        }
        return foundp;
    }

    // Fill a RangeInfo from a type that already carries its own range
    static void takeBoundsFrom(RangeInfo& info, const VhdlType* typep) {
        if (!typep || !typep->bounded()) return;
        const bool isReal = typep->base()->kind() == VhdlTypeKind::FLOATING;
        VhdlType* const nonConstp = const_cast<VhdlType*>(typep);
        info.left = isReal ? VhdlValue::mkReal(typep->leftReal(), nonConstp)
                           : VhdlValue::mkInt(typep->left(), nonConstp);
        info.right = isReal ? VhdlValue::mkReal(typep->rightReal(), nonConstp)
                            : VhdlValue::mkInt(typep->right(), nonConstp);
        info.ascending = typep->ascending();
    }

    // The type whose range a name denotes: a type mark denotes its own, and
    // prefix'RANGE the range of the prefix (16.2.4)
    VhdlType* rangeOfName(AstNodeVhdlExpr* namep, bool& reversed) {
        reversed = false;
        AstVhdlAttrName* const attrp = VN_CAST(namep, VhdlAttrName);
        if (!attrp) return resolveTypeMark(namep);
        if (attrp->name() == "reverse_range") {
            reversed = true;
        } else if (attrp->name() != "range") {
            err(namep->fileline(), "Attribute '" + attrp->name() + "' does not denote a range");
            return nullptr;
        }
        analyzeExpr(attrp, nullptr);  // Resolves the prefix
        VhdlType* typep = attrp->prefixp() ? attrp->prefixp()->vhdlTypep() : nullptr;
        if (!typep || typep->isError()) return nullptr;
        // An access value is dereferenced without saying so (8.1)
        if (typep->base()->kind() == VhdlTypeKind::ACCESS && typep->base()->designatedp()) {
            typep = typep->base()->designatedp();
        }
        if (typep->base()->kind() != VhdlTypeKind::ARRAY) return typep;
        // "v'range" where v is unconstrained -- a parameter, say -- is the
        // range of whatever it is given, not the whole of its index subtype.
        // Reporting the index subtype's own range instead would make a slice
        // written with it look four billion elements long (16.2.4).
        if (typep->unconstrained()) {
            VhdlType* const dimp = dimensionOf(attrp, typep);
            VhdlType* const openp = cloneSubtype(dimp, attrp->fileline());
            if (openp) {
                openp->unbound();
                // Where the range is one an instance settles rather than one
                // the text folded, "v'range" is that same range: the
                // expression travels with it, so a signal declared from it
                // gets the bounds the port was given (16.2.4)
                // A STRING whose length is only known while running carries
                // it with the value, so its range is read from the value
                // rather than from an expression written here
                if (typep->base() != V3VhdlStd::stringType()->base()) {
                    AstNode* boundsp = dimp ? dimp->boundsp() : nullptr;
                    // Where nothing here says the bounds either, "v'range" is
                    // "v'left to v'right" and nothing shorter: an object
                    // declared with it is as long as v turns out to be, and
                    // reading the index subtype's own range instead would say
                    // a length that is not v's (16.2.4)
                    if (!boundsp && attrp->prefixp()) {
                        FileLine* const flp = attrp->fileline();
                        AstVhdlRange* const rangep = new AstVhdlRange{
                            flp,
                            new AstVhdlAttrName{flp, attrp->prefixp()->cloneTree(false),
                                                "left"},
                            VhdlDir::TO,
                            new AstVhdlAttrName{flp, attrp->prefixp()->cloneTree(false),
                                                "right"}};
                        rangep->dirUnknown(true);
                        boundsp = rangep;
                    }
                    openp->boundsp(boundsp);
                }
            }
            return openp;
        }
        return dimensionOf(attrp, typep);
    }

    // The index type an array attribute asks about, honouring its optional
    // dimension argument (16.2.4)
    VhdlType* dimensionOf(AstVhdlAttrName* nodep, VhdlType* typep) {
        int64_t dim = 1;
        if (nodep->argp()) {
            analyzeExpr(nodep->argp(), V3VhdlStd::integerType());
            const VhdlValue arg = V3VhdlEval::eval(nodep->argp());
            if (!arg.isInt()) {
                err(nodep->argp()->fileline(), "The dimension of attribute '" + nodep->name()
                                                   + " must be a locally static integer");
                return nullptr;
            }
            dim = arg.toInt();
        }
        const int64_t dims = static_cast<int64_t>(typep->indexTypes().size());
        if (dim < 1 || dim > dims) {
            err(nodep->fileline(), "Dimension " + std::to_string(dim) + " of attribute '"
                                       + nodep->name() + " is out of range; "
                                       + typep->prettyName() + " has " + std::to_string(dims)
                                       + " dimension(s)");
            return nullptr;
        }
        return typep->indexTypes()[static_cast<size_t>(dim - 1)];
    }

    // Fold an analyzed expression, reporting the failures that are errors in
    // themselves rather than merely a value that is not static
    VhdlValue evalChecked(AstNodeVhdlExpr* nodep) {
        std::string why;
        const VhdlValue val = V3VhdlEval::eval(nodep, &why);
        if (!why.empty() && nodep) err(nodep->fileline(), why);
        return val;
    }

    // A fresh anonymous subtype of the range's type, constrained by it. This
    // is what an index constraint and a range constraint both produce.
    VhdlType* makeRangeSubtype(const RangeInfo& info, FileLine* flp) {
        VhdlType* const basep = info.typep;
        if (!basep || basep->isError()) return V3VhdlStd::errorType();
        VhdlType* const outp = cloneSubtype(basep, flp);
        applyRange(outp, info);
        return outp;
    }

    // A distinct subtype of the same type, sharing its shape
    static VhdlType* cloneSubtype(VhdlType* basep, FileLine* flp) {
        VhdlType* const outp = VhdlSymTab::newType(basep->kind(), "", flp);
        outp->base(basep->base());
        outp->elementp(basep->elementp());
        outp->designatedp(basep->designatedp());
        for (VhdlType* const idxp : basep->indexTypes()) outp->addIndexType(idxp);
        for (VhdlSymbol* const elemp : basep->elements()) outp->addElement(elemp);
        outp->resolutionp(basep->resolutionp());
        outp->unconstrained(basep->unconstrained());
        outp->copyBounds(basep);
        return outp;
    }

    // Give a type the bounds a range found, where they folded
    static void applyRange(VhdlType* outp, const RangeInfo& info) {
        if (!outp) return;
        if (info.unbounded) {
            outp->unbound();
            return;
        }
        if (!info.left.known() || !info.right.known()) {
            outp->unbound();  // Not static: the bounds arrive at elaboration
            return;
        }
        if (outp->base()->kind() == VhdlTypeKind::FLOATING) {
            outp->setRealBounds(info.left.toReal(), info.right.toReal(), info.ascending);
        } else if (info.left.isInt() && info.right.isInt()) {
            outp->setBounds(info.left.toInt(), info.right.toInt(), info.ascending);
        }
    }

    // How a type's range reads in a diagnostic
    static std::string rangeText(const VhdlType* typep) {
        if (!typep || !typep->bounded()) return "";
        VhdlType* const nonConstp = const_cast<VhdlType*>(typep);
        const bool isReal = typep->base()->kind() == VhdlTypeKind::FLOATING;
        const VhdlValue lo = isReal ? VhdlValue::mkReal(typep->leftReal(), nonConstp)
                                    : VhdlValue::mkInt(typep->left(), nonConstp);
        const VhdlValue hi = isReal ? VhdlValue::mkReal(typep->rightReal(), nonConstp)
                                    : VhdlValue::mkInt(typep->right(), nonConstp);
        return lo.prettyName() + (typep->ascending() ? " to " : " downto ") + hi.prettyName();
    }

    // A choice must denote a value of the index range it selects from (9.3.3.3)
    static bool isNullRange(const VhdlType* typep) {
        if (!typep || !typep->bounded()) return false;
        return typep->ascending() ? typep->left() > typep->right()
                                  : typep->left() < typep->right();
    }

    void checkChoiceInRange(FileLine* flp, const VhdlValue& val, VhdlType* typep) {
        if (!typep || !typep->bounded() || !val.known()) return;
        if (isNullRange(typep)) return;
        if (V3VhdlEval::inRange(val, typep)) return;
        err(flp, "Choice " + val.prettyName() + " is outside the index range, which is "
                     + rangeText(typep));
    }

    // Complain when a static value cannot belong to the subtype it is given
    // to (5.2.1). Silent whenever either the value or the range is unknown,
    // so that only a certain violation is reported.
    void checkValueInRange(AstNodeVhdlExpr* nodep, VhdlType* typep, const std::string& what) {
        if (!nodep || !typep || !typep->bounded() || !typep->isScalar()) return;
        // A null range holds no value at all (5.2.1), so every value is
        // outside it and saying so tells no one anything. Code that indexes a
        // null array guards itself -- "if v'length > 0 then v(v'high)" --
        // and that guard is a run-time test, not one this can see.
        if (isNullRange(typep)) return;
        // In a copy made for one call site the values that call supplies make
        // expressions fold that the design never wrote as static, so a branch
        // the guard around it rules out folds too: "if c >= 0 then f(c) else
        // f(-c)" would report -c as out of range on the arm that cannot run.
        // Such a value is checked while running, as the LRM asks (4.2.2.1).
        if (m_inSpecialized) return;
        // A static value that has none -- 1.0 / 0.0 -- is out of every range
        // (nvc 1.22.1 and Questa 2025.2; GHDL 7.0.0-dev warns)
        std::string why;
        const VhdlValue val = V3VhdlEval::eval(nodep, &why);
        if (!why.empty()) {
            err(nodep->fileline(), why + ", so no value of " + what + " results");
            return;
        }
        if (!val.known() || V3VhdlEval::inRange(val, typep)) return;
        err(nodep->fileline(), "Value " + val.prettyName() + " is outside the range of " + what
                                   + ", which is " + rangeText(typep));
    }

    // METHODS - types

    // The subprogram a generic subprogram is given (6.5.5). The actual is a
    // name, possibly selected -- "work.helpers.same" -- or an operator
    // symbol, and the one that matches the formal's profile is the one meant.
    VhdlSymbol* resolveSubprogramActual(AstNodeVhdlExpr* nodep, AstVhdlIfaceDecl* formalp) {
        if (!nodep) return nullptr;
        std::vector<std::string> parts;
        std::vector<VhdlSymbol*> found;
        // An operator is named by its symbol -- "op => \"+\"" -- which reads
        // as a string literal until it is known to be a designator (4.5.3)
        if (const AstVhdlLiteral* const litp = VN_CAST(nodep, VhdlLiteral)) {
            if (litp->kind() == VhdlLitKind::STRING) parts.push_back(lower(litp->text()));
        }
        if (!parts.empty()) {
            lookup(parts.back(), found);
        } else if (flatten(nodep, parts) && !parts.empty()) {
            VhdlScope* scopep = m_scopep;
            bool viaScope = parts.size() > 1;
            for (size_t i = 0; i + 1 < parts.size() && viaScope; ++i) {
                VhdlScope* const nextp = scopep ? scopep->findNamed(parts[i]) : nullptr;
                if (!nextp) {
                    viaScope = false;
                } else {
                    scopep = nextp;
                }
            }
            if (viaScope && scopep) {
                scopep->lookupLocal(parts.back(), found);
            } else {
                lookup(parts.back(), found);
            }
        }
        // The formal's own profile picks which of them is meant
        const VhdlSymbol* wantp
            = formalp->profilep() ? formalp->profilep()->vhdlSymp() : nullptr;
        VhdlSymbol* firstp = nullptr;
        for (VhdlSymbol* const symp : found) {
            VhdlSymbol* const rootp = symp->resolved();
            if (rootp->kind() != VhdlSymKind::FUNCTION
                && rootp->kind() != VhdlSymKind::PROCEDURE) {
                continue;
            }
            if (!firstp) firstp = rootp;
            if (!wantp) continue;
            if (rootp->paramTypes().size() != wantp->paramTypes().size()) continue;
            bool fits = true;
            for (size_t i = 0; i < rootp->paramTypes().size() && fits; ++i) {
                const VhdlType* const ap = rootp->paramTypes()[i];
                const VhdlType* const bp = wantp->paramTypes()[i];
                if (ap && bp && ap->base() != bp->base()) fits = false;
            }
            const VhdlType* const arp = rootp->returnTypep();
            const VhdlType* const brp = wantp->returnTypep();
            if (!arp != !brp) fits = false;
            if (arp && brp && arp->base() != brp->base()) fits = false;
            if (!fits) continue;
            nodep->vhdlSymp(rootp);
            return rootp;
        }
        if (firstp) {
            nodep->vhdlSymp(firstp);
            return firstp;
        }
        err(nodep->fileline(), "Generic subprogram '" + formalp->name()
                                   + "' is given no subprogram of that name");
        return nullptr;
    }

    // The type a generic type is given. Usually a type mark, but VHDL-2008
    // lets the actual be any subtype indication, so "T => std_logic_vector(7
    // downto 0)" hands over the constraint as well as the type (6.5.5); the
    // parser could not tell that from a call.
    VhdlType* resolveTypeActual(AstNodeVhdlExpr* nodep) {
        if (!nodep) return nullptr;
        if (VhdlSymbol* const symp = lookupTypeQuiet(nodep)) {
            nodep->vhdlSymp(symp);
            return symp->typep();
        }
        AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
        if (!callp || !callp->prefixp()) return nullptr;
        VhdlSymbol* const basep = lookupTypeQuiet(callp->prefixp());
        if (!basep || !basep->typep()) return nullptr;
        VhdlType* const outp = cloneSubtype(basep->typep(), nodep->fileline());
        applyConstraint(outp, basep->typep(), nullptr, callp->argsp(), nodep->fileline());
        nodep->vhdlTypep(outp);
        return outp;
    }

    // Resolve a type mark to the type it names
    VhdlType* resolveTypeMark(AstNodeVhdlExpr* markp) {
        if (!markp) return V3VhdlStd::errorType();
        // An attribute may stand where a type mark does: 'SUBTYPE is the
        // subtype of an object, constraint and all, and 'ELEMENT the element
        // subtype of an array (16.2.5)
        if (AstVhdlAttrName* const attrp = VN_CAST(markp, VhdlAttrName)) {
            const std::string& attr = attrp->name();
            if (attr == "subtype" || attr == "base" || attr == "element") {
                VhdlType* typep = analyzeExpr(attrp->prefixp(), nullptr);
                if (!typep || typep->isError()) return V3VhdlStd::errorType();
                if (attr == "base") typep = typep->base();
                if (attr == "element") {
                    if (typep->base()->kind() != VhdlTypeKind::ARRAY || !typep->elementp()) {
                        err(markp->fileline(),
                            "Attribute 'element needs an array, but the prefix is "
                                + typep->prettyName());
                        return V3VhdlStd::errorType();
                    }
                    typep = typep->elementp();
                }
                markp->vhdlTypep(typep);
                return typep;
            }
            // Any other attribute gives a value or a range, not a subtype
            if (!m_inSpecialized) {
                err(markp->fileline(), "A type mark names a type or a subtype, and attribute '"
                                           + attr + " gives none");
            }
            return V3VhdlStd::errorType();
        }
        if (const AstVhdlName* const namep = VN_CAST(markp, VhdlName)) {
            if (nameBeingDeclared(namep)) return V3VhdlStd::errorType();
        }
        VhdlSymbol* const symp = lookupOne(markp, VhdlSymKind::TYPE, "type");
        if (!symp || !symp->typep()) return V3VhdlStd::errorType();
        markp->vhdlSymp(symp);
        markp->vhdlTypep(symp->typep());
        return symp->typep();
    }

    // Resolve a subtype indication to a type, applying its constraint (6.3)
    VhdlType* resolveSubtype(AstVhdlSubtypeInd* nodep) {
        if (!nodep) return V3VhdlStd::errorType();
        if (nodep->vhdlTypep()) return nodep->vhdlTypep();
        return resolveSubtypeOf(nodep->typeMarkp(), nodep->constraintp(), nodep->resolutionp(),
                                nodep, nodep->fileline());
    }

    // The same, where the subtype indication was not written as one: a
    // generic type may be given "std_logic_vector(7 downto 0)" in a generic
    // map, which is a subtype indication however it reaches here (6.5.7.2)
    VhdlType* resolveSubtypeOf(AstNodeVhdlExpr* markExprp, AstNodeVhdl* constraintp,
                               AstNodeVhdlExpr* resolutionExprp, AstVhdlSubtypeInd* cachep,
                               FileLine* flp) {
        // The parser could not tell "std_logic_vector(7 downto 0)" from a
        // call, so a constrained type mark arrives as one (8.1)
        // "t(3 downto 0)(7 downto 0)" constrains the array and then its
        // element, and arrives as a call of a call (5.3.2.1)
        std::vector<AstNode*> argLists;
        AstNodeVhdlExpr* markp = markExprp;
        while (AstVhdlCall* const cp = VN_CAST(markp, VhdlCall)) {
            argLists.push_back(cp->argsp());
            markp = cp->prefixp();
        }
        std::reverse(argLists.begin(), argLists.end());  // Outermost first
        VhdlType* const basep = resolveTypeMark(markp);
        // A generic type nothing has bound yet stands in for a real type and
        // has to keep its own identity: replacing it with the one error type
        // makes every parameter of it look like every other (6.5.5)
        if (!basep || (basep->isError() && !basep->isGenericFormal())) {
            if (cachep) cachep->vhdlTypep(V3VhdlStd::errorType());
            return V3VhdlStd::errorType();
        }
        if (!constraintp && argLists.empty() && !resolutionExprp) {
            if (cachep) cachep->vhdlTypep(basep);
            return basep;
        }
        // A constrained subtype is a distinct subtype of the same type
        VhdlType* const typep = cloneSubtype(basep, flp);
        // Set before applying the constraint, so that a subtype indication
        // mentioning itself stops rather than recursing
        if (cachep) cachep->vhdlTypep(typep);
        // Written as a subtype indication, "integer (0 to 10)" is an index
        // constraint, and only an array has an index to constrain (6.3) --
        // or an access to one, where it constrains what is designated (5.4.1)
        if (cachep && !argLists.empty() && !basep->isGenericFormal()
            && basep->base()->kind() != VhdlTypeKind::ARRAY
            && basep->base()->kind() != VhdlTypeKind::RECORD
            && basep->base()->kind() != VhdlTypeKind::ACCESS && !m_inSpecialized) {
            err(flp, "An index constraint applies to an array, but " + basep->prettyName()
                         + " is not one");
            return typep;
        }
        VhdlType* curp = typep;
        VhdlType* curBasep = basep;
        for (size_t i = 0; i < argLists.size(); ++i) {
            if (i > 0) {
                VhdlType* const elemp = curp->elementp();
                if (!elemp) break;
                VhdlType* const newElemp = cloneSubtype(elemp, flp);
                curp->elementp(newElemp);
                curBasep = elemp;
                curp = newElemp;
            }
            applyConstraint(curp, curBasep, nullptr, argLists[i], flp);
        }
        if (constraintp) applyConstraint(typep, basep, constraintp, nullptr, flp);
        if (resolutionExprp) {
            VhdlSymbol* const resp
                = lookupOne(resolutionExprp, VhdlSymKind::FUNCTION, "resolution function");
            if (resp) typep->resolutionp(resp);
            // It takes a one-dimensional array of the type's values and gives
            // one back (4.6), which picks it out of an overloaded set
            const AstVhdlName* const rnamep = VN_CAST(resolutionExprp, VhdlName);
            if (resp && rnamep && !m_inSpecialized && !basep->isGenericFormal()) {
                std::vector<VhdlSymbol*> found;
                lookup(rnamep->name(), found);
                VhdlSymbol* fitp = nullptr;
                for (VhdlSymbol* const candp : found) {
                    const VhdlSymbol* const rp = candp->resolved();
                    if (rp->kind() != VhdlSymKind::FUNCTION || rp->paramTypes().size() != 1)
                        continue;
                    const VhdlType* const ap = rp->paramTypes()[0];
                    const VhdlType* const elemp = ap && ap->base()->kind() == VhdlTypeKind::ARRAY
                                                          && ap->base()->indexTypes().size() == 1
                                                      ? ap->base()->elementp()
                                                      : nullptr;
                    if (!elemp || elemp->isGenericFormal() || !rp->returnTypep()) continue;
                    // "(resolved) std_ulogic_vector" resolves the element,
                    // and "((resolved))" the element's element (6.3); the
                    // parentheses that say which do not survive parsing
                    bool anyLevel = false;
                    const VhdlType* levelp = basep;
                    for (int depth = 0; levelp && depth < 8 && !anyLevel; ++depth) {
                        anyLevel = elemp->base() == levelp->base()
                                   && rp->returnTypep()->base() == levelp->base();
                        levelp = levelp->base()->kind() == VhdlTypeKind::ARRAY
                                     ? levelp->base()->elementp()
                                     : nullptr;
                    }
                    if (!anyLevel) continue;
                    fitp = candp;
                    break;
                }
                if (fitp) {
                    typep->resolutionp(fitp);
                } else {
                    err(resolutionExprp->fileline(),
                        "A resolution function takes an array of " + basep->prettyName()
                            + " and returns one, and no '" + rnamep->name() + "' does");
                }
            }
        }
        return typep;
    }

    // Apply a range, index or record constraint to a fresh subtype (5.3.2.1,
    // 6.3). 'callArgsp' carries the index constraint of a type mark that the
    // parser had to record as a call.
    void applyConstraint(VhdlType* typep, VhdlType* basep, AstNodeVhdl* constraintp,
                         AstNode* callArgsp, FileLine* flp) {
        if (VN_IS(constraintp, VhdlBox)) {
            // "positive range <>" leaves the array unconstrained but keeps
            // POSITIVE's own range, which every index must still satisfy
            typep->unconstrained(true);
            return;
        }
        if (AstVhdlRange* const rp = VN_CAST(constraintp, VhdlRange)) {
            // A range constraint narrows a scalar type (5.2.1)
            if (!basep->isScalar() && !basep->isError() && !basep->isGenericFormal()
                && !m_inSpecialized) {
                err(flp, "A range constraint narrows a scalar type, but " + basep->prettyName()
                             + " is not one");
                return;
            }
            const RangeInfo info = analyzeRange(rp, basep->isScalar() ? basep : nullptr);
            applyRange(typep, info);
            // A scalar range whose bounds name a generic is still the range
            // this subtype has; it is only not a pair of numbers yet. Keep it
            // so that an array indexed by this subtype can emit it (14.4.2).
            if (!info.unbounded && info.valid && !typep->bounded()) typep->boundsp(rp);
            checkSubrange(typep, basep, flp, rp);
            return;
        }
        // Otherwise an index constraint, over the array's dimensions
        AstNode* firstp = callArgsp;
        if (AstVhdlIndexConstraint* const icp = VN_CAST(constraintp, VhdlIndexConstraint)) {
            firstp = icp->rangesp();
        }
        if (!firstp) return;
        // A record constraint names an element and constrains that: the
        // subtype "rec(f(1 to 3))" fixes f and leaves the rest alone (5.3.3)
        if (basep->base()->kind() == VhdlTypeKind::RECORD) {
            applyRecordConstraint(typep, basep, firstp, flp);
            return;
        }
        if (basep->base()->kind() != VhdlTypeKind::ARRAY) {
            // A parenthesized type mark that is not an array is a conversion
            // or a call the caller mistook for a subtype; leave it alone
            return;
        }
        // An array subtype whose index is already constrained keeps that
        // constraint; only "open" may stand in its place (5.3.2.2, 6.3)
        if (basep->isConstrainedArray() && !m_inSpecialized) {
            bool allOpen = true;
            for (AstNode* rp = firstp; rp; rp = rp->nextp()) {
                const AstVhdlAssoc* const ap = VN_CAST(rp, VhdlAssoc);
                const bool isOpen = VN_IS(rp, VhdlOpen)
                                    || (ap && (!ap->actualp() || VN_IS(ap->actualp(), VhdlOpen)));
                if (!isOpen && !VN_IS(rp, VhdlIndexConstraint)) allOpen = false;
            }
            if (!allOpen) {
                err(flp, basep->prettyName()
                             + " is already constrained, so its index cannot be constrained"
                               " again; write 'open' to constrain only its element");
                return;
            }
        }
        std::vector<VhdlType*> indexes;
        std::vector<bool> openAt;  // Which of them the text left open
        const std::vector<VhdlType*>& baseIndexes = basep->base()->indexTypes();
        size_t dim = 0;
        for (AstNode* rp = firstp; rp; rp = rp->nextp()) {
            if (AstVhdlIndexConstraint* const nestedp = VN_CAST(rp, VhdlIndexConstraint)) {
                // "t(3 downto 0)(7 downto 0)" constrains the element too
                VhdlType* const elemp = typep->elementp();
                if (elemp && elemp->base()->kind() == VhdlTypeKind::ARRAY) {
                    VhdlType* const newElemp = cloneSubtype(elemp, flp);
                    applyConstraint(newElemp, elemp, nestedp, nullptr, flp);
                    typep->elementp(newElemp);
                }
                continue;
            }
            VhdlType* const wantp = dim < baseIndexes.size() ? baseIndexes[dim] : nullptr;
            // The parser could not tell an index constraint from a call, so a
            // range may arrive wrapped in an association
            AstNode* itemp = rp;
            if (AstVhdlAssoc* const asp = VN_CAST(rp, VhdlAssoc)) {
                if (!asp->formalp() && asp->actualp()) itemp = asp->actualp();
            }
            // "t(open)(7 downto 0)" leaves this dimension as it was and
            // constrains the element instead (5.3.3). As it was means as the
            // base type had it: open where the base left it open, and still
            // constrained where the base already said how long it is.
            if (VN_IS(itemp, VhdlOpen)) {
                indexes.push_back(wantp ? wantp : V3VhdlStd::integerType());
                openAt.push_back(basep->base()->dimOpen(dim));
                ++dim;
                continue;
            }
            RangeInfo info = analyzeRange(itemp, wantp);
            if (!info.valid) {
                // A type mark stands for its own range: "t1(r1)" constrains
                // the array to the whole of R1 (5.3.2.1)
                if (AstNodeVhdlExpr* const exprp = VN_CAST(itemp, NodeVhdlExpr)) {
                    if (VhdlSymbol* const tsymp = lookupTypeQuiet(exprp)) {
                        exprp->vhdlSymp(tsymp);
                        info.valid = true;
                        info.typep = tsymp->typep();
                        takeBoundsFrom(info, info.typep);
                        // Of the index's own type (5.3.2.2)
                        if (wantp && info.typep && !wantp->isError() && !info.typep->isError()
                            && !wantp->isGenericFormal() && !info.typep->isGenericFormal()
                            && wantp->base() != info.typep->base() && !m_inSpecialized) {
                            err(exprp->fileline(),
                                "This index constraint names subtype " + info.typep->prettyName()
                                    + ", but the index is of type " + wantp->base()->prettyName());
                        }
                    }
                }
            }
            if (!info.valid) continue;
            VhdlType* idxp = info.typep ? info.typep : wantp;
            if (!info.unbounded && info.left.known() && info.right.known()) {
                idxp = makeRangeSubtype(info, flp);
                checkSubrange(idxp, wantp, flp, rp);
            } else if (!info.unbounded && info.valid) {
                // A range whose bounds did not fold constrains nothing
                // static, but it is still a range: a bound that names a
                // generic is fixed once the instance is elaborated, so keep
                // it as written for the back end to fold then (10.3)
                // "x'range" is x's own range: where x's bounds are ones an
                // instance settles, that expression is what this range is,
                // and it says more than the attribute name does
                AstNode* const inheritedp = idxp ? idxp->boundsp() : nullptr;
                idxp = cloneSubtype(idxp ? idxp : V3VhdlStd::integerType(), flp);
                idxp->unbound();
                if (inheritedp) {
                    idxp->boundsp(inheritedp);
                } else if (VN_IS(rp, VhdlRange)) {
                    idxp->boundsp(rp);
                }
            }
            indexes.push_back(idxp ? idxp : V3VhdlStd::integerType());
            openAt.push_back(info.unbounded);
            ++dim;
        }
        if (indexes.empty()) return;
        if (!baseIndexes.empty() && indexes.size() != baseIndexes.size()) {
            err(flp, "Index constraint gives " + std::to_string(indexes.size())
                         + " dimension(s), but " + basep->prettyName() + " has "
                         + std::to_string(baseIndexes.size()));
        }
        typep->clearIndexTypes();
        bool unconstrained = false;
        for (size_t i = 0; i < indexes.size(); ++i) {
            VhdlType* const idxp = indexes[i];
            const bool open = i < openAt.size() && openAt[i];
            if (open || !idxp->bounded()) {
                unconstrained = true;
                // "t(open)(7 downto 0)" says nothing about this dimension,
                // however bounded the index subtype it keeps may be (5.3.3)
                if (open || !idxp->boundsp()) typep->setDimOpen(typep->dimensions());
            }
            typep->addIndexType(idxp);
        }
        typep->openDimsKnown();
        typep->unconstrained(unconstrained);
    }

    // "rec (f (1 to 3), g (7 downto 0))": each element named is constrained,
    // and gets a declaration of its own in this subtype (5.3.3)
    void applyRecordConstraint(VhdlType* typep, VhdlType* basep, AstNode* firstp, FileLine* flp) {
        for (AstNode* p = firstp; p; p = p->nextp()) {
            AstNode* itemp = p;
            if (AstVhdlAssoc* const asp = VN_CAST(itemp, VhdlAssoc)) {
                if (!asp->formalp() && asp->actualp()) itemp = asp->actualp();
            }
            // "rec(f(1 to 3)(1 to 2))" constrains the element as well as
            // the index, and the parser could only read that as a call of a
            // call. Peel them apart: the innermost is the element's own
            // constraint, the outermost is f's (5.3.3).
            std::vector<AstNode*> constraints;  // f's own first
            AstNode* rootp = itemp;
            while (AstVhdlCall* const cp = VN_CAST(rootp, VhdlCall)) {
                constraints.push_back(cp->argsp());
                rootp = cp->prefixp();
            }
            std::reverse(constraints.begin(), constraints.end());
            const AstVhdlName* const namep = VN_CAST(rootp, VhdlName);
            if (!namep || constraints.empty()) {
                err(flp, "A record constraint names an element and how to constrain it");
                return;
            }
            const std::vector<VhdlSymbol*>& elems = basep->base()->elements();
            size_t pos = elems.size();
            for (size_t i = 0; i < elems.size(); ++i) {
                if (elems[i]->name() == namep->name()) pos = i;
            }
            if (pos == elems.size()) {
                err(namep->fileline(), "Record type " + basep->prettyName()
                                           + " has no element '" + namep->name() + "'");
                return;
            }
            VhdlType* const elemTypep = elems[pos]->typep();
            if (!elemTypep) return;
            VhdlType* const newTypep = cloneSubtype(elemTypep, flp);
            applyConstraint(newTypep, elemTypep, nullptr, constraints[0], flp);
            // Each further list constrains the element of the one before it
            VhdlType* levelp = newTypep;
            for (size_t d = 1; d < constraints.size() && levelp; ++d) {
                VhdlType* const innerp
                    = levelp->elementp() ? levelp->elementp() : levelp->base()->elementp();
                if (!innerp) break;
                VhdlType* const newInnerp = cloneSubtype(innerp, flp);
                applyConstraint(newInnerp, innerp, nullptr, constraints[d], flp);
                levelp->elementp(newInnerp);
                levelp = newInnerp;
            }
            VhdlSymbol* const newSymp = VhdlSymTab::newSymbol(VhdlSymKind::RECORD_ELEMENT,
                                                             elems[pos]->name(),
                                                             elems[pos]->declp());
            newSymp->typep(newTypep);
            newSymp->position(elems[pos]->position());
            typep->replaceElement(pos, newSymp);
        }
    }

    // A subtype's range must lie within the range it narrows (5.2.1)
    // A range outside its base type's is an error where the design wrote it
    // out, and a condition met at elaboration otherwise: "string(x'range)" of
    // a parameter, or a bound a function call gives, is refused by no
    // reference simulator at analysis (5.2.1, 9.4.2)
    void checkSubrange(const VhdlType* typep, const VhdlType* basep, FileLine* flp,
                       const AstNode* rangep = nullptr) {
        // In a copy made for one call site the bounds are that call's, not
        // something the design wrote (4.2.2.1)
        if (m_inSpecialized) return;
        if (rangep && notLocallyStatic(rangep)) return;
        if (!typep || !basep || !typep->bounded() || !basep->bounded()) return;
        if (typep->isNull()) return;  // A null range names no value at all
        if (basep->base()->kind() == VhdlTypeKind::FLOATING) {
            if (typep->lowReal() >= basep->lowReal() && typep->highReal() <= basep->highReal()) {
                return;
            }
        } else if (typep->low() >= basep->low() && typep->high() <= basep->high()) {
            return;
        }
        err(flp, "Range " + rangeText(typep) + " is outside the range of "
                     + basep->prettyName() + ", which is " + rangeText(basep));
    }

    // Build the type a type definition describes (clause 5)
    VhdlType* buildType(AstNodeVhdl* defp, const std::string& name, FileLine* flp) {
        if (!defp) {
            // An incomplete type declaration (5.4.2)
            return VhdlSymTab::newType(VhdlTypeKind::INCOMPLETE, name, flp);
        }
        if (AstVhdlEnumType* const enump = VN_CAST(defp, VhdlEnumType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ENUM, name, flp);
            int pos = 0;
            for (AstNode* litp = enump->literalsp(); litp; litp = litp->nextp()) {
                AstVhdlEnumLiteral* const elp = VN_AS(litp, VhdlEnumLiteral);
                VhdlSymbol* const symp = VhdlSymTab::newSymbol(
                    VhdlSymKind::ENUM_LITERAL,
                    elp->isChar() ? vhdlCharLitName(elp->name()) : elp->name(), elp);
                symp->typep(typep);
                symp->returnTypep(typep);
                symp->position(pos++);
                elp->vhdlSymp(symp);
                typep->addLiteral(symp);
                declare(symp, elp->fileline());
            }
            typep->setBounds(0, pos - 1, true);
            return typep;
        }
        if (AstVhdlNumericType* const nump = VN_CAST(defp, VhdlNumericType)) {
            // Integer or floating, depending on the bounds (5.2.3, 5.2.5)
            const RangeInfo info = analyzeRange(nump->rangep(), nullptr);
            const bool isReal = rangeIsReal(nump->rangep()) || info.left.isReal()
                                || info.right.isReal();
            VhdlType* const typep = VhdlSymTab::newType(
                isReal ? VhdlTypeKind::FLOATING : VhdlTypeKind::INTEGER, name, flp);
            checkDefinitionBounds(nump->rangep(), name, false);
            applyRange(typep, info);
            if (!typep->bounded() && nump->rangep()) {
                err(flp, "The bounds of type '" + name + "' must be locally static");
            }
            return typep;
        }
        if (AstVhdlPhysicalType* const physp = VN_CAST(defp, VhdlPhysicalType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::PHYSICAL, name, flp);
            // The range is in units of the primary unit, so it folds before
            // any unit name exists (5.2.4)
            applyRange(typep, analyzeRange(physp->rangep(), nullptr));
            checkDefinitionBounds(physp->rangep(), name, true);
            bool first = true;
            for (AstNode* unitp = physp->unitsp(); unitp; unitp = unitp->nextp()) {
                AstVhdlUnitDecl* const udp = VN_AS(unitp, VhdlUnitDecl);
                VhdlSymbol* const symp
                    = VhdlSymTab::newSymbol(VhdlSymKind::PHYSICAL_UNIT, udp->name(), udp);
                symp->typep(typep);
                udp->vhdlSymp(symp);
                int64_t scale = 1;
                if (first) {
                    // The primary unit is one of itself by definition
                    symp->value(VhdlValue::mkInt(1, typep));
                } else {
                    // A secondary unit is a multiple of one already declared,
                    // so its value folds against the units seen so far
                    analyzeExpr(udp->valuep(), typep);
                    const VhdlValue val = V3VhdlEval::eval(udp->valuep());
                    if (udp->valuep() && !valueFitsType(udp->valuep(), typep)) {
                        err(udp->fileline(), "A secondary unit is a multiple of a unit of"
                                             " its own physical type, and the one '"
                                                 + udp->name() + "' is given is not one");
                        scale = 0;
                    } else if (val.isInt()) {
                        scale = val.toInt();
                        symp->value(VhdlValue::mkInt(scale, typep));
                    } else {
                        err(udp->fileline(), "The value of physical unit '" + udp->name()
                                                 + "' must be a locally static multiple of a "
                                                   "unit already declared");
                        scale = 0;
                    }
                }
                typep->addUnit(udp->name(), scale);
                first = false;
                declare(symp, udp->fileline());
            }
            return typep;
        }
        if (AstVhdlArrayType* const arrp = VN_CAST(defp, VhdlArrayType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ARRAY, name, flp);
            bool unconstrained = false;
            for (AstNode* idxp = arrp->indexp()->rangesp(); idxp; idxp = idxp->nextp()) {
                // An index is a discrete range; "array (5, 2)" writes values
                // where the ranges belong (5.3.2.1)
                // A lone value arrives as a range the parser could not tell
                // from a type mark or a range attribute
                const AstVhdlRange* const lonep = VN_CAST(idxp, VhdlRange);
                const bool loneValue
                    = lonep && lonep->attrp() && !VN_IS(lonep->attrp(), VhdlAttrName)
                      && !VN_IS(lonep->attrp(), VhdlName) && !VN_IS(lonep->attrp(), VhdlSelected);
                if ((loneValue
                     || (!VN_IS(idxp, VhdlRange) && !VN_IS(idxp, VhdlSubtypeInd)
                         && !VN_IS(idxp, VhdlAttrName) && !VN_IS(idxp, VhdlName)
                         && !VN_IS(idxp, VhdlSelected)))
                    && !m_inSpecialized) {
                    err(idxp->fileline(), "An array index is a discrete range, not a value");
                }
                const RangeInfo info = analyzeRange(idxp, nullptr);
                if (info.typep && !info.typep->isError() && !info.typep->isDiscrete()
                    && !m_inSpecialized) {
                    err(idxp->fileline(), "An array index is a discrete range, but this one is"
                                          " of type "
                                              + info.typep->prettyName());
                }
                VhdlType* indexTypep = info.typep;
                if (!info.unbounded && info.left.known() && info.right.known()) {
                    // "array (0 to 7)" gives a constrained array type, whose
                    // index subtype is anonymous (5.3.2.1)
                    indexTypep = makeRangeSubtype(info, flp);
                } else if (!info.unbounded && info.valid) {
                    // "array (0 to WIDTH-1)" is an ordinary constrained array
                    // whose bounds are not numbers yet. Nothing requires an
                    // index range here to be locally static; it is evaluated
                    // when the type declaration is elaborated (14.4.2), by
                    // which time the generic has a value. So the range is
                    // kept as written for the back end to fold per instance,
                    // rather than falling back on the index base type -- which
                    // would make this an array of four billion elements.
                    // The index may be written as the name of a subtype
                    // rather than as a range -- "array (t_two) of boolean"
                    // -- and then the range to keep is the one that subtype
                    // was declared with, not this name
                    AstNode* const inheritedp = indexTypep ? indexTypep->boundsp() : nullptr;
                    indexTypep
                        = cloneSubtype(indexTypep ? indexTypep : V3VhdlStd::integerType(), flp);
                    indexTypep->unbound();
                    AstVhdlRange* const rp = VN_CAST(idxp, VhdlRange);
                    // A range written out has its own two bounds; one written
                    // as a type mark or as "x'range" does not, and then the
                    // range to keep is the one the subtype it names carries
                    if (rp && rp->leftp() && rp->rightp()) {
                        indexTypep->boundsp(rp);
                    } else if (inheritedp) {
                        indexTypep->boundsp(inheritedp);
                    } else if (rp) {
                        indexTypep->boundsp(rp);
                    }
                }
                if (!indexTypep) indexTypep = V3VhdlStd::integerType();
                if (info.unbounded || !indexTypep->bounded()) {
                    unconstrained = true;
                    // Only a "<>" leaves a dimension with no range at all. A
                    // bound that names a generic is a range, just not a number
                    // yet, and the back end folds it per instance.
                    if (info.unbounded) typep->setDimOpen(typep->dimensions());
                }
                typep->addIndexType(indexTypep);
            }
            typep->openDimsKnown();
            typep->unconstrained(unconstrained);
            typep->elementp(resolveSubtype(arrp->elementp()));
            checkElementConstrained(arrp->elementp()->fileline(), typep->elementp());
            return typep;
        }
        if (AstVhdlRecordType* const recp = VN_CAST(defp, VhdlRecordType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::RECORD, name, flp);
            int pos = 0;
            std::set<std::string> names;  // Element names are distinct (5.3.3)
            for (AstNode* elemp = recp->elemsp(); elemp; elemp = elemp->nextp()) {
                AstVhdlElementDecl* const edp = VN_AS(elemp, VhdlElementDecl);
                if (!names.insert(edp->name()).second) {
                    err(edp->fileline(), "Element '" + edp->prettyName()
                                             + "' is declared twice in this record type");
                }
                VhdlSymbol* const symp
                    = VhdlSymTab::newSymbol(VhdlSymKind::RECORD_ELEMENT, edp->name(), edp);
                symp->typep(resolveSubtype(edp->subtypep()));
                checkElementConstrained(edp->fileline(), symp->typep());
                symp->position(pos++);
                edp->vhdlSymp(symp);
                typep->addElement(symp);
            }
            return typep;
        }
        if (AstVhdlAccessType* const accp = VN_CAST(defp, VhdlAccessType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ACCESS, name, flp);
            typep->designatedp(resolveSubtype(accp->designatedp()));
            // An access value designates an object it may copy and free,
            // which a file is not, nor before VHDL-2019 a protected one (5.4.1)
            const VhdlType* const desp = typep->designatedp();
            if (desp && desp->kind() == VhdlTypeKind::INCOMPLETE)
                m_designatedIncomplete.insert(desp);
            if (desp && !desp->isError() && !m_inSpecialized) {
                const VhdlTypeKind dk = desp->base()->kind();
                if (dk == VhdlTypeKind::FILE
                    || (dk == VhdlTypeKind::PROTECTED
                        && flp->language() < V3LangCode::L1076_2019)) {
                    err(flp, "Access type '" + name + "' cannot designate "
                                 + (dk == VhdlTypeKind::FILE ? "file" : "protected") + " type "
                                 + desp->prettyName());
                }
            }
            return typep;
        }
        if (AstVhdlFileType* const filep = VN_CAST(defp, VhdlFileType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::FILE, name, flp);
            typep->designatedp(resolveTypeMark(filep->designatedp()));
            checkFileElement(filep->fileline(), typep->designatedp());
            return typep;
        }
        if (AstVhdlProtectedType* const protp = VN_CAST(defp, VhdlProtectedType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::PROTECTED, name, flp);
            VhdlScope* const innerp = pushScope(name);
            typep->scopep(innerp);
            iterateAndNextNull(protp->declsp());
            popScope();
            return typep;
        }
        return V3VhdlStd::errorType();
    }

    // A numeric type definition is floating when either bound is a real
    // literal (5.2.5)
    // The bounds of an integer or a physical type definition are each of some
    // integer type, and those of a floating point type definition each of
    // some floating point type; the two need not be of the same type
    // (5.2.3.1, 5.2.4.1, 5.2.5.1). What the new type is follows from them, so
    // "range 1 to 9.0" says two different things at once.
    void checkDefinitionBounds(const AstVhdlRange* rangep, const std::string& name,
                               bool physical) {
        // A range constraint gives a range: two bounds, or an attribute that
        // is one, and not a type mark (5.2.1)
        if (rangep && rangep->attrp() && !VN_IS(rangep->attrp(), VhdlAttrName)
            && !m_inSpecialized) {
            err(rangep->fileline(), "The range of type '" + name
                                        + "' is two bounds or a range attribute, not a"
                                          " type mark");
            return;
        }
        if (!rangep || rangep->attrp() || !rangep->leftp() || !rangep->rightp()) return;
        int cls[2] = {0, 0};  // 1 integer, 2 floating point
        int i = 0;
        for (const AstNodeVhdlExpr* const boundp : {rangep->leftp(), rangep->rightp()}) {
            const VhdlType* const typep = boundp->vhdlTypep();
            if (!typep || typep->isError()) return;
            switch (typep->base()->kind()) {
            case VhdlTypeKind::INTEGER:
            case VhdlTypeKind::UNIVERSAL_INTEGER: cls[i] = 1; break;
            case VhdlTypeKind::FLOATING:
            case VhdlTypeKind::UNIVERSAL_REAL: cls[i] = 2; break;
            default:
                err(boundp->fileline(),
                    "A bound of " + std::string{physical ? "physical" : "numeric"} + " type '"
                        + name + "' must be " + (physical ? "an integer" : "an integer or a real")
                        + ", but this one is of type " + typep->prettyName());
                return;
            }
            ++i;
        }
        if (physical && (cls[0] == 2 || cls[1] == 2)) {
            err(rangep->fileline(), "The bounds of physical type '" + name + "' must be integers");
        } else if (cls[0] != cls[1]) {
            err(rangep->fileline(), "The bounds of type '" + name
                                        + "' must both be integers or both be reals, which is"
                                          " what decides the kind of type it is");
        }
    }

    static bool rangeIsReal(const AstVhdlRange* rangep) {
        if (!rangep) return false;
        for (const AstNodeVhdlExpr* const p : {rangep->leftp(), rangep->rightp()}) {
            for (const AstNode* n = p; n; n = nullptr) {
                if (const AstVhdlLiteral* const litp = VN_CAST(n, VhdlLiteral)) {
                    if (litp->kind() == VhdlLitKind::REAL) return true;
                }
                if (const AstVhdlUnary* const unp = VN_CAST(n, VhdlUnary)) {
                    if (const AstVhdlLiteral* const litp = VN_CAST(unp->lhsp(), VhdlLiteral)) {
                        if (litp->kind() == VhdlLitKind::REAL) return true;
                    }
                }
            }
        }
        return false;
    }

public:
    // CONSTRUCTORS
    VhdlSemVisitor() = default;
    ~VhdlSemVisitor() override = default;

    bool errored() const { return m_errored; }

    void analyzeUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;
    void checkBodiesGiven(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;
    // Whether a value of one type converts to the other (9.3.6): the same
    // base type, two numeric types, or two arrays of the same dimensionality
    // whose element types are closely related in turn. Where either side is
    // not settled nothing is said. A generic type stands for a type nothing is
    // known of, so it is related to itself alone (6.5.3).
    static bool closelyRelated(const VhdlType* ap, const VhdlType* bp, int depth = 0) {
        if (!ap || !bp || depth > 8) return true;
        // Which is kept apart from the rest as a type of kind ERROR
        if (ap->isGenericFormal() || bp->isGenericFormal()) return ap->base() == bp->base();
        if (ap->isError() || bp->isError()) return true;
        const VhdlType* const abp = ap->base();
        const VhdlType* const bbp = bp->base();
        if (abp == bbp) return true;
        const auto numeric = [](VhdlTypeKind k) {
            return k == VhdlTypeKind::INTEGER || k == VhdlTypeKind::FLOATING
                   || k == VhdlTypeKind::UNIVERSAL_INTEGER || k == VhdlTypeKind::UNIVERSAL_REAL;
        };
        const VhdlTypeKind ak = abp->kind();
        const VhdlTypeKind bk = bbp->kind();
        if (numeric(ak) && numeric(bk)) return true;
        if (ak == VhdlTypeKind::INCOMPLETE || bk == VhdlTypeKind::INCOMPLETE) return true;
        if (ak == VhdlTypeKind::ARRAY && bk == VhdlTypeKind::ARRAY) {
            if (abp->indexTypes().size() != bbp->indexTypes().size()) return false;
            return closelyRelated(abp->elementp(), bbp->elementp(), depth + 1);
        }
        return false;
    }
    // Whether a value of this type holds an access value somewhere in it
    static bool holdsAccess(const VhdlType* typep, int depth = 0) {
        if (!typep || depth > 16) return false;
        const VhdlType* const basep = typep->base();
        if (basep->kind() == VhdlTypeKind::ACCESS) return true;
        if (basep->kind() == VhdlTypeKind::ARRAY) return holdsAccess(basep->elementp(), depth + 1);
        if (basep->kind() == VhdlTypeKind::RECORD) {
            for (const VhdlSymbol* const ep : basep->elements()) {
                if (holdsAccess(ep->typep(), depth + 1)) return true;
            }
        }
        return false;
    }
    // A file holds values it can write out and read back, which an access, a
    // file or a protected object is not, and a multi-dimensional array's
    // shape is not something a file records (5.5)
    void checkFileElement(FileLine* flp, const VhdlType* typep) {
        if (!typep || typep->isError()) return;
        const VhdlTypeKind kind = typep->base()->kind();
        std::string why;
        if (kind == VhdlTypeKind::FILE || kind == VhdlTypeKind::PROTECTED) {
            why = std::string{"it is of a "} + (kind == VhdlTypeKind::FILE ? "file" : "protected")
                  + " type";
        } else if (holdsAccess(typep)) {
            why = "it holds an access value";
        } else if (kind == VhdlTypeKind::ARRAY && typep->base()->indexTypes().size() > 1) {
            why = "it is a multi-dimensional array";
        }
        if (why.empty()) return;
        err(flp, "A file cannot hold values of " + typep->prettyName() + ", since " + why);
    }
    // A constant, a signal, a generic and a port hold values, and a value is
    // never an access, a file or a protected object: only a variable can be
    // one of those (6.4.2.2, 6.4.2.3, 6.5.2)
    void checkHoldsValue(FileLine* flp, const VhdlType* typep, const char* what,
                         const std::string& name) {
        if (!typep || typep->isError() || m_inSpecialized) return;
        const VhdlTypeKind kind = typep->base()->kind();
        const char* const which = kind == VhdlTypeKind::ACCESS      ? "an access"
                                  : kind == VhdlTypeKind::FILE      ? "a file"
                                  : kind == VhdlTypeKind::PROTECTED ? "a protected"
                                                                    : nullptr;
        if (!which) return;
        err(flp, std::string{what} + " holds a value, so it cannot be of " + which + " type: '"
                     + name + "' is of type " + typep->prettyName());
    }
    // The class of object a name denotes, and whether it is of mode IN.
    // UNKNOWN where the name reaches its object through an access value or a
    // call, whose class is not the name's to say.
    enum class ObjClass : uint8_t { UNKNOWN, CONSTANT, SIGNAL, VARIABLE, FILE };
    struct ObjInfo final {
        ObjClass cls = ObjClass::UNKNOWN;
        bool modeIn = false;
        bool modeOut = false;
        const VhdlSymbol* symp = nullptr;
    };
    // Subprogram bodies already found able to suspend, or not
    std::map<const AstVhdlSubprogDecl*, bool> m_suspends;
    AstNode* suspendingStmt(AstNode* stmtsp) VL_MT_DISABLED;
    bool procedureSuspends(const AstVhdlProcCall* pcp) VL_MT_DISABLED;
    void checkSensitivityName(AstNodeVhdlExpr* exprp) VL_MT_DISABLED;
    static const VhdlSymbol* nonStaticIndex(AstNodeVhdlExpr* nodep) VL_MT_DISABLED;
    static const VhdlSymbol* nonStaticSignalIndex(AstNodeVhdlExpr* exprp) VL_MT_DISABLED;
    void checkOutReads(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    static bool targetRangeOpen(const AstNodeVhdlExpr* targetp) VL_MT_DISABLED;
    static bool writtenBare(const AstVhdlSubtypeInd* sip) VL_MT_DISABLED;
    static bool markLeavesIndexOpen(const VhdlSymbol* markp, int depth = 0) VL_MT_DISABLED;
    void checkOthersTarget(const AstNodeVhdlExpr* targetp,
                           const AstNodeVhdlExpr* valuep) VL_MT_DISABLED;
    void checkPurity(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    void checkElaborationOrder(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    static ObjInfo objectOfName(const AstNodeVhdlExpr* nodep) VL_MT_DISABLED;
    static ObjInfo objectOfSymbol(const VhdlSymbol* symp) VL_MT_DISABLED;
    static const char* objClassName(ObjClass cls) {
        switch (cls) {
        case ObjClass::CONSTANT: return "a constant";
        case ObjClass::SIGNAL: return "a signal";
        case ObjClass::VARIABLE: return "a variable";
        case ObjClass::FILE: return "a file";
        default: return "not an object";
        }
    }
    void checkAssignTarget(const AstNodeVhdlExpr* targetp, bool signal) VL_MT_DISABLED;
    void checkIndexablePrefix(const AstVhdlCall* nodep) VL_MT_DISABLED;
    void checkDiscreteRange(const RangeInfo& info, const AstNode* rangep,
                            const char* what) VL_MT_DISABLED;
    void checkSequentialForm(const AstNode* nodep, VhdlAssignKind kind) VL_MT_DISABLED;
    bool conflictingPotentials(const std::vector<VhdlSymbol*>& found) const VL_MT_DISABLED;
    void checkConfigSpecs(const AstNode* declsp, const AstNode* stmtsp) VL_MT_DISABLED;
    void checkConfiguration(const AstVhdlConfiguration* cfgp, const std::string& lib,
                            const std::string& ent) VL_MT_DISABLED;
    std::string signalNamedIn(const AstNode* exprsp, const AstNode* stmtp) VL_MT_DISABLED;
    void checkBlockConfigItems(const AstVhdlBlockConfig* bcp,
                               const AstNode* stmtsp) VL_MT_DISABLED;
    void checkAggregateTargetNames(const AstVhdlAggregate* aggp) VL_MT_DISABLED;
    void checkAttrPrefix(AstVhdlAttrName* nodep, const VhdlType* prefixTypep,
                         bool isArray) VL_MT_DISABLED;
    void checkNullWaveforms(AstVhdlSigAssign* nodep) VL_MT_DISABLED;
    void checkActualClass(const VhdlSymbol* subp, int formal,
                          const AstNodeVhdlExpr* actualp) VL_MT_DISABLED;
    // A function computes a value from what it is given, so its parameters
    // are all of mode IN and none is a variable it could write; a procedure's
    // are IN, OUT or INOUT, since BUFFER and LINKAGE describe ports (4.2.2.1)
    void checkParamMode(const AstVhdlSubprogDecl* subp, const AstVhdlIfaceDecl* ifp) {
        if (ifp->kind() != VhdlIfaceKind::CONSTANT && ifp->kind() != VhdlIfaceKind::SIGNAL
            && ifp->kind() != VhdlIfaceKind::VARIABLE) {
            return;
        }
        static const char* const modes[] = {"in", "out", "inout", "buffer", "linkage"};
        const char* const mode = modes[static_cast<int>(ifp->mode())];
        if (subp->isFunction()) {
            if (ifp->mode() != VhdlMode::IN) {
                err(ifp->fileline(), "A parameter of a function can only be of mode in, but '"
                                         + ifp->prettyName() + "' is of mode " + mode);
            } else if (ifp->kind() == VhdlIfaceKind::VARIABLE) {
                err(ifp->fileline(),
                    "A parameter of a function cannot be a variable: '" + ifp->prettyName() + "'");
            }
        } else if (ifp->mode() == VhdlMode::BUFFER || ifp->mode() == VhdlMode::LINKAGE) {
            err(ifp->fileline(), "A parameter of a procedure can only be of mode in, out or"
                                 " inout, but '"
                                     + ifp->prettyName() + "' is of mode " + mode);
        }
    }

    // Give a subprogram the parameter subtypes one call site supplies, and
    // analyze it as if it had been written that way (see specialize())
    VhdlSymbol* specializeSubprogram(AstVhdlSubprogDecl* originalp, VhdlScope* regionp,
                                     const std::vector<VhdlType*>& actualTypes,
                                     const std::vector<VhdlValue>& actualValues,
                                     const std::string& name) VL_MT_DISABLED;
    void findReturnedType(AstNode* stmtsp, VhdlType*& foundp, bool& disagreep, bool skipZero,
                          bool& sawOpen, bool& sawUnfolded) VL_MT_DISABLED;
    static bool returnsACall(AstNode* stmtsp) VL_MT_DISABLED;
    bool writtenWithGenericType(AstNode* subtypep) const VL_MT_DISABLED;
    static bool sameButForUniversal(const std::vector<VhdlSymbol*>& cands) VL_MT_DISABLED;
    VhdlType* intrinsicResultType(VhdlSymbol* symp, const std::vector<AstVhdlAssoc*>& assocs,
                                  VhdlType* resultp) VL_MT_DISABLED;
    VhdlSymbol* specializeSubprogram(AstVhdlSubprogDecl* originalp, VhdlScope* regionp,
                                     const std::vector<VhdlType*>& actualTypes,
                                     const std::string& name) VL_MT_DISABLED;

    // Give every resolved signal's resolution function the copy that takes as
    // many driver values as it has drivers
    void specializeResolvers() VL_MT_DISABLED;
    bool reconstrainDecls(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;
    void mergeIndividualFormals(AstVhdlCall* nodep) VL_MT_DISABLED;
    void mergeIndividualGenerics(AstVhdlInstance* nodep) VL_MT_DISABLED;
    void mergeIndividualIn(AstNode* assocsp, AstVhdlCall* callp,
                           bool alwaysIn) VL_MT_DISABLED;
    static bool formalIsReadOnly(const VhdlSymbol* symp,
                                 const std::string& formal) VL_MT_DISABLED;
    void spreadElabConstants(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;
    // The pass that gives each call to an unconstrained subprogram a copy.
    // A copy's own body is source like any other, and what it calls has to
    // be settled before its result subtype can be read off what it returns.
    void specializer(VhdlSpecVisitor* specp) { m_specp = specp; }
    void specializeWithin(AstNode* nodep) VL_MT_DISABLED;
    // Whether every element of a record has a length of its own (5.3.3)
    static bool fullyConstrainedRecord(const VhdlType* typep) VL_MT_DISABLED;
    // Whether a subtype indication is written with no constraint of its own.
    // One written with a constraint arrives as a call of the type mark.
    static bool writtenUnconstrained(const AstVhdlSubtypeInd* sip) {
        return sip && !sip->constraintp() && sip->typeMarkp() && !VN_IS(sip->typeMarkp(), VhdlCall)
               && !VN_IS(sip->typeMarkp(), VhdlAttrName);
    }
    // Whether a subtype indication leaves some bound open, as the text writes
    // it: "bit_vector" does, and so does a subtype declared as one or a
    // record with such an element, but "bit_vector(x'range)" does not, even
    // where x'range is known only once a subprogram is called (5.3.2.2, 5.3.3)
    // Whether a constraint written as a call -- "t(0 to 3)", "t(open)(7 downto
    // 0)" -- still leaves a bound open: an index said to be 'open', or an
    // array constrained in its index whose element the type leaves open and
    // nothing here constrains (5.3.2.2)
    static bool callConstraintLeavesOpen(const AstNodeVhdlExpr* markp, bool elementFromValue) {
        std::vector<const AstVhdlCall*> levels;  // Outermost first
        while (const AstVhdlCall* const callp = VN_CAST(markp, VhdlCall)) {
            levels.push_back(callp);
            markp = callp->prefixp();
        }
        if (levels.empty()) return false;
        const auto isOpen = [](const AstNode* ap) {
            if (VN_IS(ap, VhdlOpen)) return true;
            const AstVhdlAssoc* const asp = VN_CAST(ap, VhdlAssoc);
            return asp && !asp->formalp() && (!asp->actualp() || VN_IS(asp->actualp(), VhdlOpen));
        };
        // 'open' keeps what the type says, which leaves it open only where
        // the type did
        const VhdlSymbol* const symp = markp ? markp->vhdlSymp() : nullptr;
        const bool typeOpen = symp && symp->typep() && symp->typep()->unconstrained();
        for (const AstNode* ap = levels.back()->argsp(); ap && typeOpen; ap = ap->nextp()) {
            if (isOpen(ap)) return true;
        }
        // An element left open may take its bounds from an initial value,
        // which nvc 1.22.1 accepts
        if (levels.size() > 1 || elementFromValue) return false;
        const AstVhdlTypeDecl* const tdp = symp ? VN_CAST(symp->declp(), VhdlTypeDecl) : nullptr;
        const AstVhdlArrayType* const arrp = tdp ? VN_CAST(tdp->defp(), VhdlArrayType) : nullptr;
        return arrp && leavesBoundsOpen(arrp->elementp());
    }
    static bool leavesBoundsOpen(const AstVhdlSubtypeInd* sip, bool elementFromValue = false) {
        if (sip && !sip->constraintp() && VN_IS(sip->typeMarkp(), VhdlCall)) {
            return callConstraintLeavesOpen(sip->typeMarkp(), elementFromValue);
        }
        const VhdlSymbol* const markSymp
            = sip && sip->typeMarkp() ? sip->typeMarkp()->vhdlSymp() : nullptr;
        if (const AstVhdlIndexConstraint* const icp
            = markSymp && markSymp->typep() && markSymp->typep()->unconstrained()
                  ? VN_CAST(sip->constraintp(), VhdlIndexConstraint)
                  : nullptr) {
            for (const AstNode* rp = icp->rangesp(); rp; rp = rp->nextp()) {
                if (VN_IS(rp, VhdlOpen)) return true;
            }
        }
        if (!writtenUnconstrained(sip)) return false;
        const VhdlSymbol* const symp = sip->typeMarkp()->vhdlSymp();
        if (!symp) return false;
        // A generic type an instantiation has given an unconstrained array
        // type: the copy's objects of it have no bounds (nvc 1.22.1, GHDL
        // 7.0.0-dev; Questa 2025.2 takes it)
        if (const AstVhdlIfaceDecl* const ifp = VN_CAST(symp->declp(), VhdlIfaceDecl)) {
            const VhdlType* const typep = symp->typep();
            return ifp->kind() == VhdlIfaceKind::TYPE && typep && !typep->isError()
                   && typep->base()->kind() == VhdlTypeKind::ARRAY && typep->unconstrained();
        }
        if (const AstVhdlSubtypeDecl* const sdp = VN_CAST(symp->declp(), VhdlSubtypeDecl)) {
            return leavesBoundsOpen(sdp->subtypep());
        }
        if (const AstVhdlTypeDecl* const tdp = VN_CAST(symp->declp(), VhdlTypeDecl)) {
            if (const AstVhdlArrayType* const arrp = VN_CAST(tdp->defp(), VhdlArrayType)) {
                if (!arrp->indexp()) return false;
                for (const AstNode* rp = arrp->indexp()->rangesp(); rp; rp = rp->nextp()) {
                    const AstVhdlSubtypeInd* const isp = VN_CAST(rp, VhdlSubtypeInd);
                    if (isp && VN_IS(isp->constraintp(), VhdlBox)) return true;
                }
                return leavesBoundsOpen(arrp->elementp());
            }
            if (const AstVhdlRecordType* const recp = VN_CAST(tdp->defp(), VhdlRecordType)) {
                for (const AstNode* ep = recp->elemsp(); ep; ep = ep->nextp()) {
                    const AstVhdlElementDecl* const edp = VN_CAST(ep, VhdlElementDecl);
                    if (edp && leavesBoundsOpen(edp->subtypep())) return true;
                }
            }
            return false;
        }
        // A type STD.STANDARD declares is built in code and has no declaration
        if (symp->declp()) return false;
        const VhdlType* const typep = symp->typep();
        return typep && typep->base()->kind() == VhdlTypeKind::ARRAY && typep->unconstrained();
    }
    // Before VHDL-2008 an element of a composite type has all its bounds
    // (VHDL-93 3.2.1.1, 3.2.2): only 2008 lets them be given per object
    void checkElementConstrained(FileLine* flp, const VhdlType* elemp) {
        if (!elemp || elemp->isError() || flp->language() >= V3LangCode::L1076_2008) return;
        if (elemp->base()->kind() != VhdlTypeKind::ARRAY || elemp->lengthEmittable()) return;
        err(flp, "Before VHDL-2008 an element of an array or record must be constrained, but "
                     + elemp->prettyName() + " leaves its bounds open");
    }
    // Whether a subprogram may be called with no arguments at all (4.2.2.1)
    static bool allParamsDefaulted(const VhdlSymbol* symp) VL_MT_DISABLED;
    // A record subtype whose open elements take their bounds from the values
    // written for them one at a time (5.3.3, 6.5.7.1)
    VhdlType* constrainRecordFromParts(VhdlType* typep,
                                       const std::vector<AstNodeVhdlExpr*>& byPos,
                                       FileLine* flp) VL_MT_DISABLED;
    VhdlType* constrainArrayFromIndices(VhdlType* typep, const std::vector<int64_t>& indices,
                                        FileLine* flp) VL_MT_DISABLED;

private:
    void visit(AstNode* nodep) override { iterateChildren(nodep); }
    // Declarations
    void visit(AstVhdlTypeDecl* nodep) override;
    void visit(AstVhdlSubtypeDecl* nodep) override;
    void visit(AstVhdlObjectDecl* nodep) override;
    void visit(AstVhdlIfaceDecl* nodep) override;
    void visit(AstVhdlSubprogDecl* nodep) override;
    void visit(AstVhdlComponentDecl* nodep) override;
    void visit(AstVhdlAliasDecl* nodep) override;
    void visit(AstVhdlPackage* nodep) override;
    void visit(AstVhdlPackageInst* nodep) override;
    void visit(AstVhdlSubprogInst* nodep) override;

    void instantiatePackage(AstVhdlPackageInst* nodep, VhdlScope* intop) VL_MT_DISABLED;
    void recordTemplateSyms(AstNode* origp, AstNode* copyp) VL_MT_DISABLED;
    void bindPackageGeneric(AstVhdlIfaceDecl* formalp, AstNodeVhdlExpr* actualp,
                            VhdlScope* outerp) VL_MT_DISABLED;
    std::map<std::string, AstVhdlPackageBody*> m_packageBodies;  // "lib.name" -> its body
    // Every generic subprogram body in the design, by name, for instantiation
    std::multimap<std::string, AstVhdlSubprogDecl*> m_genericBodies;
    void visit(AstVhdlAttrDecl* nodep) override;
    void visit(AstVhdlAttrSpec* nodep) override;
    void recordUserAttr(AstVhdlAttrSpec* nodep) VL_MT_DISABLED;
    bool parseForeign(const std::string& text, FileLine* flp, std::string& cnamer,
                      bool& dpir);
    void visit(AstVhdlContextItem* nodep) override;
    // Regions
    void visit(AstVhdlProcess* nodep) override;
    void visit(AstVhdlBlock* nodep) override;
    void visit(AstVhdlGenerate* nodep) override;
    void visit(AstVhdlGenerateBody* nodep) override;
    VhdlType* m_genSelTypep = nullptr;  // Selector of the case generate being analyzed
    void visit(AstVhdlLoop* nodep) override;
    // Statements, where the expected type of each expression is known
    void visit(AstVhdlSigAssign* nodep) override;
    void visit(AstVhdlVarAssign* nodep) override;
    void visit(AstVhdlIf* nodep) override;
    // A branch whose condition a copy settled to false: nothing reaches it
    bool m_unreached = false;
    void visit(AstVhdlCase* nodep) override;
    void visit(AstVhdlAssert* nodep) override;
    void visit(AstVhdlWait* nodep) override;
    void visit(AstVhdlReturn* nodep) override;
    void visit(AstVhdlProcCall* nodep) override;
    void visit(AstVhdlNext* nodep) override;
    void visit(AstVhdlInstance* nodep) override;
    void visit(AstVhdlConfigSpec* nodep) override;
    // A port whose subtype is left unconstrained takes its bounds from the
    // actual, which differ per instance (5.3.2.2, 6.5.6.3)
    struct OpenBound final {
        std::string port;  // The port the bounds belong to
        // Where inside its type the array is: each step names a record
        // element, or is empty for "the element of the array above". Empty
        // for the port's own type, which is the ordinary case.
        std::vector<std::string> path;
        int dim;  // Which of its dimensions
        std::string leftName;  // The generics carrying the bounds
        std::string rightName;
        std::string ascName;  // And which way round they run
        bool isGeneric;  // A generic left unconstrained, rather than a port
    };
    std::map<VhdlScope*, std::vector<OpenBound>> m_openBounds;
    void openPortGenerics(AstNode* portsp, AstNodeVhdl* unitp, VhdlScope* scopep) VL_MT_DISABLED;
    AstVhdlIndexConstraint* openConstraintFor(VhdlType* typep, AstVhdlIfaceDecl* ifp,
                                              AstVhdlEntity* entp, AstVhdlComponentDecl* compp,
                                              VhdlScope* scopep,
                                              std::vector<std::string>& path) VL_MT_DISABLED;
    bool hasConditionOperator(const VhdlType* typep) VL_MT_DISABLED;
    void bindOpenBounds(AstVhdlInstance* nodep, VhdlScope* unitScopep) VL_MT_DISABLED;
    void bindAscending(AstVhdlInstance* nodep, const OpenBound& bound,
                       AstNodeVhdlExpr* prefixp, bool ascending,
                       FileLine* flp) VL_MT_DISABLED;
    VhdlScope* specializeEntity(AstVhdlInstance* nodep, const std::string& entKey)
        VL_MT_DISABLED;
    void analyzeSpecializedUnit(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    void retargetInstance(AstVhdlInstance* nodep, const std::string& newName,
                          const std::vector<AstVhdlIfaceDecl*>& generics,
                          const std::vector<AstVhdlAssoc*>& assocs) VL_MT_DISABLED;
    void bindOpenGenericBounds(AstVhdlInstance* nodep, VhdlScope* unitScopep) VL_MT_DISABLED;
    void fillOpenGenericDefaults(VhdlScope* scopep) VL_MT_DISABLED;
    bool defaultBounds(VhdlSymbol* symp, AstNodeVhdlExpr* valuep, int dim, int64_t& leftr,
                       int64_t& rightr) VL_MT_DISABLED;
    void constrainBlockPorts(AstVhdlBlockHeader* nodep) VL_MT_DISABLED;

    VhdlType* loopParamType(const RangeInfo& info, FileLine* flp,
                            AstVhdlRange* rangep = nullptr) VL_MT_DISABLED;
    void analyzeChoice(AstVhdlChoice* chp, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeIndexedName(AstVhdlCall* nodep, VhdlType* arrayp) VL_MT_DISABLED;
    VhdlType* analyzeSliceName(AstVhdlCall* nodep, VhdlType* arrayp) VL_MT_DISABLED;
    int64_t staticLength(AstNodeVhdlExpr* nodep) VL_MT_DISABLED;
    void checkArrayValue(AstNodeVhdlExpr* nodep, VhdlType* typep,
                         const std::string& what) VL_MT_DISABLED;
    void checkAssignedValue(AstNodeVhdlExpr* nodep, VhdlType* targetp) VL_MT_DISABLED;
    bool valueFitsType(AstNodeVhdlExpr* nodep, VhdlType* targetp) VL_MT_DISABLED;
    static bool altUnreached(const AstVhdlAlternative* altp, bool& settled) VL_MT_DISABLED;
    void checkCaseChoices(AstVhdlCase* nodep, VhdlType* typep) VL_MT_DISABLED;
    void checkSelector(AstNodeVhdlExpr* exprp, const VhdlType* typep,
                       const std::string& what) VL_MT_DISABLED;
    void checkGuardedTargets(AstVhdlSigAssign* nodep) VL_MT_DISABLED;
    void checkChoices(AstNode* altsp, bool matching, FileLine* flp, VhdlType* typep,
                      const std::string& what) VL_MT_DISABLED;
    void checkWaveform(AstNode* wavesp) VL_MT_DISABLED;
    void checkDelay(AstNodeVhdlExpr* nodep, const char* what) VL_MT_DISABLED;
    void checkAggregate(AstVhdlAggregate* nodep, VhdlType* typep,
                        const std::string& what) VL_MT_DISABLED;
    int64_t aggregateLength(AstVhdlAggregate* nodep) VL_MT_DISABLED;
    int64_t elementSpan(AstNodeVhdlExpr* valuep, VhdlType* arrayTypep) VL_MT_DISABLED;
    VhdlType* aggregateSubtype(AstVhdlAggregate* nodep, VhdlType* typep) VL_MT_DISABLED;
    void checkCompositeElements(VhdlType* typep, FileLine* flp) VL_MT_DISABLED;
    bool checkAssignedType(AstNodeVhdlExpr* nodep, VhdlType* targetp) VL_MT_DISABLED;
    static bool indexedCallWouldFit(const AstNodeVhdlExpr* nodep, const VhdlType* valuep,
                                    const VhdlType* targetp) VL_MT_DISABLED;
    void checkSignalAttrOfOutPort(AstVhdlAttrName* nodep,
                                  const std::string& attrName) VL_MT_DISABLED;
    VhdlType* convertedType(VhdlType* top, AstVhdlAssoc* argp) VL_MT_DISABLED;
    VhdlType* constrainFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                 FileLine* flp) VL_MT_DISABLED;
    VhdlType* aggregateTargetType(AstNodeVhdlExpr* valuep, AstNode* wavesp,
                                  AstNode* altsp) VL_MT_DISABLED;
    VhdlType* constrainRecordFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                      FileLine* flp) VL_MT_DISABLED;
    VhdlType* constrainElementFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                        FileLine* flp) VL_MT_DISABLED;
    static std::string dimensionText(size_t dim, size_t dims) VL_MT_DISABLED;
    void setupUnitContext(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    void applyContextItem(AstVhdlContextItem* ctxp) VL_MT_DISABLED;
    void declareLibrary(const std::string& name) VL_MT_DISABLED;
    static std::set<std::string> usedNames(const VhdlScope* pkgScopep,
                                           const std::string& name) VL_MT_DISABLED;
    void analyzeAssociations(AstNode* assocsp, VhdlScope* unitScopep,
                             bool generics = false) VL_MT_DISABLED;
    void analyzeDrivers(AstVhdlArchitecture* archp) VL_MT_DISABLED;
    static bool isLogicValued(const VhdlType* typep) VL_MT_DISABLED;
    static bool driversCombined(const VhdlType* typep, int depth = 0) VL_MT_DISABLED;
    void collectCallDrivers(AstNode* stmtp, std::set<const VhdlSymbol*>& whole,
                            std::set<const VhdlSymbol*>& partial) VL_MT_DISABLED;
    struct NeedResolve final {
        VhdlSymbol* symp;
        VhdlSymbol* resolvep;
        int count;
        FileLine* flp;
    };
    std::vector<NeedResolve> m_needResolve;  // Signals awaiting a sized resolver
    static VhdlType* formalAt(VhdlScope* scopep, int position) VL_MT_DISABLED;
    static VhdlSymbol* formalSymAt(VhdlScope* scopep, int position,
                                   bool generics) VL_MT_DISABLED;
    static const VhdlSymbol* portAt(VhdlScope* scopep, int position) VL_MT_DISABLED;
    void markDrivingPorts(AstVhdlInstance* nodep, VhdlScope* unitScopep) VL_MT_DISABLED;
    void checkPortAssociations(AstVhdlInstance* nodep, VhdlScope* unitScopep) VL_MT_DISABLED;
    void checkPartialAssociations(AstVhdlInstance* nodep) VL_MT_DISABLED;
    static const VhdlSymbol* notLocallyStatic(const AstNode* exprsp) VL_MT_DISABLED;
    void checkSelectorStatic(const AstNodeVhdlExpr* exprp, const std::string& what) VL_MT_DISABLED;
    void checkGenericAssociations(AstVhdlInstance* nodep, VhdlScope* unitScopep) VL_MT_DISABLED;
    static void dumpScope(const VhdlScope* scopep, const std::string& indent) VL_MT_DISABLED;

    // Expressions (clauses 8, 9, 12.5)
    //
    // Overload resolution in VHDL runs in both directions: an expression's
    // possible types are gathered from the bottom up, and the context then
    // picks one from the top down. analyzeExpr does both, with wantp carrying
    // the type the context expects, or null where the context imposes none.
    VhdlType* analyzeExpr(AstNodeVhdlExpr* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeName(AstVhdlName* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeLiteral(AstVhdlLiteral* nodep, VhdlType* wantp) VL_MT_DISABLED;
    void checkLiteralChars(AstVhdlLiteral* nodep, VhdlType* arrayp) VL_MT_DISABLED;
    VhdlType* analyzeUnary(AstVhdlUnary* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeBinary(AstVhdlBinary* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeCall(AstVhdlCall* nodep, VhdlType* wantp) VL_MT_DISABLED;
    static bool bindActuals(const VhdlSymbol* symp, const std::vector<AstVhdlAssoc*>& assocs,
                            std::vector<int>& formalOf,
                            std::vector<std::string>* elemOf = nullptr) VL_MT_DISABLED;
    VhdlType* analyzeSelected(AstVhdlSelected* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeAttrName(AstVhdlAttrName* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeAggregate(AstVhdlAggregate* nodep, VhdlType* wantp) VL_MT_DISABLED;

    // Choose among candidate interpretations, reporting when the choice is
    // not unique (12.5)
    VhdlType* pick(AstNodeVhdlExpr* nodep, std::vector<VhdlSymbol*>& cands, VhdlType* wantp,
                   const std::string& what) VL_MT_DISABLED;

    // True if an actual of type 'fromp' may stand where 'top' is wanted. A
    // literal's universal type (9.3.2) fits any type of the same class.
    static bool typesMatch(const VhdlType* fromp, const VhdlType* top) VL_MT_DISABLED;

    // What an expression could be, without committing to one interpretation:
    // the bottom-up half of the overload resolution of 12.5. An expression
    // with no type of its own, such as an aggregate or a string literal, is
    // recorded as a restriction on the class of type it will accept.
    struct CandSet final {
        std::vector<VhdlType*> types;  // Empty when the context decides
        enum class Need : uint8_t { ANY, ARRAY, ACCESS, COMPOSITE } need = Need::ANY;
        // The distinct characters of a string literal. Only an array whose
        // element type has a literal for each of them can be its type, which
        // is what tells "101" apart as a BIT_VECTOR and not a BOOLEAN_VECTOR
        // (9.3.2).
        std::string chars;
    };
    void candidateTypes(AstNodeVhdlExpr* nodep, CandSet& out) VL_MT_DISABLED;
    static void dropUninstantiated(std::vector<VhdlSymbol*>& found) VL_MT_DISABLED;
    VhdlType* attrTypeQuiet(AstVhdlAttrName* nodep) VL_MT_DISABLED;
    static bool scopeEncloses(const VhdlScope* outerp, const VhdlScope* innerp) VL_MT_DISABLED;
    // Set while the call that is a statement of its own is resolved
    bool m_procCall = false;
    // The expression that is a condition, if any: only that one has the
    // condition operator applied to it (9.2.9)
    const AstNodeVhdlExpr* m_condExprp = nullptr;
    // Set while a copy made for one call site is analyzed: the bounds inside
    // it fold because the call supplied them, not because the design wrote
    // them, so a range that does not fit is a run-time condition
    bool m_inSpecialized = false;
    // True if the candidates could stand where 'wantp' is expected
    static bool anyMatches(const CandSet& cands, const VhdlType* wantp) VL_MT_DISABLED;
    // True if that match needs no implicit conversion (9.3.2)
    static bool exactMatch(const CandSet& cands, const VhdlType* wantp) VL_MT_DISABLED;
    // True when every reading of an operand is of a universal type
    static bool allUniversal(const CandSet& cands) VL_MT_DISABLED;
    // The distinct characters of a string literal, in no particular order
    static std::string distinctChars(const std::string& text) VL_MT_DISABLED;
    // True if an array's element type is an enumeration with a literal for
    // every one of those characters
    static bool elementHasChars(const VhdlType* arrayp,
                                const std::string& chars) VL_MT_DISABLED;
    // Where every operand is universal, nothing asks for a conversion, so the
    // universal operator is the one meant (9.3.2). Narrows in place.
    static void preferUniversal(std::vector<VhdlSymbol*>& cands,
                                const std::vector<const CandSet*>& operands) VL_MT_DISABLED;
};

//######################################################################
// Declarations

void VhdlSemVisitor::visit(AstVhdlTypeDecl* nodep) {
    // A protected type body completes a declaration rather than making a new
    // type (5.6.3), and its declarations belong to the region that
    // declaration opened, so that the methods see the type's own variables
    if (AstVhdlProtectedType* const protp = VN_CAST(nodep->defp(), VhdlProtectedType)) {
        if (protp->isBody()) {
            std::vector<VhdlSymbol*> found;
            m_scopep->lookupLocal(nodep->name(), found);
            // In a package body the declaration being completed is in the
            // package declaration, one region out (12.1)
            if (found.empty() && m_inPackageBody) m_scopep->lookup(nodep->name(), found);
            VhdlSymbol* declp = nullptr;
            for (VhdlSymbol* const symp : found) {
                if (symp->kind() == VhdlSymKind::TYPE && symp->typep()
                    && symp->typep()->kind() == VhdlTypeKind::PROTECTED) {
                    declp = symp;
                    break;
                }
            }
            if (!declp) {
                err(nodep->fileline(), "Protected type body '" + nodep->name()
                                           + "' has no protected type declaration in this region");
                return;
            }
            nodep->vhdlSymp(declp);
            declp->typep()->bodyp(protp);
            VL_RESTORER(m_scopep);
            // What is visible where the body is written is visible inside
            // it: the body's region is nested in that one (12.1), so it sees
            // what that region declares and also what a use clause there has
            // made visible -- potential visibility does not chain on its own
            declp->typep()->scopep()->addUse(m_scopep);
            for (VhdlScope* const usedp : m_scopep->uses()) {
                declp->typep()->scopep()->addUse(usedp);
            }
            m_scopep = declp->typep()->scopep();
            iterateAndNextNull(protp->declsp());
            return;
        }
        // A protected type's own name is visible inside its declaration: its
        // methods take parameters of the type itself (5.6.2), so the name has
        // to be declared before they are analyzed
        VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::PROTECTED, nodep->name(),
                                                    nodep->fileline());
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, nodep->name(), nodep);
        symp->typep(typep);
        nodep->vhdlSymp(symp);
        declare(symp, nodep->fileline());
        V3VhdlStd::addImplicitOperators(m_scopep, typep);
        VhdlScope* const innerp = pushScope(nodep->name());
        typep->scopep(innerp);
        // A protected type's declaration says what its methods are and
        // nothing more: subprograms, attribute specifications and use
        // clauses (5.6.2)
        for (AstNode* ip = protp->declsp(); ip && !m_inSpecialized; ip = ip->nextp()) {
            if (VN_IS(ip, VhdlSubprogDecl) || VN_IS(ip, VhdlAttrSpec) || VN_IS(ip, VhdlContextItem)
                || VN_IS(ip, VhdlSubprogInst)) {
                continue;
            }
            err(ip->fileline(), "A protected type declaration declares only its methods,"
                                " attribute specifications and use clauses");
        }
        iterateAndNextNull(protp->declsp());
        popScope();
        return;
    }
    VhdlType* const typep = buildType(nodep->defp(), nodep->name(), nodep->fileline());
    typep->name(nodep->name());
    // An incomplete type declaration is completed by the full one, not
    // redeclared by it, and an access type already declared against the name
    // must see the completion (5.4.2)
    std::vector<VhdlSymbol*> prior;
    m_scopep->lookupLocal(nodep->name(), prior);
    for (VhdlSymbol* const otherp : prior) {
        if (otherp->kind() != VhdlSymKind::TYPE || !otherp->typep()) continue;
        if (otherp->typep()->kind() != VhdlTypeKind::INCOMPLETE) continue;
        // It was declared so an access type could designate it, which a
        // file cannot be, nor before VHDL-2019 a protected object (5.4.2)
        if ((typep->kind() == VhdlTypeKind::FILE
             || (typep->kind() == VhdlTypeKind::PROTECTED
                 && nodep->fileline()->language() < V3LangCode::L1076_2019))
            && m_designatedIncomplete.count(otherp->typep()) && !m_inSpecialized) {
            err(nodep->fileline(), "Incomplete type '" + nodep->name()
                                       + "' is completed as a type the access type designating"
                                         " it cannot designate");
        }
        otherp->typep()->becomes(typep);
        otherp->typep()->name(nodep->name());
        nodep->vhdlSymp(otherp);
        V3VhdlStd::addImplicitOperators(m_scopep, otherp->typep());
        return;
    }
    checkCompositeElements(typep, nodep->fileline());
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, nodep->name(), nodep);
    symp->typep(typep);
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
    // A type declaration implicitly declares its operators (5.2.6)
    V3VhdlStd::addImplicitOperators(m_scopep, typep);
}

// What a composite type is made of. A file is a handle on something outside
// the design and a protected type is an object with an identity, so neither
// can be copied, and an element of a composite is copied whenever the
// composite is (5.3.1, 5.3.3).
void VhdlSemVisitor::checkCompositeElements(VhdlType* typep, FileLine* flp) {
    if (!typep) return;
    const auto bad = [&](const VhdlType* elemp, const std::string& what) {
        if (!elemp) return;
        const VhdlTypeKind kind = elemp->base()->kind();
        if (kind != VhdlTypeKind::FILE && kind != VhdlTypeKind::PROTECTED) return;
        err(flp, std::string{kind == VhdlTypeKind::FILE ? "A file" : "A protected"}
                     + " type cannot be " + what + ": '" + elemp->prettyName() + "'");
    };
    if (typep->kind() == VhdlTypeKind::ARRAY) {
        bad(typep->elementp(), "the element of an array type");
    } else if (typep->kind() == VhdlTypeKind::RECORD) {
        for (const VhdlSymbol* const elemp : typep->elements()) {
            bad(elemp->typep(), "the type of a record element");
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlSubtypeDecl* nodep) {
    VL_RESTORER_COPY(m_declaring);
    m_declaring.insert(nodep->name());
    VhdlType* const typep = resolveSubtype(nodep->subtypep());
    // resolveSubtype makes a fresh type when the indication is constrained;
    // that one is ours to name. An unconstrained indication returns the base
    // type itself, which must keep the name it already has.
    if (typep && typep->name().empty()) typep->name(nodep->name());
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, nodep->name(), nodep);
    symp->typep(typep);
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::visit(AstVhdlObjectDecl* nodep) {
    VL_RESTORER_COPY(m_declaring);
    m_declaring.insert(nodep->name());
    VhdlSymKind kind = VhdlSymKind::CONSTANT;
    switch (nodep->kind()) {
    case VhdlObjKind::CONSTANT: kind = VhdlSymKind::CONSTANT; break;
    case VhdlObjKind::SIGNAL: kind = VhdlSymKind::SIGNAL; break;
    case VhdlObjKind::VARIABLE: kind = VhdlSymKind::VARIABLE; break;
    case VhdlObjKind::SHARED_VARIABLE: kind = VhdlSymKind::SHARED_VARIABLE; break;
    case VhdlObjKind::FILE: kind = VhdlSymKind::FILE; break;
    }
    // A pure function reads and writes nothing outside itself, and a file
    // is outside whatever declares it (4.3)
    if (kind == VhdlSymKind::FILE && !m_inSpecialized) {
        const AstVhdlSubprogDecl* const subp = VN_CAST(parentOf(nodep), VhdlSubprogDecl);
        if (subp && subp->isFunction() && subp->vhdlPure()) {
            err(nodep->fileline(),
                "A pure function cannot declare a file: '" + nodep->prettyName() + "'");
        }
    }
    VhdlSymbol* symp = nullptr;
    // A package body gives a deferred constant its value; that completes the
    // declaration in the package, it does not make a new one (6.4.2.2)
    if (m_inPackageBody && kind == VhdlSymKind::CONSTANT && nodep->defaultp()) {
        std::vector<VhdlSymbol*> found;
        m_scopep->lookup(nodep->name(), found);
        for (VhdlSymbol* const otherp : found) {
            if (otherp->kind() == VhdlSymKind::CONSTANT && otherp->deferred()) {
                // The full declaration repeats the deferred one's subtype
                // indication lexically (4.7, 4.10)
                const AstVhdlObjectDecl* const deferredp
                    = VN_CAST(otherp->declp(), VhdlObjectDecl);
                if (deferredp && !deferredp->specText().empty() && !nodep->specText().empty()
                    && deferredp->specText() != nodep->specText() && !m_inSpecialized
                    && !m_inPackageInstance) {
                    err(nodep->fileline(),
                        "The full declaration of deferred constant '" + nodep->prettyName()
                            + "' does not repeat its subtype exactly as the deferred one"
                              " writes it\n"
                            + nodep->warnContextPrimary() + "\n" + deferredp->warnOther()
                            + "... Location of the deferred declaration\n"
                            + deferredp->warnContextSecondary());
                }
                symp = otherp;
                symp->deferred(false);
                // The value is written here, not where the constant was
                // declared, and it is this declaration that carries it
                symp->fullDeclp(nodep);
                break;
            }
        }
    }
    if (symp) {
        nodep->vhdlSymp(symp);
    } else {
        symp = VhdlSymTab::newSymbol(kind, nodep->name(), nodep);
        symp->typep(resolveSubtype(nodep->subtypep()));
        nodep->vhdlSymp(symp);
        declare(symp, nodep->fileline());
        // Only a constant takes its bounds from its value. A signal or a
        // variable is given them where it is declared, since what it holds
        // later can be of any length the subtype allows (5.3.2.2, 5.3.3).
        const VhdlType* const typep = symp->typep();
        if (kind == VhdlSymKind::CONSTANT) {
            checkHoldsValue(nodep->fileline(), typep, "A constant", nodep->prettyName());
        } else if (kind == VhdlSymKind::SIGNAL) {
            checkHoldsValue(nodep->fileline(), typep, "A signal", nodep->prettyName());
        }
        // A constant given no value is deferred, and only a package
        // declaration may defer one: its package body gives the value
        // (4.7, 6.4.2.2)
        if (kind == VhdlSymKind::CONSTANT && !nodep->defaultp() && !m_inSpecialized) {
            bool inPackage = false;
            for (const AstNode* upp = parentOf(nodep); upp; upp = parentOf(upp)) {
                if (VN_IS(upp, VhdlSubprogDecl) || VN_IS(upp, VhdlProcess)
                    || VN_IS(upp, VhdlProtectedType)) {
                    break;
                }
                if (VN_IS(upp, VhdlPackage) || VN_IS(upp, VhdlPackageInst)) {
                    inPackage = true;
                    break;
                }
                if (VN_IS(upp, NodeVhdlUnit)) break;
            }
            if (!inPackage && !m_inPackageInstance) {
                err(nodep->fileline(), "A constant given no value is a deferred constant, which"
                                       " only a package declaration may declare: '"
                                           + nodep->prettyName() + "'");
            }
        }
        // Since VHDL-2000 a shared variable is of a protected type, which is
        // what makes it safe for several processes to use (6.4.2.4). UVVM
        // is written with ordinary ones throughout and built with GHDL's
        // -frelaxed and nvc's --relaxed, and Questa 2025.2 only warns, so
        // this warns too.
        if (kind == VhdlSymKind::SHARED_VARIABLE && typep && !typep->isError()
            && typep->base()->kind() != VhdlTypeKind::PROTECTED
            && typep->base()->kind() != VhdlTypeKind::INCOMPLETE
            && nodep->fileline()->language() >= V3LangCode::L1076_2000 && !m_inSpecialized) {
            nodep->v3warn(VHDLSHARED, "A shared variable is of a protected type since"
                                      " VHDL-2000, but '"
                                          << nodep->prettyName() << "' is of type "
                                          << typep->prettyName());
        }
        if ((kind == VhdlSymKind::SIGNAL || kind == VhdlSymKind::VARIABLE) && typep
            && !typep->isError()
            // VHDL-2019 lets an initial value give them (6.4.2.3)
            && !(nodep->defaultp() && nodep->fileline()->language() >= V3LangCode::L1076_2019)
            && leavesBoundsOpen(nodep->subtypep(), nodep->defaultp() != nullptr)) {
            err(nodep->fileline(), std::string{"A "} + symp->kindName()
                                       + " must be given its bounds"
                                       + " where it is declared, but " + typep->prettyName()
                                       + " leaves them open; only a constant takes them from"
                                         " its value: '"
                                       + nodep->prettyName() + "'");
        }
    }
    // "file f : t [open <mode>] is <name>" names the file to open and how
    // (6.4.2.5). The name is a STRING and the mode a FILE_OPEN_KIND.
    if (nodep->fileNamep()) {
        analyzeExpr(nodep->fileNamep(), V3VhdlStd::stringType());
    }
    if (nodep->fileOpenp()) analyzeExpr(nodep->fileOpenp(), nullptr);
    if (nodep->defaultp()) {
        const int errsBefore = V3Error::errorCount();
        analyzeExpr(nodep->defaultp(), symp->typep());
        VhdlType* const declaredp = symp->typep();
        // The initial value is of the object's type (6.4.2.2 - 6.4.2.4).
        // Where analyzing it said something already, that was the mistake.
        if (V3Error::errorCount() == errsBefore && !valueFitsType(nodep->defaultp(), declaredp)) {
            err(nodep->defaultp()->fileline(),
                "The initial value of " + std::string{symp->kindName()} + " '"
                    + nodep->prettyName() + "' must be of type " + declaredp->prettyName()
                    + ", but is of type " + nodep->defaultp()->vhdlTypep()->prettyName());
        }
        symp->typep(constrainFromValue(declaredp, nodep->defaultp(), nodep->fileline()));
        // Where the value is what fixed the bounds, it has to be looked at
        // again with them known: an aggregate carries the type it was checked
        // against, and hands its elements the type that gives them
        if (symp->typep() != declaredp && symp->typep()
            && symp->typep()->isConstrainedArray()) {
            if (AstVhdlAggregate* const aggp = VN_CAST(nodep->defaultp(), VhdlAggregate)) {
                // An aggregate keeps the type it was checked against, and
                // hands its elements the type that gives them, so both have
                // to be forgotten before the bounds can reach them
                aggp->foreach([](AstVhdlAggregate* p) { p->vhdlTypep(nullptr); });
                analyzeExpr(aggp, symp->typep());
            }
        }
        checkValueInRange(nodep->defaultp(), symp->typep(),
                          std::string{symp->kindName()} + " '" + nodep->name() + "'");
        // A scalar subtype with a null range has no value at all (5.2.1), so
        // none can be given an object of it (nvc 1.22.1 and Questa 2025.2;
        // GHDL 7.0.0-dev warns)
        if (symp->typep() && symp->typep()->isScalar() && symp->typep()->bounded()
            && isNullRange(symp->typep()) && !m_inSpecialized
            && V3VhdlEval::eval(nodep->defaultp()).known()) {
            err(nodep->defaultp()->fileline(),
                std::string{"The subtype of "} + symp->kindName() + " '" + nodep->name()
                    + "' has a null range, so no value can be given it");
        }
        // An object whose own bounds are not locally static -- taken from a
        // function's result, say -- is checked against its value when the
        // design elaborates, which is where the references report it
        if (!notLocallyStatic(nodep->subtypep())) {
            checkArrayValue(nodep->defaultp(), symp->typep(),
                            std::string{symp->kindName()} + " '" + nodep->name() + "'");
        }
        // A constant with a static value is what every later fold builds on
        if (kind == VhdlSymKind::CONSTANT) symp->value(V3VhdlEval::eval(nodep->defaultp()));
    } else if (kind == VhdlSymKind::CONSTANT) {
        // A constant declared with no value in a package declaration is
        // deferred; its value comes with the package body (6.4.2.2)
        symp->deferred(true);
    }
}

void VhdlSemVisitor::visit(AstVhdlIfaceDecl* nodep) {
    // In a copy of an entity made for one instance, a generic subprogram or
    // package the instance said nothing about is the one visible where the
    // instance is written, which "is <>" asks for (6.5.5). Inside the copy
    // that is the ordinary visible declaration, so declaring the generic
    // again would only make every use of it ambiguous.
    if (nodep->isBound()
        && (nodep->kind() == VhdlIfaceKind::SUBPROGRAM
            || nodep->kind() == VhdlIfaceKind::PACKAGE)) {
        return;
    }
    // In a copy of an entity made for one instance, a generic subprogram is
    // the subprogram that instance named, so the name denotes it directly
    if (nodep->kind() == VhdlIfaceKind::SUBPROGRAM) {
        const auto boundIt = m_boundGenSubs.find(nodep->name());
        if (boundIt != m_boundGenSubs.end()) {
            VhdlSymbol* const targetp = boundIt->second;
            VhdlSymbol* const aliasp
                = VhdlSymTab::newSymbol(targetp->kind(), nodep->name(), nodep);
            aliasp->aliasp(targetp);
            aliasp->typep(targetp->typep());
            aliasp->returnTypep(targetp->returnTypep());
            for (size_t i = 0; i < targetp->paramTypes().size(); ++i) {
                aliasp->addParam(targetp->paramName(i), targetp->paramTypes()[i],
                                 targetp->paramHasDefault(i));
            }
            aliasp->bodyp(targetp->bodyp());
            aliasp->innerp(targetp->innerp());
            aliasp->predefined(targetp->predefined());
            nodep->bound(true);
            nodep->vhdlSymp(aliasp);
            declare(aliasp, nodep->fileline());
            return;
        }
    }
    // A generic subprogram is declared by its own profile, so that calls to
    // it inside the package resolve before any instantiation (6.5.4)
    if (nodep->kind() == VhdlIfaceKind::SUBPROGRAM && nodep->profilep()) {
        iterate(nodep->profilep());
        nodep->vhdlSymp(nodep->profilep()->vhdlSymp());
        return;
    }
    VhdlSymKind kind = VhdlSymKind::CONSTANT;
    switch (nodep->kind()) {
    case VhdlIfaceKind::CONSTANT: kind = VhdlSymKind::GENERIC; break;
    case VhdlIfaceKind::SIGNAL: kind = VhdlSymKind::PORT; break;
    case VhdlIfaceKind::VARIABLE: kind = VhdlSymKind::PARAMETER; break;
    case VhdlIfaceKind::FILE: kind = VhdlSymKind::FILE; break;
    case VhdlIfaceKind::TYPE: kind = VhdlSymKind::TYPE; break;
    case VhdlIfaceKind::SUBPROGRAM: kind = VhdlSymKind::FUNCTION; break;
    case VhdlIfaceKind::PACKAGE: kind = VhdlSymKind::PACKAGE; break;
    }
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(kind, nodep->name(), nodep);
    // Its place in the clause, so that a positional association finds it. A
    // generic synthesized to carry the bounds of another one has no place: it
    // is never written, so a positional association must skip over it.
    const bool synthesized = nodep->name().compare(0, 3, "__v") == 0;
    symp->position(synthesized ? -1 : m_ifacePos++);
    if (nodep->kind() == VhdlIfaceKind::PACKAGE) {
        // "package p is new work.poly generic map (<>)" declares a name for
        // a package that is not instantiated yet (6.5.5). Inside the unit
        // that declares it, what "p" offers is what the uninstantiated
        // package declares, with its own generics still unbound -- so the
        // formal's region is that package's, and every use of it is checked
        // against the template. What the instantiation actually supplies is
        // bound later, in bindPackageGeneric, when the unit is copied.
        // In a copy of the entity made for one instance, the generic package
        // is the package that instance named -- an instantiated one, whose
        // types and constants are settled -- rather than the template
        const auto boundIt = m_boundGenPackages.find(nodep->name());
        if (boundIt != m_boundGenPackages.end()) {
            symp->innerp(boundIt->second);
            m_scopep->addNamed(lower(nodep->name()), boundIt->second);
            nodep->bound(true);
            nodep->vhdlSymp(symp);
            declare(symp, nodep->fileline());
            return;
        }
        std::string key = lower(nodep->newFrom());
        const size_t dot = key.rfind('.');
        if (dot == std::string::npos) {
            key = m_libname + "." + key;
        } else if (key.compare(0, dot, "work") == 0) {
            key = m_libname + key.substr(dot);
        }
        const auto scopeIt = m_unitScopes.find(key);
        // What its generic map names is a generic of that package (6.5.7.2)
        const auto unitIt = m_primaryUnits.find(key);
        const AstVhdlPackage* const genPkgp
            = unitIt == m_primaryUnits.end() ? nullptr : VN_CAST(unitIt->second, VhdlPackage);
        for (const auto& formal : nodep->mapFormals()) {
            if (!genPkgp || m_inSpecialized) break;
            bool known = false;
            for (const AstNode* p = genPkgp->genericsp(); p && !known; p = p->nextp()) {
                known = VN_AS(p, VhdlIfaceDecl)->name() == formal.second;
            }
            if (!known) {
                err(formal.first, "Package '" + genPkgp->prettyName() + "' has no generic named '"
                                      + formal.second + "'");
            }
        }
        if (scopeIt != m_unitScopes.end()) {
            symp->innerp(scopeIt->second);
            m_scopep->addNamed(lower(nodep->name()), scopeIt->second);
        } else if (!nodep->newFrom().empty()) {
            err(nodep->fileline(),
                "Cannot find package '" + nodep->newFrom() + "' that generic package '"
                    + nodep->name() + "' is new of");
        }
        nodep->vhdlSymp(symp);
        declare(symp, nodep->fileline());
        return;
    }
    if (nodep->kind() == VhdlIfaceKind::TYPE) {
        // In a copy of the entity made for one instance, the generic type is
        // the type that instance named, and the copy is an ordinary entity
        const auto boundIt = m_boundGenTypes.find(nodep->name());
        if (boundIt != m_boundGenTypes.end()) {
            symp->typep(boundIt->second);
            nodep->bound(true);
        } else {
            // A generic type stands for whatever the instantiation supplies, so
            // give it a type of its own that matches nothing else yet (6.5.5)
            VhdlType* const typep
                = VhdlSymTab::newType(VhdlTypeKind::ERROR, nodep->name(), nodep->fileline());
            typep->genericFormal(true);
            symp->typep(typep);
        }
    } else {
        symp->typep(resolveSubtype(nodep->subtypep()));
    }
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
    if (kind == VhdlSymKind::GENERIC) {
        checkHoldsValue(nodep->fileline(), symp->typep(), "A generic", nodep->prettyName());
        // A generic constant is given its value from outside and only read
        // inside, so its mode is IN (6.5.6.2)
        if (nodep->modeExplicit() && nodep->mode() != VhdlMode::IN && !m_inSpecialized) {
            err(nodep->fileline(),
                "A generic can only be of mode in: '" + nodep->prettyName() + "'");
        }
    } else if (kind == VhdlSymKind::PORT) {
        checkHoldsValue(nodep->fileline(), symp->typep(), "A port", nodep->prettyName());
    }
    if (nodep->defaultp()) {
        analyzeExpr(nodep->defaultp(), symp->typep());
        if ((kind == VhdlSymKind::GENERIC || kind == VhdlSymKind::PORT)
            && !valueFitsType(nodep->defaultp(), symp->typep())) {
            err(nodep->defaultp()->fileline(),
                "The default value of '" + nodep->prettyName() + "' must be of type "
                    + symp->typep()->prettyName() + ", but is of type "
                    + nodep->defaultp()->vhdlTypep()->prettyName());
        } else if ((kind == VhdlSymKind::GENERIC || kind == VhdlSymKind::PORT)
                   && !m_inSpecialized) {
            // And of its subtype, as long as that is (6.5.2)
            checkArrayValue(nodep->defaultp(), symp->typep(),
                            "the default value of '" + nodep->prettyName() + "'");
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlSubprogDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(
        nodep->isFunction() ? VhdlSymKind::FUNCTION : VhdlSymKind::PROCEDURE, nodep->name(),
        nodep);

    // Parameters live in the subprogram's own region, but their types are
    // named in the enclosing one, so resolve them before pushing
    VhdlScope* const outerp = m_scopep;
    VhdlScope* const innerp = pushScope(nodep->name());
    // A copy made for one call site may have been given the values that call
    // site passes, so that lengths depending on them fold (see specialize())
    std::vector<VhdlValue> specValues;
    specValues.swap(m_specValues);
    // The generics of a generic subprogram come first and are not part of
    // its profile (4.2.1). An instantiation supplies their actuals; the
    // uninstantiated declaration itself declares them as they are written, so
    // that its body analyzes.
    size_t genericNum = 0;
    for (AstNode* paramp = nodep->paramsp();
         paramp && genericNum < static_cast<size_t>(nodep->genericCount());
         paramp = paramp->nextp(), ++genericNum) {
        AstVhdlIfaceDecl* const ifp = VN_AS(paramp, VhdlIfaceDecl);
        // An instantiation writes each actual into the formal it binds, so
        // the copy carries its own bindings wherever it is analyzed again
        if (nodep->isInstance() && ifp->defaultp()) {
            bindPackageGeneric(ifp, ifp->defaultp(), outerp);
        } else if (nodep->isInstance()) {
            // "is <>" left as it stands: the copy uses whatever is visible
            // where it was instantiated, so nothing is declared to hide it
        } else {
            // As written: a generic type, subprogram or package declares
            // itself here so that the body which uses it analyzes (6.5.5)
            iterate(ifp);
        }
    }
    size_t paramNum = 0;
    AstNode* firstParamp = nodep->paramsp();
    for (int i = 0; i < nodep->genericCount() && firstParamp; ++i) {
        firstParamp = firstParamp->nextp();
    }
    // A parameter's type is named in the enclosing region -- except that a
    // generic subprogram's generics are visible to its parameters too (6.5.5)
    VhdlScope* const typeScopep = nodep->genericCount() > 0 ? innerp : outerp;
    for (AstNode* paramp = firstParamp; paramp; paramp = paramp->nextp(), ++paramNum) {
        AstVhdlIfaceDecl* const ifp = VN_AS(paramp, VhdlIfaceDecl);
        if (!m_inSpecialized && !nodep->isInstance()) checkParamMode(nodep, ifp);
        m_scopep = typeScopep;
        VhdlType* const typep = resolveSubtype(ifp->subtypep());
        m_scopep = innerp;
        VhdlSymbol* const psymp = VhdlSymTab::newSymbol(
            ifp->kind() == VhdlIfaceKind::SIGNAL ? VhdlSymKind::SIGNAL : VhdlSymKind::PARAMETER,
            ifp->name(), ifp);
        psymp->typep(typep);
        // Only a value read into the subprogram may be folded: one the body
        // may write is not the same throughout (4.2.2.1). A parameter
        // written without a class is a constant where its mode is "in".
        if (paramNum < specValues.size() && specValues[paramNum].known()
            && ifp->kind() != VhdlIfaceKind::SIGNAL && ifp->kind() != VhdlIfaceKind::FILE
            && ifp->mode() == VhdlMode::IN) {
            psymp->value(specValues[paramNum]);
        }
        ifp->vhdlSymp(psymp);
        symp->addParam(ifp->name(), typep, ifp->defaultp() != nullptr,
                       writtenWithGenericType(ifp->subtypep()));
        declare(psymp, ifp->fileline());
        // A parameter's default value is an expression like any other, and
        // names what the subprogram's own region sees -- a constant of the
        // package it is declared in, or an earlier parameter (4.2.2.1)
        if (ifp->defaultp()) analyzeExpr(ifp->defaultp(), typep);
    }
    if (nodep->isFunction()) {
        m_scopep = typeScopep;
        VhdlType* const retp = resolveTypeMark(nodep->returnTypep());
        m_scopep = innerp;
        symp->returnTypep(retp);
        symp->typep(retp);
    }

    // Declare the subprogram in the enclosing region, where it is visible.
    // A body completes an earlier declaration of the same signature rather
    // than redeclaring it (4.3), so reuse that symbol when there is one.
    m_scopep = outerp;
    symp->innerp(innerp);
    symp->hasBody(nodep->hasBody());
    VhdlSymbol* reusep = nullptr;
    if (nodep->hasBody()) {
        std::vector<VhdlSymbol*> local;
        outerp->lookupLocal(nodep->name(), local);
        // In a package body the declaration being completed is in the package
        // declaration, one region out (12.1). It has to be looked for even
        // when the body's own region already has declarations of that name --
        // which it does as soon as the name is overloaded, and every IEEE
        // package overloads heavily.
        if (m_inPackageBody) {
            std::vector<VhdlSymbol*> outer;
            outerp->lookup(nodep->name(), outer);
            for (VhdlSymbol* const otherp : outer) local.push_back(otherp);
        }
        for (VhdlSymbol* const otherp : local) {
            // Only another subprogram of the design can be the declaration
            // this body completes. A name STD.STANDARD also declares -- TIME
            // has a unit called MIN -- is a homograph of it and is not (4.3).
            if (otherp->kind() != symp->kind() || otherp->predefined()) continue;
            if (!otherp->hasBody() && symp->homographOf(otherp)) {
                reusep = otherp;
                break;
            }
            // A subprogram has one body (4.3)
            if (otherp->hasBody() && symp->homographOf(otherp) && !m_inSpecialized && !m_inPkgInst
                && !m_inPackageInstance && !nodep->isInstance() && !nodep->inPkgInstance()
                && otherp->bodyp() && otherp->bodyp() != nodep && !otherp->bodyp()->isInstance()
                && parentOf(otherp->bodyp()) == parentOf(nodep)) {
                err(nodep->fileline(), std::string{nodep->isFunction() ? "Function" : "Procedure"}
                                           + " '" + nodep->prettyName() + "' already has a body");
                break;
            }
        }
    }
    // A parameter is an object: a type, a subprogram or a package is given
    // only as a generic (6.5.6.1)
    if (!m_inSpecialized) {
        int idx = -nodep->genericCount();
        for (const AstNode* p = nodep->paramsp(); p; p = p->nextp(), ++idx) {
            const AstVhdlIfaceDecl* const ifp = VN_CAST(p, VhdlIfaceDecl);
            if (idx < 0 || !ifp) continue;
            if (ifp->kind() == VhdlIfaceKind::SUBPROGRAM || ifp->kind() == VhdlIfaceKind::TYPE
                || ifp->kind() == VhdlIfaceKind::PACKAGE) {
                err(ifp->fileline(), "A subprogram's parameters are objects; a type, a"
                                     " subprogram or a package is given only as a generic");
            }
        }
    }
    // A package declaration declares its subprograms; their bodies go in the
    // package body (4.7)
    if (nodep->hasBody() && VN_IS(parentOf(nodep), VhdlPackage) && !m_inSpecialized
        && !nodep->isInstance()) {
        err(nodep->fileline(), "A package declaration declares subprograms, and their bodies go"
                               " in the package body: '"
                                   + nodep->prettyName() + "'");
    }
    // A call to the uninstantiated name inside a copy means the copy itself:
    // that is how a generic subprogram recurses (4.4)
    if (nodep->isInstance() && !nodep->templateName().empty()
        && nodep->templateName() != nodep->name()) {
        VhdlSymbol* const selfp
            = VhdlSymTab::newSymbol(symp->kind(), nodep->templateName(), nodep);
        selfp->aliasp(symp);
        selfp->typep(symp->typep());
        // The profile too, since overload resolution reads it from the name
        // it was given rather than from what the name denotes
        for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
            selfp->addParam(symp->paramName(i), symp->paramTypes()[i],
                            symp->paramHasDefault(i));
        }
        selfp->returnTypep(symp->returnTypep());
        selfp->hasBody(true);
        m_scopep = innerp;
        declare(selfp, nodep->fileline());
        m_scopep = outerp;
    }
    if (reusep) {
        // A body is written with the specification of the declaration it
        // completes, element for element (4.2.1, 4.10): "(x, y : integer)"
        // and "(x : integer; y : integer)" say the same thing and still do
        // not conform. A copy made by an instantiation or for one call site
        // was never written by anyone.
        const AstVhdlSubprogDecl* const declp = VN_CAST(reusep->declp(), VhdlSubprogDecl);
        if (declp && !m_inSpecialized && !nodep->isInstance() && !nodep->inPkgInstance()
            && !declp->specText().empty() && !nodep->specText().empty()
            && declp->specText() != nodep->specText()) {
            err(nodep->fileline(),
                "The body of " + std::string{nodep->isFunction() ? "function" : "procedure"} + " '"
                    + nodep->prettyName()
                    + "' does not repeat the specification of its declaration exactly; the"
                      " two may differ only in case, comments, how a number is written and"
                      " whether a name is expanded\n"
                    + nodep->warnContextPrimary() + "\n" + declp->warnOther()
                    + "... Location of the declaration\n" + declp->warnContextSecondary());
        }
        reusep->hasBody(true);
        reusep->innerp(innerp);
        nodep->vhdlSymp(reusep);
        if (nodep->hasBody()) reusep->bodyp(nodep);
    } else {
        nodep->vhdlSymp(symp);
        if (nodep->hasBody()) symp->bodyp(nodep);
        declare(symp, nodep->fileline());
    }
    // A path writes a subprogram with its profile, and the region's name
    // alone does not carry one (16.2.7)
    innerp->ownerp(reusep ? reusep : symp);

    if (nodep->hasBody()) {
        m_scopep = innerp;
        VhdlType* const oldReturnp = m_returnTypep;
        m_returnTypep = symp->returnTypep();
        // The body of an uninstantiated generic subprogram is written
        // against types and operations that only an instantiation supplies,
        // so it is checked in each copy rather than here (4.4)
        const bool isTemplate = nodep->genericCount() > 0 && !nodep->isInstance();
        if (!isTemplate) {
            rejectDecls(nodep->declsp(), "a subprogram");
            iterateAndNextNull(nodep->declsp());
            iterateAndNextNull(nodep->stmtsp());
            // A function computes its value at once, in no simulated time, so
            // nothing in it waits -- itself or in a procedure it calls (10.2)
            // Nor does a method of a protected type, which holds the object
            // exclusively until it returns (5.6.2, 10.2)
            bool method = false;
            for (const AstNode* upp = parentOf(nodep); upp; upp = parentOf(upp)) {
                if (VN_IS(upp, VhdlProtectedType)) method = true;
                if (VN_IS(upp, VhdlSubprogDecl) || VN_IS(upp, NodeVhdlUnit)) break;
            }
            if (method && !nodep->isFunction() && !m_inSpecialized) {
                if (AstNode* const waitp = suspendingStmt(nodep->stmtsp())) {
                    err(waitp->fileline(), "A method of a protected type cannot wait: it holds"
                                           " the object for itself until it returns");
                }
            }
            if (nodep->isFunction() && !m_inSpecialized) {
                if (AstNode* const waitp = suspendingStmt(nodep->stmtsp())) {
                    err(waitp->fileline(),
                        std::string{"A function cannot wait"}
                            + (VN_IS(waitp, VhdlWait) ? ""
                                                      : ", and the procedure called here does")
                            + ": it computes its value at once, in no simulated time");
                }
            }
            // A function whose result type leaves something open gets it
            // from what the body returns (5.3.3, 4.2.1). That is a record
            // with an element of an unconstrained array type, and it is
            // equally an unconstrained array: in a copy made for one call
            // site the body's own declarations have folded, so the object it
            // returns has a length even where the result subtype never did.
            VhdlType* const retp = symp->returnTypep();
            const bool openRecord = retp && retp->base()->kind() == VhdlTypeKind::RECORD
                                    && !fullyConstrainedRecord(retp);
            const bool openArray = retp && retp->base()->kind() == VhdlTypeKind::ARRAY
                                   && !retp->isConstrainedArray()
                                   && retp->base() != V3VhdlStd::stringType()->base();
            if (openRecord || openArray) {
                VhdlType* foundp = nullptr;
                bool disagreep = false;
                bool sawOpen = false;
                bool sawUnfolded = false;
                findReturnedType(nodep->stmtsp(), foundp, disagreep, true, sawOpen, sawUnfolded);
                if (!foundp && !disagreep && !sawOpen) {
                    findReturnedType(nodep->stmtsp(), foundp, disagreep, false, sawOpen,
                                     sawUnfolded);
                }
                if (foundp && (foundp->base()->kind() != VhdlTypeKind::ARRAY
                               || foundp->isConstrainedArray())) {
                    symp->returnTypep(foundp);
                }
            }
        }
        m_returnTypep = oldReturnp;
    }
    m_scopep = outerp;
}

void VhdlSemVisitor::visit(AstVhdlComponentDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::COMPONENT, nodep->name(), nodep);
    VhdlScope* const innerp = pushScope(nodep->name());
    openPortGenerics(nodep->genericsp(), nodep, innerp);
    openPortGenerics(nodep->portsp(), nodep, innerp);
    m_ifacePos = 0;
    iterateAndNextNull(nodep->genericsp());
    fillOpenGenericDefaults(innerp);
    m_ifacePos = 0;
    iterateAndNextNull(nodep->portsp());
    popScope();
    symp->innerp(innerp);
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::visit(AstVhdlAliasDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::ALIAS, nodep->name(), nodep);
    {
        VL_RESTORER_COPY(m_declaring);
        m_declaring.insert(nodep->name());
        if (nodep->subtypep()) symp->typep(resolveSubtype(nodep->subtypep()));
        // "alias a is a" names nothing; an expanded name reaches the outer a.
        // An alias of a subprogram is overloadable, and hides nothing its
        // signature does not match, so it may repeat the designator.
        const AstVhdlName* const targetp = VN_CAST(nodep->targetp(), VhdlName);
        if (targetp && !nodep->signaturep()) nameBeingDeclared(targetp);
    }
    // Record what the alias denotes, so that an alias of a type may be used
    // as a type mark and an alias of an object as an object (6.6)
    std::vector<std::string> parts;
    std::vector<VhdlSymbol*> found;
    if (flatten(nodep->targetp(), parts) && !parts.empty()) {
        VhdlScope* scopep = m_scopep;
        bool viaScope = parts.size() > 1;
        for (size_t i = 0; i + 1 < parts.size() && viaScope; ++i) {
            VhdlScope* const nextp = scopep ? scopep->findNamed(parts[i]) : nullptr;
            if (!nextp) {
                viaScope = false;
            } else {
                scopep = nextp;
            }
        }
        if (viaScope && scopep) {
            scopep->lookupLocal(parts.back(), found);
        } else {
            lookup(parts.back(), found);
        }
        // "alias r is p[real, real]": a signature says which of an overloaded
        // name's declarations the alias denotes (4.5.3, 6.6.2)
        if (nodep->signaturep() && !found.empty()) {
            std::vector<VhdlType*> wantParams;
            for (AstNode* p = nodep->signaturep()->paramTypesp(); p; p = p->nextp()) {
                wantParams.push_back(resolveTypeMark(VN_AS(p, NodeVhdlExpr)));
            }
            VhdlType* const wantReturnp
                = nodep->signaturep()->returnTypep()
                      ? resolveTypeMark(nodep->signaturep()->returnTypep())
                      : nullptr;
            std::vector<VhdlSymbol*> matched;
            for (VhdlSymbol* const candp : found) {
                if (candp->paramTypes().size() != wantParams.size()) continue;
                bool ok = true;
                for (size_t i = 0; i < wantParams.size() && ok; ++i) {
                    const VhdlType* const havep = candp->paramTypes()[i];
                    if (!havep || !wantParams[i] || havep->base() != wantParams[i]->base()) {
                        ok = false;
                    }
                }
                if (ok && wantReturnp) {
                    ok = candp->returnTypep()
                         && candp->returnTypep()->base() == wantReturnp->base();
                } else if (ok && !wantReturnp) {
                    ok = !candp->returnTypep();  // A procedure has no result
                }
                if (ok) matched.push_back(candp);
            }
            if (matched.size() == 1) found.swap(matched);
            // A signature that matches none of them names nothing to alias
            bool anyOverloadable = false;
            for (const VhdlSymbol* const candp : found) {
                if (candp->isOverloadable()) anyOverloadable = true;
            }
            if (matched.empty() && anyOverloadable && !m_inSpecialized) {
                err(nodep->fileline(), "No declaration of '" + parts.back()
                                           + "' matches the signature of alias '"
                                           + nodep->prettyName() + "'");
            }
        }
        // An overloaded name aliases every one of its declarations at once,
        // so only an unambiguous target is worth recording here
        if (found.size() == 1) {
            symp->aliasp(found[0]);
            if (!symp->typep() && found[0]->typep()) symp->typep(found[0]->typep());
            // An alias of a subprogram is called like one, so it carries the
            // same profile: what it takes and what it gives back
            if (found[0]->isOverloadable()) {
                for (size_t i = 0; i < found[0]->paramTypes().size(); ++i) {
                    symp->addParam(found[0]->paramName(i), found[0]->paramTypes()[i],
                                   found[0]->paramHasDefault(i));
                }
                if (found[0]->returnTypep()) symp->returnTypep(found[0]->returnTypep());
            }
        }
    }
    // An alias of an object denotes a name that may be a slice or an element,
    // not only a simple name (6.6.2), so the whole of it is analyzed. An
    // alias of a type or a subprogram denotes no value, and analyzing it as
    // an expression would be wrong.
    bool objectAlias = !nodep->signaturep();
    if (symp->aliasp()) {
        objectAlias = objectAlias && symp->aliasp()->resolved()->isObject();
    } else if (!found.empty()) {
        // An overloaded name: a subprogram or a literal, which denotes no
        // value of its own
        objectAlias = false;
    }
    // Only an alias of an object has a subtype of its own (6.6.1)
    if (!objectAlias && nodep->subtypep() && !m_inSpecialized) {
        err(nodep->subtypep()->fileline(), "Only an alias of an object is given a subtype, and '"
                                               + nodep->prettyName()
                                               + "' names something that is not one");
    }
    // Otherwise the target denotes no single declaration reachable by name --
    // a record element, a slice, an indexed name -- and that is precisely the
    // object alias of 6.6.2, so it is analyzed as the expression it is
    if (objectAlias) {
        VhdlType* const typep = analyzeExpr(nodep->targetp(), symp->typep());
        // The subtype it is given is of the type of what it names (6.6.2)
        const VhdlType* const ownp = nodep->subtypep() ? symp->typep() : nullptr;
        if (ownp && typep && !ownp->isError() && !typep->isError() && !m_inSpecialized
            && !ownp->isGenericFormal() && !typep->isGenericFormal()
            && ownp->base() != typep->base()) {
            err(nodep->fileline(), "Alias '" + nodep->prettyName() + "' is of type "
                                       + ownp->prettyName() + ", but what it names is of type "
                                       + typep->prettyName());
        } else if (ownp && typep && !m_inSpecialized && ownp->isConstrainedArray()
                   && typep->isConstrainedArray() && ownp->dimensions() == 1
                   && typep->dimensions() == 1 && ownp->lengthEmittable()
                   && typep->lengthEmittable() && ownp->dimLength(0) != typep->dimLength(0)) {
            // And as long as it, an element for each (6.6.2)
            err(nodep->fileline(), "Alias '" + nodep->prettyName() + "' is "
                                       + std::to_string(ownp->dimLength(0))
                                       + " elements long, but what it names is "
                                       + std::to_string(typep->dimLength(0)));
        }
        // Before VHDL-2008 an object alias names no multidimensional array
        // (VHDL-93 4.3.3.1)
        const VhdlType* const aliasTypep = ownp ? ownp : typep;
        if (aliasTypep && !aliasTypep->isError() && !m_inSpecialized
            && nodep->fileline()->language() < V3LangCode::L1076_2008
            && aliasTypep->base()->kind() == VhdlTypeKind::ARRAY
            && aliasTypep->base()->indexTypes().size() > 1) {
            err(nodep->fileline(), "Before VHDL-2008 an alias cannot name an object of a"
                                   " multidimensional array type: '"
                                       + nodep->prettyName() + "'");
        }
        // What an object alias names is an object; the result of a function
        // or of a conversion is a value, which nothing can alias (6.6.2), and
        // so is an attribute's -- but for the four that denote signals
        if (const AstVhdlAttrName* const attrp = VN_CAST(nodep->targetp(), VhdlAttrName)) {
            if (!makesImplicitSignal(attrp->name()) && !m_inSpecialized) {
                err(nodep->targetp()->fileline(), "An alias names an object, but '" + attrp->name()
                                                      + " gives a value: '" + nodep->prettyName()
                                                      + "'");
            }
        }
        if (const AstVhdlCall* const callp = VN_CAST(nodep->targetp(), VhdlCall)) {
            const VhdlSymbol* csymp = callp->vhdlSymp();
            if (!csymp) csymp = callp->prefixp()->vhdlSymp();
            csymp = csymp ? csymp->resolved() : nullptr;
            if (csymp
                && (csymp->kind() == VhdlSymKind::FUNCTION || csymp->kind() == VhdlSymKind::TYPE)
                && !m_inSpecialized) {
                err(nodep->targetp()->fileline(),
                    "An alias names an object, but this is the result of a "
                        + std::string{csymp->kind() == VhdlSymKind::TYPE ? "type conversion"
                                                                         : "function call"}
                        + ", which is a value: '" + nodep->prettyName() + "'");
            }
        }
        if (!symp->typep() && typep && !typep->isError()) symp->typep(typep);
        // An alias written with a subtype of its own may constrain the index
        // and leave the element alone, in which case the element is what the
        // aliased object already has (5.3.2.2)
        if (symp->typep()) {
            symp->typep(constrainFromValue(symp->typep(), nodep->targetp(), nodep->fileline()));
        }
    }
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
    // Aliasing a type implicitly aliases everything the type declaration
    // brought with it, so its enumeration literals become visible too (6.6.3)
    VhdlSymbol* const targetp = symp->aliasp();
    if (targetp && targetp->kind() == VhdlSymKind::TYPE && targetp->typep()
        && targetp->typep()->base()->kind() == VhdlTypeKind::ENUM) {
        for (VhdlSymbol* const litp : targetp->typep()->base()->literals()) {
            VhdlSymbol* const litAliasp
                = VhdlSymTab::newSymbol(VhdlSymKind::ALIAS, litp->name(), nodep);
            litAliasp->aliasp(litp);
            litAliasp->typep(litp->typep());
            litAliasp->returnTypep(litp->typep());
            litAliasp->position(litp->position());
            m_scopep->insert(litAliasp);
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlAttrDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::ATTRIBUTE, nodep->name(), nodep);
    symp->typep(resolveTypeMark(nodep->typeMarkp()));
    // An attribute's value is a constant, which no access, file or protected
    // value can be (6.7)
    const VhdlType* const typep = symp->typep();
    if (typep && !typep->isError() && !m_inSpecialized) {
        const VhdlTypeKind kind = typep->base()->kind();
        if (kind == VhdlTypeKind::FILE || kind == VhdlTypeKind::PROTECTED || holdsAccess(typep)) {
            err(nodep->fileline(), "Attribute '" + nodep->name()
                                       + "' holds a constant value, which a value of type "
                                       + typep->prettyName() + " cannot be");
        }
    }
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

// The entity classes of 7.2 that name something analysis has a symbol for.
// A class the symbol table does not distinguish matches any kind.
static bool entityClassMatches(const std::string& cls, VhdlSymKind kind) {
    // A subprogram parameter is an object of whichever class it was declared
    if (kind == VhdlSymKind::PARAMETER) {
        return cls == "constant" || cls == "variable" || cls == "signal" || cls == "file";
    }
    if (cls == "signal") return kind == VhdlSymKind::SIGNAL || kind == VhdlSymKind::PORT;
    if (cls == "variable") {
        return kind == VhdlSymKind::VARIABLE || kind == VhdlSymKind::SHARED_VARIABLE;
    }
    if (cls == "constant") {
        return kind == VhdlSymKind::CONSTANT || kind == VhdlSymKind::GENERIC
               || kind == VhdlSymKind::LOOP_PARAMETER || kind == VhdlSymKind::GENERATE_PARAMETER;
    }
    if (cls == "type" || cls == "subtype") return kind == VhdlSymKind::TYPE;
    if (cls == "function") return kind == VhdlSymKind::FUNCTION;
    if (cls == "procedure") return kind == VhdlSymKind::PROCEDURE;
    if (cls == "component") return kind == VhdlSymKind::COMPONENT;
    if (cls == "literal") return kind == VhdlSymKind::ENUM_LITERAL;
    if (cls == "file") return kind == VhdlSymKind::FILE;
    return true;
}

// A user-defined attribute's value belongs to the entity it is given to, and
// is read back later as "name'attribute" (7.2). It is a static expression, so
// what is recorded is the expression itself and every use folds it.
void VhdlSemVisitor::recordUserAttr(AstVhdlAttrSpec* nodep) {
    VhdlType* attrTypep = nullptr;
    std::vector<VhdlSymbol*> attrs;
    lookup(nodep->name(), attrs);
    for (VhdlSymbol* const symp : attrs) {
        if (symp->resolved()->kind() == VhdlSymKind::ATTRIBUTE) {
            attrTypep = symp->resolved()->typep();
            break;
        }
    }
    if (!attrTypep) {
        err(nodep->fileline(), "No attribute '" + nodep->name() + "' is declared here");
        return;
    }
    analyzeExpr(nodep->valuep(), attrTypep);
    // The value is of the attribute's type (7.2)
    if (!valueFitsType(nodep->valuep(), attrTypep)) {
        err(nodep->valuep()->fileline(), "Attribute '" + nodep->name() + "' is of type "
                                             + attrTypep->prettyName()
                                             + ", but the value given it is of type "
                                             + nodep->valuep()->vhdlTypep()->prettyName());
    }
    AstVhdlEntitySpec* const specp = nodep->specp();
    if (!specp) return;
    const std::string cls = specp->entityClass();
    // One that says "others" or "all" is the last for its attribute and class
    // in its declarative part (7.2)
    const std::string closedKey = cvtToHex(m_scopep) + " " + nodep->name() + " " + cls;
    if (m_attrSpecsClosed.count(closedKey) && !m_inSpecialized) {
        err(nodep->fileline(), "An attribute specification that says 'others' or 'all' is the"
                               " last for attribute '"
                                   + nodep->name() + "' of class " + cls
                                   + " in its declarative part");
    }
    if (specp->all() || specp->others()) {
        m_attrSpecsClosed.insert(closedKey);
        return;
    }
    const bool ofUnit
        = cls == "entity" || cls == "architecture" || cls == "package" || cls == "configuration";
    for (AstNode* p = specp->namesp(); p; p = p->nextp()) {
        AstNodeVhdlExpr* const namep = VN_AS(p, NodeVhdlExpr);
        std::vector<std::string> parts;
        if (!flatten(namep, parts) || parts.empty()) continue;
        // A design unit's attributes are specified in its own declarative
        // part (7.2)
        if (ofUnit) {
            // A package declared in this region -- an interface package, or
            // one nested here -- is decorated here like anything else
            std::vector<VhdlSymbol*> local;
            m_scopep->lookupLocal(parts.back(), local);
            if (!local.empty()) continue;
            const AstNodeVhdlUnit* const unitp = VN_CAST(parentOf(nodep), NodeVhdlUnit);
            const bool kindOk
                = unitp
                  && ((cls == "entity" && VN_IS(unitp, VhdlEntity))
                      || (cls == "architecture" && VN_IS(unitp, VhdlArchitecture))
                      || (cls == "package" && VN_IS(unitp, VhdlPackage))
                      || (cls == "configuration" && VN_IS(unitp, VhdlConfiguration)));
            if (!m_inSpecialized && (!kindOk || lower(unitp->name()) != lower(parts.back()))) {
                err(namep->fileline(), "An attribute of " + cls + " '" + parts.back()
                                           + "' is specified in that " + cls
                                           + "'s own declarative part");
            }
            continue;
        }
        std::vector<VhdlSymbol*> found;
        lookup(parts.back(), found);
        // It names something declared already (7.2). Only the classes the
        // symbol table holds by simple name are asked: a label is declared
        // by the statement it labels, which follows.
        const bool ofObjectOrType = cls == "signal" || cls == "variable" || cls == "constant"
                                    || cls == "file" || cls == "type" || cls == "subtype"
                                    || cls == "component";
        if (found.empty() && ofObjectOrType && !m_inSpecialized) {
            err(namep->fileline(), "Attribute '" + nodep->name() + "' is given to '" + parts.back()
                                       + "', which is not declared here");
            continue;
        }
        // What the specification names is of the class it says (7.2): a type
        // declaration is of class type, and a subtype declaration of subtype
        const auto ofClass = [&](const VhdlSymbol* symp) {
            const VhdlSymbol* const realp = symp->resolved();
            if (!entityClassMatches(cls, realp->kind())) return false;
            if (cls == "type") return !VN_IS(realp->declp(), VhdlSubtypeDecl);
            if (cls == "subtype") return !VN_IS(realp->declp(), VhdlTypeDecl);
            return true;
        };
        bool anyOfClass = false;
        bool anyHere = false;
        for (VhdlSymbol* const symp : found) {
            if (!ofClass(symp)) continue;
            anyOfClass = true;
            if (symp->declScopep() == m_scopep) anyHere = true;
        }
        if (!found.empty() && !anyOfClass && !m_inSpecialized) {
            err(namep->fileline(), "'" + parts.back() + "' is not of entity class " + cls
                                       + ", so attribute '" + nodep->name()
                                       + "' cannot be given to it as one");
            continue;
        }
        // It appears in the declarative part that declares what it names
        // (7.2). nvc 1.22.1 takes one elsewhere, GHDL 7.0.0-dev and Questa
        // 2025.2 do not.
        if (anyOfClass && !anyHere && ofObjectOrType && !m_inSpecialized && !m_inPkgInst
            && !m_inPackageInstance) {
            err(namep->fileline(), "Attribute '" + nodep->name() + "' is given to '" + parts.back()
                                       + "' in the declarative part that declares it, and"
                                         " this is not that one");
            continue;
        }
        for (VhdlSymbol* const symp : found) {
            VhdlSymbol* const realp = symp->resolved();
            if (!entityClassMatches(cls, realp->kind())) continue;
            if (realp->userAttr(nodep->name())) {
                err(nodep->fileline(), "Attribute '" + nodep->name() + "' is already given a "
                                       "value for '" + parts.back() + "'");
                continue;
            }
            realp->userAttr(nodep->name(), nodep->valuep());
        }
    }
}

// An attribute specification gives an attribute a value for named entities
// (7.2). Most attributes are of interest only to the tool that defined them
// and are carried no further, but FOREIGN is the language's own way of saying
// that a subprogram is implemented outside VHDL (16.2), so it is acted on.
void VhdlSemVisitor::visit(AstVhdlAttrSpec* nodep) {
    if (nodep->name() != "foreign") {
        recordUserAttr(nodep);
        return;
    }
    iterateAndNextNull(nodep->valuep());
    AstVhdlEntitySpec* const specp = nodep->specp();
    if (!specp) return;
    const std::string cls = specp->entityClass();
    if (cls == "architecture") {
        nodep->v3warn(E_UNSUPPORTED,
                      "Unsupported: FOREIGN attribute on an architecture; only a subprogram "
                      "may be implemented outside VHDL here");
        return;
    }
    if (cls != "function" && cls != "procedure") {
        err(nodep->fileline(),
            "The FOREIGN attribute may only be given to a function or a procedure");
        return;
    }
    const VhdlValue val = V3VhdlEval::eval(nodep->valuep());
    if (!val.isStr()) {
        err(nodep->fileline(), "The FOREIGN attribute's value must be a static string");
        return;
    }
    if (specp->others() || specp->all()) {
        err(nodep->fileline(), "The FOREIGN attribute must name the subprogram it applies to, "
                               "since each one binds to a C symbol of its own");
        return;
    }
    std::string cname;
    bool dpi = false;
    if (!parseForeign(val.toStr(), nodep->fileline(), cname, dpi)) return;
    const VhdlSymKind want
        = cls == "function" ? VhdlSymKind::FUNCTION : VhdlSymKind::PROCEDURE;
    for (AstNode* p = specp->namesp(); p; p = p->nextp()) {
        AstNodeVhdlExpr* const namep = VN_AS(p, NodeVhdlExpr);
        std::vector<std::string> parts;
        if (!flatten(namep, parts) || parts.size() != 1) {
            err(namep->fileline(), "The FOREIGN attribute names a subprogram declared here");
            continue;
        }
        std::vector<VhdlSymbol*> found;
        lookup(parts[0], found);
        int marked = 0;
        for (VhdlSymbol* const symp : found) {
            VhdlSymbol* const realp = symp->resolved();
            if (realp->kind() != want) continue;
            // One attribute of one named entity is specified once (7.2)
            if (!realp->foreignName().empty() && !m_inSpecialized && !m_inPkgInst
                && !m_inPackageInstance) {
                err(nodep->fileline(),
                    "Attribute 'foreign' is already given a value for '" + parts[0] + "'");
                ++marked;
                continue;
            }
            realp->foreignName(cname);
            realp->foreignDpi(dpi);
            ++marked;
        }
        if (!marked) {
            err(namep->fileline(),
                "Cannot find the " + cls + " '" + parts[0] + "' the FOREIGN attribute names");
        } else if (marked > 1) {
            err(namep->fileline(), "'" + parts[0]
                                       + "' is overloaded, so the FOREIGN attribute cannot say "
                                         "which declaration it applies to");
        }
        v3Global.dpi(true);
    }
}

// The content of a FOREIGN string is implementation defined (16.2). Accepted
// are GHDL's convention, so that source written for it needs no change, and an
// explicit "DPI-C" form that shares one C model with a SystemVerilog
// testbench. A bare symbol name is taken as VHPIDIRECT.
bool VhdlSemVisitor::parseForeign(const std::string& text, FileLine* flp, std::string& cnamer,
                                  bool& dpir) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!word.empty()) words.push_back(word);
            word.clear();
        } else {
            word += c;
        }
    }
    if (!word.empty()) words.push_back(word);
    if (words.empty()) {
        err(flp, "The FOREIGN attribute's value is empty");
        return false;
    }
    std::string kind = words[0];
    for (char& c : kind) c = std::toupper(static_cast<unsigned char>(c));
    size_t first = 1;
    if (kind == "VHPIDIRECT" || kind == "VHPI") {
        // nvc 1.22.1 writes "VHPI <library> <symbol>" for a model it loads
        // through VHPI; the symbol is bound the same way here
        dpir = false;
    } else if (kind == "DPI-C" || kind == "DPI") {
        dpir = true;
    } else {
        // A bare name, which GHDL also accepts
        dpir = false;
        first = 0;
    }
    // GHDL allows "VHPIDIRECT <library> <symbol>". Verilator links statically,
    // so the library name has nothing to name and is ignored; so is anything
    // else the string carries, since its content is implementation defined.
    if (words.size() == first) {
        err(flp, "The FOREIGN attribute's value names no C symbol");
        return false;
    }
    cnamer = words.back();
    return true;
}

//######################################################################
// Regions with their own declarations

void VhdlSemVisitor::visit(AstVhdlProcess* nodep) {
    // The sensitivity list names signals of the enclosing region, not of the
    // process's own, so it is analyzed before that region is opened (11.3)
    for (AstNode* sp = nodep->sensp(); sp; sp = sp->nextp()) {
        analyzeExpr(VN_AS(sp, NodeVhdlExpr), nullptr);
        checkSensitivityName(VN_AS(sp, NodeVhdlExpr));
    }
    pushScope(nodep->name().empty() ? "process" : nodep->name());
    rejectDecls(nodep->declsp(), "a process");
    iterateAndNextNull(nodep->declsp());
    // The labels of its statements are declared in the process's own
    // region (12.1), so each is distinct from its declarations and from one
    // another
    if (!m_inSpecialized) {
        std::set<std::string> labels;
        for (AstNode* sp = nodep->stmtsp(); sp; sp = sp->nextp()) {
            sp->foreach([&](const AstNodeVhdlStmt* stmtp) {
                if (stmtp->label().empty()) return;
                std::vector<VhdlSymbol*> found;
                m_scopep->lookupLocal(stmtp->label(), found);
                if (!found.empty() || !labels.insert(stmtp->label()).second) {
                    err(stmtp->fileline(),
                        "Label '" + stmtp->label() + "' is already declared in this process");
                }
            });
        }
    }
    iterateAndNextNull(nodep->stmtsp());
    popScope();
    // A process with a sensitivity list resumes when one of those signals
    // changes and at no other point, so nothing in it may wait -- itself or
    // in a procedure it calls (11.3, 10.2)
    if ((nodep->hasSensList() || nodep->sensAll()) && !m_inSpecialized) {
        if (AstNode* const waitp = suspendingStmt(nodep->stmtsp())) {
            err(waitp->fileline(),
                std::string{"A process with a sensitivity list cannot wait"}
                    + (VN_IS(waitp, VhdlWait) ? "" : ", and the procedure called here does")
                    + ": it resumes when a signal in its list changes and at no other point");
        }
    }
}

// The first statement among these that can suspend the process running
// them: a wait statement, or a call of a procedure that can (10.2)
AstNode* VhdlSemVisitor::suspendingStmt(AstNode* stmtsp) {
    AstNode* foundp = nullptr;
    for (AstNode* sp = stmtsp; sp && !foundp; sp = sp->nextp()) {
        sp->foreach([&](AstNode* np) {
            if (foundp) return;
            if (VN_IS(np, VhdlWait)) {
                foundp = np;
            } else if (const AstVhdlProcCall* const pcp = VN_CAST(np, VhdlProcCall)) {
                if (procedureSuspends(pcp)) foundp = np;
            }
        });
    }
    return foundp;
}

bool VhdlSemVisitor::procedureSuspends(const AstVhdlProcCall* pcp) {
    const AstNodeVhdlExpr* callp = pcp->callp();
    const VhdlSymbol* symp = callp ? callp->vhdlSymp() : nullptr;
    if (!symp) {
        if (const AstVhdlCall* const cp = VN_CAST(callp, VhdlCall)) {
            symp = cp->prefixp()->vhdlSymp();
        }
    }
    if (!symp) return false;
    AstVhdlSubprogDecl* const bodyp = symp->resolved()->bodyp();
    if (!bodyp) return false;
    const auto it = m_suspends.find(bodyp);
    if (it != m_suspends.end()) return it->second;
    m_suspends[bodyp] = false;  // A recursion is decided by the rest of the body
    const bool suspends = suspendingStmt(bodyp->stmtsp()) != nullptr;
    m_suspends[bodyp] = suspends;
    return suspends;
}

// What a sensitivity list or a "wait on" names is a signal, by a static name,
// since the list is fixed when the process is elaborated (10.2, 11.3)
void VhdlSemVisitor::checkSensitivityName(AstNodeVhdlExpr* exprp) {
    if (m_inSpecialized || !exprp) return;
    const bool notName = VN_IS(exprp, VhdlLiteral) || VN_IS(exprp, VhdlBinary)
                         || VN_IS(exprp, VhdlUnary) || VN_IS(exprp, VhdlAggregate)
                         || VN_IS(exprp, VhdlQualified);
    const ObjInfo info = notName ? ObjInfo{} : objectOfName(exprp);
    if (notName || (info.cls != ObjClass::UNKNOWN && info.cls != ObjClass::SIGNAL)) {
        err(exprp->fileline(),
            "A sensitivity list names signals, but "
                + (notName ? std::string{"this is a value"}
                           : "'" + info.symp->name() + "' is " + objClassName(info.cls)));
        return;
    }
    if (info.cls != ObjClass::SIGNAL) return;
    if (const VhdlSymbol* const varyingp = nonStaticSignalIndex(exprp)) {
        err(exprp->fileline(), "A signal in a sensitivity list is named statically, but this"
                               " name's index depends on '"
                                   + varyingp->name() + "'");
    }
}

// Whether a name denoting an object reads it where it stands: everywhere
// but as (the root of) an assignment's target. An association is left to the
// rules for associations, and an attribute's prefix to the rules for
// attributes, since whether those read the object depends on which.
static bool nameIsRead(const AstNode* nodep) {
    // Anywhere inside an association's formal part -- a conversion written
    // on the formal, "to_int(o) => s" -- the object is the one the
    // association writes through, not a value read
    {
        const AstNode* childp = nodep;
        for (const AstNode* upp = parentOf(nodep); upp; childp = upp, upp = parentOf(upp)) {
            if (const AstVhdlAssoc* const asp = VN_CAST(upp, VhdlAssoc)) {
                if (asp->formalp() == childp) return false;
            }
            if (VN_IS(upp, NodeVhdlStmt) || VN_IS(upp, NodeVhdlDecl)) break;
        }
    }
    // Going through an access value to what it designates reads the access
    // value, whatever is then done with what it points at (8.1)
    const auto derefs = [](const AstNode* childp) {
        const AstNodeVhdlExpr* const exprp = VN_CAST(childp, NodeVhdlExpr);
        return exprp && exprp->vhdlTypep()
               && exprp->vhdlTypep()->base()->kind() == VhdlTypeKind::ACCESS;
    };
    const AstNode* childp = nodep;
    for (const AstNode* upp = parentOf(nodep); upp; childp = upp, upp = parentOf(upp)) {
        if (const AstVhdlCall* const callp = VN_CAST(upp, VhdlCall)) {
            if (callp->prefixp() == childp) {  // Indexed or sliced
                if (derefs(childp)) return true;
                continue;
            }
            return true;  // An index or an argument
        }
        if (const AstVhdlSelected* const selp = VN_CAST(upp, VhdlSelected)) {
            if (selp->prefixp() == childp) {  // A record element of it
                if (derefs(childp)) return true;
                continue;
            }
            return false;
        }
        if (const AstVhdlAttrName* const attrp = VN_CAST(upp, VhdlAttrName)) {
            return attrp->prefixp() != childp;  // An argument is a value read
        }
        if (const AstVhdlAssoc* const asp = VN_CAST(upp, VhdlAssoc)) {
            if (asp->formalp() == childp) return false;
            // An index, a function's argument and an attribute's argument
            // are values read; what a procedure or a port map does with its
            // actual depends on the formal's mode, which their own rules ask
            const AstNode* const ownerp = parentOf(upp);
            if (const AstVhdlCall* const callp = VN_CAST(ownerp, VhdlCall)) {
                const VhdlSymbol* symp = callp->vhdlSymp();
                if (!symp) symp = callp->prefixp()->vhdlSymp();
                if (symp && symp->resolved()->kind() == VhdlSymKind::PROCEDURE) return false;
                // A conversion standing as the whole actual of a port map or
                // a procedure call converts what the formal's mode says
                // flows, which is the association's to say
                if (const AstVhdlAssoc* const outerp = VN_CAST(parentOf(callp), VhdlAssoc)) {
                    const AstVhdlCall* const ocallp = VN_CAST(parentOf(outerp), VhdlCall);
                    if (!ocallp) return false;  // A port or generic map
                    const VhdlSymbol* osymp = ocallp->vhdlSymp();
                    if (!osymp) osymp = ocallp->prefixp()->vhdlSymp();
                    if (osymp && osymp->resolved()->kind() == VhdlSymKind::PROCEDURE) {
                        return false;
                    }
                }
                return true;
            }
            return VN_IS(ownerp, VhdlAttrName);
        }
        // An alias names the object; it reads nothing until it is used
        if (VN_IS(upp, VhdlAliasDecl)) return false;
        if (VN_IS(upp, VhdlElemAssoc) || VN_IS(upp, VhdlAggregate)) continue;
        if (const AstVhdlSigAssign* const asgp = VN_CAST(upp, VhdlSigAssign)) {
            return asgp->targetp() != childp;
        }
        if (const AstVhdlVarAssign* const asgp = VN_CAST(upp, VhdlVarAssign)) {
            return asgp->targetp() != childp;
        }
        return true;
    }
    return false;
}

static bool readsTheSignal(const std::string& attr);
static bool isSubprogramParam(const AstNode* declp);

// Before VHDL-2008 a port or a parameter of mode out is written and never
// read (6.5.2); VHDL-2008 lets both be read. A port of mode linkage is read
// only by being given to another of mode linkage, in any revision (6.5.2).
void VhdlSemVisitor::checkOutReads(AstNodeVhdlUnit* unitp) {
    unitp->foreach([&](const AstVhdlName* namep) {
        // A subprogram is given values, and a type is not one (4.2.2)
        if (namep->vhdlSymp() && namep->vhdlSymp()->resolved()->kind() == VhdlSymKind::TYPE
            && !m_inSpecialized) {
            const AstVhdlAssoc* const asp = VN_CAST(parentOf(namep), VhdlAssoc);
            const AstVhdlCall* const callp
                = asp && asp->actualp() == namep ? VN_CAST(parentOf(asp), VhdlCall) : nullptr;
            const VhdlSymbol* const subp
                = callp && callp->vhdlSymp() ? callp->vhdlSymp()->resolved() : nullptr;
            if (subp
                && (subp->kind() == VhdlSymKind::FUNCTION
                    || subp->kind() == VhdlSymKind::PROCEDURE)) {
                err(namep->fileline(), "A subprogram is given values, and '"
                                           + namep->vhdlSymp()->name() + "' is a type");
                return;
            }
        }
        const VhdlSymbol* const lsymp
            = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
        const AstVhdlIfaceDecl* const lifp
            = lsymp ? VN_CAST(lsymp->declp(), VhdlIfaceDecl) : nullptr;
        if (lifp && lifp->mode() == VhdlMode::LINKAGE && !m_inSpecialized) {
            const AstVhdlAttrName* const attrp = VN_CAST(parentOf(namep), VhdlAttrName);
            const bool read = attrp && attrp->prefixp() == namep ? readsTheSignal(attrp->name())
                                                                 : nameIsRead(namep);
            if (read) {
                err(namep->fileline(), "A port of mode linkage is read only as the actual of"
                                       " another port of mode linkage: '"
                                           + lsymp->name() + "'");
            }
            return;
        }
        // A signal parameter of mode out has no value of its own to read:
        // it names a driver the procedure writes, in every revision (4.2.2.3)
        if (lifp && lifp->kind() == VhdlIfaceKind::SIGNAL && lifp->mode() == VhdlMode::OUT
            && isSubprogramParam(lifp) && !m_inSpecialized && nameIsRead(namep)) {
            err(namep->fileline(),
                "A signal parameter of mode out cannot be read: '" + lsymp->name() + "'");
            return;
        }
        if (namep->fileline()->language() >= V3LangCode::L1076_2008) return;
        const ObjInfo info = objectOfSymbol(namep->vhdlSymp());
        if (!info.modeOut || !nameIsRead(namep)) return;
        err(namep->fileline(),
            std::string{"A "} + (info.symp->kind() == VhdlSymKind::PORT ? "port" : "parameter")
                + " of mode out cannot be read before VHDL-2008: '" + info.symp->name() + "'");
    });
}

// A pure function returns the same value for the same actuals, so it reads no
// signal, variable or file declared outside it, and calls no impure function
// (4.2.1). nvc 1.22.1, GHDL 7.0.0-dev and Questa 2025.2 hold the function to
// that where it is the innermost subprogram, but take a procedure nested in it
// that reads what the function could not, and take an attribute of an object
// declared outside -- s'length, s'event -- as asking about it, not reading it.
void VhdlSemVisitor::checkPurity(AstNodeVhdlUnit* unitp) {
    if (m_inSpecialized) return;
    // The innermost subprogram a node is written in, if it is a pure function
    // analyzed as written rather than a copy made of one. A parameter's default
    // is evaluated where the function is called, so the specification is not
    // the function's own text (UVVM defaults a parameter to a shared variable).
    const auto pureFunctionOf = [](const AstNode* np) -> const AstVhdlSubprogDecl* {
        const AstNode* belowp = np;
        for (const AstNode* upp = parentOf(np); upp; belowp = upp, upp = parentOf(upp)) {
            if (VN_IS(upp, VhdlPackageInst)) return nullptr;
            if (const AstVhdlSubprogDecl* const subp = VN_CAST(upp, VhdlSubprogDecl)) {
                if (!subp->isFunction() || !subp->vhdlPure() || subp->inPkgInstance()
                    || !subp->templateName().empty() || VN_IS(belowp, VhdlIfaceDecl)
                    || belowp == subp->returnTypep()) {
                    return nullptr;
                }
                return subp;
            }
        }
        return nullptr;
    };
    // The name a prefix of: climbing a call's or a selection's prefix, where it
    // ends as an attribute's prefix, or as what a call calls
    const auto climbPrefix = [](const AstNode* np) {
        for (;;) {
            const AstNode* const upp = parentOf(np);
            const AstVhdlCall* const callp = VN_CAST(upp, VhdlCall);
            const AstVhdlSelected* const selp = VN_CAST(upp, VhdlSelected);
            if ((callp && callp->prefixp() == np) || (selp && selp->prefixp() == np)) {
                np = upp;
                continue;
            }
            return np;
        }
    };
    // NOW is impure too (16.3), but GHDL 7.0.0-dev's own testsuite has a pure
    // function return it (gna/issue1691), and a corpus that must be accepted
    // outranks nvc 1.22.1 and Questa 2025.2 refusing it
    const auto impure = [](const VhdlSymbol* symp) {
        if (symp->kind() != VhdlSymKind::FUNCTION || symp->predefined()) return false;
        const AstVhdlSubprogDecl* const declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
        return declp && !declp->vhdlPure();
    };
    unitp->foreach([&](const AstVhdlName* namep) {
        const VhdlSymbol* const symp = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
        if (!symp) return;
        const AstVhdlSubprogDecl* const funcp = pureFunctionOf(namep);
        if (!funcp) return;
        if (symp->kind() == VhdlSymKind::FUNCTION) {
            // What a call names is checked at the call
            const AstNode* calleep = namep;
            if (const AstVhdlSelected* const selp = VN_CAST(parentOf(namep), VhdlSelected)) {
                if (selp->suffixp() == namep) calleep = selp;
            }
            const AstVhdlCall* const callp = VN_CAST(parentOf(calleep), VhdlCall);
            if (callp && callp->prefixp() == calleep) return;
            if (impure(symp)) {
                err(namep->fileline(), "A pure function cannot call impure function '"
                                           + symp->name() + "': '" + funcp->prettyName() + "'");
            }
            return;
        }
        const bool object
            = symp->kind() == VhdlSymKind::SIGNAL || symp->kind() == VhdlSymKind::PORT
              || symp->kind() == VhdlSymKind::VARIABLE
              || symp->kind() == VhdlSymKind::SHARED_VARIABLE || symp->kind() == VhdlSymKind::FILE;
        if (!object || !symp->declp()) return;
        if (VN_IS(parentOf(climbPrefix(namep)), VhdlAttrName)) return;
        for (const AstNode* upp = parentOf(symp->declp()); upp; upp = parentOf(upp)) {
            if (upp == funcp) return;
        }
        const char* const what = symp->kind() == VhdlSymKind::FILE ? "a file"
                                 : symp->kind() == VhdlSymKind::SHARED_VARIABLE
                                     ? "a shared variable"
                                 : symp->kind() == VhdlSymKind::VARIABLE ? "a variable"
                                                                         : "a signal";
        err(namep->fileline(), "A pure function reads nothing declared outside it, and '"
                                   + symp->name() + "' is " + what + " declared outside '"
                                   + funcp->prettyName() + "'");
    });
    unitp->foreach([&](const AstVhdlCall* callp) {
        const VhdlSymbol* const symp = callp->vhdlSymp() ? callp->vhdlSymp()->resolved() : nullptr;
        if (!symp || !impure(symp)) return;
        const AstVhdlSubprogDecl* const funcp = pureFunctionOf(callp);
        if (!funcp) return;
        err(callp->fileline(), "A pure function cannot call impure function '" + symp->name()
                                   + "': '" + funcp->prettyName() + "'");
    });
    unitp->foreach([&](const AstVhdlExternalName* extp) {
        if (extp->objClass() == "constant") return;
        const AstVhdlSubprogDecl* const funcp = pureFunctionOf(extp);
        if (!funcp) return;
        err(extp->fileline(),
            "A pure function reads nothing declared outside it, and an"
            " external name denotes "
                + std::string{extp->objClass() == "signal" ? "a signal" : "a variable"} + ": '"
                + funcp->prettyName() + "'");
    });
}

// What makes a signal name not static (9.4.3): an object in an index, as
// for any name, or a loop parameter, which takes each of its values in turn
const VhdlSymbol* VhdlSemVisitor::nonStaticSignalIndex(AstNodeVhdlExpr* exprp) {
    const VhdlSymbol* varyingp = nonStaticIndex(exprp);
    for (AstNodeVhdlExpr* np = exprp; np && !varyingp;) {
        AstVhdlCall* const callp = VN_CAST(np, VhdlCall);
        if (!callp) {
            AstVhdlSelected* const selp = VN_CAST(np, VhdlSelected);
            np = selp ? selp->prefixp() : nullptr;
            continue;
        }
        for (AstNode* ap = callp->argsp(); ap; ap = ap->nextp()) {
            ap->foreach([&](const AstVhdlName* namep) {
                const VhdlSymbol* const symp
                    = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
                if (!varyingp && symp && symp->kind() == VhdlSymKind::LOOP_PARAMETER) {
                    varyingp = symp;
                }
            });
        }
        np = callp->prefixp();
    }
    return varyingp;
}

// An object of a name's indexes or slice bounds that is not static: a
// variable or a signal, whose value is only known as the design runs
const VhdlSymbol* VhdlSemVisitor::nonStaticIndex(AstNodeVhdlExpr* nodep) {
    for (int depth = 0; nodep && depth < 64; ++depth) {
        if (AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            nodep = selp->prefixp();
            continue;
        }
        AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
        if (!callp) return nullptr;
        const VhdlSymbol* foundp = nullptr;
        for (AstNode* ap = callp->argsp(); ap && !foundp; ap = ap->nextp()) {
            ap->foreach([&](const AstVhdlName* namep) {
                const VhdlSymbol* const symp
                    = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
                if (!foundp && symp
                    && (symp->kind() == VhdlSymKind::VARIABLE
                        || symp->kind() == VhdlSymKind::SHARED_VARIABLE
                        || symp->kind() == VhdlSymKind::SIGNAL
                        || symp->kind() == VhdlSymKind::PORT)) {
                    foundp = symp;
                }
            });
        }
        if (foundp) return foundp;
        nodep = callp->prefixp();
    }
    return nullptr;
}

void VhdlSemVisitor::visit(AstVhdlBlock* nodep) {
    // The maps are written outside the block's own region, since they name
    // what is visible where the block appears (11.2)
    if (nodep->headerp()) {
        analyzeAssociations(nodep->headerp()->genericMapp(), nullptr, true);
        analyzeAssociations(nodep->headerp()->portMapp(), nullptr);
    }
    // The guard expression names what is visible where the block appears; the
    // GUARD signal it declares is visible only inside (11.2)
    if (nodep->guardp()) analyzeCondition(nodep->guardp());
    pushScope(nodep->name().empty() ? "block" : nodep->name());
    if (nodep->guardp()) {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::SIGNAL, "guard", nodep);
        symp->typep(V3VhdlStd::booleanType());
        nodep->guardSymp(symp);
        declare(symp, nodep->fileline());
    }
    if (nodep->headerp()) {
        m_ifacePos = 0;
        iterateAndNextNull(nodep->headerp()->genericsp());
        m_ifacePos = 0;
        iterateAndNextNull(nodep->headerp()->portsp());
        // A block is elaborated where it stands, so a port left unconstrained
        // simply takes the subtype of the one actual it is ever given
        constrainBlockPorts(nodep->headerp());
        // A conversion written on the formal converts what the port drives
        // on its way out to the actual (6.5.7.1). It names the port, which
        // is declared here, so it is analyzed once that has happened.
        for (AstNode* p = nodep->headerp()->portMapp(); p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (asp->formalp() && !VN_IS(asp->formalp(), VhdlName)) {
                analyzeExpr(asp->formalp(), nullptr);
            }
        }
    }
    // A generic with no default is given a value by the block's generic
    // map (6.5.6.2), just as by an instance's
    if (nodep->headerp() && !m_inSpecialized) {
        int pos = 0;
        for (const AstNode* gp = nodep->headerp()->genericsp(); gp; gp = gp->nextp(), ++pos) {
            const AstVhdlIfaceDecl* const ifp = VN_CAST(gp, VhdlIfaceDecl);
            if (!ifp || ifp->defaultp() || ifp->kind() != VhdlIfaceKind::CONSTANT) continue;
            bool given = false;
            int apos = 0;
            for (const AstNode* ap = nodep->headerp()->genericMapp(); ap;
                 ap = ap->nextp(), ++apos) {
                const AstVhdlAssoc* const asp = VN_AS(ap, VhdlAssoc);
                const AstVhdlName* const fp = VN_CAST(asp->formalp(), VhdlName);
                if (fp ? fp->name() == ifp->name() : (!asp->formalp() && apos == pos)) {
                    given = given || !asp->isOpen();
                }
            }
            if (!given) {
                err(nodep->fileline(), "Generic '" + ifp->prettyName()
                                           + "' has no default value, so it must be given one"
                                             " in the block's generic map");
            }
        }
    }
    if (!m_inSpecialized) rejectUnsharedVariables(nodep->declsp(), "a block");
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    checkConfigSpecs(nodep->declsp(), nodep->stmtsp());
    popScope();
}

// A block port whose subtype is unconstrained takes that of its actual
void VhdlSemVisitor::constrainBlockPorts(AstVhdlBlockHeader* nodep) {
    std::vector<AstVhdlAssoc*> assocs;
    for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
        assocs.push_back(VN_AS(p, VhdlAssoc));
    }
    size_t position = 0;
    for (AstNode* p = nodep->portsp(); p; p = p->nextp(), ++position) {
        AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        VhdlSymbol* const symp = ifp->vhdlSymp();
        if (!symp || !symp->typep()) continue;
        // A record with an element of an unconstrained array type is open
        // too, though its own index constraint says nothing (5.3.3)
        const VhdlType* const declp = symp->typep();
        const bool isRecord = declp->base()->kind() == VhdlTypeKind::RECORD;
        if (!declp->unconstrained() && !(isRecord && !fullyConstrainedRecord(declp))) continue;
        for (size_t i = 0; i < assocs.size(); ++i) {
            const AstVhdlName* const namep = VN_CAST(assocs[i]->formalp(), VhdlName);
            // A conversion written on the formal means the actual's subtype
            // is not the port's: what the port drives goes through the
            // conversion on its way there (6.5.7.1). A conversion is
            // element by element, so the port is as long as the actual --
            // which is all an unconstrained port needs to know.
            if (assocs[i]->formalp() && !namep) {
                if (i != position || isRecord) continue;
                VhdlType* const fromp
                    = assocs[i]->actualp() ? assocs[i]->actualp()->vhdlTypep() : nullptr;
                if (!fromp || !fromp->isConstrainedArray() || fromp->indexTypes().size() != 1
                    || declp->indexTypes().size() != 1) {
                    continue;
                }
                VhdlType* const portTypep = cloneSubtype(symp->typep(), ifp->fileline());
                portTypep->clearIndexTypes();
                portTypep->addIndexType(fromp->indexTypes()[0]);
                portTypep->unconstrained(false);
                symp->typep(portTypep);
                break;
            }
            if (namep ? namep->name() != symp->name() : i != position) continue;
            if (!assocs[i]->actualp() || VN_IS(assocs[i]->actualp(), VhdlOpen)) break;
            VhdlType* const actualp = assocs[i]->actualp()->vhdlTypep();
            if (!actualp) break;
            if (isRecord ? fullyConstrainedRecord(actualp) : !actualp->unconstrained()) {
                symp->typep(actualp);
            }
            break;
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlGenerate* nodep) {
    // The range is named outside the generate's own region, but the parameter
    // it declares is inside it (11.8)
    RangeInfo info;
    if (nodep->kind() == VhdlLoopKind::FOR) {
        info = analyzeRange(nodep->rangep(), nullptr);
        checkDiscreteRange(info, nodep->rangep(), "A for generate");
    }
    // A case generate's selector is named outside the generate too, and each
    // alternative's choices are of its type (11.8), the same rule the case
    // statement follows
    VL_RESTORER(m_genSelTypep);
    m_genSelTypep = nodep->exprp() ? analyzeExpr(nodep->exprp(), nullptr) : nullptr;
    // The alternatives' labels are distinct (11.8)
    if (!m_inSpecialized) {
        std::set<std::string> altLabels;
        for (const AstVhdlGenerateBody* bp = nodep->altsp(); bp;
             bp = VN_CAST(bp->nextp(), VhdlGenerateBody)) {
            if (!bp->name().empty() && !altLabels.insert(bp->name()).second) {
                err(bp->fileline(), "The alternatives of a generate statement have distinct"
                                    " labels, and '"
                                        + bp->name() + "' is used twice");
            }
        }
    }
    pushScope(nodep->name().empty() ? "generate" : nodep->name());
    if (nodep->kind() == VhdlLoopKind::FOR) {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::GENERATE_PARAMETER,
                                                       nodep->paramName(), nodep);
        symp->typep(loopParamType(info, nodep->fileline(),
                                  VN_CAST(nodep->rangep(), VhdlRange)));
        nodep->vhdlSymp(symp);
        declare(symp, nodep->fileline());
    }
    iterateAndNextNull(nodep->altsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlGenerateBody* nodep) {
    // The condition is an expression, not a statement, so it needs analyzing
    // rather than iterating: iterating would leave its names unresolved
    if (nodep->condp()) analyzeCondition(nodep->condp());
    for (AstNode* p = nodep->choicesp(); p; p = p->nextp()) {
        analyzeChoice(VN_AS(p, VhdlChoice), m_genSelTypep);
    }
    pushScope(nodep->name().empty() ? "generate body" : nodep->name());
    if (!m_inSpecialized) rejectUnsharedVariables(nodep->declsp(), "a generate statement");
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

// A loop or generate parameter steps through a discrete range (10.10, 11.8)
void VhdlSemVisitor::checkDiscreteRange(const RangeInfo& info, const AstNode* rangep,
                                        const char* what) {
    if (!info.typep || info.typep->isError() || info.typep->isGenericFormal()
        || info.typep->isDiscrete() || m_inSpecialized || !rangep) {
        return;
    }
    err(rangep->fileline(), std::string{what}
                                + " runs over a discrete range, and this one is"
                                  " of type "
                                + info.typep->prettyName());
}

void VhdlSemVisitor::visit(AstVhdlLoop* nodep) {
    if (nodep->kind() == VhdlLoopKind::FOR) {
        // The loop parameter is declared by the loop itself, and its subtype
        // is that of the discrete range (10.10)
        const RangeInfo info = analyzeRange(nodep->rangep(), nullptr);
        checkDiscreteRange(info, nodep->rangep(), "A for loop");
        pushScope("loop");
        VhdlSymbol* const symp
            = VhdlSymTab::newSymbol(VhdlSymKind::LOOP_PARAMETER, nodep->paramName(), nodep);
        symp->typep(loopParamType(info, nodep->fileline()));
        nodep->vhdlSymp(symp);
        declare(symp, nodep->fileline());
        iterateAndNextNull(nodep->stmtsp());
        popScope();
        return;
    }
    // A while loop tests a condition of type BOOLEAN before each pass (10.10)
    if (nodep->condp()) analyzeCondition(nodep->condp());
    iterateAndNextNull(nodep->stmtsp());
}

// The subtype of a loop or generate parameter: the range's type, narrowed to
// the range itself where it is static (10.10, 11.8)
VhdlType* VhdlSemVisitor::loopParamType(const RangeInfo& info, FileLine* flp,
                                        AstVhdlRange* rangep) {
    if (!info.typep || info.typep->isError()) return V3VhdlStd::integerType();
    if (!info.left.known() || !info.right.known()) {
        // A range that names a generic -- "for i in 0 to COUNT-1" -- is fixed
        // once the instance is elaborated but is not a number here. Handing
        // back the base type instead would say the loop runs over every value
        // NATURAL has, which is not a design.
        if (rangep && rangep->leftp() && rangep->rightp()) {
            VhdlType* const outp = cloneSubtype(info.typep, flp);
            outp->unbound();
            outp->boundsp(rangep);
            return outp;
        }
        return info.typep;
    }
    return makeRangeSubtype(info, flp);
}

//######################################################################
// Design units

void VhdlSemVisitor::setupUnitContext(AstNodeVhdlUnit* unitp) {
    m_curUnitp = unitp;
    // A unit written before VHDL-2008 sees STANDARD as its revision had it
    VhdlScope::hide2008(unitp->fileline()->language() < V3LangCode::L1076_2008);
    m_libname = unitp->libname().empty() ? "work" : lower(unitp->libname());

    // Every design unit begins as if it said
    //   library STD, WORK;  use STD.STANDARD.all;      (13.2)
    m_scopep = VhdlSymTab::newScope(nullptr, unitp->name() + "$context");
    m_rootp = m_scopep;
    m_scopep->addUse(V3VhdlStd::standardScope());
    m_scopep->addNamed("std", V3VhdlStd::stdLibScope());
    m_scopep->addNamed("work", libScope(m_libname));
    m_scopep->addNamed(m_libname, libScope(m_libname));
    declareLibrary("std");
    declareLibrary("work");

    // The context clause of an entity or package also applies to its
    // secondary units (13.1), so gather both lists
    std::vector<AstNode*> contexts{unitp->contextp()};
    std::string primaryName;
    if (const AstVhdlArchitecture* const archp = VN_CAST(unitp, VhdlArchitecture)) {
        std::vector<std::string> parts;
        if (flatten(archp->entityNamep(), parts) && !parts.empty()) primaryName = parts.back();
    } else if (VN_IS(unitp, VhdlPackageBody)) {
        primaryName = lower(unitp->name());
    }
    if (!primaryName.empty()) {
        const auto it = m_primaryUnits.find(m_libname + "." + primaryName);
        if (it != m_primaryUnits.end()) contexts.insert(contexts.begin(), it->second->contextp());
    }

    // The unit's own name is declared by its library, whose region takes in
    // the unit, so it hides a homograph a use clause would make visible there
    // -- an enumeration literal RTL inside "architecture rtl" (12.3, 12.4).
    // An architecture's entity is named the same way, which is how one
    // instantiates itself: "entity fft" inside an architecture of fft.
    // A package's name is left out: a copy of one of its generic subprograms
    // is analyzed in a region that does not reach its declarations, and would
    // find the package where it means a type of the same name (nvc
    // regress/issue654).
    if (VN_IS(unitp, VhdlEntity) || VN_IS(unitp, VhdlArchitecture)) {
        const VhdlSymKind kind
            = VN_IS(unitp, VhdlEntity) ? VhdlSymKind::ENTITY : VhdlSymKind::ARCHITECTURE;
        const std::string ownName = lower(unitp->name());
        const bool archOfSame = kind == VhdlSymKind::ARCHITECTURE && ownName == primaryName;
        if (!archOfSame) m_scopep->insert(VhdlSymTab::newSymbol(kind, ownName, unitp));
        if (kind == VhdlSymKind::ARCHITECTURE && !primaryName.empty()) {
            const auto it = m_primaryUnits.find(m_libname + "." + primaryName);
            if (it != m_primaryUnits.end()) {
                VhdlSymbol* const entp
                    = VhdlSymTab::newSymbol(VhdlSymKind::ENTITY, primaryName, it->second);
                const auto scopeIt = m_unitScopes.find(m_libname + "." + primaryName);
                if (scopeIt != m_unitScopes.end()) entp->innerp(scopeIt->second);
                m_scopep->insert(entp);
            }
        }
    }

    for (AstNode* listp : contexts) {
        for (AstNode* itemp = listp; itemp; itemp = itemp->nextp()) {
            applyContextItem(VN_AS(itemp, VhdlContextItem));
        }
    }
}

// A library's logical name is a declaration like any other, so it hides a
// homograph a use clause would make visible: a type named WORK is not
// directly visible where WORK names the library (12.3, 13.2)
void VhdlSemVisitor::declareLibrary(const std::string& name) {
    std::vector<VhdlSymbol*> here;
    m_scopep->lookupLocal(name, here);
    for (const VhdlSymbol* const symp : here) {
        if (symp->kind() == VhdlSymKind::LIBRARY) return;
    }
    m_scopep->insert(VhdlSymTab::newSymbol(VhdlSymKind::LIBRARY, name, nullptr));
}

// A library clause names a library; a use clause makes a package's
// declarations potentially visible (13.2). Both may appear in a context
// clause and, for a use clause, in any declarative part as well (12.4).
void VhdlSemVisitor::applyContextItem(AstVhdlContextItem* ctxp) {
    for (AstNode* namep = ctxp->namesp(); namep; namep = namep->nextp()) {
        std::vector<std::string> parts;
        AstNodeVhdlExpr* const exprp = VN_AS(namep, NodeVhdlExpr);
        if (ctxp->kind() == VhdlContextKind::LIBRARY) {
            const AstVhdlName* const lnp = VN_CAST(exprp, VhdlName);
            if (lnp) {
                m_scopep->addNamed(lnp->name(), libScope(lnp->name()));
                declareLibrary(lnp->name());
            }
            continue;
        }
        if (ctxp->kind() == VhdlContextKind::CONTEXT_REF) {
            // "context lib.name;" means the context items that context
            // declares, written out here (13.4). It is not a use clause: the
            // declaration has nothing of its own to make visible.
            std::vector<std::string> refParts;
            if (!flattenUse(exprp, refParts) || refParts.empty()) continue;
            std::string reflib = refParts.size() >= 2 ? refParts[0] : m_libname;
            if (reflib == "work") reflib = m_libname;
            const auto ctxIt = m_contextDecls.find(reflib + "." + refParts.back());
            if (ctxIt == m_contextDecls.end()) {
                err(ctxp->fileline(), "Cannot find context '" + refParts.back() + "'");
                continue;
            }
            if (m_contextDepth > 16) continue;  // A context that names itself
            VL_RESTORER(m_contextDepth);
            ++m_contextDepth;
            for (AstNode* p = ctxIt->second->contextp(); p; p = p->nextp()) {
                applyContextItem(VN_AS(p, VhdlContextItem));
            }
            continue;
        }
        // A use clause: "use lib.unit.all" or "use lib.unit.item"
        if (!flattenUse(exprp, parts) || parts.size() < 2) continue;
        // Each name is an expanded name whose prefix is a library or a
        // package, so nothing follows ".all" and no entity is looked into
        // (12.4)
        if (!m_inSpecialized) {
            const auto allIt = std::find(parts.begin(), parts.end(), "all");
            if (allIt != parts.end() && allIt + 1 != parts.end()) {
                err(exprp->fileline(), "Nothing follows '.all' in a use clause");
                continue;
            }
            // The prefix is a name visible here: a library only once a library
            // clause has named it, WORK and STD aside (13.2)
            if (!m_scopep->findNamed(parts[0])) {
                err(exprp->fileline(), "'" + parts[0]
                                           + "' is not visible here: a library is named by a"
                                             " library clause before a use clause looks into it");
                continue;
            }
            if (parts.size() >= 3) {
                const std::string ulib = parts[0] == "work" ? m_libname : parts[0];
                const auto unitIt = m_primaryUnits.find(ulib + "." + parts[1]);
                // A package with generics is used through an instance of it
                // (4.9); what the uninstantiated one declares has no values
                const AstVhdlPackage* const genPkgp = unitIt != m_primaryUnits.end()
                                                          ? VN_CAST(unitIt->second, VhdlPackage)
                                                          : nullptr;
                if (genPkgp && genPkgp->genericsp() && !genPkgp->genericMapp() && !m_inPkgInst) {
                    err(exprp->fileline(), "Package '" + parts[1]
                                               + "' has generics, so a use clause names an"
                                                 " instance of it rather than it");
                    continue;
                }
                if (unitIt != m_primaryUnits.end()
                    && (VN_IS(unitIt->second, VhdlEntity)
                        || VN_IS(unitIt->second, VhdlConfiguration))) {
                    err(exprp->fileline(),
                        "A use clause looks into a library or a package, and '" + parts[0] + "."
                            + parts[1] + "' is "
                            + (VN_IS(unitIt->second, VhdlEntity) ? "an entity"
                                                                 : "a configuration"));
                    continue;
                }
            }
        }
        // "use inst.all", where inst is a package instantiated in this
        // region rather than a design unit of a library (4.9, 12.4)
        if (parts.size() == 2) {
            if (VhdlScope* const localp = m_scopep->findNamed(parts[0])) {
                if (parts[1] == "all") {
                    m_scopep->addUse(localp);
                    continue;
                }
                std::vector<VhdlSymbol*> found;
                localp->lookupLocal(parts[1], found);
                if (!found.empty()) {
                    m_scopep->addUseOf(localp, usedNames(localp, parts[1]));
                    continue;
                }
            }
        }
        std::string lib = parts[0];
        if (lib == "work") lib = m_libname;
        // "use work.all" makes the library's design units visible by simple
        // name (12.4), which "entity e" relies on
        if (parts.size() == 2 && parts[1] == "all") {
            m_scopep->addNamed("*all:" + lib, libScope(lib));
        }
        VhdlScope* targetp = nullptr;
        const auto it = m_unitScopes.find(lib + "." + parts[1]);
        if (it != m_unitScopes.end()) {
            targetp = it->second;
        } else {
            // A package Verilator supplies, such as STD.TEXTIO
            targetp = libScope(lib)->findNamed(parts[1]);
        }
        if (!targetp) continue;  // Not one of ours, or not yet analyzed
        m_scopep->addNamed(parts[1], targetp);
        // "use pkg.all" makes everything potentially visible, and
        // "use pkg.name" only the declarations of that name
        if (parts.size() == 3 && parts[2] != "all") {
            m_scopep->addUseOf(targetp, usedNames(targetp, parts[2]));
        } else {
            m_scopep->addUse(targetp);
        }
    }
}

// What "use pkg.name" identifies (12.4): the declarations of that name, and
// where it is a type mark, the enumeration literals, the units and the
// predefined operations of its type that the package declares, with the
// package's own homographs of those operations. nvc 1.22.1, GHDL 7.0.0-dev
// and Questa 2025.2 give a VHDL-93 design the literals as well, as 2008 does.
std::set<std::string> VhdlSemVisitor::usedNames(const VhdlScope* pkgScopep,
                                                const std::string& name) {
    std::set<std::string> names{name};
    std::vector<VhdlSymbol*> found;
    pkgScopep->lookupLocal(name, found);
    const VhdlType* basep = nullptr;
    for (const VhdlSymbol* const symp : found) {
        // An alias of a type is a type mark as well
        const VhdlSymbol* const realp = symp->resolved();
        if (realp->kind() == VhdlSymKind::TYPE && realp->typep()) basep = realp->typep()->base();
    }
    if (!basep) return names;
    // The designators of the predefined operations (5.2.6, 5.3.2.4, 5.4.2,
    // 5.5.2, 9.2), which an explicit homograph in the package may hide
    static const std::set<std::string> s_opNames{
        "=",       "/=",        "<",          "<=",    ">",         ">=",   "minimum",   "maximum",
        "+",       "-",         "abs",        "*",     "/",         "mod",  "rem",       "**",
        "and",     "or",        "nand",       "nor",   "xor",       "xnor", "not",       "sll",
        "srl",     "sla",       "sra",        "rol",   "ror",       "&",    "?=",        "?/=",
        "?<",      "?<=",       "?>",         "?>=",   "??",        "read", "to_string", "write",
        "endfile", "file_open", "file_close", "flush", "deallocate"};
    const auto ofType = [basep](const VhdlType* typep) { return typep && typep->base() == basep; };
    for (const auto& pair : pkgScopep->direct()) {
        const VhdlSymbol* const symp = pair.second;
        bool identified = false;
        switch (symp->kind()) {
        case VhdlSymKind::ENUM_LITERAL:
        case VhdlSymKind::PHYSICAL_UNIT: identified = ofType(symp->typep()); break;
        case VhdlSymKind::FUNCTION:
        case VhdlSymKind::PROCEDURE:
            if (!symp->predefined() && !s_opNames.count(pair.first)) break;
            identified = ofType(symp->returnTypep());
            for (const VhdlType* const ptp : symp->paramTypes()) {
                if (ofType(ptp)) identified = true;
            }
            break;
        default: break;
        }
        if (identified) names.insert(pair.first);
    }
    return names;
}

void VhdlSemVisitor::visit(AstVhdlContextItem* nodep) { applyContextItem(nodep); }

// Print a region and everything declared in it, for --dump-vhdl-syms
void VhdlSemVisitor::dumpScope(const VhdlScope* scopep, const std::string& indent) {
    for (const auto& pair : scopep->direct()) {
        const VhdlSymbol* const symp = pair.second;
        if (symp->predefined()) continue;  // std.standard would drown the rest
        std::cout << indent << symp->kindName() << " " << symp->name();
        if (symp->typep()) std::cout << " : " << symp->typep()->prettyName();
        // A type's range and a constant's value are what static evaluation
        // worked out, so they are the interesting part of the dump
        if (symp->kind() == VhdlSymKind::TYPE && symp->typep()) {
            const VhdlType* const typep = symp->typep();
            if (typep->bounded()) {
                std::cout << " range " << rangeText(typep);
            } else if (typep->base()->kind() == VhdlTypeKind::ARRAY) {
                std::cout << " (";
                bool first = true;
                for (const VhdlType* const idxp : typep->indexTypes()) {
                    if (!first) std::cout << ", ";
                    first = false;
                    // An unconstrained array has no range of its own, however
                    // bounded its index subtype may be
                    const bool known = idxp && idxp->bounded() && !typep->unconstrained();
                    std::cout << (known ? rangeText(idxp) : "<>");
                }
                std::cout << ")";
            }
        }
        if (symp->hasValue()) std::cout << " = " << symp->value().prettyName();
        if (symp->kind() == VhdlSymKind::FUNCTION || symp->kind() == VhdlSymKind::PROCEDURE) {
            std::cout << " (";
            for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
                if (i) std::cout << ", ";
                std::cout << (symp->paramTypes()[i] ? symp->paramTypes()[i]->prettyName()
                                                    : "<unknown>");
            }
            std::cout << ")";
        }
        std::cout << "\n";
        if (symp->innerp() && symp->innerp() != scopep) {
            dumpScope(symp->innerp(), indent + "  ");
        }
    }
}

void VhdlSemVisitor::analyzeUnits(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    const bool dump = v3Global.opt.dumpLevel("vhdl-syms") != 0;
    // A package instantiation needs the body as well as the declaration, and
    // may be written before either is analyzed, so find them all first
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        if (AstVhdlPackageBody* const bodyp = VN_CAST(unitp, VhdlPackageBody)) {
            const std::string lib = bodyp->libname().empty() ? "work" : lower(bodyp->libname());
            m_packageBodies[lib + "." + lower(bodyp->name())] = bodyp;
        }
        // A generic subprogram may be instantiated from a unit that sees only
        // its declaration, while the body it has to copy is in a package body
        // analyzed later (4.4)
        unitp->foreach([this](AstVhdlSubprogDecl* declp) {
            if (declp->genericCount() > 0 && declp->hasBody()) {
                m_genericBodies.emplace(declp->name(), declp);
            }
        });
    }
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        setupUnitContext(unitp);
        const std::string lib = m_libname;
        VhdlScope* const unitScopep = pushScope(unitp->name());

        unitScopep->prefixes(lib + ":" + lower(unitp->name()),
                             lib + ":" + lower(unitp->name()));
        if (AstVhdlEntity* const entp = VN_CAST(unitp, VhdlEntity)) {
            openPortGenerics(entp->genericsp(), entp, unitScopep);
            openPortGenerics(entp->portsp(), entp, unitScopep);
            m_ifacePos = 0;
            iterateAndNextNull(entp->genericsp());
            fillOpenGenericDefaults(unitScopep);
            m_ifacePos = 0;
            iterateAndNextNull(entp->portsp());
            if (!m_inSpecialized) rejectUnsharedVariables(entp->declsp(), "an entity");
            iterateAndNextNull(entp->declsp());
            // An entity's statements are passive: they may watch the design
            // but drive nothing, since every architecture shares them (3.2)
            for (AstNode* dp = entp->declsp(); dp && !m_inSpecialized; dp = dp->nextp()) {
                if (!VN_IS(dp, NodeVhdlStmt)) continue;
                AstVhdlSigAssign* drivep = VN_CAST(dp, VhdlSigAssign);
                if (!drivep) {
                    dp->foreach([&](AstVhdlSigAssign* asgp) {
                        if (!drivep) drivep = asgp;
                    });
                }
                if (drivep) {
                    err(drivep->fileline(), "An entity's statements are passive, so they cannot"
                                            " assign a signal");
                }
            }
            m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
            m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
            libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        } else if (AstVhdlArchitecture* const archp = VN_CAST(unitp, VhdlArchitecture)) {
            // An architecture continues its entity's declarative region
            // (3.3, 12.1), so a port is directly visible in it -- and hides
            // an enumeration literal of that name a use clause brought in
            std::vector<std::string> parts;
            if (flatten(archp->entityNamep(), parts) && !parts.empty()) {
                const auto it = m_unitScopes.find(lib + "." + parts.back());
                if (it != m_unitScopes.end()) {
                    m_scopep->addExtends(it->second);
                    // and reachable by selection, for a package the entity
                    // declares: "use pack_int.all" in the architecture
                    m_scopep->addUse(it->second);
                }
                // An instance path names the entity, and the instance form
                // names the architecture with it (16.2.7)
                unitScopep->prefixes(parts.back() + "(" + lower(archp->name()) + ")",
                                     parts.back());
                // A configuration of this architecture names what it
                // declares, so its region has to be findable (3.4)
                m_unitScopes[lib + "." + parts.back() + "(" + lower(archp->name()) + ")"]
                    = unitScopep;
                // Default binding takes the architecture analyzed last (7.3.3),
                // which is the one an entity copied for a generic type gets
                m_archUnits[lib + "." + parts.back()] = archp;
                m_archByKey[lib + "." + parts.back() + "(" + lower(archp->name()) + ")"] = archp;
            }
            if (!m_inSpecialized) rejectUnsharedVariables(archp->declsp(), "an architecture");
            VL_RESTORER(m_archScopep);
            VL_RESTORER(m_archEntityScopep);
            m_archScopep = m_scopep;
            if (!parts.empty()) {
                const auto it = m_unitScopes.find(lib + "." + parts.back());
                m_archEntityScopep = it != m_unitScopes.end() ? it->second : nullptr;
            }
            iterateAndNextNull(archp->declsp());
            iterateAndNextNull(archp->stmtsp());
            checkConfigSpecs(archp->declsp(), archp->stmtsp());
            analyzeDrivers(archp);
        } else if (AstVhdlPackage* const pkgp = VN_CAST(unitp, VhdlPackage)) {
            // A package's own name is visible inside it (12.1)
            unitScopep->addNamed(lower(unitp->name()), unitScopep);
            iterateAndNextNull(pkgp->genericsp());
            if (!m_inSpecialized) rejectUnsharedVariables(pkgp->declsp(), "a package");
            iterateAndNextNull(pkgp->declsp());
            m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
            m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
            libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        } else if (AstVhdlPackageInst* const instp = VN_CAST(unitp, VhdlPackageInst)) {
            // The same as a package instantiated inside a declarative part,
            // except that this one is a design unit of its own (4.9)
            instantiatePackage(instp, unitScopep);
            m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
            m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
            libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        } else if (AstVhdlPackageBody* const bodyp = VN_CAST(unitp, VhdlPackageBody)) {
            const auto it = m_unitScopes.find(lib + "." + lower(unitp->name()));
            if (it != m_unitScopes.end()) {
                m_scopep->addUse(it->second);
                // The body continues the declaration's declarative region
                // (12.1), so whatever a use clause there made visible, and
                // whatever regions it named, are visible here too
                for (VhdlScope* const usedp : it->second->uses()) m_scopep->addUse(usedp);
                for (const auto& pair : it->second->named()) {
                    m_scopep->addNamed(pair.first, pair.second);
                }
            }
            m_unitScopes[lib + "." + lower(unitp->name()) + "%body"] = unitScopep;
            const auto pkgIt = m_primaryUnits.find(lib + "." + lower(unitp->name()));
            if (pkgIt != m_primaryUnits.end()) {
                if (const AstVhdlPackage* const pkgp = VN_CAST(pkgIt->second, VhdlPackage)) {
                    m_pkgsWithBody.insert(pkgp);
                }
            }
            VL_RESTORER(m_inPackageBody);
            m_inPackageBody = true;
            if (!m_inSpecialized) rejectUnsharedVariables(bodyp->declsp(), "a package body");
            iterateAndNextNull(bodyp->declsp());
        } else if (AstVhdlConfiguration* const cfgp = VN_CAST(unitp, VhdlConfiguration)) {
            // A binding indication may carry generic and port maps of its
            // own. Their actuals name what the configured architecture
            // declares as well as what is visible where the configuration is
            // written (3.4, 7.3.2), so that region is made visible here.
            std::vector<std::string> parts;
            if (flatten(cfgp->entityNamep(), parts) && !parts.empty()) {
                std::string arch;
                if (AstVhdlBlockConfig* const bcp = cfgp->blockConfigp()) {
                    if (const AstVhdlEntitySpec* const specp
                        = VN_CAST(bcp->specp(), VhdlEntitySpec)) {
                        if (const AstVhdlName* const namep = VN_CAST(specp->namesp(), VhdlName)) {
                            arch = lower(namep->name());
                        }
                    }
                }
                const auto archIt
                    = m_unitScopes.find(lib + "." + parts.back() + "(" + arch + ")");
                if (archIt != m_unitScopes.end()) m_scopep->addUse(archIt->second);
            }
            if (!m_inSpecialized && !parts.empty()) checkConfiguration(cfgp, lib, parts.back());
            unitp->foreach([this](AstVhdlBinding* bindp) {
                for (AstNode* p = bindp->genericMapp(); p; p = p->nextp()) {
                    AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                    if (asp && asp->actualp()) analyzeExpr(asp->actualp(), nullptr);
                }
                // Not the port map: its actuals name the *component's*
                // ports, which live in the component's own region and say
                // which of the entity's ports each one becomes (7.3.2.2)
            });
        } else if (AstVhdlContextDecl* const cdeclp = VN_CAST(unitp, VhdlContextDecl)) {
            // A context declaration declares nothing itself: it is a name for
            // the context items a reference to it stands for (13.3), so it is
            // recorded rather than analyzed here
            m_contextDecls[lib + "." + lower(unitp->name())] = cdeclp;
        } else {
            iterateChildren(unitp);
        }
        popScope();
        if (dump) {
            std::cout << unitp->typeName() << " " << m_libname << "." << unitp->name() << "\n";
            dumpScope(unitScopep, "  ");
        }
    }
}

//######################################################################
// V3VhdlSem

//######################################################################
// Making one copy of a subprogram

// Everything the analyzer cached on a node belongs to the original, so a copy
// starts clean and is analyzed from scratch
class VhdlUnanalyzeVisitor final : public VNVisitor {
    void visit(AstNodeVhdlExpr* nodep) override {
        nodep->vhdlTypep(nullptr);
        nodep->vhdlSymp(nullptr);
        iterateChildren(nodep);
    }
    void visit(AstVhdlSubtypeInd* nodep) override {
        nodep->vhdlTypep(nullptr);
        iterateChildren(nodep);
    }
    void visit(AstNodeVhdlDecl* nodep) override {
        nodep->vhdlSymp(nullptr);
        iterateChildren(nodep);
    }
    void visit(AstVhdlLoop* nodep) override {
        nodep->vhdlSymp(nullptr);
        iterateChildren(nodep);
    }
    void visit(AstVhdlGenerate* nodep) override {
        nodep->vhdlSymp(nullptr);
        iterateChildren(nodep);
    }
    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // iterate, not iterateAndNext: the node given is a fresh clone with
    // nothing behind it, and walking a list from a node that has no back is
    // what a debug build rightly complains about
    explicit VhdlUnanalyzeVisitor(AstNode* nodep) { iterate(nodep); }
    ~VhdlUnanalyzeVisitor() override = default;
};

// "package q is new work.p generic map (...)" makes a copy of p with the
// generics fixed (4.9). A generic package is a template, exactly as an
// unconstrained subprogram is: the copy is analyzed from scratch, so
// everything inside it -- a type whose bounds name a generic, a subprogram
// whose body does -- comes out as ordinary VHDL with every bound known.
// A package declared inside another declarative region rather than as a
// design unit of its own (4.7). It has a region of its own, and its name is
// how the outside reaches into it.
void VhdlSemVisitor::visit(AstVhdlPackage* nodep) {
    VhdlScope* const outerp = m_scopep;
    VhdlScope* const innerp = pushScope(nodep->name());
    // A package's own name is visible inside it (12.1), so a nested package
    // may write "outer.t" for a type the outer one declares
    innerp->addNamed(lower(nodep->name()), innerp);
    iterateAndNextNull(nodep->genericsp());
    iterateAndNextNull(nodep->declsp());
    m_scopep = outerp;
    outerp->addNamed(lower(nodep->name()), innerp);
    VhdlSymbol* const symp
        = VhdlSymTab::newSymbol(VhdlSymKind::PACKAGE, nodep->name(), nodep);
    symp->innerp(innerp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::visit(AstVhdlPackageInst* nodep) {
    VhdlScope* const outerp = m_scopep;
    VhdlScope* const innerp = pushScope(nodep->name());
    m_scopep = outerp;
    instantiatePackage(nodep, innerp);
    m_scopep = outerp;
    // The instance is a name of its own, so "q.thing" and "use q.all" work
    outerp->addNamed(lower(nodep->name()), innerp);
    VhdlSymbol* const symp
        = VhdlSymTab::newSymbol(VhdlSymKind::PACKAGE, nodep->name(), nodep);
    symp->innerp(innerp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::instantiatePackage(AstVhdlPackageInst* nodep, VhdlScope* intop) {
    std::vector<std::string> parts;
    if (!flatten(nodep->uninstantiatedp(), parts) || parts.empty()) {
        err(nodep->fileline(), "A package instantiation needs the name of a package");
        return;
    }
    // "work.p" or just "p". WORK names the library being compiled into, so
    // it is the same unit either way (13.2)
    std::string key = m_libname + "." + lower(parts.back());
    if (parts.size() >= 2) {
        std::string lib = lower(parts[parts.size() - 2]);
        if (lib == "work") lib = m_libname;
        key = lib + "." + lower(parts.back());
    }
    bool nested = false;
    const auto unitIt = m_primaryUnits.find(key);
    AstVhdlPackage* pkgp
        = unitIt == m_primaryUnits.end() ? nullptr : VN_CAST(unitIt->second, VhdlPackage);
    if (!pkgp && parts.size() >= 3) {
        // The uninstantiated package may be one declared inside another --
        // "package p is new work.axi4s.make" -- and then the design unit is
        // the outer one and the rest of the name walks into it (4.9)
        std::string olib = lower(parts[0]);
        if (olib == "work") olib = m_libname;
        const auto outerIt = m_primaryUnits.find(olib + "." + lower(parts[1]));
        AstVhdlPackage* nestedp
            = outerIt == m_primaryUnits.end() ? nullptr : VN_CAST(outerIt->second, VhdlPackage);
        for (size_t i = 2; nestedp && i < parts.size(); ++i) {
            AstVhdlPackage* foundp = nullptr;
            for (AstNode* p = nestedp->declsp(); p; p = p->nextp()) {
                AstVhdlPackage* const np = VN_CAST(p, VhdlPackage);
                if (np && lower(np->name()) == lower(parts[i])) {
                    foundp = np;
                    break;
                }
            }
            nestedp = foundp;
        }
        pkgp = nestedp;
        if (pkgp) {
            // The copy is analyzed as if written where the nested package
            // is, so the outer package's own declarations are visible to it
            key = olib + "." + lower(parts[1]);
            nested = true;
        }
    }
    if (!pkgp) {
        err(nodep->fileline(), "Cannot find package '" + parts.back() + "' to instantiate");
        return;
    }
    // The actuals are read where the instantiation is written, before the
    // copy's own region is opened
    std::vector<AstVhdlAssoc*> assocs;
    for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
        assocs.push_back(VN_AS(p, VhdlAssoc));
    }

    VhdlScope* const savedp = m_scopep;
    m_scopep = intop;
    // The copy is analyzed where the *package* was written, not where it is
    // instantiated: its declarations name what its own context clause made
    // visible, which the instantiation site need not have at all
    const auto scopeIt = m_unitScopes.find(key);
    if (scopeIt != m_unitScopes.end() && scopeIt->second->parentp()) {
        const VhdlScope* const ctxp = scopeIt->second->parentp();
        for (VhdlScope* const usedp : ctxp->uses()) intop->addUse(usedp);
        for (const auto& pair : ctxp->named()) intop->addNamed(pair.first, pair.second);
    }
    // A package nested in another names what that one declares, and may name
    // the outer package itself -- "subtype t is axi4s.axis_t(...)"
    if (nested && scopeIt != m_unitScopes.end()) {
        intop->addUse(scopeIt->second);
        intop->addNamed(lower(parts[1]), scopeIt->second);
    }
    intop->addUse(savedp);
    VL_RESTORER(m_inPkgInst);
    VL_RESTORER_COPY(m_pkgGenTypeNames);
    m_inPkgInst = true;
    m_pkgGenTypeNames.clear();
    size_t position = 0;
    for (AstNode* p = pkgp->genericsp(); p; p = p->nextp(), ++position) {
        AstVhdlIfaceDecl* const formalp = VN_AS(p, VhdlIfaceDecl);
        AstNodeVhdlExpr* actualp = nullptr;
        // By name where the map names its formal, by position otherwise
        for (size_t i = 0; i < assocs.size(); ++i) {
            const AstVhdlName* const namep
                = assocs[i]->formalp() ? VN_CAST(assocs[i]->formalp(), VhdlName) : nullptr;
            if (namep) {
                if (namep->name() == formalp->name()) actualp = assocs[i]->actualp();
            } else if (i == position) {
                actualp = assocs[i]->actualp();
            }
        }
        // "open" written out means the same as leaving the formal out: it
        // takes its own default (6.5.7.1)
        if (actualp && VN_IS(actualp, VhdlOpen)) actualp = nullptr;
        if (!actualp) {
            if (formalp->defaultp()) {
                actualp = formalp->defaultp();
            } else if (formalp->profilep() && formalp->profilep()->defaultsToVisible()) {
                // "is <>": bound to whatever subprogram of that designator
                // and profile is visible where the package is instantiated
                // (6.5.6.2). Declaring nothing leaves the copy's region
                // transparent to the instantiation site, which the addUse
                // above has already made reachable.
                continue;
            } else {
                err(nodep->fileline(), "Generic '" + formalp->name()
                                           + "' of the instantiated package has no value");
                continue;
            }
        }
        bindPackageGeneric(formalp, actualp, savedp);
    }
    // Every formal the map names is one of the package's generics (6.5.7.2)
    for (const AstVhdlAssoc* const asp : assocs) {
        const AstVhdlName* const namep
            = asp->formalp() ? VN_CAST(asp->formalp(), VhdlName) : nullptr;
        if (!namep) continue;
        bool known = false;
        for (const AstNode* p = pkgp->genericsp(); p && !known; p = p->nextp()) {
            known = VN_AS(p, VhdlIfaceDecl)->name() == namep->name();
        }
        if (!known) {
            err(namep->fileline(), "Package '" + pkgp->prettyName() + "' has no generic named '"
                                       + namep->prettyName() + "'");
        }
    }
    // What a previous analysis expanded here is output, not source: an
    // instantiation nested in an uninstantiated package is analyzed once as
    // the template and again in every copy of it, and the copies would
    // otherwise pile up one on top of another
    if (AstNode* const stalep = nodep->declsp()) {
        VL_DO_DANGLING(stalep->unlinkFrBackWithNext()->deleteTree(), stalep);
    }
    // A generic whose value is not static became a constant of its own, and
    // it comes first: the package's own declarations may name it
    for (AstNodeVhdl* const declp : m_pendingPkgDecls) nodep->addDeclsp(declp);
    m_pendingPkgDecls.clear();
    // The declarations, and the body's too, so that a subprogram declared in
    // the package has the body the copy needs
    for (AstNode* p = pkgp->declsp(); p; p = p->nextp()) {
        AstNodeVhdl* const copyp = VN_AS(p->cloneTree(false), NodeVhdl);
        recordTemplateSyms(p, copyp);
        VhdlUnanalyzeVisitor{copyp};
        nodep->addDeclsp(copyp);
    }
    AstNode* firstBodyp = nullptr;
    // A nested package has no body of its own; the key here names the outer
    // one, whose body is not what this copy takes
    const auto bodyIt = nested ? m_packageBodies.end() : m_packageBodies.find(key);
    // A package whose declarations a body completes is instantiated only
    // once that body is analyzed (4.9): the copy needs it
    if (!nested && bodyIt == m_packageBodies.end() && !m_inSpecialized) {
        bool needsBody = false;
        for (const AstNode* p = pkgp->declsp(); p && !needsBody; p = p->nextp()) {
            if (const AstVhdlSubprogDecl* const subp = VN_CAST(p, VhdlSubprogDecl)) {
                needsBody = !subp->hasBody() && !subp->isInstance();
            } else if (const AstVhdlObjectDecl* const odp = VN_CAST(p, VhdlObjectDecl)) {
                needsBody = odp->kind() == VhdlObjKind::CONSTANT && !odp->defaultp();
            }
        }
        if (needsBody) {
            err(nodep->fileline(), "Package '" + pkgp->prettyName()
                                       + "' is completed by a body, which is analyzed before"
                                         " the package is instantiated, and there is none");
        }
    }
    if (bodyIt != m_packageBodies.end()) {
        // The body has a context clause of its own -- IEEE's fixed_generic_pkg
        // body uses MATH_REAL where its declaration does not
        const auto bodyScopeIt = m_unitScopes.find(key + "%body");
        if (bodyScopeIt != m_unitScopes.end() && bodyScopeIt->second->parentp()) {
            const VhdlScope* const ctxp = bodyScopeIt->second->parentp();
            for (VhdlScope* const usedp : ctxp->uses()) intop->addUse(usedp);
            for (const auto& pair : ctxp->named()) intop->addNamed(pair.first, pair.second);
        }
        for (AstNode* p = bodyIt->second->declsp(); p; p = p->nextp()) {
            AstNodeVhdl* const copyp = VN_AS(p->cloneTree(false), NodeVhdl);
            recordTemplateSyms(p, copyp);
            VhdlUnanalyzeVisitor{copyp};
            if (!firstBodyp) firstBodyp = copyp;
            nodep->addDeclsp(copyp);
        }
    }
    // The copy of the body is still a package body: a constant there gives a
    // deferred one its value rather than declaring a second (6.4.2.2)
    std::vector<AstNode*> declOrder;
    for (AstNode* p = nodep->declsp(); p; p = p->nextp()) declOrder.push_back(p);
    bool inBody = false;
    for (AstNode* const p : declOrder) {
        if (p == firstBodyp) inBody = true;
        VL_RESTORER(m_inPackageBody);
        VL_RESTORER(m_inPackageInstance);
        m_inPackageBody = m_inPackageBody || inBody;
        m_inPackageInstance = true;
        // A copy made later for one call site is analyzed away from here, and
        // it is still the instantiated package's own source
        p->foreachAndNext(
            [](AstVhdlSubprogDecl* subp) { subp->inPkgInstance(true); });
        iterate(p);
    }
    m_scopep = savedp;
}

// Which subprogram each call of the template resolved to, recorded against
// the copy's own node. The two trees are clones of each other, so walking
// them together pairs the nodes up. Substituting the generics may leave two
// declarations of a name with the same profile, and then only the template
// knows which one a call inside the package meant (4.9).
void VhdlSemVisitor::recordTemplateSyms(AstNode* origp, AstNode* copyp) {
    while (origp && copyp) {
        if (const AstNodeVhdlExpr* const ep = VN_CAST(origp, NodeVhdlExpr)) {
            const VhdlSymbol* const symp = ep->vhdlSymp();
            if (symp
                && (symp->kind() == VhdlSymKind::FUNCTION
                    || symp->kind() == VhdlSymKind::PROCEDURE)) {
                m_templateSyms.emplace(copyp, symp);
            }
        }
        recordTemplateSyms(origp->op1p(), copyp->op1p());
        recordTemplateSyms(origp->op2p(), copyp->op2p());
        recordTemplateSyms(origp->op3p(), copyp->op3p());
        recordTemplateSyms(origp->op4p(), copyp->op4p());
        origp = origp->nextp();
        copyp = copyp->nextp();
    }
}

// "function add1 is new adder generic map (1);" (4.4). The instance is a copy
// of the uninstantiated subprogram with its generics bound, analyzed here.
void VhdlSemVisitor::visit(AstVhdlSubprogInst* nodep) {
    std::vector<std::string> parts;
    if (!flatten(nodep->uninstantiatedp(), parts) || parts.empty()) {
        err(nodep->fileline(), "A subprogram instantiation needs the name of a subprogram");
        return;
    }
    std::vector<VhdlSymbol*> found;
    lookup(parts.back(), found);
    AstVhdlSubprogDecl* templp = nullptr;
    for (VhdlSymbol* const symp : found) {
        // The body may be in a package body the instantiation cannot see,
        // which is where analysis keeps it for copying
        AstVhdlSubprogDecl* declp = symp->resolved()->bodyp();
        if (!declp) declp = VN_CAST(symp->resolved()->declp(), VhdlSubprogDecl);
        if (declp && declp->genericCount() > 0 && declp->hasBody()) {
            templp = declp;
            break;
        }
    }
    if (!templp) {
        const auto range = m_genericBodies.equal_range(parts.back());
        if (range.first != range.second) {
            templp = range.first->second;
            if (std::next(range.first) != range.second) {
                err(nodep->fileline(), "More than one generic subprogram '" + parts.back()
                                           + "' could be instantiated here");
            }
        }
    }
    if (!templp) {
        err(nodep->fileline(),
            "Cannot find a generic subprogram '" + parts.back() + "' with a body to instantiate");
        return;
    }
    AstVhdlSubprogDecl* const copyp = templp->cloneTree(false);
    VhdlUnanalyzeVisitor{copyp};
    copyp->templateName(copyp->name());
    copyp->name(nodep->name());
    copyp->isInstance(true);
    // The actual for each generic, by name where the map names its formal
    std::vector<AstVhdlAssoc*> assocs;
    for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
        assocs.push_back(VN_AS(p, VhdlAssoc));
    }
    size_t position = 0;
    for (AstNode* p = copyp->paramsp();
         p && position < static_cast<size_t>(copyp->genericCount());
         p = p->nextp(), ++position) {
        AstVhdlIfaceDecl* const formalp = VN_AS(p, VhdlIfaceDecl);
        AstNodeVhdlExpr* actualp = nullptr;
        for (size_t i = 0; i < assocs.size(); ++i) {
            const AstVhdlName* const namep
                = assocs[i]->formalp() ? VN_CAST(assocs[i]->formalp(), VhdlName) : nullptr;
            if (namep) {
                if (namep->name() == formalp->name()) actualp = assocs[i]->actualp();
            } else if (i == position) {
                actualp = assocs[i]->actualp();
            }
        }
        if (!actualp && formalp->profilep() && formalp->profilep()->defaultsToVisible()) {
            // Left to whatever is visible: the formal binds to nothing, and
            // its own default is dropped so no later copy binds it either
            if (formalp->defaultp()) formalp->defaultp()->unlinkFrBack()->deleteTree();
            continue;
        }
        if (actualp) {
            if (formalp->defaultp()) formalp->defaultp()->unlinkFrBack()->deleteTree();
            formalp->defaultp(actualp->cloneTree(false));
        } else if (!formalp->defaultp()) {
            err(nodep->fileline(),
                "Generic '" + formalp->name() + "' of the instantiated subprogram has no value");
        }
    }
    nodep->copyp(copyp);
    iterate(copyp);
}

// One generic of an instantiated package: a value, a type, or a subprogram
void VhdlSemVisitor::bindPackageGeneric(AstVhdlIfaceDecl* formalp, AstNodeVhdlExpr* actualp,
                                        VhdlScope* outerp) {
    FileLine* const flp = formalp->fileline();
    VhdlScope* const innerp = m_scopep;
    if (formalp->kind() == VhdlIfaceKind::TYPE) {
        m_scopep = outerp;
        // The actual is a subtype indication, not merely a type mark:
        // "t_element => std_logic_vector(7 downto 0)" says how long the
        // elements of every object of it are (6.5.7.2)
        VhdlType* const typep = resolveSubtypeOf(actualp, nullptr, nullptr, nullptr, flp);
        m_scopep = innerp;
        // A generic type comes with "=" and "/=" (6.5.3), which a file type
        // and a protected type do not have to give it
        if (typep && !typep->isError()
            && (typep->base()->kind() == VhdlTypeKind::FILE
                || typep->base()->kind() == VhdlTypeKind::PROTECTED)
            && !m_inSpecialized) {
            err(actualp->fileline(),
                "Generic type '" + formalp->name() + "' has the predefined \"=\" and \"/=\", and "
                    + typep->prettyName() + " is a "
                    + (typep->base()->kind() == VhdlTypeKind::FILE ? "file" : "protected")
                    + " type, which has neither");
        }
        VhdlSymbol* const symp
            = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, formalp->name(), formalp);
        symp->typep(typep);
        declare(symp, flp);
        // What the copy's own declarations name when they say this name
        m_pkgGenTypeNames.insert(formalp->name());
        // The actual type's own operators are already visible wherever the
        // type is; declaring them again here would make every use of them
        // ambiguous inside the copy
        return;
    }
    if (formalp->kind() == VhdlIfaceKind::SUBPROGRAM
        || formalp->kind() == VhdlIfaceKind::PACKAGE) {
        m_scopep = outerp;
        std::vector<std::string> parts;
        std::vector<VhdlSymbol*> found;
        // An operator is named by its symbol -- "op => \"+\"" -- which reads
        // as a string literal until it is known to be a designator (4.5.3)
        if (const AstVhdlLiteral* const litp = VN_CAST(actualp, VhdlLiteral)) {
            if (litp->kind() == VhdlLitKind::STRING) parts.push_back(lower(litp->text()));
        }
        if (!parts.empty()) {
            lookup(parts.back(), found);
        } else if (flatten(actualp, parts) && !parts.empty()) {
            // The actual may be a selected name: "work.helpers.same"
            VhdlScope* scopep = outerp;
            bool viaScope = parts.size() > 1;
            for (size_t i = 0; i + 1 < parts.size() && viaScope; ++i) {
                VhdlScope* const nextp = scopep ? scopep->findNamed(parts[i]) : nullptr;
                if (!nextp) {
                    viaScope = false;
                } else {
                    scopep = nextp;
                }
            }
            if (viaScope && scopep) {
                scopep->lookupLocal(parts.back(), found);
            } else {
                lookup(parts.back(), found);
            }
        }
        m_scopep = innerp;
        for (VhdlSymbol* const targetp : found) {
            VhdlSymbol* const symp
                = VhdlSymTab::newSymbol(targetp->kind(), formalp->name(), formalp);
            symp->aliasp(targetp);
            symp->typep(targetp->typep());
            symp->returnTypep(targetp->returnTypep());
            for (size_t i = 0; i < targetp->paramTypes().size(); ++i) {
                symp->addParam(targetp->paramName(i), targetp->paramTypes()[i],
                               targetp->paramHasDefault(i));
            }
            symp->bodyp(targetp->bodyp());
            symp->innerp(targetp->innerp());
            symp->predefined(targetp->predefined());
            declare(symp, flp);
            // A formal generic package is a name of its own inside the copy,
            // so "p.thing" and "use p.all" reach what it was mapped to
            if (formalp->kind() == VhdlIfaceKind::PACKAGE && targetp->innerp()) {
                innerp->addNamed(lower(formalp->name()), targetp->innerp());
            }
        }
        // A package may be named by its library -- "fixed_pkg => IEEE.fixed_pkg"
        // -- with no use clause bringing it into view, so the design unit
        // itself is what to look for (4.7)
        if (found.empty() && formalp->kind() == VhdlIfaceKind::PACKAGE && parts.size() > 1) {
            std::string lib = parts[parts.size() - 2];
            if (lib == "work") lib = m_libname;
            const auto it = m_unitScopes.find(lib + "." + parts.back());
            if (it != m_unitScopes.end()) {
                VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::PACKAGE,
                                                               formalp->name(), formalp);
                symp->innerp(it->second);
                declare(symp, flp);
                innerp->addNamed(lower(formalp->name()), it->second);
                return;
            }
        }
        if (found.empty()) {
            err(flp, "Cannot find what generic '" + formalp->name() + "' is mapped to");
        }
        return;
    }
    // A value: its subtype is named in the package, its value given here
    VhdlType* const typep = formalp->subtypep() ? resolveSubtype(formalp->subtypep()) : nullptr;
    m_scopep = outerp;
    analyzeExpr(actualp, typep);
    const VhdlValue val = V3VhdlEval::eval(actualp);
    m_scopep = innerp;
    VhdlSymbol* const symp
        = VhdlSymTab::newSymbol(VhdlSymKind::CONSTANT, formalp->name(), formalp);
    symp->typep(typep);
    if (val.known()) {
        symp->value(val);
    } else {
        // A value that names a generic of the enclosing entity is fixed once
        // the instance is elaborated but is not a number now. The copy's
        // declarations are emitted into the module that instantiates the
        // package, so the generic becomes an ordinary constant of that module
        // whose value is the expression written here, and the back end folds
        // it per instance (4.9). A generic *subprogram* instantiation has no
        // declarations of its own to put such a constant in, so there the
        // value must still be one that folds.
        if (!m_inPkgInst) {
            err(flp, "The value of generic '" + formalp->name()
                         + "' of an instantiated package must be static");
            declare(symp, flp);
            return;
        }
        // The symbol built above is simply not declared; the symbol table
        // owns it and frees it with the rest.
        //
        // The constant is given a name of its own, because the actual is very
        // often the generic of the same name -- "generic map (item_w =>
        // item_w)" -- and a declaration that names itself in its own value is
        // a circle. An alias then gives it the name the package wrote.
        const std::string hiddenName = "__vpkgg_" + formalp->name();
        AstVhdlObjectDecl* const declp = new AstVhdlObjectDecl{
            formalp->fileline(), hiddenName, VhdlObjKind::CONSTANT};
        if (formalp->subtypep()) {
            declp->subtypep(formalp->subtypep()->cloneTree(false));
        }
        declp->defaultp(actualp->cloneTree(false));
        declp->elabConstant(true);
        VhdlUnanalyzeVisitor{static_cast<AstNode*>(declp)};
        AstVhdlAliasDecl* const aliasp = new AstVhdlAliasDecl{
            formalp->fileline(), formalp->name(),
            new AstVhdlName{formalp->fileline(), hiddenName}};
        m_pendingPkgDecls.push_back(declp);
        m_pendingPkgDecls.push_back(aliasp);
        m_scopep = innerp;
        return;
    }
    declare(symp, flp);
}

VhdlSymbol* VhdlSemVisitor::specializeSubprogram(AstVhdlSubprogDecl* originalp,
                                                 VhdlScope* regionp,
                                                 const std::vector<VhdlType*>& actualTypes,
                                                 const std::string& name) {
    return specializeSubprogram(originalp, regionp, actualTypes, std::vector<VhdlValue>{}, name);
}

VhdlSymbol* VhdlSemVisitor::specializeSubprogram(AstVhdlSubprogDecl* originalp,
                                                 VhdlScope* regionp,
                                                 const std::vector<VhdlType*>& actualTypes,
                                                 const std::vector<VhdlValue>& actualValues,
                                                 const std::string& name) {
    AstVhdlSubprogDecl* const copyp = originalp->cloneTree(false);
    copyp->name(name);
    VhdlUnanalyzeVisitor{copyp};
    // Each parameter is declared with the subtype the call supplies. Setting
    // the resolved type on the indication is enough: resolveSubtype takes it
    // as already worked out, which is exactly what a written-out constraint
    // would have produced.
    size_t i = 0;
    // A generic subprogram's generics come ahead of its parameters and are
    // not what the call supplies (4.2.1)
    AstNode* firstp = copyp->paramsp();
    for (int g = 0; g < copyp->genericCount() && firstp; ++g) firstp = firstp->nextp();
    for (AstNode* p = firstp; p; p = p->nextp(), ++i) {
        AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        if (i >= actualTypes.size() || !actualTypes[i]) continue;
        if (!ifp->subtypep()) continue;
        ifp->subtypep()->vhdlTypep(actualTypes[i]);
    }
    // Analyze the copy in the region the original was declared in, so it sees
    // exactly the names the original did
    VhdlScope* const savedp = m_scopep;
    VhdlType* const savedReturnp = m_returnTypep;
    m_scopep = regionp;
    m_returnTypep = nullptr;
    m_specValues = actualValues;
    // A copy of a subprogram of an instantiated package is that package's own
    // source, wherever it is analyzed (4.7)
    VL_RESTORER(m_inPackageInstance);
    if (originalp->inPkgInstance()) {
        copyp->inPkgInstance(true);
        m_inPackageInstance = true;
    }
    {
        // What the call supplies makes the body's bounds fold, but they are
        // not static where the design wrote them: a slice or an index out of
        // range in here is a condition to be met while running, not an error
        // in the source (8.4, 8.5)
        VL_RESTORER(m_inSpecialized);
        m_inSpecialized = true;
        // The copy reads as the revision it was written in, which a call
        // from a VHDL-93 unit into IEEE's 2008 sources does not share
        const bool savedHide = VhdlScope::hide2008();
        VhdlScope::hide2008(copyp->fileline()->language() < V3LangCode::L1076_2008);
        iterate(copyp);
        VhdlScope::hide2008(savedHide);
    }
    m_specValues.clear();
    m_scopep = savedp;
    m_returnTypep = savedReturnp;
    // Now that the body has been analyzed with the lengths this call site
    // supplies, the result has one too -- so the copy is an ordinary
    // subprogram, and nothing tries to specialize it again
    VhdlSymbol* const newSymp = copyp->vhdlSymp();
    if (newSymp && newSymp->returnTypep()
        && newSymp->returnTypep()->base()->kind() == VhdlTypeKind::ARRAY
        && !newSymp->returnTypep()->lengthEmittable()) {
        // What the body returns may itself be a call whose own result
        // subtype only one call site settles, so that call gets its copy
        // before the length is read off it (4.2.1)
        if (m_specp) specializeWithin(copyp->stmtsp());
        VhdlType* foundp = nullptr;
        bool disagreep = false;
        bool sawOpen = false;
        bool sawUnfolded = false;
        findReturnedType(copyp->stmtsp(), foundp, disagreep, true, sawOpen, sawUnfolded);
        if (!foundp && !disagreep && !sawOpen) {
            findReturnedType(copyp->stmtsp(), foundp, disagreep, false, sawOpen, sawUnfolded);
        }
        if (!foundp && !disagreep && !returnsACall(copyp->stmtsp())) {
            // "return v and m" gives a result as long as its operands, so a
            // parameter of the same type says how long that is. What a *call*
            // returns is another matter: "return resize(v, v'length + n)" is
            // as long as the call makes it, which may be longer than any
            // parameter -- so the result carries its own bounds instead.
            for (VhdlType* const typep : newSymp->paramTypes()) {
                if (typep && typep->base() == newSymp->returnTypep()->base()
                    && typep->lengthEmittable()) {
                    foundp = typep;
                    break;
                }
            }
        }
        // One return path whose length only running measures is enough: the
        // result is as long as whichever path ran, so taking the length off
        // a path that happens to fold would be the wrong answer on the other
        if (foundp && !sawOpen) newSymp->returnTypep(foundp);
    }
    // The copy belongs beside the original, so lowering finds it. The
    // original is a template from here on: every call names a copy, and
    // emitting it would mean emitting a body whose lengths nothing settled.
    originalp->specialized(true);
    originalp->addNextHere(copyp);
    return newSymp;
}

// What a type conversion is written on, since the conversion is as long as
// that is and says no length of its own (9.3.6)
namespace {
AstNodeVhdlExpr* throughTypeConversions(AstNodeVhdlExpr* nodep) {
    for (int depth = 0; nodep && depth < 8; ++depth) {
        AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
        if (!callp || !callp->argsp() || callp->argsp()->nextp()) return nodep;
        const VhdlSymbol* const prefixp
            = callp->prefixp() ? callp->prefixp()->vhdlSymp() : nullptr;
        if (!prefixp || prefixp->resolved()->kind() != VhdlSymKind::TYPE) return nodep;
        AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
        AstNodeVhdlExpr* const nextp
            = asp ? asp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
        if (!nextp) return nodep;
        nodep = nextp;
    }
    return nodep;
}
}  // namespace

// Whether any return statement returns what a call gives, whose length is
// the call's business and not any parameter's
bool VhdlSemVisitor::returnsACall(AstNode* stmtsp) {
    for (AstNode* p = stmtsp; p; p = p->nextp()) {
        if (AstVhdlReturn* const retp = VN_CAST(p, VhdlReturn)) {
            // A type conversion is exactly as long as what it is written on
            // (9.3.6), so "return unsigned(f(x))" returns what f gives back
            AstNodeVhdlExpr* const valuep = throughTypeConversions(retp->valuep());
            AstVhdlCall* const callp = VN_CAST(valuep, VhdlCall);
            const VhdlSymbol* namedp = callp ? callp->vhdlSymp() : nullptr;
            if (!namedp && callp && callp->prefixp()) namedp = callp->prefixp()->vhdlSymp();
            // A function that may be called with no arguments at all is
            // called by writing its name and nothing else (4.2.2.1), so
            // "return put_msg" is a call however little it looks like one
            if (!namedp && valuep && !VN_IS(valuep, VhdlCall) && valuep->vhdlSymp()) {
                const VhdlSymbol* const barep = valuep->vhdlSymp()->resolved();
                if (barep->kind() == VhdlSymKind::FUNCTION && allParamsDefaulted(barep)) {
                    namedp = barep;
                }
            }
            // An indexed name or a slice is written the same way a call is;
            // neither is one
            if (namedp && namedp->resolved()->kind() != VhdlSymKind::TYPE) return true;
            // "return v & v" is as long as its operands *together*, which is
            // no parameter's length (9.2.5) -- so the result carries its own
            // bounds rather than borrowing one of them
            if (AstVhdlBinary* const binp = VN_CAST(valuep, VhdlBinary)) {
                if (binp->op() == VhdlBinaryOp::CONCAT) return true;
            }
            // An object the body returns whose own subtype says a length that
            // does not fold -- numeric_std's RESIZE returns a
            // "(NEW_SIZE-1 downto 0)" -- is as long as that says, whatever
            // any parameter is
            if (valuep && valuep->vhdlTypep()) {
                const VhdlType* const typep = valuep->vhdlTypep();
                if (typep->base()->kind() == VhdlTypeKind::ARRAY
                    && typep->indexTypes().size() == 1 && typep->indexTypes()[0]
                    && typep->indexTypes()[0]->boundsp()) {
                    return true;
                }
            }
        }
        if (p->op1p() && returnsACall(p->op1p())) return true;
        if (p->op2p() && returnsACall(p->op2p())) return true;
        if (p->op3p() && returnsACall(p->op3p())) return true;
        if (p->op4p() && returnsACall(p->op4p())) return true;
    }
    return false;
}

// The type of the first return statement that has a length of its own
void VhdlSemVisitor::findReturnedType(AstNode* stmtsp, VhdlType*& foundp, bool& disagreep,
                                      bool skipZero, bool& sawOpen, bool& sawUnfolded) {
    for (AstNode* p = stmtsp; p && !disagreep; p = p->nextp()) {
        if (AstVhdlReturn* const retp = VN_CAST(p, VhdlReturn)) {
            if (retp->valuep() && retp->valuep()->vhdlTypep()) {
                // A conversion keeps the length of what it converts (9.3.6),
                // and where that is a call whose copy was only just made,
                // the length arrived after the conversion was first read
                if (AstVhdlCall* const convp = VN_CAST(retp->valuep(), VhdlCall)) {
                    const VhdlSymbol* const symp
                        = convp->prefixp() ? convp->prefixp()->vhdlSymp() : nullptr;
                    const VhdlType* const hadp = convp->vhdlTypep();
                    if (symp && symp->kind() == VhdlSymKind::TYPE
                        && (!hadp || !hadp->lengthEmittable())) {
                        VhdlType* const madep = convertedType(
                            symp->typep(), VN_CAST(convp->argsp(), VhdlAssoc));
                        if (madep && madep->lengthEmittable()) convp->vhdlTypep(madep);
                    }
                }
                // The returned value may constrain itself, as an aggregate
                // written with its own range does (9.3.3.3)
                VhdlType* const typep = constrainFromValue(
                    retp->valuep()->vhdlTypep(), retp->valuep(), retp->fileline());
                const bool usable
                    = typep
                      && (typep->base()->kind() == VhdlTypeKind::ARRAY
                              ? typep->lengthEmittable()
                              : fullyConstrainedRecord(typep));
                // A return whose value has no length of its own: nothing but
                // the value itself says how long the result is
                if (!usable && typep && typep->base()->kind() == VhdlTypeKind::ARRAY) {
                    sawOpen = true;
                }
                const bool folds = usable && typep->base()->kind() == VhdlTypeKind::ARRAY
                                   && typep->indexTypes().size() == 1
                                   && typep->indexTypes()[0] && typep->indexTypes()[0]->bounded();
                // A path whose length is not a number here cannot be shown to
                // agree with one whose length is: "iv_t(1 to n)" for a
                // parameter n is a length the back end can emit and nothing
                // can compare with a constant. The result is as long as
                // whichever path ran, so it carries its bounds.
                if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY && !folds) {
                    sawUnfolded = true;
                    if (foundp) {
                        foundp = nullptr;
                        disagreep = true;
                        return;
                    }
                }
                if (folds) {
                    // A guard returning a null value ("if arg'length < 1 then
                    // return NSLV") says nothing about how long the result is
                    // on the paths that have one
                    if (skipZero && typep->dimLength(0) == 0) continue;
                    if (sawUnfolded) {
                        foundp = nullptr;
                        disagreep = true;
                        return;
                    }
                    // Two returns of different lengths settle nothing: the
                    // length is whichever path ran, so the result carries it
                    if (foundp && foundp->base()->kind() == VhdlTypeKind::ARRAY
                        && foundp->indexTypes().size() == 1 && foundp->indexTypes()[0]
                        && foundp->indexTypes()[0]->bounded()
                        && foundp->dimLength(0) != typep->dimLength(0)) {
                        foundp = nullptr;
                        disagreep = true;
                        return;
                    }
                }
                if (usable && !foundp) foundp = typep;
            }
            continue;
        }
        if (AstVhdlIf* const ifp = VN_CAST(p, VhdlIf)) {
            for (AstNode* ap = ifp->altsp(); ap && !disagreep; ap = ap->nextp()) {
                findReturnedType(VN_AS(ap, VhdlAlternative)->stmtsp(), foundp, disagreep, skipZero,
                                 sawOpen, sawUnfolded);
            }
            continue;
        }
        if (AstVhdlCase* const casep = VN_CAST(p, VhdlCase)) {
            for (AstNode* ap = casep->altsp(); ap && !disagreep; ap = ap->nextp()) {
                findReturnedType(VN_AS(ap, VhdlAlternative)->stmtsp(), foundp, disagreep, skipZero,
                                 sawOpen, sawUnfolded);
            }
            continue;
        }
        if (AstVhdlLoop* const loopp = VN_CAST(p, VhdlLoop)) {
            findReturnedType(loopp->stmtsp(), foundp, disagreep, skipZero, sawOpen, sawUnfolded);
        }
    }
}

//######################################################################
// Specialization of unconstrained subprograms
//
// VHDL writes "function reverse (v : std_logic_vector) return
// std_logic_vector" once, and every call gives the parameter a length of its
// own. An event-driven simulator carries that length on the value, in a
// runtime descriptor read on every operation -- which is a large part of why
// those simulators are slow, and exactly what a compiler must not do.
//
// So each distinct set of actual lengths gets its own copy of the subprogram,
// written as if the parameters had been declared with those lengths, and
// analyzed from scratch. Everything downstream then sees an ordinary
// subprogram whose every bound is known: 'LENGTH folds, ranges fold, and the
// body emits the same code a hand-written fixed-width version would.
//
// This is what the plan calls tier one, and it is where all of numeric_std
// lands: those functions are written against unconstrained operands, but
// every call site has a static width.

bool vhdlBindActuals(const VhdlSymbol* symp, const std::vector<AstVhdlAssoc*>& assocs,
                     std::vector<int>& formalOf,
                     std::vector<std::string>* elemOf = nullptr) VL_MT_DISABLED;
// The subtype an actual must have: the parameter's own, or, where the formal
// part names one element of it, that element's (6.5.7.1)
VhdlType* vhdlFormalTypeAt(const VhdlSymbol* symp, int idx,
                           const std::string& elem) VL_MT_DISABLED;

class VhdlSpecVisitor final : public VNVisitor {
    // STATE
    VhdlSemVisitor& m_sem;
    // Subprogram and actual types -> the copy made for them
    std::map<std::string, VhdlSymbol*> m_specializations;
    std::set<const VhdlSymbol*> m_madeSyms;  // Copies, which are never copied again
    // A subprogram a copy was made from, and the actuals it was made for
    using VhdlSpecFrame = std::pair<const VhdlSymbol*, std::vector<VhdlType*>>;
    std::map<const VhdlSymbol*, VhdlSpecFrame> m_copyOf;  // What each copy was made for
    std::vector<VhdlSpecFrame> m_within;  // The copies being made or walked
    int m_count = 0;
    bool m_madeOne = false;

    VL_UNCOPYABLE(VhdlSpecVisitor);

public:
    explicit VhdlSpecVisitor(VhdlSemVisitor& sem)
        : m_sem{sem} {}
    ~VhdlSpecVisitor() override = default;

    // Returns true if it made a copy, so the caller can look at the copies
    bool pass(const std::vector<AstNodeVhdlUnit*>& unitsp) {
        m_madeOne = false;
        for (AstNodeVhdlUnit* const unitp : unitsp) iterate(unitp);
        return m_madeOne;
    }
    // The same over one subtree, without disturbing whether the round that
    // is running has made a copy
    void passOver(AstNode* nodep) { iterateAndNextNull(nodep); }
    bool madeOne() const { return m_madeOne; }

private:
    void visit(AstNode* nodep) override { iterateChildren(nodep); }
    // A copy's body is walked again on every later round, so what a call is
    // written inside is read from the body rather than from how it was reached
    void visit(AstVhdlSubprogDecl* nodep) override {
        const VhdlSymbol* const symp = nodep->vhdlSymp();
        const auto it = symp ? m_copyOf.find(symp->resolved()) : m_copyOf.end();
        if (it == m_copyOf.end()) {
            iterateChildren(nodep);
            return;
        }
        m_within.push_back(it->second);
        iterateChildren(nodep);
        m_within.pop_back();
    }
    void visit(AstVhdlCall* nodep) override {
        iterateChildren(nodep);
        trySpecialize(nodep, nodep->vhdlSymp(), nodep->argsp());
    }
    // A function that may be called with no arguments at all is called by
    // writing its name and nothing else (4.2.2.1). Such a call has no
    // argument list to specialize on, but its result may still be something
    // only a copy of the body can settle the bounds of.
    void visit(AstVhdlName* nodep) override {
        iterateChildren(nodep);
        VhdlSymbol* const symp = nodep->vhdlSymp();
        if (!symp) return;
        const VhdlSymbol* const barep = symp->resolved();
        if (barep->kind() != VhdlSymKind::FUNCTION) return;
        if (!VhdlSemVisitor::allParamsDefaulted(barep)) return;
        // The name of the function a call is written on is not itself a call
        if (const AstVhdlCall* const callp = VN_CAST(nodep->backp(), VhdlCall)) {
            if (callp->prefixp() == nodep) return;
        }
        trySpecialize(nodep, symp, nullptr);
    }
    // An operator a design declares is an ordinary subprogram whose name
    // happens to be an operator symbol (4.5.2), so it is copied per use in
    // the same way. std_logic_unsigned's "*" is one: its result is as long
    // as its operands together, which only the use says.
    void visit(AstVhdlBinary* nodep) override {
        iterateChildren(nodep);
        trySpecializeOperator(nodep, nodep->vhdlSymp(), {nodep->lhsp(), nodep->rhsp()});
    }
    void visit(AstVhdlUnary* nodep) override {
        iterateChildren(nodep);
        trySpecializeOperator(nodep, nodep->vhdlSymp(), {nodep->lhsp()});
    }
    void trySpecializeOperator(AstNodeVhdlExpr* nodep, VhdlSymbol* symp,
                               std::vector<AstNodeVhdlExpr*> args) {
        if (!symp) return;
        symp = symp->resolved();
        if (symp->predefined()) return;  // Emitted as a native operation
        if (!needsSpecializing(symp)) return;
        if (m_madeSyms.count(symp)) return;
        if (symp->paramTypes().size() != args.size()) return;
        for (AstNodeVhdlExpr* const argp : args) {
            if (!argp) return;
        }
        specializeWithActuals(nodep, symp, args, {});
    }

    // STRING is carried dynamically, so a parameter or result of it needs no
    // length decided here and is no reason to make a copy
    static bool isStringType(const VhdlType* typep) {
        const VhdlType* const strp = V3VhdlStd::stringType();
        return typep && strp && typep->base() == strp->base();
    }

    // Whether a subprogram has a parameter or result whose length is not
    // known until it is called
    static bool needsSpecializing(const VhdlSymbol* symp) {
        if (!symp) return false;
        if (symp->kind() != VhdlSymKind::FUNCTION && symp->kind() != VhdlSymKind::PROCEDURE) {
            return false;
        }
        if (symp->predefined() || !symp->bodyp()) return false;
        // A subprogram the frontend emits as a native operation is never
        // compiled, so making copies of it is wasted work
        if (V3VhdlStd::isIntrinsic(symp)) return false;
        for (const VhdlType* const typep : symp->paramTypes()) {
            // A bound that names a generic is a length: the back end folds it
            // per instance, so one copy of the subprogram serves every call
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && !typep->lengthEmittable()) {
                return true;
            }
            // A record may have an element of an unconstrained array type,
            // and then the call is what says how long that element is (5.3.3)
            if (typep && typep->base()->kind() == VhdlTypeKind::RECORD
                && !VhdlSemVisitor::fullyConstrainedRecord(typep)) {
                return true;
            }
        }
        return resultIsUnconstrained(symp) || bodyNeedsParamValues(symp);
    }

    // A function whose result has no length of its own gets that length from
    // what its body builds, which may depend on what the call passes -- so
    // the copy is made for the values as well as for the types
    static bool resultIsUnconstrained(const VhdlSymbol* symp) {
        const VhdlType* const retp = symp->returnTypep();
        if (!retp) return false;
        // A record result may have an element of an unconstrained array
        // type, and then what the body builds is what says how long it is
        if (retp->base()->kind() == VhdlTypeKind::RECORD) {
            return !VhdlSemVisitor::fullyConstrainedRecord(retp);
        }
        return retp->base()->kind() == VhdlTypeKind::ARRAY && !retp->lengthEmittable();
    }

    // A subprogram whose body declares something as long as one of its own
    // parameters -- "variable r : bit_vector(1 to n)", or a type declared
    // inside it whose element is that long -- needs a copy per value, since
    // by the time the declaration is emitted the length has to be a number.
    static bool bodyNeedsParamValues(const VhdlSymbol* symp) {
        AstVhdlSubprogDecl* const declp = symp->bodyp();
        if (!declp) return false;
        std::set<const VhdlSymbol*> params;
        AstNode* p = declp->paramsp();
        for (int g = 0; p && g < declp->genericCount(); ++g) p = p->nextp();
        for (; p; p = p->nextp()) {
            const AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            // Only a value read into the subprogram is the same throughout
            // it: one the body may write is not (4.2.2.1). A parameter
            // written without a class is a constant where its mode is "in".
            if (ifp->kind() != VhdlIfaceKind::SIGNAL && ifp->kind() != VhdlIfaceKind::FILE
                && ifp->mode() == VhdlMode::IN && ifp->vhdlSymp()) {
                params.insert(ifp->vhdlSymp());
            }
        }
        if (params.empty()) return false;
        bool found = false;
        declp->foreach([&](AstVhdlSubtypeInd* sip) {
            if (found) return;
            sip->foreach([&](AstNodeVhdlExpr* refp) {
                if (refp->vhdlSymp() && params.count(refp->vhdlSymp())) found = true;
            });
        });
        return found;
    }

    // Whether the length of an actual is one the copy can carry: a number, or
    // an expression written in terms the region the copy is analyzed in can
    // see. A bound naming a generic folds once the instance is elaborated; one
    // naming a parameter of the subprogram the call is written in means
    // nothing outside that subprogram.
    static bool lengthTravels(const VhdlType* typep, int depth = 0) {
        if (!typep || depth > 8) return false;
        if (typep->base()->kind() != VhdlTypeKind::ARRAY) return true;
        if (!typep->lengthEmittable()) return false;
        bool ok = true;
        for (const VhdlType* const idxp : typep->indexTypes()) {
            if (!idxp) return false;
            if (idxp->bounded()) continue;
            AstNode* const boundsp = idxp->boundsp();
            if (!boundsp) return false;
            boundsp->foreach([&](AstNodeVhdlExpr* refp) {
                const VhdlSymbol* const refSymp = refp->vhdlSymp();
                if (!refSymp) return;
                // A bound naming something the calling subprogram declared --
                // its own parameter, or a constant of its own worked out from
                // one -- says nothing where the copy is analyzed: the copy
                // cannot see the caller's declarations
                switch (refSymp->resolved()->kind()) {
                case VhdlSymKind::PARAMETER:
                case VhdlSymKind::VARIABLE: ok = false; break;
                case VhdlSymKind::CONSTANT:
                    if (!refSymp->resolved()->hasValue()) ok = false;
                    break;
                default: break;
                }
            });
        }
        const VhdlType* const elemp
            = typep->elementp() ? typep->elementp() : typep->base()->elementp();
        if (ok && elemp && elemp != typep && elemp->base()->kind() == VhdlTypeKind::ARRAY) {
            ok = lengthTravels(elemp, depth + 1);
        }
        return ok;
    }

    // The default written for one parameter of a subprogram, or null
    static AstNodeVhdlExpr* paramDefaultOf(const VhdlSymbol* symp, size_t which) {
        const AstVhdlSubprogDecl* declp = symp->bodyp();
        if (!declp) declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
        if (!declp) return nullptr;
        AstNode* p = declp->paramsp();
        // A generic subprogram's generics come ahead of its parameters (4.2.1)
        for (int g = 0; g < declp->genericCount() && p; ++g) p = p->nextp();
        for (size_t i = 0; p; p = p->nextp(), ++i) {
            if (i == which) {
                AstVhdlIfaceDecl* const ifp = VN_CAST(p, VhdlIfaceDecl);
                return ifp ? ifp->defaultp() : nullptr;
            }
        }
        return nullptr;
    }

    // What a copy of a subprogram actually depends on about an actual's
    // subtype. For an array that is its element type and its bounds and
    // nothing else: two subtypes that agree on those are interchangeable
    // inside the body, and keying on the subtype object instead would make a
    // separate copy for every place a length is written -- which for a
    // verification framework is thousands of copies of the same code.
    // Anything else keeps its own identity: two scalar subtypes of one type
    // differ in their bounds, and a body may read those.
    static std::string specKeyOf(const VhdlType* typep, int depth = 0) {
        if (!typep) return "0";
        const std::string ownKey = std::to_string(reinterpret_cast<uintptr_t>(typep));
        const VhdlType* const basep = typep->base();
        // A record subtype constrains its elements, and one written inside a
        // subprogram is a fresh object every time that subprogram is copied.
        // Keyed on the object, a recursive call inside a copy never finds the
        // copy it is already in and asks for another, for ever -- so what a
        // record subtype is worth here is its base and what it constrains.
        if (basep->kind() == VhdlTypeKind::RECORD) {
            if (depth > 4) return ownKey;
            std::string out = "R" + std::to_string(reinterpret_cast<uintptr_t>(basep));
            for (const VhdlSymbol* const elemp : typep->elements()) {
                if (!elemp) return ownKey;
                out += ":" + specKeyOf(elemp->typep(), depth + 1);
            }
            return out;
        }
        if (basep->kind() != VhdlTypeKind::ARRAY) return ownKey;
        const std::vector<VhdlType*>& indexes = typep->indexTypes();
        if (indexes.empty()) return ownKey;
        std::string out = "A" + std::to_string(reinterpret_cast<uintptr_t>(basep)) + ":"
                          + std::to_string(reinterpret_cast<uintptr_t>(typep->elementp()));
        for (const VhdlType* const idxp : indexes) {
            // A bound that does not fold here -- one that names a generic --
            // is carried as it is written, so it keeps the subtype's identity
            if (!idxp || !idxp->bounded()) return ownKey;
            out += "[" + std::to_string(idxp->left()) + (idxp->ascending() ? "to" : "dt")
                   + std::to_string(idxp->right()) + "]";
        }
        // An array of records is constrained by its element as well as by its
        // own bounds, and that element is a fresh object per copy too
        if (typep->elementp() && typep->elementp()->base()
            && typep->elementp()->base()->kind() == VhdlTypeKind::RECORD && depth <= 4) {
            out += "{" + specKeyOf(typep->elementp(), depth + 1) + "}";
        }
        return out;
    }

    void trySpecialize(AstNodeVhdlExpr* nodep, VhdlSymbol* symp, AstNode* argsp) {
        if (!needsSpecializing(symp)) return;
        // A copy is never itself specialized: it was made for one call site
        // and has nothing left to fix
        if (m_madeSyms.count(symp)) return;
        // The actual types, which must all be known for the copy to be made.
        // Which formal each actual names, since a named association may put
        // them in any order and a formal with a default may be left out
        // altogether (6.5.7.1)
        std::vector<AstVhdlAssoc*> assocs;
        for (AstNode* p = argsp; p; p = p->nextp()) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            if (!asp || !asp->actualp()) return;
            assocs.push_back(asp);
        }
        const size_t nparams = symp->paramTypes().size();
        std::vector<int> formalOf;
        std::vector<std::string> elemOf;
        if (!vhdlBindActuals(symp, assocs, formalOf, &elemOf)) return;
        std::vector<AstVhdlAssoc*> byFormal(nparams, nullptr);
        // A formal whose elements the call names one at a time has no actual
        // of its own; its subtype is the one its elements' actuals make up
        std::map<size_t, std::vector<AstNodeVhdlExpr*>> individual;
        std::map<size_t, std::vector<int64_t>> individualIdx;
        for (size_t k = 0; k < assocs.size(); ++k) {
            const int f = formalOf[k];
            if (f < 0 || static_cast<size_t>(f) >= nparams) return;
            if (!elemOf[k].empty()) {
                VhdlType* const recTypep = symp->paramTypes()[f];
                if (!recTypep) return;
                // An array formal takes its bounds from the indices the call
                // names, so it is those that are collected rather than the
                // actuals in element order
                if (vhdlIsIndexPart(elemOf[k])) {
                    individualIdx[f].push_back(vhdlIndexPartAt(elemOf[k]));
                    continue;
                }
                auto it = individual.find(f);
                if (it == individual.end()) {
                    it = individual
                             .emplace(f, std::vector<AstNodeVhdlExpr*>(
                                             recTypep->elements().size(), nullptr))
                             .first;
                }
                for (size_t e = 0; e < recTypep->elements().size(); ++e) {
                    if (recTypep->elements()[e]->name() == elemOf[k]) {
                        it->second[e] = assocs[k]->actualp();
                    }
                }
                continue;
            }
            byFormal[f] = assocs[k];
        }
        std::vector<AstNodeVhdlExpr*> byFormalExpr(nparams, nullptr);
        for (size_t i = 0; i < nparams; ++i) {
            if (byFormal[i]) byFormalExpr[i] = byFormal[i]->actualp();
        }
        specializeWithActuals(nodep, symp, byFormalExpr, individual, individualIdx);
    }

    // Whether a copy for these actuals would be one of a terminating series.
    // A call a copy makes to the subprogram it is a copy of is written in
    // terms of the copy's own formals, so the actuals it passes on say
    // whether the recursion is getting anywhere: every length no longer than
    // the copy's own means there is a last copy, and a longer one means
    // there is not -- "p(prefix & \"  \")" asks for a copy per depth, and
    // the depth is a value, which no copy can settle.
    // Whether a declaration stands inside a protected type's own region
    static bool inProtectedType(const AstNode* nodep) {
        for (const AstNode* p = nodep; p; p = p->backp()) {
            if (VN_IS(p, VhdlProtectedType)) return true;
        }
        return false;
    }

    // Whether an expression names a generic of the module it is written in
    static bool namesGeneric(const AstNode* nodep, int depth = 0) {
        if (!nodep || depth > 32) return false;
        for (const AstNode* p = nodep; p; p = p->nextp()) {
            if (const AstNodeVhdlExpr* const exprp = VN_CAST(p, NodeVhdlExpr)) {
                if (const VhdlSymbol* const symp = exprp->vhdlSymp()) {
                    if (symp->resolved()->kind() == VhdlSymKind::GENERIC) return true;
                }
            }
            if (namesGeneric(p->op1p(), depth + 1)) return true;
            if (namesGeneric(p->op2p(), depth + 1)) return true;
            if (namesGeneric(p->op3p(), depth + 1)) return true;
            if (namesGeneric(p->op4p(), depth + 1)) return true;
        }
        return false;
    }

    // Whether any of these types says how long it is by naming a generic
    static bool boundsNameGeneric(const std::vector<VhdlType*>& types) {
        for (const VhdlType* const typep : types) {
            if (!typep) continue;
            for (const VhdlType* const idxp : typep->indexTypes()) {
                if (idxp && !idxp->bounded() && namesGeneric(idxp->boundsp())) return true;
            }
        }
        return false;
    }

    bool descends(const VhdlSymbol* symp,
                  const std::vector<VhdlType*>& actualTypes) const {
        for (const VhdlSpecFrame& frame : m_within) {
            if (frame.first != symp) continue;
            if (frame.second.size() != actualTypes.size()) continue;
            for (size_t i = 0; i < actualTypes.size(); ++i) {
                const VhdlType* const newp = actualTypes[i];
                const VhdlType* const oldp = frame.second[i];
                if (!newp || !oldp) continue;
                if (newp->base()->kind() != VhdlTypeKind::ARRAY) continue;
                if (oldp->base()->kind() != VhdlTypeKind::ARRAY) continue;
                if (newp->dimLength(0) > oldp->dimLength(0)) return false;
            }
        }
        return true;
    }

    // The copy itself, given what each formal is associated with. An operator
    // the design declared is a subprogram like any other (4.5.2), and its
    // operands are its actuals, so it comes here too.
    void specializeWithActuals(
        AstNodeVhdlExpr* nodep, VhdlSymbol* symp,
        const std::vector<AstNodeVhdlExpr*>& byFormalExpr,
        const std::map<size_t, std::vector<AstNodeVhdlExpr*>>& individual,
        const std::map<size_t, std::vector<int64_t>>& individualIdx = {}) {
        const size_t nparams = symp->paramTypes().size();
        const bool byValue = resultIsUnconstrained(symp) || bodyNeedsParamValues(symp);
        std::vector<VhdlType*> actualTypes;
        std::vector<VhdlValue> actualValues;
        for (size_t i = 0; i < nparams; ++i) {
            const auto idxIt = individualIdx.find(i);
            if (idxIt != individualIdx.end()) {
                // The indices the call named are the bounds the formal has:
                // "p(v(0) => s)" hands a parameter of exactly one element
                VhdlType* const madep = m_sem.constrainArrayFromIndices(
                    symp->paramTypes()[i], idxIt->second, nodep->fileline());
                if (!madep) return;
                if (byValue) actualValues.push_back(VhdlValue{});
                actualTypes.push_back(madep);
                continue;
            }
            const auto indIt = individual.find(i);
            if (indIt != individual.end()) {
                VhdlType* const madep = m_sem.constrainRecordFromParts(
                    symp->paramTypes()[i], indIt->second, nodep->fileline());
                // Nothing is gained by a copy that still does not know how
                // long the elements are
                if (!VhdlSemVisitor::fullyConstrainedRecord(madep)) return;
                if (byValue) actualValues.push_back(VhdlValue{});
                actualTypes.push_back(madep);
                continue;
            }
            AstNodeVhdlExpr* const actp = byFormalExpr[i];
            if (!actp) {
                // A formal the call leaves out takes its default, and the
                // copy is made for that default's own subtype and value
                AstNodeVhdlExpr* const defp = paramDefaultOf(symp, i);
                if (!defp) return;
                if (byValue) {
                    const VhdlValue val = V3VhdlEval::eval(defp);
                    actualValues.push_back(val.isInt() ? val : VhdlValue{});
                }
                VhdlType* const dtypep
                    = defp->vhdlTypep() ? defp->vhdlTypep() : symp->paramTypes()[i];
                actualTypes.push_back(dtypep);
                continue;
            }
            if (byValue) {
                const VhdlValue val = V3VhdlEval::eval(actp);
                actualValues.push_back(val.isInt() ? val : VhdlValue{});
            }
            VhdlType* typep = actp->vhdlTypep();
            // Only a length that is known is worth specializing on
            // A STRING the call cannot measure -- the result of a function,
            // say -- is carried dynamically rather than by length, so it is
            // no reason to leave every other parameter of the call unsettled
            const bool looseString
                = isStringType(symp->paramTypes()[i])
                  && (!typep || (!typep->isConstrainedArray() && !lengthTravels(typep)));
            if (looseString) typep = symp->paramTypes()[i];
            if (!looseString && symp->paramTypes()[i]
                && symp->paramTypes()[i]->base()->kind() == VhdlTypeKind::ARRAY) {
                // A bound that names a generic is a length even though it
                // is not a number here, and the copy carries it as written --
                // but one that names a parameter of the subprogram the call
                // is written in means nothing where the copy is analyzed
                if (!typep || (!typep->isConstrainedArray() && !lengthTravels(typep))) return;
                // A length of two billion is an index subtype's range, not an
                // object's; specializing on it would be meaningless and would
                // make the copy's own bounds checks fire
                const int64_t length = typep->dimLength(0);
                if (length > (1 << 20)) return;
                // Zero is a length: a null array has no elements at all (5.3.2.1).
                // What this guard is for is a length that was never constrained
                // -- but a bound that names a generic is a length even though
                // it is not a number here, and the copy carries it as written.
                if (length < 0 && !lengthTravels(typep)) return;
            }
            // The same for a record with an element of an unconstrained array
            // type: only an actual that says how long its elements are is
            // worth making a copy for (5.3.3)
            if (symp->paramTypes()[i]
                && symp->paramTypes()[i]->base()->kind() == VhdlTypeKind::RECORD
                && !VhdlSemVisitor::fullyConstrainedRecord(symp->paramTypes()[i])
                && !VhdlSemVisitor::fullyConstrainedRecord(typep)) {
                return;
            }
            actualTypes.push_back(typep);
        }
        if (actualTypes.size() != symp->paramTypes().size()) return;
        // Nothing to specialize on: no parameter has a length to fix, and no
        // value the call passes is known, so the copy would be the original
        if (byValue) {
            // A result whose subtype only the body settles is itself something
            // to specialize on, and the only thing a function with no
            // parameters has: the copy is what reads the result's bounds off
            // what the body returns, so without it "return s" out of a
            // "string(50 to 55)" arrives at the caller renumbered from 1
            bool worthIt = resultIsUnconstrained(symp);
            for (const VhdlValue& val : actualValues) {
                if (val.known()) worthIt = true;
            }
            for (const VhdlType* const typep : symp->paramTypes()) {
                if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                    && !typep->isConstrainedArray()) {
                    worthIt = true;
                }
            }
            if (!worthIt) return;
        }

        // Which subprogram, not merely which name: a package and a protected
        // type may each declare one of the same name and profile, and they
        // are different subprograms with different bodies (5.6.2)
        std::string key = std::to_string(reinterpret_cast<uintptr_t>(symp));
        for (const VhdlType* const typep : actualTypes) key += "|" + specKeyOf(typep);
        for (const VhdlValue& val : actualValues) {
            key += val.isInt() ? ("=" + std::to_string(val.toInt())) : "=?";
        }
        const auto it = m_specializations.find(key);
        if (it != m_specializations.end()) {
            // The copy for this key is the one whose body is being analyzed
            // right now, so this is the call it makes to itself. It names the
            // original until the copy is finished, and the round that follows
            // binds it to the copy (4.2.1).
            if (!it->second) return;
            nodep->vhdlSymp(it->second);
            takeResultType(nodep, it->second);
            return;
        }
        AstVhdlSubprogDecl* const bodyp = symp->bodyp();
        VhdlScope* const regionp = symp->innerp() ? symp->innerp()->parentp() : nullptr;
        if (!bodyp || !regionp) return;
        // A subprogram that calls itself with a longer argument has no last
        // copy, since each copy asks for one longer still. Such a call keeps
        // the original, whose parameter carries its bounds with the value.
        if (!descends(symp, actualTypes)) return;
        // A method of a protected type belongs to one class, and the class
        // is shared however many instances a design elaborates. A copy made
        // for an actual whose bounds name an instance's generics -- which is
        // what an unconstrained port's bounds are -- would name them from
        // outside the module they belong to, and outlive them. Such a call
        // keeps the original, whose parameter carries its bounds along with
        // the value.
        if (inProtectedType(bodyp) && boundsNameGeneric(actualTypes)) return;
        // A copy per call site is bounded by the source; a copy per *value*
        // could not be, if a recursive call kept folding to a new one
        if (m_count > 4096) return;
        const std::string name = symp->name() + "__V" + std::to_string(++m_count);
        // The copy's own body is analyzed before it is made, so the frame is
        // in place from here: a call it makes to what it is a copy of is the
        // recursive one, and it is that call the descent test has to see
        m_within.emplace_back(symp, actualTypes);
        // Claimed before the copy's body is analyzed, since analyzing it
        // reaches the calls the body makes -- and one of those may be to the
        // subprogram being copied, which is this same copy and not another
        m_specializations.emplace(key, nullptr);
        VhdlSymbol* const newp
            = m_sem.specializeSubprogram(bodyp, regionp, actualTypes, actualValues, name);
        if (newp) {
            m_specializations[key] = newp;
        } else {
            // Nothing was made, so nothing is claimed: another call with the
            // same key may still succeed, and one that found the claim would
            // have been told to wait for a copy that never arrives
            m_specializations.erase(key);
        }
        if (newp) {
            m_copyOf.emplace(newp, m_within.back());
            m_madeSyms.insert(newp);
            nodep->vhdlSymp(newp);
            takeResultType(nodep, newp);
            m_madeOne = true;
            // The copy's formals have lengths the original's had none of, so
            // a call written inside it can be specialized where it could not
            // be before. Its own declarations and statements, not what
            // follows it: the copy was put beside the original, so its
            // siblings are the rest of the package.
            if (AstVhdlSubprogDecl* const copyDeclp = newp->bodyp()) {
                passOver(copyDeclp->declsp());
                passOver(copyDeclp->stmtsp());
            }
        }
        m_within.pop_back();
    }

    // The copy's result has a length where the original's had none, so the
    // call has one too -- which is what lets a constant declared with the
    // unconstrained type take its bounds from what the call returns (6.4.2.2)
    static void takeResultType(AstNodeVhdlExpr* nodep, const VhdlSymbol* symp) {
        if (!symp || symp->kind() != VhdlSymKind::FUNCTION) return;
        VhdlType* const retp = symp->returnTypep();
        if (!retp) return;
        const VhdlType* const hadp = nodep->vhdlTypep();
        // The copy's own body settles no length -- two returns of different
        // lengths, say -- so what the call gives back carries its bounds,
        // whatever the declaration had suggested
        if (!retp->lengthEmittable()) {
            if (hadp && hadp->base() == retp->base()) nodep->vhdlTypep(retp);
            return;
        }
        // The copy's body settled a length, and that is what the call gives
        // back. Where the call already carried one it was inferred before
        // this copy existed -- an operator whose declared result is
        // unconstrained is given an operand's length to be going on with --
        // and an inference does not outrank the body it was standing in for.
        if (hadp && hadp->lengthEmittable() && !retp->isConstrainedArray()) return;
        nodep->vhdlTypep(retp);
    }
};

void VhdlSemVisitor::specializeWithin(AstNode* nodep) {
    if (m_specp) m_specp->passOver(nodep);
}

//######################################################################
// Drivers (6.5.2, 14.7.2)

// How many places drive each signal, and what resolves them where there is
// more than one. A driver belongs to the longest static prefix of the target
// name, so assigning one element of an array here and another there is not
// two drivers of the array.
namespace {
const VhdlSymbol* driverRootSym(const AstNode* nodep) {
    while (nodep) {
        if (const AstNodeVhdlExpr* const vp = VN_CAST(nodep, NodeVhdlExpr)) {
            if (vp->vhdlSymp()) return vp->vhdlSymp();
        }
        if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
            nodep = callp->prefixp();
        } else if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            nodep = selp->prefixp();
        } else {
            return nullptr;
        }
    }
    return nullptr;
}
// The signal an actual names, through whatever conversions are written on it.
// "f(s)" as an actual is an association with s itself (6.5.7.1), so a
// conversion written on the formal drives s and not a signal of its own.
const AstNodeVhdlExpr* actualThroughConversions(const AstNodeVhdlExpr* nodep) {
    for (int depth = 0; nodep && depth < 8; ++depth) {
        const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
        if (!callp || !callp->argsp() || callp->argsp()->nextp()) return nodep;
        // A call whose prefix names a type or a function is a conversion; one
        // whose prefix names an object is an index or a slice (8.1)
        const VhdlSymbol* const prefixp
            = callp->prefixp() ? callp->prefixp()->vhdlSymp() : nullptr;
        const VhdlSymbol* const namedp = prefixp ? prefixp->resolved() : nullptr;
        if (!namedp
            || (namedp->kind() != VhdlSymKind::TYPE
                && namedp->kind() != VhdlSymKind::FUNCTION)) {
            return nodep;
        }
        const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
        nodep = asp ? asp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
    }
    return nodep;
}

// The port a conversion on a formal is applied to, as in "f(p) => a" (6.5.7.1)
const VhdlSymbol* convertedFormalPort(const AstNodeVhdlExpr* formalp) {
    const AstVhdlCall* const callp = VN_CAST(formalp, VhdlCall);
    if (!callp || !callp->argsp() || callp->argsp()->nextp()) return nullptr;
    const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
    const AstNodeVhdlExpr* const argp
        = asp ? asp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
    const AstVhdlName* const namep = VN_CAST(argp, VhdlName);
    const VhdlSymbol* const symp = namep ? namep->vhdlSymp() : nullptr;
    return symp && symp->kind() == VhdlSymKind::PORT ? symp : nullptr;
}

// Whether a conversion written on this formal makes the association a driver
// of the actual: the port's driving value goes out through the conversion,
// which is a source of the actual like any other (6.5.7.1, 14.7.2)
bool convertedFormalDrives(const AstNodeVhdlExpr* formalp) {
    const VhdlSymbol* const portp = convertedFormalPort(formalp);
    const AstVhdlIfaceDecl* const declp
        = portp ? VN_CAST(portp->declp(), VhdlIfaceDecl) : nullptr;
    if (!declp) return false;
    return declp->mode() == VhdlMode::OUT || declp->mode() == VhdlMode::INOUT
           || declp->mode() == VhdlMode::BUFFER;
}

void collectDrivenSyms(AstNode* nodep, std::set<const VhdlSymbol*>& whole,
                       std::set<const VhdlSymbol*>& partial, bool follow) {
    if (!nodep) return;
    if (const AstVhdlAssoc* const asp = VN_CAST(nodep, VhdlAssoc)) {
        // An instance's out port drives the signal it is associated with
        if (asp->drives() && asp->actualp()) {
            const AstNodeVhdlExpr* const namedp = actualThroughConversions(asp->actualp());
            if (const VhdlSymbol* const symp = driverRootSym(namedp)) {
                if (VN_IS(namedp, VhdlName)) {
                    whole.insert(symp);
                } else {
                    partial.insert(symp);
                }
            }
        }
    }
    if (const AstVhdlSigAssign* const asgp = VN_CAST(nodep, VhdlSigAssign)) {
        // A force overrides every driver rather than being one of them, and
        // a release hands the signal back to the drivers it already had
        // (10.5.2.1), so neither adds a driver
        if (asgp->force() || asgp->release()) {
            collectDrivenSyms(nodep->op1p(), whole, partial, true);
            collectDrivenSyms(nodep->op2p(), whole, partial, true);
            collectDrivenSyms(nodep->op3p(), whole, partial, true);
            collectDrivenSyms(nodep->op4p(), whole, partial, true);
            if (follow) collectDrivenSyms(nodep->nextp(), whole, partial, true);
            return;
        }
        if (const VhdlSymbol* const symp = driverRootSym(asgp->targetp())) {
            if (VN_IS(asgp->targetp(), VhdlName)) {
                whole.insert(symp);
            } else {
                partial.insert(symp);
            }
        }
    }
    collectDrivenSyms(nodep->op1p(), whole, partial, true);
    collectDrivenSyms(nodep->op2p(), whole, partial, true);
    collectDrivenSyms(nodep->op3p(), whole, partial, true);
    collectDrivenSyms(nodep->op4p(), whole, partial, true);
    if (follow) collectDrivenSyms(nodep->nextp(), whole, partial, true);
}
}  // namespace

// An enumeration of the nine logic values, or an array of them
bool VhdlSemVisitor::isLogicValued(const VhdlType* typep) {
    if (!typep) return false;
    const VhdlType* basep = typep->base();
    if (basep->kind() == VhdlTypeKind::ARRAY && basep->elementp()) {
        basep = basep->elementp()->base();
    }
    if (basep->kind() != VhdlTypeKind::ENUM) return false;
    const std::vector<VhdlSymbol*>& lits = basep->literals();
    if (lits.size() != 9) return false;
    static const char* const NINE = "UX01ZWLH-";
    for (size_t i = 0; i < lits.size(); ++i) {
        const std::string& name = lits[i]->name();
        if (name.size() != 3 || name[1] != NINE[i]) return false;
    }
    return true;
}

// Whether the drivers of such a signal are combined into one value without
// a resolution function of the design's own being run: the nine logic
// values resolve natively, and a composite of them resolves element by
// element, which for a value made of bits is the same thing (6.5.2)
bool VhdlSemVisitor::driversCombined(const VhdlType* typep, int depth) {
    if (!typep || depth > 8) return false;
    const VhdlType* const basep = typep->base();
    if (basep->kind() == VhdlTypeKind::RECORD) {
        if (basep->elements().empty()) return false;
        for (const VhdlSymbol* const elemp : basep->elements()) {
            if (!driversCombined(elemp->typep(), depth + 1)) return false;
        }
        return true;
    }
    if (!isLogicValued(typep)) return false;
    // An unresolved logic type -- STD_ULOGIC -- combines nothing
    if (typep->resolutionp()) return true;
    const VhdlType* const elemp
        = typep->elementp() ? typep->elementp() : basep->elementp();
    return elemp && elemp->resolutionp();
}

// A procedure with a signal parameter of mode out or inout drives the
// signal the call passes, and the driver belongs to the process that made
// the call (4.2.2.3, 14.7.2): two processes calling the same procedure
// with the same signal are two drivers of it.
void VhdlSemVisitor::collectCallDrivers(AstNode* stmtp, std::set<const VhdlSymbol*>& whole,
                                        std::set<const VhdlSymbol*>& partial) {
    stmtp->foreach([&](AstVhdlProcCall* pcallp) {
        const AstVhdlCall* const callp = VN_CAST(pcallp->callp(), VhdlCall);
        VhdlSymbol* const namedp
            = callp ? callp->vhdlSymp()
                    : (pcallp->callp() ? pcallp->callp()->vhdlSymp() : nullptr);
        VhdlSymbol* const csymp = namedp ? namedp->resolved() : nullptr;
        AstVhdlSubprogDecl* const declp
            = csymp ? (csymp->bodyp() ? csymp->bodyp()
                                      : VN_CAST(csymp->declp(), VhdlSubprogDecl))
                    : nullptr;
        if (!declp || !callp) return;
        // The formals, past the generics a generic subprogram keeps ahead
        // of them (4.2.1)
        std::vector<const AstVhdlIfaceDecl*> formals;
        AstNode* fp = declp->paramsp();
        for (int i = 0; i < declp->genericCount() && fp; ++i) fp = fp->nextp();
        for (; fp; fp = fp->nextp()) formals.push_back(VN_CAST(fp, VhdlIfaceDecl));
        size_t pos = 0;
        for (AstNode* ap = callp->argsp(); ap; ap = ap->nextp(), ++pos) {
            const AstVhdlAssoc* const asp = VN_CAST(ap, VhdlAssoc);
            if (!asp || !asp->actualp()) continue;
            const AstVhdlIfaceDecl* formalp = nullptr;
            if (const AstVhdlName* const fnamep = VN_CAST(asp->formalp(), VhdlName)) {
                for (const AstVhdlIfaceDecl* const one : formals) {
                    if (one && one->name() == fnamep->name()) formalp = one;
                }
            } else if (!asp->formalp() && pos < formals.size()) {
                formalp = formals[pos];
            }
            if (!formalp || formalp->kind() != VhdlIfaceKind::SIGNAL) continue;
            if (formalp->mode() == VhdlMode::IN) continue;
            const VhdlSymbol* const symp = driverRootSym(asp->actualp());
            if (!symp) continue;
            // Only where the drivers are combined at all: a signal whose
            // resolution is a function of the design's own is left as it
            // was, since one driver per call site is not what it expects
            if (!driversCombined(symp->typep())) continue;
            if (VN_IS(asp->actualp(), VhdlName)) {
                whole.insert(symp);
            } else {
                partial.insert(symp);
            }
        }
    });
}

void VhdlSemVisitor::analyzeDrivers(AstVhdlArchitecture* archp) {
    std::map<const VhdlSymbol*, int> counts;
    std::map<const VhdlSymbol*, FileLine*> where;
    std::set<const VhdlSymbol*> partial;
    // A signal every one of whose drivers is inside a generate statement may
    // well have one driver once the design is elaborated: which branches are
    // taken is not known until the generics have values (11.8)
    std::set<const VhdlSymbol*> outsideGenerate;
    // A block statement is not a driver of its own: the statements inside it
    // are, and so are those of a block inside that one (11.2)
    std::function<void(AstNode*)> walk = [&](AstNode* stmtsp) {
        for (AstNode* p = stmtsp; p; p = p->nextp()) {
            if (AstVhdlBlock* const blockp = VN_CAST(p, VhdlBlock)) {
                // A block is not a driver of its own, but a conversion
                // written on one of its ports is: what the port drives
                // leaves through the conversion and arrives at the actual as
                // a source of it, which a plain association -- where the
                // port and the actual are one signal -- never is (6.5.7.1)
                AstVhdlBlockHeader* const headerp = blockp->headerp();
                for (AstNode* mp = headerp ? headerp->portMapp() : nullptr; mp;
                     mp = mp->nextp()) {
                    AstVhdlAssoc* const asp = VN_CAST(mp, VhdlAssoc);
                    if (!asp || !asp->actualp() || !asp->formalp()) continue;
                    if (!convertedFormalDrives(asp->formalp())) continue;
                    asp->drives(true);
                    const AstNodeVhdlExpr* const namedp
                        = actualThroughConversions(asp->actualp());
                    const VhdlSymbol* const symp = driverRootSym(namedp);
                    if (!symp) continue;
                    if (!VN_IS(namedp, VhdlName)) {
                        partial.insert(symp);
                        continue;
                    }
                    ++counts[symp];
                    outsideGenerate.insert(symp);
                    if (!where.count(symp)) where[symp] = asp->fileline();
                }
                walk(blockp->stmtsp());
                continue;
            }
            std::set<const VhdlSymbol*> whole;
            collectDrivenSyms(p, whole, partial, false);
            collectCallDrivers(p, whole, partial);
            const AstVhdlGenerate* const genp = VN_CAST(p, VhdlGenerate);
            const bool inGenerate = genp;
            // Every iteration of a for generate elaborates the statements of
            // its body again, and each copy is a driver of its own (11.8)
            int repeat = 1;
            if (genp && genp->kind() == VhdlLoopKind::FOR) {
                const VhdlSymbol* const gsymp = genp->vhdlSymp();
                const VhdlType* const rangep = gsymp ? gsymp->typep() : nullptr;
                if (rangep && rangep->bounded()) {
                    const int64_t len = rangep->ascending() ? rangep->right() - rangep->left() + 1
                                                            : rangep->left() - rangep->right() + 1;
                    repeat = len <= 0 ? 0 : static_cast<int>(len);
                }
            }
            if (!repeat) continue;
            for (const VhdlSymbol* const symp : whole) {
                counts[symp] += repeat;
                if (!inGenerate) outsideGenerate.insert(symp);
                if (!where.count(symp)) where[symp] = p->fileline();
            }
        }
    };
    walk(archp->stmtsp());
    for (const auto& pair : counts) {
        VhdlSymbol* const symp = const_cast<VhdlSymbol*>(pair.first);
        if (partial.count(symp)) continue;  // Its elements are driven, not it
        symp->driverCount(pair.second);
        VhdlType* const typep = symp->typep();
        VhdlSymbol* const resolvep = typep ? typep->resolutionp() : nullptr;
        // A resolution function is run however many drivers there are, since
        // with one it may still change the value it is given (6.5.2). Only
        // the *absence* of one is an error, and only for several drivers.
        if (pair.second < 2 && !resolvep) continue;
        if (!resolvep && !outsideGenerate.count(symp)) {
            // Every driver is inside a generate statement, so how many there
            // are is settled by elaboration rather than here. They are left
            // as ordinary assignments; if two really do elaborate, the back
            // end says so where it can see it.
            symp->driverCount(1);
            continue;
        }
        if (!resolvep && driversCombined(typep)) {
            // A composite of logic values: each element resolves as the
            // logic values do, and the drivers of a composite are resolved
            // element by element (6.5.2), so the signal needs no resolution
            // function of its own
            continue;
        }
        if (!resolvep) {
            // The one place the language says this must be an error, and the
            // only way a design can be told its drivers will not be combined
            err(where[symp], "Signal '" + symp->name() + "' has "
                                 + std::to_string(pair.second)
                                 + " drivers, but its subtype names no resolution function");
            continue;
        }
        symp->resolverp(resolvep);
        // The nine-value logic types resolve natively, so their resolution
        // function is never run and must not be copied per driver count
        if (!isLogicValued(typep)) {
            m_needResolve.push_back(NeedResolve{symp, resolvep, pair.second, where[symp]});
        }
    }
}

// Give each resolution function the copy that takes as many values as the
// signal has drivers. Its parameter is an unconstrained array, so it needs
// the same specializing a call site would have given it -- and so this runs
// where that specializing runs, once analysis is done.
void VhdlSemVisitor::specializeResolvers() {
    // One copy per function and driver count, however many signals want it
    std::map<std::pair<const VhdlSymbol*, int>, VhdlSymbol*> made;
    for (const NeedResolve& need : m_needResolve) {
        VhdlSymbol* const resolvep = need.resolvep;
        if (resolvep->paramTypes().size() != 1) continue;
        VhdlType* const paramp = resolvep->paramTypes()[0];
        if (!paramp || paramp->base()->kind() != VhdlTypeKind::ARRAY) continue;
        if (paramp->isConstrainedArray()) continue;  // Already a fixed size
        const auto madeIt = made.find(std::make_pair(resolvep, need.count));
        if (madeIt != made.end()) {
            need.symp->resolverp(madeIt->second);
            continue;
        }
        AstVhdlSubprogDecl* const bodyp = resolvep->bodyp();
        VhdlScope* const regionp = resolvep->innerp() ? resolvep->innerp()->parentp() : nullptr;
        if (!bodyp || !regionp) continue;  // Predefined, as ieee's own is
        if (!bodyp->backp()) continue;  // Not in the tree, so nothing to copy beside
        // The drivers as an array starting where the parameter's own index
        // subtype starts: a function written against "array (positive range
        // <>)" is entitled to find its first driver at 1, and one written
        // against NATURAL at 0 (5.2.1)
        VhdlType* const argp = cloneSubtype(paramp->base(), need.flp);
        const std::vector<VhdlType*>& baseIdx = paramp->base()->indexTypes();
        VhdlType* const baseIdxp = baseIdx.empty() ? nullptr : baseIdx[0];
        const bool asc = !baseIdxp || baseIdxp->ascending();
        const int64_t left = (baseIdxp && baseIdxp->bounded()) ? baseIdxp->left() : 0;
        VhdlType* const idxp = VhdlSymTab::newType(
            baseIdxp ? baseIdxp->kind() : VhdlTypeKind::INTEGER, "", need.flp);
        idxp->base(baseIdxp ? baseIdxp->base() : V3VhdlStd::integerType()->base());
        idxp->setBounds(left, asc ? left + need.count - 1 : left - need.count + 1, asc);
        argp->clearIndexTypes();
        argp->addIndexType(idxp);
        argp->unconstrained(false);
        const std::string name
            = resolvep->name() + "__V" + std::to_string(need.count) + "drv";
        std::vector<VhdlType*> actualTypes{argp};
        if (VhdlSymbol* const newp
            = specializeSubprogram(bodyp, regionp, actualTypes, name)) {
            need.symp->resolverp(newp);
            made.emplace(std::make_pair(resolvep, need.count), newp);
        }
    }
}

// The entity and architecture copies made for generic types. They are design
// units like any other and have to reach lowering, but they did not exist
// when the units were put in dependency order.
static std::vector<AstNodeVhdlUnit*> s_specUnits;

void V3VhdlSem::addSpecializedUnit(AstNodeVhdlUnit* unitp) { s_specUnits.push_back(unitp); }

const std::vector<AstNodeVhdlUnit*>& V3VhdlSem::specializedUnits() { return s_specUnits; }

bool V3VhdlSem::analyze(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    s_specUnits.clear();
    V3VhdlStd::build();
    VhdlSemVisitor visitor;
    visitor.analyzeUnits(unitsp);
    VhdlScope::hide2008(false);
    visitor.checkBodiesGiven(unitsp);
    // Give every call to an unconstrained subprogram a copy with the lengths
    // it supplies. A copy may itself call one, so repeat until nothing new
    // appears; the bound is defensive, since a chain that never settles is a
    // recursion on data-dependent lengths, which has no static answer.
    VhdlSpecVisitor specializer{visitor};
    visitor.specializer(&specializer);
    for (int round = 0; round < 20; ++round) {
        if (!specializer.pass(unitsp)) break;
    }
    // A constant whose bounds came from a call now has them, and another
    // constant may take its own bounds from that one, so repeat
    for (int round = 0; round < 8; ++round) {
        if (!visitor.reconstrainDecls(unitsp)) break;
    }
    visitor.spreadElabConstants(unitsp);
    // A resolution function is called once per resolved signal rather than
    // from anywhere in the source, so its copy is made here rather than by
    // the pass above
    visitor.specializeResolvers();
    return !visitor.errored();
}

// Every subprogram declaration is completed by a body in the same
// declarative region, or for a package declaration in its package body
// (4.2.1, 4.8). A package whose body this analysis has not seen may be given
// one later, and a subprogram given the FOREIGN attribute is implemented
// outside VHDL, so neither is reported. Nor is a generic subprogram: its body
// is written against generic types that make it no homograph of the
// declaration, so the two are not paired up by that route.
// Whether a is written before b in one file
static bool writtenBefore(const AstNode* ap, const AstNode* bp) {
    const FileLine* const afl = ap->fileline();
    const FileLine* const bfl = bp->fileline();
    if (afl->filename() != bfl->filename()) return false;
    if (afl->lineno() != bfl->lineno()) return afl->lineno() < bfl->lineno();
    return afl->firstColumn() < bfl->firstColumn();
}

// The declarations of a region are elaborated in order (14.4.2), so one
// cannot call a function whose body comes later -- in the same region, or,
// for a package declaration, in its body -- nor make an object of a
// protected type whose body does. Only declarations of the unit itself are
// asked: a process or a subprogram is elaborated or called after them all.
void VhdlSemVisitor::checkElaborationOrder(AstNodeVhdlUnit* unitp) {
    if (m_inSpecialized) return;
    // Whether a unit is the body of this package
    const auto sameRegionBody = [unitp](const AstNodeVhdlUnit* otherp) {
        return otherp && VN_IS(otherp, VhdlPackageBody)
               && lower(otherp->name()) == lower(unitp->name());
    };
    // Whether a node is part of a declaration the unit itself elaborates
    const auto elaboratedWithUnit = [unitp](const AstNode* nodep) {
        bool inDecl = false;
        for (const AstNode* upp = parentOf(nodep); upp; upp = parentOf(upp)) {
            if (upp == unitp) return inDecl;
            if (VN_IS(upp, VhdlSubprogDecl) || VN_IS(upp, NodeVhdlStmt)
                || VN_IS(upp, VhdlComponentDecl) || VN_IS(upp, VhdlProtectedType)
                || VN_IS(upp, NodeVhdlUnit)) {
                return false;
            }
            if (VN_IS(upp, NodeVhdlDecl)) inDecl = true;
        }
        return false;
    };
    // A package body continues its package's declarative region (12.1)
    const auto sameRegion = [unitp](const AstNodeVhdlUnit* declUnitp) {
        if (declUnitp == unitp) return true;
        return VN_IS(unitp, VhdlPackageBody) && VN_IS(declUnitp, VhdlPackage)
               && lower(declUnitp->name()) == lower(unitp->name());
    };
    const auto bodyLater = [&](const AstNode* usep, const AstNode* declp, const AstNode* bodyp) {
        if (!declp || !bodyp || !sameRegion(unitOf(declp))) return false;
        const AstNodeVhdlUnit* const bodyUnitp = unitOf(bodyp);
        // Written in the package body, after all of the package declaration
        if (bodyUnitp != unitp) return VN_IS(unitp, VhdlPackage) && sameRegionBody(bodyUnitp);
        return !writtenBefore(bodyp, usep);
    };
    unitp->foreach([&](const AstNodeVhdlExpr* exprp) {
        if (!VN_IS(exprp, VhdlCall) && !VN_IS(exprp, VhdlName) && !VN_IS(exprp, VhdlBinary)
            && !VN_IS(exprp, VhdlUnary)) {
            return;
        }
        const VhdlSymbol* const symp = exprp->vhdlSymp() ? exprp->vhdlSymp()->resolved() : nullptr;
        // A deferred constant has its value only once its full declaration,
        // in the package body, is elaborated (4.8)
        if (symp && symp->kind() == VhdlSymKind::CONSTANT && symp->fullDeclp()
            && symp->fullDeclp() != symp->declp() && VN_IS(exprp, VhdlName)
            && bodyLater(exprp, symp->declp(), symp->fullDeclp()) && elaboratedWithUnit(exprp)) {
            // nvc 1.22.1, GHDL 7.0.0-dev and Questa 2025.2 all refuse this
            // one outright, unlike a call made too early
            err(exprp->fileline(), "Deferred constant '" + symp->name()
                                       + "' is read before its full declaration is elaborated");
            return;
        }
        if (!symp || symp->kind() != VhdlSymKind::FUNCTION || symp->predefined()) return;
        if (!symp->foreignName().empty()) return;
        if (!bodyLater(exprp, symp->declp(), symp->bodyp())) return;
        if (!elaboratedWithUnit(exprp)) return;
        exprp->v3warn(VHDLELABORDER, "Function '" << symp->name()
                                                  << "' is called before its body is elaborated:"
                                                     " the declarations of a region are"
                                                     " elaborated in order");
    });
    unitp->foreach([&](const AstVhdlObjectDecl* declp) {
        const VhdlSymbol* const symp = declp->vhdlSymp();
        const VhdlType* const typep = symp ? symp->typep() : nullptr;
        if (!typep || typep->isError() || typep->base()->kind() != VhdlTypeKind::PROTECTED) return;
        // The protected type's declaration, found through the type mark
        const AstVhdlSubtypeInd* const sip = declp->subtypep();
        const VhdlSymbol* const tsymp = sip && sip->typeMarkp() && sip->typeMarkp()->vhdlSymp()
                                            ? sip->typeMarkp()->vhdlSymp()->resolved()
                                            : nullptr;
        const AstNode* const tdeclp = tsymp ? tsymp->declp() : nullptr;
        if (!bodyLater(declp, tdeclp, typep->base()->bodyp())) return;
        if (!VN_IS(parentOf(declp), NodeVhdlUnit)) return;
        declp->v3warn(VHDLELABORDER, "An object of protected type "
                                         << typep->prettyName()
                                         << " is declared before the type's body is"
                                            " elaborated: '"
                                         << declp->prettyName() << "'");
    });
}

void VhdlSemVisitor::checkBodiesGiven(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    for (AstNodeVhdlUnit* const unitp : unitsp) checkOutReads(unitp);
    for (AstNodeVhdlUnit* const unitp : unitsp) checkPurity(unitp);
    for (AstNodeVhdlUnit* const unitp : unitsp) checkElaborationOrder(unitp);
    // An incomplete type is completed by a full declaration in the same
    // declarative part, or for a package in its body (5.4.2)
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        if (m_inSpecialized) break;
        unitp->foreach([&](const AstVhdlTypeDecl* declp) {
            if (declp->defp() || !declp->vhdlSymp() || !declp->vhdlSymp()->typep()) return;
            if (declp->vhdlSymp()->typep()->kind() != VhdlTypeKind::INCOMPLETE) return;
            err(declp->fileline(),
                "Incomplete type '" + declp->prettyName() + "' is given no full declaration");
        });
    }
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        unitp->foreach([&](const AstVhdlSubprogDecl* declp) {
            if (declp->hasBody() || declp->isInstance() || declp->inPkgInstance()
                || declp->genericCount() > 0) {
                return;
            }
            const VhdlSymbol* const symp = declp->vhdlSymp();
            if (!symp || symp->hasBody() || symp->isForeign()) return;
            for (const AstNode* upp = parentOf(declp); upp; upp = parentOf(upp)) {
                // A subprogram of an interface list is a generic, and one of
                // a protected type is completed by the protected type body
                if (VN_IS(upp, VhdlIfaceDecl) || VN_IS(upp, VhdlProtectedType)) return;
                if (const AstVhdlPackage* const pkgp = VN_CAST(upp, VhdlPackage)) {
                    if (!m_pkgsWithBody.count(pkgp)) return;
                    break;
                }
                if (VN_IS(upp, NodeVhdlUnit)) break;
            }
            err(declp->fileline(),
                std::string{declp->isFunction() ? "Function" : "Procedure"} + " '"
                    + declp->prettyName() + "' is declared but never given a body"
                    + (VN_IS(unitp, VhdlPackage) ? " in the package body" : " in this region"));
        });
    }
}

void V3VhdlSem::clear() {
    s_specUnits.clear();
    V3VhdlStd::clear();
    VhdlSymTab::clear();
}

//######################################################################
// Expressions (clauses 8, 9, 12.5)

// Whether a subtype indication was written with the name of a generic type
// this copy has bound. In the uninstantiated unit that name matches nothing
// else, so a parameter of it cannot be what a call means unless the argument
// is of the type the instantiation supplied for it (4.7).
bool VhdlSemVisitor::writtenWithGenericType(AstNode* subtypep) const {
    if ((m_boundGenTypes.empty() && m_pkgGenTypeNames.empty()) || !subtypep) return false;
    bool found = false;
    subtypep->foreachAndNext([&](AstNodeVhdlExpr* refp) {
        if (found) return;
        if (const AstVhdlName* const namep = VN_CAST(refp, VhdlName)) {
            if (m_boundGenTypes.count(namep->name())
                || m_pkgGenTypeNames.count(namep->name())) {
                found = true;
            }
        }
    });
    return found;
}

// Whether every candidate says the same thing, differing only in whether a
// parameter is written as a universal type or as the INTEGER or REAL it
// converts to. STANDARD declares both for every physical type.
bool VhdlSemVisitor::sameButForUniversal(const std::vector<VhdlSymbol*>& cands) {
    if (cands.size() < 2) return false;
    const VhdlSymbol* const firstp = cands[0];
    const VhdlType* const retp = firstp->returnTypep();
    if (!retp) return false;
    const auto alike = [](const VhdlType* ap, const VhdlType* bp) {
        if (!ap || !bp) return false;
        if (ap->base() == bp->base()) return true;
        const VhdlTypeKind ak = ap->kind();
        const VhdlTypeKind bk = bp->kind();
        const auto intish = [](VhdlTypeKind k) {
            return k == VhdlTypeKind::INTEGER || k == VhdlTypeKind::UNIVERSAL_INTEGER;
        };
        const auto realish = [](VhdlTypeKind k) {
            return k == VhdlTypeKind::FLOATING || k == VhdlTypeKind::UNIVERSAL_REAL;
        };
        return (intish(ak) && intish(bk)) || (realish(ak) && realish(bk));
    };
    for (const VhdlSymbol* const symp : cands) {
        if (!symp->returnTypep() || symp->returnTypep()->base() != retp->base()) return false;
        if (symp->paramTypes().size() != firstp->paramTypes().size()) return false;
        for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
            if (!alike(symp->paramTypes()[i], firstp->paramTypes()[i])) return false;
        }
    }
    return true;
}

bool VhdlSemVisitor::typesMatch(const VhdlType* fromp, const VhdlType* top) {
    if (!fromp || !top) return true;
    // A generic type nothing has bound yet is a type of its own: it matches
    // itself and nothing else, which is what keeps two subprograms that
    // differ only in it from being the same subprogram (6.5.5). Until an
    // instantiation says what it stands for, treating it as matching
    // anything makes every such pair a homograph.
    if (fromp->base()->isGenericFormal() || top->base()->isGenericFormal()) {
        return fromp->base() == top->base();
    }
    if (fromp->isError() || top->isError()) return true;
    if (fromp->base() == top->base()) return true;
    // An integer or real literal has an anonymous universal type, which is
    // implicitly converted to whatever the context needs (9.3.2)
    if (fromp->kind() == VhdlTypeKind::UNIVERSAL_INTEGER
        && (top->kind() == VhdlTypeKind::INTEGER
            || top->kind() == VhdlTypeKind::UNIVERSAL_INTEGER)) {
        return true;
    }
    if (fromp->kind() == VhdlTypeKind::UNIVERSAL_REAL
        && (top->kind() == VhdlTypeKind::FLOATING
            || top->kind() == VhdlTypeKind::UNIVERSAL_REAL)) {
        return true;
    }
    // The conversion goes one way only: a universal value becomes an INTEGER
    // or a REAL where the context asks for one, but an INTEGER never becomes
    // universal (9.3.6). That asymmetry is what makes "x = 0" mean the
    // INTEGER "=" rather than the universal one when x is an INTEGER.
    return false;
}

// Whether one region encloses another, so that a declaration in the inner
// one hides a homograph in the outer (12.3)
bool VhdlSemVisitor::scopeEncloses(const VhdlScope* outerp, const VhdlScope* innerp) {
    if (!outerp || !innerp || outerp == innerp) return false;
    for (const VhdlScope* p = innerp->parentp(); p; p = p->parentp()) {
        if (p == outerp) return true;
    }
    return false;
}

// The type of an attribute of a type mark, worked out without reporting
// anything: used where a name is being weighed rather than analyzed (16.2)
VhdlType* VhdlSemVisitor::attrTypeQuiet(AstVhdlAttrName* nodep) {
    if (!nodep->prefixp()) return nullptr;
    const std::string& attr = nodep->name();
    // These answer with a type of their own whatever they are asked about,
    // so the prefix need not be worked out at all
    if (attr == "length" || attr == "pos") return V3VhdlStd::integerType();
    if (attr == "ascending" || attr == "event" || attr == "active" || attr == "stable"
        || attr == "quiet" || attr == "driving") {
        return V3VhdlStd::booleanType();
    }
    if (attr == "transaction") return V3VhdlStd::bitType();
    if (attr == "image" || attr == "instance_name" || attr == "path_name"
        || attr == "simple_name") {
        return V3VhdlStd::stringType();
    }
    if (attr == "last_event" || attr == "last_active") return V3VhdlStd::timeType();
    // The rest answer about the prefix, so it has to name a type here
    VhdlSymbol* const symp = lookupTypeQuiet(nodep->prefixp());
    VhdlType* const prefixp = symp ? symp->typep() : nullptr;
    if (!prefixp) return nullptr;
    if (attr == "left" || attr == "right" || attr == "high" || attr == "low") {
        // Of an array these ask about one of its index ranges, not about the
        // array (16.2.4). Answering with the array offers "arr'high" to
        // overload resolution as a value of the array type, and every
        // operator then refuses it.
        if (prefixp->base()->kind() == VhdlTypeKind::ARRAY) {
            return dimensionOf(nodep, prefixp);
        }
        return prefixp;
    }
    if (attr == "base" || attr == "leftof" || attr == "rightof" || attr == "pred"
        || attr == "succ" || attr == "val" || attr == "value") {
        return prefixp;
    }
    return nullptr;
}

// An uninstantiated generic subprogram is not callable: only an instance of
// it is (4.4). It stays visible so that "is new" can find it.
void VhdlSemVisitor::dropUninstantiated(std::vector<VhdlSymbol*>& found) {
    found.erase(std::remove_if(found.begin(), found.end(),
                               [](const VhdlSymbol* symp) {
                                   const AstVhdlSubprogDecl* const declp
                                       = VN_CAST(symp->resolved()->declp(), VhdlSubprogDecl);
                                   return declp && declp->genericCount() > 0
                                          && !declp->isInstance();
                               }),
                found.end());
}

void VhdlSemVisitor::candidateTypes(AstNodeVhdlExpr* nodep, CandSet& out) {
    if (!nodep) return;
    if (nodep->vhdlTypep()) {
        out.types.push_back(nodep->vhdlTypep());
        return;
    }
    if (AstVhdlAttrName* const attrp = VN_CAST(nodep, VhdlAttrName)) {
        // Many attributes have one type whatever the context, and that is
        // often what settles an expression whose other half is overloaded --
        // "NOTE to t_alert_level'right", or "width <= s'length". Where the
        // attribute's type depends on a prefix that is not a type mark, the
        // ordinary path decides it instead.
        if (VhdlType* const typep = attrTypeQuiet(attrp)) {
            out.types.push_back(typep);
            return;
        }
    }
    if (const AstVhdlLiteral* const litp = VN_CAST(nodep, VhdlLiteral)) {
        switch (litp->kind()) {
        case VhdlLitKind::CHARACTER: {
            // A character literal may belong to several enumeration types
            std::vector<VhdlSymbol*> found;
            lookup(vhdlCharLitName(litp->text()), found);
            for (const VhdlSymbol* const symp : found) {
                if (symp->kind() == VhdlSymKind::ENUM_LITERAL) out.types.push_back(symp->typep());
            }
            return;
        }
        case VhdlLitKind::STRING:
            // A string literal is always an array, never a single element
            out.need = CandSet::Need::ARRAY;
            out.chars = distinctChars(litp->text());
            return;
        case VhdlLitKind::BITSTRING:
            out.need = CandSet::Need::ARRAY;
            // Whatever its base, a bit string is written in ones and zeros
            out.chars = "01";
            return;
        case VhdlLitKind::NULLLIT: out.need = CandSet::Need::ACCESS; return;
        case VhdlLitKind::INTEGER:
            // Of the anonymous universal type until the context names one
            // (9.3.2), so it must not be settled here: analyzing it with no
            // context would fix it as universal, and a named integer type of
            // its own width would never reach it
            out.types.push_back(V3VhdlStd::universalIntegerType());
            return;
        case VhdlLitKind::REAL:
            out.types.push_back(V3VhdlStd::universalRealType());
            return;
        default: break;
        }
    }
    if (VN_IS(nodep, VhdlAggregate)) {
        out.need = CandSet::Need::COMPOSITE;
        return;
    }
    if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
        // A name may denote several overloaded declarations, as ERROR denotes
        // both a SEVERITY_LEVEL and, inside STD_LOGIC_1164's body, an MVL9PLUS.
        // Committing to one here would report an ambiguity the surrounding
        // expression is about to resolve.
        std::vector<VhdlSymbol*> found;
        lookup(namep->isChar() ? vhdlCharLitName(namep->name()) : namep->name(), found);
        for (const VhdlSymbol* const symp : found) {
            // A subprogram whose parameters all have defaults may be called
            // by naming it, so its result is one of the readings (4.2.2.1)
            if (symp->isOverloadable() && !symp->paramTypes().empty()
                && !allParamsDefaulted(symp)) {
                continue;
            }
            if (symp->kind() == VhdlSymKind::TYPE) continue;
            VhdlType* const typep = symp->returnTypep() ? symp->returnTypep() : symp->typep();
            if (typep) out.types.push_back(typep);
        }
        std::sort(out.types.begin(), out.types.end());
        out.types.erase(std::unique(out.types.begin(), out.types.end()), out.types.end());
        if (!out.types.empty()) return;
    }
    // An operator or a call has as many possible types as it has applicable
    // overloads. Gathering them without committing is what lets the context
    // settle a whole expression at once rather than each operand separately.
    if (AstVhdlBinary* const binp = VN_CAST(nodep, VhdlBinary)) {
        CandSet lcands;
        CandSet rcands;
        candidateTypes(binp->lhsp(), lcands);
        candidateTypes(binp->rhsp(), rcands);
        std::vector<VhdlSymbol*> found;
        lookup(vhdlBinaryOpName(binp->op()), found);
        for (const VhdlSymbol* const symp : found) {
            if (symp->kind() != VhdlSymKind::FUNCTION) continue;
            if (symp->paramTypes().size() != 2) continue;
            if (!anyMatches(lcands, symp->paramTypes()[0])) continue;
            if (!anyMatches(rcands, symp->paramTypes()[1])) continue;
            if (symp->returnTypep()) out.types.push_back(symp->returnTypep());
        }
        std::sort(out.types.begin(), out.types.end());
        out.types.erase(std::unique(out.types.begin(), out.types.end()), out.types.end());
        if (!out.types.empty()) return;
    } else if (AstVhdlUnary* const unp = VN_CAST(nodep, VhdlUnary)) {
        CandSet ocands;
        candidateTypes(unp->lhsp(), ocands);
        std::vector<VhdlSymbol*> found;
        lookup(vhdlUnaryOpName(unp->op()), found);
        for (const VhdlSymbol* const symp : found) {
            if (symp->kind() != VhdlSymKind::FUNCTION) continue;
            if (symp->paramTypes().size() != 1) continue;
            if (!anyMatches(ocands, symp->paramTypes()[0])) continue;
            if (symp->returnTypep()) out.types.push_back(symp->returnTypep());
        }
        std::sort(out.types.begin(), out.types.end());
        out.types.erase(std::unique(out.types.begin(), out.types.end()), out.types.end());
        if (!out.types.empty()) return;
    } else if (AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
        // A call has as many possible types as it has applicable overloads,
        // and two functions may differ only in their result (12.5). Settling
        // it here would report an ambiguity the surrounding expression is
        // about to resolve -- "v := not f(3, 0)" picks f by way of "not".
        std::vector<std::string> parts;
        std::vector<VhdlSymbol*> found;
        if (flatten(callp->prefixp(), parts) && !parts.empty()) lookup(parts.back(), found);
        dropUninstantiated(found);
        // A name that also denotes a type or an object makes this a
        // conversion or an indexed name, which the ordinary path settles.
        // A procedure of the same name does not: it cannot stand in an
        // expression, so the functions are still the only readings (4.5.3).
        bool onlySubprograms = false;
        for (const VhdlSymbol* const symp : found) {
            const VhdlSymKind kind = symp->resolved()->kind();
            if (kind == VhdlSymKind::FUNCTION) {
                onlySubprograms = true;
            } else if (kind != VhdlSymKind::PROCEDURE) {
                onlySubprograms = false;
                break;
            }
        }
        if (onlySubprograms) {
            std::vector<CandSet> argCands;
            std::vector<AstVhdlAssoc*> assocs;
            bool usable = true;
            for (AstNode* p = callp->argsp(); p && usable; p = p->nextp()) {
                AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
                if (!asp || !asp->actualp()) {
                    usable = false;
                    break;
                }
                assocs.push_back(asp);
                argCands.emplace_back();
                candidateTypes(asp->actualp(), argCands.back());
            }
            if (usable) {
                std::vector<int> formalOf;
                std::vector<std::string> elemOf;
                for (VhdlSymbol* const symp : found) {
                    if (!bindActuals(symp, assocs, formalOf, &elemOf)) continue;
                    bool fits = true;
                    for (size_t i = 0; i < argCands.size() && fits; ++i) {
                        if (formalOf[i] < 0) continue;
                        if (!anyMatches(argCands[i],
                                        vhdlFormalTypeAt(symp, formalOf[i], elemOf[i]))) {
                            fits = false;
                        }
                    }
                    if (fits && symp->returnTypep()) out.types.push_back(symp->returnTypep());
                }
                std::sort(out.types.begin(), out.types.end());
                out.types.erase(std::unique(out.types.begin(), out.types.end()),
                                out.types.end());
                if (!out.types.empty()) return;
            }
        }
    }
    out.types.push_back(analyzeExpr(nodep, nullptr));
}

bool VhdlSemVisitor::anyMatches(const CandSet& cands, const VhdlType* wantp) {
    if (!cands.types.empty()) {
        for (const VhdlType* const typep : cands.types) {
            if (typesMatch(typep, wantp)) return true;
        }
        return false;
    }
    if (!wantp) return true;
    switch (cands.need) {
    case CandSet::Need::ARRAY:
        if (wantp->isError()) return true;
        if (wantp->kind() != VhdlTypeKind::ARRAY) return false;
        return elementHasChars(wantp, cands.chars);
    case CandSet::Need::ACCESS:
        return wantp->kind() == VhdlTypeKind::ACCESS || wantp->isError();
    case CandSet::Need::COMPOSITE: return wantp->isComposite() || wantp->isError();
    case CandSet::Need::ANY: return true;
    }
    return true;  // LCOV_EXCL_LINE
}

std::string VhdlSemVisitor::distinctChars(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (out.find(c) == std::string::npos) out += c;
    }
    return out;
}

bool VhdlSemVisitor::elementHasChars(const VhdlType* arrayp, const std::string& chars) {
    if (chars.empty()) return true;
    const VhdlType* const elemp = arrayp->base()->elementp();
    if (!elemp) return true;
    if (elemp->base()->kind() != VhdlTypeKind::ENUM) return false;
    for (const char c : chars) {
        const std::string want = vhdlCharLitName(std::string(1, c));
        bool found = false;
        for (const VhdlSymbol* const litp : elemp->base()->literals()) {
            if (litp->name() == want) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

bool VhdlSemVisitor::allUniversal(const CandSet& cands) {
    if (cands.types.empty()) return false;
    for (const VhdlType* const typep : cands.types) {
        if (!typep || !typep->isUniversal()) return false;
    }
    return true;
}

void VhdlSemVisitor::preferUniversal(std::vector<VhdlSymbol*>& cands,
                                     const std::vector<const CandSet*>& operands) {
    if (cands.size() < 2) return;
    for (const CandSet* const setp : operands) {
        if (!allUniversal(*setp)) return;
    }
    // 3 < 2 is a comparison of universal integers, not of INTEGERs that the
    // literals were converted to, so the universal "<" is what it denotes
    std::vector<VhdlSymbol*> universal;
    for (VhdlSymbol* const symp : cands) {
        bool ok = !symp->paramTypes().empty();
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (!typep || !typep->isUniversal()) ok = false;
        }
        if (ok) universal.push_back(symp);
    }
    if (universal.size() == 1) cands.swap(universal);
}

bool VhdlSemVisitor::exactMatch(const CandSet& cands, const VhdlType* wantp) {
    if (cands.types.empty()) return true;  // The context settles it either way
    for (const VhdlType* const typep : cands.types) {
        if (!typep || !wantp) continue;
        // Exact means "needs no implicit conversion", so a universal literal
        // is exact only against a universal parameter. Callers rank overloads
        // by how many operands are exact, which is what makes 2 * 500 ns the
        // universal "*" rather than the INTEGER one (9.3.2).
        if (typep->base() == wantp->base()) return true;
    }
    return false;
}

// What a symbol finally denotes, following any chain of aliases (6.6)
static VhdlSymbol* aliasRoot(VhdlSymbol* symp) {
    for (int depth = 0; symp && symp->aliasp() && depth < 32; ++depth) symp = symp->aliasp();
    return symp;
}

VhdlType* VhdlSemVisitor::pick(AstNodeVhdlExpr* nodep, std::vector<VhdlSymbol*>& cands,
                               VhdlType* wantp, const std::string& what) {
    // An alias and what it aliases are one declaration, not two: aliasing a
    // type brings its literals into view again (6.6.3), and a name matching
    // both readings is not ambiguous.
    if (cands.size() > 1) {
        std::vector<VhdlSymbol*> distinct;
        std::set<const VhdlSymbol*> seen;
        for (VhdlSymbol* const symp : cands) {
            if (seen.insert(aliasRoot(symp)).second) distinct.push_back(symp);
        }
        if (distinct.size() < cands.size()) cands.swap(distinct);
    }
    if (cands.empty()) {
        err(nodep->fileline(), "No visible " + what + " matches this use");
        return V3VhdlStd::errorType();
    }
    // A procedure call statement names a procedure and nothing else (4.3,
    // 10.7), so nothing else competes with one -- not a function of the same
    // name, and not an enumeration literal of it either. A call written with
    // parentheses settles that where its arguments are read; a procedure
    // taking none at all is written as a bare name (4.2.2.1), and there is
    // nowhere else for it to be settled but here.
    // A condition that is not already a BOOLEAN has the condition operator
    // applied to it, and the expression is legal where exactly one reading of
    // it is a BOOLEAN once it is (9.2.9, and note 1 of 12.5). So a bare 'H' as
    // a condition is the STD_ULOGIC one, since CHARACTER has no "??" -- while
    // a bare '1' stays ambiguous, because BIT has one as well as STD_ULOGIC.
    // Only in a condition: "variable b : boolean := 'H';" is still an error.
    // Before VHDL-2008 there is no condition operator, and a condition is a
    // BOOLEAN outright.
    if (cands.size() > 1 && nodep == m_condExprp) {
        const bool conditionOp = nodep->fileline()->language() >= V3LangCode::L1076_2008;
        const VhdlType* const boolp = V3VhdlStd::booleanType();
        std::vector<VhdlSymbol*> kept;
        for (VhdlSymbol* const symp : cands) {
            const VhdlType* const typep = symp->typep();
            if (conditionOp ? hasConditionOperator(typep)
                            : (typep && boolp && typep->base() == boolp->base())) {
                kept.push_back(symp);
            }
        }
        if (kept.size() == 1) cands.swap(kept);
    }
    if (cands.size() > 1 && m_procCall) {
        std::vector<VhdlSymbol*> kept;
        for (VhdlSymbol* const symp : cands) {
            if (symp->resolved()->kind() != VhdlSymKind::PROCEDURE) continue;
            kept.push_back(symp);
        }
        if (!kept.empty()) cands.swap(kept);
    }
    if (cands.size() > 1) {
        // Inside a copy of an instantiated package, a call means what it
        // meant in the template: substituting the generics may have left two
        // declarations of the name with the same profile, and nothing here
        // could tell them apart any more (4.9)
        const auto tmplIt = m_templateSyms.find(nodep);
        const VhdlSymbol* const tmplp
            = tmplIt == m_templateSyms.end() ? nullptr : tmplIt->second;
        if (tmplp) {
            // A subprogram is written twice -- a declaration and a body --
            // and the template may have named either, so both places count
            const auto placesOf = [](const VhdlSymbol* symp) {
                std::set<std::string> out;
                if (symp->declp()) out.insert(symp->declp()->fileline()->ascii());
                if (symp->bodyp()) out.insert(symp->bodyp()->fileline()->ascii());
                return out;
            };
            const std::set<std::string> want = placesOf(tmplp);
            std::vector<VhdlSymbol*> same;
            for (VhdlSymbol* const symp : cands) {
                for (const std::string& place : placesOf(symp)) {
                    if (want.count(place)) {
                        same.push_back(symp);
                        break;
                    }
                }
            }
            if (same.size() == 1) cands.swap(same);
        }
    }
    if (cands.size() > 1) {
        // A declaration hides a homograph of it declared further out (12.3).
        // That is how a protected type's method of the same profile as a
        // package's function is the one meant inside the type's body.
        std::vector<VhdlSymbol*> inner;
        for (VhdlSymbol* const symp : cands) {
            bool hidden = false;
            for (VhdlSymbol* const otherp : cands) {
                if (otherp == symp) continue;
                if (!symp->homographOf(otherp)) continue;
                if (scopeEncloses(symp->declScopep(), otherp->declScopep())) hidden = true;
            }
            if (!hidden) inner.push_back(symp);
        }
        if (!inner.empty() && inner.size() < cands.size()) cands.swap(inner);
    }
    if (cands.size() > 1) {
        // A declaration written in the design hides the predefined operation
        // of the same profile that the language declares implicitly (12.3),
        // even where both are reached through use clauses: every tool takes
        // "function to_string (v : string) return string" in a package to be
        // the one meant, not STANDARD's.
        std::vector<VhdlSymbol*> explicits;
        for (VhdlSymbol* const symp : cands) {
            if (!symp->resolved()->predefined()) explicits.push_back(symp);
        }
        if (!explicits.empty() && explicits.size() < cands.size()) {
            // Only where they are homographs: a predefined one with a
            // different profile is a real alternative, not a hidden one
            bool sameProfile = true;
            for (VhdlSymbol* const symp : cands) {
                if (!symp->resolved()->predefined()) continue;
                bool matched = false;
                for (VhdlSymbol* const exp : explicits) {
                    if (exp->homographOf(symp)) matched = true;
                }
                if (!matched) sameProfile = false;
            }
            if (sameProfile) cands.swap(explicits);
        }
    }
    if (cands.size() > 1 && wantp) {
        // The context settles it (12.5)
        std::vector<VhdlSymbol*> narrowed;
        for (VhdlSymbol* const symp : cands) {
            VhdlType* const resultp = symp->returnTypep() ? symp->returnTypep() : symp->typep();
            if (typesMatch(resultp, wantp)) narrowed.push_back(symp);
        }
        if (!narrowed.empty()) cands.swap(narrowed);
    }
    if (cands.size() > 1) {
        // Several interpretations remain. Where the context names a type,
        // prefer the reading that gives exactly it rather than one that
        // converts; where there is no context at all, prefer the universal
        // reading, which is what a literal means on its own (9.3.2).
        std::vector<VhdlSymbol*> preferred;
        for (VhdlSymbol* const symp : cands) {
            VhdlType* const resultp = symp->returnTypep() ? symp->returnTypep() : symp->typep();
            if (!resultp) continue;
            if (wantp ? resultp->base() == wantp->base() : resultp->isUniversal()) {
                preferred.push_back(symp);
            }
        }
        // Two implicit declarations of a physical operator differ only in
        // whether the count is universal or INTEGER -- "*"(universal_integer,
        // TIME) and "*"(INTEGER, TIME) -- and mean the same thing, so an
        // operand that reads as either is not an ambiguity to report (16.3).
        // The universal one is what a literal means on its own (9.3.2).
        if (preferred.size() != 1 && sameButForUniversal(cands)) {
            VhdlSymbol* bestp = cands[0];
            int most = -1;
            for (VhdlSymbol* const symp : cands) {
                int universal = 0;
                for (const VhdlType* const typep : symp->paramTypes()) {
                    if (typep && typep->isUniversal()) ++universal;
                }
                if (universal > most) {
                    most = universal;
                    bestp = symp;
                }
            }
            preferred.assign(1, bestp);
        }
        if (preferred.size() == 1) {
            cands.swap(preferred);
        } else {
            std::string detail;
            for (const VhdlSymbol* const symp : cands) {
                detail += "\n... candidate: " + symp->name() + "(";
                for (size_t i = 0; i < symp->paramTypes().size(); ++i) {
                    if (i) detail += ", ";
                    detail += symp->paramTypes()[i] ? symp->paramTypes()[i]->prettyName()
                                                    : "?";
                }
                detail += ")";
                if (symp->returnTypep()) detail += " return " + symp->returnTypep()->prettyName();
                if (symp->declp()) detail += " at " + symp->declp()->fileline()->ascii();
            }
            err(nodep->fileline(),
                "Ambiguous " + what + ": " + std::to_string(cands.size())
                    + " visible declarations match, and the context does not decide between them"
                    + detail);
            nodep->vhdlSymp(cands[0]);
            return cands[0]->returnTypep() ? cands[0]->returnTypep() : cands[0]->typep();
        }
    }
    nodep->vhdlSymp(cands[0]);
    VhdlType* const resultp = cands[0]->returnTypep() ? cands[0]->returnTypep() : cands[0]->typep();
    return resultp ? resultp : V3VhdlStd::errorType();
}

// Declarations of one name that two packages make potentially visible,
// where one of them is not overloadable: then neither is visible (12.4). A
// directly visible declaration would have hidden the rest, so all of them
// having come from packages -- two different ones -- is what says so.
bool VhdlSemVisitor::conflictingPotentials(const std::vector<VhdlSymbol*>& found) const {
    std::set<const VhdlSymbol*> roots;
    std::set<const void*> units;
    bool anyPlain = false;
    for (VhdlSymbol* const symp : found) {
        VhdlSymbol* const rootp = aliasRoot(symp);
        if (!roots.insert(rootp).second) continue;
        // STANDARD and TEXTIO are packages too, built in code: TIME's unit
        // MIN and a package's function MIN hide each other (GHDL 7.0.0-dev
        // and Questa 2025.2; nvc 1.22.1 takes the function)
        if (!symp->declp() && rootp->predefined() && rootp->declScopep()) {
            units.insert(rootp->declScopep());
            if (!rootp->isOverloadable()) anyPlain = true;
            continue;
        }
        const AstNodeVhdlUnit* const unitp = symp->declp() ? unitOf(symp->declp()) : nullptr;
        if (!VN_IS(unitp, VhdlPackage) && !VN_IS(unitp, VhdlPackageInst)) return false;
        // Declared by the unit being analyzed, or by the package whose body
        // this is: directly visible, so it hides the rest (12.3)
        if (unitp == m_curUnitp) return false;
        if (VN_IS(m_curUnitp, VhdlPackageBody) && VN_IS(unitp, VhdlPackage)
            && lower(unitp->name()) == lower(m_curUnitp->name())) {
            return false;
        }
        units.insert(unitp);
        if (!rootp->isOverloadable()) anyPlain = true;
    }
    return anyPlain && roots.size() > 1 && units.size() > 1;
}

VhdlType* VhdlSemVisitor::analyzeName(AstVhdlName* nodep, VhdlType* wantp) {
    if (nameBeingDeclared(nodep)) return V3VhdlStd::errorType();
    const std::string key = nodep->isChar() ? vhdlCharLitName(nodep->name()) : nodep->name();
    std::vector<VhdlSymbol*> found;
    lookup(key, found);
    if (found.empty()) {
        err(nodep->fileline(), "Cannot find declaration of '" + nodep->name() + "'");
        return V3VhdlStd::errorType();
    }
    // An object denotes itself; overloadables need the context to choose
    std::vector<VhdlSymbol*> objects;
    std::vector<VhdlSymbol*> callables;
    for (VhdlSymbol* const symp : found) {
        // A subprogram every one of whose parameters has a default may be
        // called by naming it, with no parentheses at all (4.2.2.1)
        if (symp->isOverloadable() && !symp->paramTypes().empty()
            && !allParamsDefaulted(symp)) {
            continue;
        }
        if (symp->isOverloadable()) {
            callables.push_back(symp);
        } else {
            objects.push_back(symp);
        }
    }
    if (!objects.empty()) {
        // A directly visible declaration would have hidden the rest, so two
        // distinct ones came by use clauses, and then neither is visible
        // (12.4)
        if (conflictingPotentials(found) && !m_inSpecialized) {
            err(nodep->fileline(), "'" + nodep->name()
                                       + "' is declared by more than one package a use clause"
                                         " makes visible here, so neither is visible");
            return V3VhdlStd::errorType();
        }
        nodep->vhdlSymp(objects[0]);
        // A design unit's name denotes the unit, which has no value; only an
        // attribute of it may be asked for (16.2)
        const VhdlSymKind kind = objects[0]->kind();
        // A design unit's or a library's name may be the prefix of a selected
        // name or of an attribute: "work.e'delay", "work.pkg.c"
        const AstNode* upp = nodep;
        while (const AstVhdlSelected* const selp = VN_CAST(parentOf(upp), VhdlSelected)) {
            if (selp->prefixp() != upp) break;
            upp = selp;
        }
        const AstVhdlAttrName* const attrp = VN_CAST(parentOf(upp), VhdlAttrName);
        if ((kind == VhdlSymKind::ENTITY || kind == VhdlSymKind::ARCHITECTURE
             || kind == VhdlSymKind::CONFIGURATION || kind == VhdlSymKind::CONTEXT
             || kind == VhdlSymKind::LIBRARY)
            && !(attrp && attrp->prefixp() == upp) && upp == nodep && !m_inSpecialized) {
            const bool vowel = kind == VhdlSymKind::ARCHITECTURE || kind == VhdlSymKind::ENTITY;
            err(nodep->fileline(), "'" + nodep->name() + "' here names " + (vowel ? "an " : "a ")
                                       + objects[0]->kindName() + ", which has no value");
            return V3VhdlStd::errorType();
        }
        return objects[0]->typep() ? objects[0]->typep() : V3VhdlStd::errorType();
    }
    if (callables.empty()) {
        err(nodep->fileline(), "'" + nodep->name() + "' needs arguments here");
        return V3VhdlStd::errorType();
    }
    return pick(nodep, callables, wantp, "declaration of '" + nodep->name() + "'");
}

// Whether every parameter of a subprogram has a default value, so that it
// may be called with no arguments at all (4.2.2.1)
bool VhdlSemVisitor::allParamsDefaulted(const VhdlSymbol* symp) {
    const AstVhdlSubprogDecl* const declp
        = symp->declp() ? VN_CAST(symp->declp(), VhdlSubprogDecl) : nullptr;
    if (!declp) return false;
    int seen = 0;
    int skip = declp->genericCount();
    for (AstNode* p = declp->paramsp(); p; p = p->nextp()) {
        const AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        if (skip > 0) {
            --skip;
            continue;
        }
        if (!ifp->defaultp()) return false;
        ++seen;
    }
    return seen == static_cast<int>(symp->paramTypes().size());
}

VhdlType* VhdlSemVisitor::analyzeLiteral(AstVhdlLiteral* nodep, VhdlType* wantp) {
    switch (nodep->kind()) {
    case VhdlLitKind::INTEGER:
        // An integer literal is of the anonymous universal type until the
        // context gives it a named one (9.3.2)
        if (wantp && (wantp->kind() == VhdlTypeKind::INTEGER
                      || wantp->kind() == VhdlTypeKind::PHYSICAL)) {
            return wantp;
        }
        return V3VhdlStd::universalIntegerType();
    case VhdlLitKind::REAL:
        if (wantp && wantp->kind() == VhdlTypeKind::FLOATING) return wantp;
        return V3VhdlStd::universalRealType();
    case VhdlLitKind::PHYSICAL: {
        // The unit names the type (9.3.2)
        if (AstNodeVhdlExpr* const unitp = nodep->unitp()) {
            std::vector<std::string> parts;
            if (flatten(unitp, parts) && !parts.empty()) {
                std::vector<VhdlSymbol*> found;
                lookup(parts.back(), found);
                for (VhdlSymbol* const symp : found) {
                    if (symp->kind() == VhdlSymKind::PHYSICAL_UNIT) {
                        unitp->vhdlSymp(symp);
                        return symp->typep();
                    }
                }
                err(nodep->fileline(), "'" + parts.back() + "' is not a physical unit");
            }
        }
        return V3VhdlStd::errorType();
    }
    case VhdlLitKind::CHARACTER: {
        // A character literal denotes an enumeration literal, and may belong
        // to several enumeration types at once
        std::vector<VhdlSymbol*> found;
        lookup(vhdlCharLitName(nodep->text()), found);
        std::vector<VhdlSymbol*> lits;
        for (VhdlSymbol* const symp : found) {
            if (symp->kind() == VhdlSymKind::ENUM_LITERAL) lits.push_back(symp);
        }
        if (lits.empty()) {
            err(nodep->fileline(),
                "Character literal '" + nodep->text() + "' belongs to no visible type");
            return V3VhdlStd::errorType();
        }
        return pick(nodep, lits, wantp, "character literal '" + nodep->text() + "'");
    }
    case VhdlLitKind::STRING:
    case VhdlLitKind::BITSTRING:
        // A string or bit string literal takes its type from the context; if
        // none is given it is a STRING (9.3.2)
        if (wantp && wantp->base()->kind() == VhdlTypeKind::ARRAY) {
            checkLiteralChars(nodep, wantp);
            return wantp;
        }
        return nodep->kind() == VhdlLitKind::STRING ? V3VhdlStd::stringType()
                                                    : V3VhdlStd::bitVectorType();
    case VhdlLitKind::NULLLIT:
        if (wantp && wantp->kind() == VhdlTypeKind::ACCESS) return wantp;
        return V3VhdlStd::errorType();
    }
    return V3VhdlStd::errorType();  // LCOV_EXCL_LINE
}

// Each character of a string or bit string literal stands for the character
// literal of the element type that it spells (9.3.2), so B"0012" is not a
// BIT_VECTOR: BIT has no '2'. The context alone fixes the literal's type, so
// this is a check on a type already chosen, never a reason to choose another.
void VhdlSemVisitor::checkLiteralChars(AstVhdlLiteral* nodep, VhdlType* arrayp) {
    const VhdlType* const elemp = arrayp->base()->elementp();
    if (!elemp || elemp->isError() || elemp->base()->kind() != VhdlTypeKind::ENUM) return;
    const std::vector<VhdlSymbol*>& lits = elemp->base()->literals();
    if (lits.empty()) return;
    std::set<std::string> chars;
    for (const VhdlSymbol* const symp : lits) chars.insert(symp->name());
    for (const char c : nodep->text()) {
        if (chars.count(vhdlCharLitName(std::string(1, c)))) continue;
        err(nodep->fileline(), "Type " + elemp->base()->prettyName() + " has no character '"
                                   + std::string(1, c) + "', so this literal cannot be of type "
                                   + arrayp->prettyName());
        return;
    }
    // And each of those character literals is visible where the string is
    // written (9.3.2): an UNSIGNED of "0101" needs STD_LOGIC_1164's literals,
    // which NUMERIC_STD alone does not make visible
    if (m_inSpecialized || m_inPackageInstance || m_inPkgInst) return;
    std::set<char> checked;
    for (const char c : nodep->text()) {
        if (!checked.insert(c).second) continue;
        std::vector<VhdlSymbol*> found;
        lookup(vhdlCharLitName(std::string(1, c)), found);
        const bool visible = std::any_of(found.begin(), found.end(), [&](VhdlSymbol* symp) {
            const VhdlSymbol* const realp = symp->resolved();
            return realp->kind() == VhdlSymKind::ENUM_LITERAL && realp->typep()
                   && realp->typep()->base() == elemp->base();
        });
        if (visible) continue;
        err(nodep->fileline(), "Character literal '" + std::string(1, c) + "' of type "
                                   + elemp->base()->prettyName()
                                   + " is not visible here, so a string literal cannot use it");
        return;
    }
}

VhdlType* VhdlSemVisitor::analyzeUnary(AstVhdlUnary* nodep, VhdlType* wantp) {
    CandSet ocands;
    candidateTypes(nodep->lhsp(), ocands);
    const std::string op = vhdlUnaryOpName(nodep->op());
    std::vector<VhdlSymbol*> found;
    lookup(op, found);
    std::vector<VhdlSymbol*> exact;
    std::vector<VhdlSymbol*> loose;
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() != VhdlSymKind::FUNCTION) continue;
        if (symp->paramTypes().size() != 1) continue;
        if (!anyMatches(ocands, symp->paramTypes()[0])) continue;
        (exactMatch(ocands, symp->paramTypes()[0]) ? exact : loose).push_back(symp);
    }
    std::vector<VhdlSymbol*> cands = exact.empty() ? loose : exact;
    preferUniversal(cands, {&ocands});
    if (!ocands.types.empty() && ocands.types[0] && ocands.types[0]->isError()) {
        return V3VhdlStd::errorType();
    }
    if (cands.empty()) {
        err(nodep->fileline(), "No visible operator \"" + op + "\" matches this use");
        analyzeExpr(nodep->lhsp(), nullptr);
        return V3VhdlStd::errorType();
    }
    VhdlType* const resultp = pick(nodep, cands, wantp, "operator \"" + op + "\"");
    if (nodep->vhdlSymp() && nodep->vhdlSymp()->paramTypes().size() == 1) {
        analyzeExpr(nodep->lhsp(), nodep->vhdlSymp()->paramTypes()[0]);
    }
    return resultp;
}

// Whether the result of this operator is as long as the operands it was
// handed, rather than a length of its own
static bool sameLengthAsOperands(VhdlBinaryOp op) {
    switch (op) {
    case VhdlBinaryOp::AND:
    case VhdlBinaryOp::OR:
    case VhdlBinaryOp::NAND:
    case VhdlBinaryOp::NOR:
    case VhdlBinaryOp::XOR:
    case VhdlBinaryOp::XNOR:
    case VhdlBinaryOp::ADD:
    case VhdlBinaryOp::SUB:
    case VhdlBinaryOp::SLL:
    case VhdlBinaryOp::SRL:
    case VhdlBinaryOp::SLA:
    case VhdlBinaryOp::SRA:
    case VhdlBinaryOp::ROL:
    case VhdlBinaryOp::ROR: return true;
    default: return false;
    }
}

VhdlType* VhdlSemVisitor::analyzeBinary(AstVhdlBinary* nodep, VhdlType* wantp) {
    // Resolve the operand that can stand alone first, so its type can settle
    // the one that cannot
    // Gather what each operand could be, without settling either yet
    CandSet lcands;
    CandSet rcands;
    candidateTypes(nodep->lhsp(), lcands);
    candidateTypes(nodep->rhsp(), rcands);
    const std::string op = vhdlBinaryOpName(nodep->op());
    std::vector<VhdlSymbol*> found;
    lookup(op, found);
    // Prefer operators whose parameter types match exactly; only if none do
    // is an implicit conversion from a universal type considered (9.3.2)
    // Score each applicable operator by how many of its operands need no
    // implicit conversion, and keep only the best. That is what tells
    // "2 * 500 ns" apart: the universal "*" fits both operands exactly, while
    // the INTEGER one would have to convert the literal (9.3.2).
    std::vector<VhdlSymbol*> cands;
    int best = -1;
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() != VhdlSymKind::FUNCTION) continue;
        if (symp->paramTypes().size() != 2) continue;
        if (!anyMatches(lcands, symp->paramTypes()[0])) continue;
        if (!anyMatches(rcands, symp->paramTypes()[1])) continue;
        const int score = (exactMatch(lcands, symp->paramTypes()[0]) ? 1 : 0)
                          + (exactMatch(rcands, symp->paramTypes()[1]) ? 1 : 0);
        if (score > best) {
            best = score;
            cands.clear();
        }
        if (score == best) cands.push_back(symp);
    }
    // A universal real scaled by a universal integer is a universal real,
    // which becomes no named type a context asks for: nvc 1.22.1 and GHDL
    // 7.0.0-dev both refuse "constant r : real := 1000.0 / 100" and take
    // "real(1000.0 / 100)", where nothing asks for one (9.3.6)
    if (wantp && !wantp->isError() && !wantp->isUniversal() && !wantp->isGenericFormal()) {
        const auto mixed = [](const VhdlSymbol* symp) {
            if (!symp->paramTypes()[0] || !symp->paramTypes()[1]) return false;
            const VhdlTypeKind a = symp->paramTypes()[0]->base()->kind();
            const VhdlTypeKind b = symp->paramTypes()[1]->base()->kind();
            return (a == VhdlTypeKind::UNIVERSAL_REAL && b == VhdlTypeKind::UNIVERSAL_INTEGER)
                   || (a == VhdlTypeKind::UNIVERSAL_INTEGER && b == VhdlTypeKind::UNIVERSAL_REAL);
        };
        cands.erase(std::remove_if(cands.begin(), cands.end(), mixed), cands.end());
    }
    preferUniversal(cands, {&lcands, &rcands});
    const bool sawError = (!lcands.types.empty() && lcands.types[0] && lcands.types[0]->isError())
                          || (!rcands.types.empty() && rcands.types[0]
                              && rcands.types[0]->isError());
    // An operand that is already in error matches everything, so anything
    // said about this operator would be about the earlier mistake
    if (sawError) return V3VhdlStd::errorType();
    if (cands.empty()) {
        if (!sawError) {
            err(nodep->fileline(),
                "No visible operator \"" + op + "\" takes "
                    + (lcands.types.empty() ? "this operand" : lcands.types[0]->prettyName())
                    + " and "
                    + (rcands.types.empty() ? "this operand" : rcands.types[0]->prettyName()));
        }
        analyzeExpr(nodep->lhsp(), nullptr);
        analyzeExpr(nodep->rhsp(), nullptr);
        return V3VhdlStd::errorType();
    }
    VhdlType* const resultp = pick(nodep, cands, wantp, "operator \"" + op + "\"");
    // The chosen signature now settles both operands
    if (nodep->vhdlSymp() && nodep->vhdlSymp()->paramTypes().size() == 2) {
        analyzeExpr(nodep->lhsp(), nodep->vhdlSymp()->paramTypes()[0]);
        analyzeExpr(nodep->rhsp(), nodep->vhdlSymp()->paramTypes()[1]);
    }
    // VHDL-2008 gave physical types "mod" and "rem"; before it they had
    // neither (9.2.7)
    if ((nodep->op() == VhdlBinaryOp::MOD || nodep->op() == VhdlBinaryOp::REM) && resultp
        && resultp->base()->kind() == VhdlTypeKind::PHYSICAL && nodep->vhdlSymp()
        && nodep->vhdlSymp()->predefined()
        && nodep->fileline()->language() < V3LangCode::L1076_2008) {
        err(nodep->fileline(), std::string{"Before VHDL-2008 a physical type has no \""}
                                   + (nodep->op() == VhdlBinaryOp::MOD ? "mod" : "rem")
                                   + "\" operator: " + resultp->prettyName());
    }
    // An operator declared to return an unconstrained array may give a result
    // as long as what it was handed, and then an operand's own subtype says
    // how long it is. Only for the operators that do: "&" is as long as both
    // together, and numeric_std's "*" as long as both operands' lengths
    // added, so neither takes its length from one of them (9.2.2, 16.5).
    if (resultp && resultp->base()->kind() == VhdlTypeKind::ARRAY
        && !resultp->lengthEmittable() && sameLengthAsOperands(nodep->op())) {
        VhdlType* bestp = nullptr;
        for (AstNodeVhdlExpr* const opp : {nodep->lhsp(), nodep->rhsp()}) {
            VhdlType* typep = opp ? opp->vhdlTypep() : nullptr;
            if (!typep || typep->base() != resultp->base()) continue;
            // An operand whose own subtype says no length may still be one
            // that can be measured: a concatenation is as long as its parts
            // together (9.2.5), and an aggregate as long as it was written.
            // Passing over such an operand leaves the result with the other
            // one's length, short by exactly the part that went uncounted.
            if (!typep->lengthEmittable()) {
                VhdlType* const measuredp = constrainFromValue(resultp, opp, nodep->fileline());
                if (!measuredp || !measuredp->lengthEmittable()) continue;
                typep = measuredp;
            }
            // Of two lengths, the result is the longer (16.5)
            if (!bestp || (typep->isConstrainedArray() && bestp->isConstrainedArray()
                           && typep->dimLength(0) > bestp->dimLength(0))) {
                bestp = typep;
            }
        }
        if (bestp) return bestp;
    }
    return resultp;
}

VhdlType* VhdlSemVisitor::analyzeSelected(AstVhdlSelected* nodep, VhdlType* wantp) {
    // "prefix.suffix" is a selected name (8.3): a record element, a
    // dereference through an access value, or a name inside a package
    if (!nodep->suffixp()) {
        // "prefix.all" dereferences an access value (8.3)
        VhdlType* const prefixp = analyzeExpr(nodep->prefixp(), nullptr);
        if (prefixp && prefixp->kind() == VhdlTypeKind::ACCESS) return prefixp->designatedp();
        if (prefixp && !prefixp->isError()) {
            err(nodep->fileline(), "'.all' needs an access value, but the prefix is "
                                       + prefixp->prettyName());
        }
        return V3VhdlStd::errorType();
    }

    // A name inside a library or package, e.g. work.pkg.thing
    std::vector<std::string> parts;
    if (flatten(nodep, parts) && parts.size() >= 2) {
        VhdlScope* scopep = m_scopep;
        bool viaScope = true;
        for (size_t i = 0; i + 1 < parts.size() && viaScope; ++i) {
            VhdlScope* const nextp = scopep ? scopep->findNamed(parts[i]) : nullptr;
            if (!nextp) {
                viaScope = false;
                break;
            }
            scopep = nextp;
        }
        if (viaScope && scopep) {
            std::vector<VhdlSymbol*> found;
            scopep->lookupLocal(parts.back(), found);
            if (!found.empty()) {
                std::vector<VhdlSymbol*> objects;
                for (VhdlSymbol* const symp : found) {
                    if (!symp->isOverloadable()) objects.push_back(symp);
                }
                if (!objects.empty()) {
                    nodep->vhdlSymp(objects[0]);
                    return objects[0]->typep() ? objects[0]->typep() : V3VhdlStd::errorType();
                }
                // Written without arguments, the name can only denote a
                // parameterless declaration: "std.env.stop" is the STOP that
                // takes none, not the one that takes a status (4.2.2.1)
                std::vector<VhdlSymbol*> callables;
                for (VhdlSymbol* const symp : found) {
                    if (symp->paramTypes().empty()) callables.push_back(symp);
                }
                if (callables.empty()) callables = found;
                return pick(nodep, callables, wantp,
                            "declaration of '" + parts.back() + "'");
            }
        }
    }

    // A method of a protected type taking no arguments, named through an
    // object of that type (5.6.2)
    {
        VhdlScope* methScopep = nullptr;
        VhdlSymbol* const objp = protectedObject(nodep->prefixp(), methScopep);
        const AstVhdlName* const sufp = objp ? VN_CAST(nodep->suffixp(), VhdlName) : nullptr;
        if (sufp) {
            std::vector<VhdlSymbol*> methods;
            methScopep->lookupLocal(sufp->name(), methods);
            // Only what the protected type declares is a method of it. Its
            // body may declare subprograms of its own, which are local to the
            // body and are not reached through an object (5.6.2). A declared
            // method has no body where it is declared; the body's own
            // subprograms do.
            {
                std::vector<VhdlSymbol*> declared;
                for (VhdlSymbol* const mp : methods) {
                    const AstVhdlSubprogDecl* const dp
                        = VN_CAST(mp->declp(), VhdlSubprogDecl);
                    if (dp && !dp->hasBody()) declared.push_back(mp);
                }
                if (!declared.empty()) methods.swap(declared);
            }
            // Named with no parentheses, so only a method that needs no
            // arguments can be meant (4.2.2.1)
            {
                std::vector<VhdlSymbol*> callable;
                for (VhdlSymbol* const mp : methods) {
                    if (mp->paramTypes().empty() || allParamsDefaulted(mp)) {
                        callable.push_back(mp);
                    }
                }
                if (!callable.empty()) methods.swap(callable);
            }
            if (!methods.empty()) {
                nodep->prefixp()->vhdlSymp(objp);
                return pick(nodep, methods, wantp, "method '" + sufp->name() + "'");
            }
        }
    }

    // Otherwise a record element (8.3)
    VhdlType* const prefixp = analyzeExpr(nodep->prefixp(), nullptr);
    if (!prefixp || prefixp->isError()) return V3VhdlStd::errorType();
    // Of an object: a type has elements, but no values to select them from.
    // Asking an attribute of one -- "rec_t.field'length" -- is what nvc
    // 1.22.1 and Questa 2025.2 accept, since only the element's subtype is
    // wanted (GHDL 7.0.0-dev refuses it).
    const AstVhdlAttrName* const askedp = VN_CAST(parentOf(nodep), VhdlAttrName);
    if (VN_IS(nodep->prefixp(), VhdlName) && nodep->prefixp()->vhdlSymp()
        && nodep->prefixp()->vhdlSymp()->resolved()->kind() == VhdlSymKind::TYPE
        && !(askedp && askedp->prefixp() == nodep) && !m_inSpecialized) {
        err(nodep->fileline(), "A selected name selects from an object or a design unit, and '"
                                   + nodep->prefixp()->vhdlSymp()->name() + "' is a type");
        return V3VhdlStd::errorType();
    }
    VhdlType* recp = prefixp;
    if (recp->kind() == VhdlTypeKind::ACCESS) recp = recp->designatedp();
    if (!recp || recp->kind() != VhdlTypeKind::RECORD) {
        err(nodep->fileline(),
            "Selected name needs a record or package prefix, but this one is "
                + prefixp->prettyName());
        return V3VhdlStd::errorType();
    }
    const AstVhdlName* const suffixp = VN_CAST(nodep->suffixp(), VhdlName);
    if (!suffixp) return V3VhdlStd::errorType();
    // The subtype's own elements first: a record element of an unconstrained
    // array type is constrained per object, and the base type's element still
    // says nothing about how long it is (5.3.3)
    for (const std::vector<VhdlSymbol*>* listp : {&recp->elements(), &recp->base()->elements()}) {
        for (VhdlSymbol* const elemp : *listp) {
            if (elemp->name() == suffixp->name()) {
                nodep->vhdlSymp(elemp);
                return elemp->typep();
            }
        }
    }
    err(nodep->fileline(), "Record type " + recp->prettyName() + " has no element '"
                               + suffixp->name() + "'");
    return V3VhdlStd::errorType();
}

// A port of mode OUT may not be read before VHDL-2008 (93's 4.3.2; 2008
// lifted it in 6.5.2). Asking one of these attributes is reading it: they
// answer about the value the signal has or had. 'DRIVING and 'DRIVING_VALUE
// ask about what this process drives, which an output is exactly for, and
// 'LENGTH and its kind ask about the subtype rather than the signal.
static bool readsTheSignal(const std::string& attr) {
    return attr == "stable" || attr == "event" || attr == "active" || attr == "quiet"
           || attr == "last_value" || attr == "last_event" || attr == "last_active"
           || attr == "transaction" || attr == "delayed";
}

// Whether this declaration is a parameter of a subprogram rather than a port
static bool isSubprogramParam(const AstNode* declp) {
    for (const AstNode* upp = declp; upp; upp = parentOf(upp)) {
        if (VN_IS(upp, VhdlSubprogDecl)) return true;
    }
    return false;
}

void VhdlSemVisitor::checkSignalAttrOfOutPort(AstVhdlAttrName* nodep,
                                              const std::string& attrName) {
    if (!readsTheSignal(attrName)) return;
    const AstNodeVhdlExpr* const prefixp = nodep->prefixp();
    const VhdlSymbol* const symp
        = prefixp && prefixp->vhdlSymp() ? prefixp->vhdlSymp()->resolved() : nullptr;
    // An interface object -- a port, or a subprogram parameter -- carries its
    // mode and its class on the declaration rather than on the symbol
    const AstVhdlIfaceDecl* const portp
        = symp ? VN_CAST(symp->declp(), VhdlIfaceDecl) : nullptr;
    if (!portp) return;
    if (portp->kind() == VhdlIfaceKind::SIGNAL && makesImplicitSignal(attrName)
        && isSubprogramParam(portp)) {
        err(nodep->fileline(), "'" + attrName
                                   + " of a signal parameter cannot be read in a subprogram,"
                                     " since the implicit signal it asks about has nowhere"
                                     " there to live: '"
                                   + symp->name() + "'");
        return;
    }
    if (nodep->fileline()->language() >= V3LangCode::L1076_2008) return;
    if (portp->mode() != VhdlMode::OUT) return;
    err(nodep->fileline(), "A port of mode out cannot be read before VHDL-2008, and '"
                               + attrName + " reads it: '" + symp->name() + "'");
}

// Which predefined attributes a prefix may be asked (16.2). An array's
// ranges are asked of the array, object or subtype; a scalar type's range
// and its conversions are asked of the type -- of an object of it only from
// VHDL-2019 -- and only an array has dimensions to choose among.
void VhdlSemVisitor::checkAttrPrefix(AstVhdlAttrName* nodep, const VhdlType* prefixTypep,
                                     bool isArray) {
    const std::string& attr = nodep->name();
    // 'DRIVING and 'DRIVING_VALUE ask what this process drives a signal to,
    // so their prefix is a signal it may drive (16.2.4)
    // An attribute of a signal is an implicit signal, or reads what one
    // keeps, for that one signal: its prefix is a static name (16.2.4)
    if (readsTheSignal(attr) || attr == "driving" || attr == "driving_value") {
        if (objectOfName(nodep->prefixp()).cls == ObjClass::SIGNAL) {
            if (const VhdlSymbol* const varyingp = nonStaticSignalIndex(nodep->prefixp())) {
                err(nodep->fileline(), "The prefix of '" + attr
                                           + " is a static signal name, but this one's index"
                                             " depends on '"
                                           + varyingp->name() + "'");
                return;
            }
        }
    }
    if (attr == "driving" || attr == "driving_value") {
        const ObjInfo info = objectOfName(nodep->prefixp());
        if (info.cls != ObjClass::SIGNAL) {
            err(nodep->fileline(), "The prefix of '" + attr + " is a signal");
        } else if (info.modeIn && info.symp) {
            err(nodep->fileline(), "'" + attr + " asks what is driven, and '" + info.symp->name()
                                       + "' is of mode in");
        }
        return;
    }
    if (isArray) return;
    const bool ofRange = attr == "left" || attr == "right" || attr == "high" || attr == "low"
                         || attr == "ascending";
    const bool ofArray = attr == "length" || attr == "range" || attr == "reverse_range";
    const bool ofType = attr == "base" || attr == "pos" || attr == "val" || attr == "succ"
                        || attr == "pred" || attr == "leftof" || attr == "rightof"
                        || attr == "image" || attr == "value";
    if (!ofRange && !ofArray && !ofType) return;
    if ((ofRange || ofArray) && nodep->argp()) {
        err(nodep->fileline(), "Attribute '" + attr
                                   + " is given a dimension, which only an array has, but the"
                                     " prefix is of type "
                                   + prefixTypep->prettyName());
        return;
    }
    // Only an array has a length, whatever names it. "T'range" of a scalar
    // type is taken as T's own range, which nvc 1.22.1 accepts and its own
    // tests use (GHDL 7.0.0-dev and Questa 2025.2 refuse it).
    if (attr == "length" && objectOfName(nodep->prefixp()).cls == ObjClass::UNKNOWN) {
        err(nodep->fileline(), "Attribute '" + attr
                                   + " asks about an array, but the prefix is"
                                     " of type "
                                   + prefixTypep->prettyName());
        return;
    }
    if (nodep->fileline()->language() >= V3LangCode::L1076_2019) return;
    const ObjInfo info = objectOfName(nodep->prefixp());
    if (info.cls == ObjClass::UNKNOWN || !info.symp) return;
    if (ofArray) {
        err(nodep->fileline(), "Attribute '" + attr + " asks about an array, but '"
                                   + info.symp->name() + "' is of type "
                                   + prefixTypep->prettyName());
    } else {
        err(nodep->fileline(), "Before VHDL-2019 attribute '" + attr
                                   + " is asked of a type, not of an object of it like '"
                                   + info.symp->name() + "'");
    }
}

VhdlType* VhdlSemVisitor::analyzeAttrName(AstVhdlAttrName* nodep, VhdlType* wantp) {
    // Predefined attributes (16.2). The prefix may be a type mark or an
    // object; either way its type is what most attributes ask about.
    VhdlType* prefixTypep = nullptr;
    std::vector<std::string> parts;
    if (flatten(nodep->prefixp(), parts) && !parts.empty()) {
        std::vector<VhdlSymbol*> found;
        lookup(parts.back(), found);
        for (VhdlSymbol* const symp : found) {
            if (symp->kind() == VhdlSymKind::TYPE) {
                prefixTypep = symp->typep();
                nodep->prefixp()->vhdlSymp(symp);
                break;
            }
        }
    }
    // A type mark may itself be written as an attribute, and one attribute
    // may then be asked of another: "y'subtype'right" asks about the right
    // bound of the subtype 'subtype gives (16.2.5)
    //
    // Not for the name attributes: those ask about the name the prefix is
    // written as, and "v'subtype" is a subtype no declaration names, which
    // is a rule of its own below.
    if (!prefixTypep && nodep->name() != "instance_name" && nodep->name() != "path_name"
        && nodep->name() != "simple_name") {
        if (AstVhdlAttrName* const markp = VN_CAST(nodep->prefixp(), VhdlAttrName)) {
            // 'base is left out: it is asked of a type mark, and asking it
            // of an object is the pre-2019 error checkAttrPrefix reports
            const std::string& inner = markp->name();
            if (inner == "subtype" || inner == "element") {
                VhdlType* const typep = resolveTypeMark(markp);
                if (typep && !typep->isError()) prefixTypep = typep;
            }
        }
    }
    // The name attributes ask about the *name*, not about a value, so their
    // prefix may be a design unit, a package or a statement label -- none of
    // which analyzes as an expression (16.2.7)
    const std::string& attrName = nodep->name();
    if (!prefixTypep
        && (attrName == "instance_name" || attrName == "path_name"
            || attrName == "simple_name")) {
        std::vector<VhdlSymbol*> found;
        if (!parts.empty()) lookup(parts.back(), found);
        if (!found.empty()) nodep->prefixp()->vhdlSymp(found[0]);
        // The prefix names a named entity (16.2.7), and "v'subtype" of an
        // object declared with a constraint is a subtype no declaration names
        // (nvc 1.22.1 and GHDL 7.0.0-dev; Questa 2025.2 takes it)
        if (AstVhdlAttrName* const subAttrp = VN_CAST(nodep->prefixp(), VhdlAttrName)) {
            std::vector<std::string> objParts;
            if (subAttrp->name() == "subtype" && !m_inSpecialized
                && flatten(subAttrp->prefixp(), objParts) && !objParts.empty()) {
                std::vector<VhdlSymbol*> objs;
                lookup(objParts.back(), objs);
                const AstVhdlObjectDecl* const declp
                    = objs.size() == 1 ? VN_CAST(objs[0]->resolved()->declp(), VhdlObjectDecl)
                                       : nullptr;
                // "bit_vector(0 to 1)" may be written into the type mark
                const AstVhdlSubtypeInd* const sip = declp ? declp->subtypep() : nullptr;
                if (sip
                    && (sip->constraintp() || sip->resolutionp()
                        || VN_IS(sip->typeMarkp(), VhdlCall))) {
                    err(nodep->fileline(), "The prefix of '" + attrName
                                               + " is a named entity, and the subtype of '"
                                               + objs[0]->name() + "' has no name");
                }
            }
        }
        return V3VhdlStd::stringType();
    }
    if (!prefixTypep) prefixTypep = analyzeExpr(nodep->prefixp(), nullptr);
    checkSignalAttrOfOutPort(nodep, attrName);
    // Record what the prefix denotes, so that folding an attribute later does
    // not have to work it out again
    if (nodep->prefixp() && !nodep->prefixp()->vhdlTypep()) {
        nodep->prefixp()->vhdlTypep(prefixTypep);
    }

    const std::string& attr = nodep->name();
    // An access value is dereferenced without saying so where the attribute
    // asks about what it points at (8.1), so "l'left" on a LINE is the left
    // bound of the string it designates
    if (prefixTypep && prefixTypep->base()->kind() == VhdlTypeKind::ACCESS
        && prefixTypep->base()->designatedp()) {
        prefixTypep = prefixTypep->base()->designatedp();
    }
    const bool isArray = prefixTypep && prefixTypep->base()->kind() == VhdlTypeKind::ARRAY;
    if (prefixTypep && !prefixTypep->isError() && !m_inSpecialized) {
        checkAttrPrefix(nodep, prefixTypep, isArray);
    }

    // Attributes of an array ask about one of its index ranges; those of any
    // other type about the type's own range (16.2.3, 16.2.4)
    if (attr == "left" || attr == "right" || attr == "high" || attr == "low"
        || attr == "ascending" || attr == "length" || attr == "range"
        || attr == "reverse_range" || attr == "element") {
        // The argument of one of these is a dimension, not a value
        VhdlType* rangeTypep = prefixTypep;
        if (isArray) {
            rangeTypep = dimensionOf(nodep, prefixTypep);
            if (!rangeTypep) return V3VhdlStd::errorType();
        } else if (nodep->argp()) {
            analyzeExpr(nodep->argp(), V3VhdlStd::integerType());
        }
        if (attr == "element") {
            // A subtype may constrain the element as well as the index --
            // t(0 to 3)(7 downto 0) -- and it is that element the attribute
            // gives, not the base type's unconstrained one (16.2.3)
            if (!isArray) return V3VhdlStd::errorType();
            VhdlType* const elemp = prefixTypep->elementp() ? prefixTypep->elementp()
                                                            : prefixTypep->base()->elementp();
            return elemp ? elemp : V3VhdlStd::errorType();
        }
        if (attr == "length") return V3VhdlStd::integerType();
        if (attr == "ascending") return V3VhdlStd::booleanType();
        return rangeTypep ? rangeTypep : V3VhdlStd::errorType();
    }
    // Attributes taking a value of the prefix type, or giving one (16.2.3)
    if (attr == "pos" || attr == "succ" || attr == "pred" || attr == "leftof"
        || attr == "rightof") {
        if (nodep->argp()) analyzeExpr(nodep->argp(), prefixTypep);
        if (attr == "pos") return V3VhdlStd::integerType();
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (attr == "val") {
        if (nodep->argp()) analyzeExpr(nodep->argp(), V3VhdlStd::integerType());
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (attr == "image") {
        if (nodep->argp()) analyzeExpr(nodep->argp(), prefixTypep);
        return V3VhdlStd::stringType();
    }
    if (attr == "value") {
        if (nodep->argp()) analyzeExpr(nodep->argp(), V3VhdlStd::stringType());
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (nodep->argp()) analyzeExpr(nodep->argp(), nullptr);
    // The time 'DELAYED, 'STABLE and 'QUIET take is static: each makes an
    // implicit signal when the design is elaborated, and the time is part of
    // what that signal is (16.2.3)
    if (makesImplicitSignal(attr) && attr != "transaction" && nodep->argp() && !m_inSpecialized) {
        const VhdlSymbol* varyingp = nullptr;
        nodep->argp()->foreach([&](const AstVhdlName* namep) {
            const VhdlSymbol* const symp
                = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
            if (!varyingp && symp
                && (symp->kind() == VhdlSymKind::VARIABLE
                    || symp->kind() == VhdlSymKind::SHARED_VARIABLE
                    || symp->kind() == VhdlSymKind::SIGNAL || symp->kind() == VhdlSymKind::PORT
                    || symp->kind() == VhdlSymKind::PARAMETER
                    // A constant of a subprogram that does not fold is only
                    // as static as the parameters it was given
                    || (symp->kind() == VhdlSymKind::CONSTANT && !symp->value().known()
                        && symp->declp() && isSubprogramParam(symp->declp())))) {
                varyingp = symp;
            }
        });
        if (varyingp) {
            err(nodep->argp()->fileline(), "The time '" + attr
                                               + " is given must be static, but this one"
                                                 " depends on '"
                                               + varyingp->name() + "'");
        }
    }
    if (attr == "event" || attr == "active" || attr == "quiet" || attr == "stable"
        || attr == "driving") {
        return V3VhdlStd::booleanType();
    }
    if (attr == "instance_name" || attr == "path_name" || attr == "simple_name") {
        return V3VhdlStd::stringType();
    }
    // S'TRANSACTION is a BIT that toggles, whatever the type of S (16.2.4)
    if (attr == "transaction") return V3VhdlStd::bitType();
    if (attr == "last_value" || attr == "delayed" || attr == "driving_value"
        || attr == "base") {
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (attr == "last_event" || attr == "last_active") return V3VhdlStd::timeType();
    // 'SUBTYPE and 'ELEMENT name a subtype, so one written in an expression
    // is the type mark of a conversion -- "fmt'subtype(v)" (16.2.5, 9.3.6)
    if (attr == "subtype" || attr == "element") {
        VhdlType* const typep = resolveTypeMark(nodep);
        if (!typep || typep->isError()) return V3VhdlStd::errorType();
        if (nodep->argp()) analyzeExpr(nodep->argp(), nullptr);
        return typep;
    }
    // A user-defined attribute (7.2)
    std::vector<VhdlSymbol*> found;
    lookup(attr, found);
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() == VhdlSymKind::ATTRIBUTE) {
            nodep->vhdlSymp(symp);
            VhdlType* const typep = symp->typep();
            // A user-defined attribute holds an ordinary value, so where that
            // value is an array "obj'attr(i)" is one element of it rather
            // than an argument to anything (7.2, 8.4). The predefined
            // attributes that take an argument are answered above.
            if (nodep->argp() && typep && typep->base()->kind() == VhdlTypeKind::ARRAY) {
                const std::vector<VhdlType*>& idx = typep->indexTypes().empty()
                                                        ? typep->base()->indexTypes()
                                                        : typep->indexTypes();
                analyzeExpr(nodep->argp(), idx.empty() ? nullptr : idx[0]);
                VhdlType* const elemp
                    = typep->elementp() ? typep->elementp() : typep->base()->elementp();
                if (elemp) return elemp;
            }
            if (nodep->argp() && typep && !typep->isError() && !m_inSpecialized) {
                err(nodep->fileline(), "Attribute '" + attr + "' is of type " + typep->prettyName()
                                           + ", which is not an array, so it cannot be indexed");
                return V3VhdlStd::errorType();
            }
            return typep;
        }
    }
    err(nodep->fileline(), "Unknown attribute '" + attr + "'");
    return V3VhdlStd::errorType();
}

VhdlType* VhdlSemVisitor::analyzeAggregate(AstVhdlAggregate* nodep, VhdlType* wantp) {
    // "others" stands for whatever is left, so nothing can be left after it:
    // it must be the last choice of the last association (9.3.3.3)
    {
        bool sawOthers = false;
        for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
            // A positional element after it is as much after it as a named one
            if (sawOthers && !VN_AS(elemp, VhdlElemAssoc)->choicesp()) {
                err(elemp->fileline(), "Nothing can follow an 'others' choice in an aggregate");
                break;
            }
            for (AstNode* cp = VN_AS(elemp, VhdlElemAssoc)->choicesp(); cp; cp = cp->nextp()) {
                if (sawOthers) {
                    err(cp->fileline(), "Nothing can follow an 'others' choice in an aggregate");
                    sawOthers = false;  // One report is enough for this aggregate
                    break;
                }
                if (VN_AS(cp, VhdlChoice)->isOthers()) sawOthers = true;
            }
        }
    }
    // An aggregate takes its type entirely from the context (9.3.3)
    if (!wantp || wantp->isError() || !wantp->isComposite()) {
        for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
            analyzeExpr(VN_AS(elemp, VhdlElemAssoc)->valuep(), nullptr);
        }
        if (wantp && !wantp->isError()) {
            err(nodep->fileline(),
                "An aggregate needs a composite type here, but the context expects "
                    + wantp->prettyName());
        }
        return wantp ? wantp : V3VhdlStd::errorType();
    }
    if (wantp->kind() == VhdlTypeKind::ARRAY) {
        // A subtype may constrain the element as well as the index --
        // "t(0 to 3)(7 downto 0)" -- and it is that element the aggregate's
        // own elements have (5.3.3)
        VhdlType* elemTypep
            = wantp->elementp() ? wantp->elementp() : wantp->base()->elementp();
        if (wantp->base()->dimensions() > 1) {
            // In a multidimensional aggregate each element is itself an
            // aggregate covering the remaining dimensions (9.3.3.3), so hand
            // the elements an array type with one fewer index
            VhdlType* const subp = VhdlSymTab::newType(VhdlTypeKind::ARRAY, "",
                                                       nodep->fileline());
            // Where the aggregate's own type is constrained, its bounds are
            // what the remaining dimensions have; the base type's are the
            // index subtypes, which say nothing about how long a row is
            const bool constrained = wantp->isConstrainedArray();
            const std::vector<VhdlType*>& idx
                = constrained ? wantp->indexTypes() : wantp->base()->indexTypes();
            for (size_t i = 1; i < idx.size(); ++i) subp->addIndexType(idx[i]);
            subp->elementp(wantp->base()->elementp());
            subp->unconstrained(!constrained);
            elemTypep = subp;
        }
        // Every choice is an index value or a range of them (9.3.3.3). What
        // the choices must *cover* depends on the context, so that check
        // belongs to the caller: an aggregate standing as an operand takes
        // its index range from its own choices, not from the operand type.
        VhdlType* const indexTypep
            = wantp->indexTypes().empty() ? nullptr : wantp->indexTypes()[0];
        for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
            for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                if (chp->isOthers()) continue;
                if (AstVhdlRange* const rp = chp->rangep()) {
                    analyzeRange(rp, indexTypep);
                } else if (AstNodeVhdlExpr* const valuep = chp->valuep()) {
                    analyzeExpr(valuep, indexTypep);
                }
            }
            // An element may itself be an array of the same type, as in
            // "(others => (others => '0'))", so try the element type first
            VhdlType* hintp = elemTypep;
            // An element association may give the whole run at once -- a
            // choice that is a range, as in "(3 downto 0 => x\"C\")", or a
            // positional element as in "('1', x\"5a\")" -- and then the value
            // is of the aggregate's own type rather than of its element's
            // (9.3.3.3). Which one it is is settled by what the value could
            // be at all.
            const bool wholeRun
                = !eap->choicesp() || (eap->choicesp()->rangep() != nullptr);
            if (elemTypep && wholeRun && eap->valuep()) {
                CandSet cs;
                candidateTypes(eap->valuep(), cs);
                bool asElement = false;
                bool asWhole = false;
                for (VhdlType* const tp : cs.types) {
                    if (!tp) continue;
                    if (tp->base() == elemTypep->base()) asElement = true;
                    if (tp->base() == wantp->base()) asWhole = true;
                }
                // A string or bit-string literal names no type of its own,
                // only that it must be an array. Where the element is not an
                // array, the whole run is the only reading left.
                if (cs.types.empty() && cs.need == CandSet::Need::ARRAY
                    && elemTypep->base()->kind() != VhdlTypeKind::ARRAY) {
                    asWhole = true;
                } else if (cs.types.empty()) {
                    asElement = true;
                }
                // The aggregate's own type, but not its constraint: what the
                // value covers is as long as the value is, not as long as
                // the whole aggregate
                if (!asElement && asWhole) hintp = wantp->base();
            }
            analyzeExpr(eap->valuep(), hintp);
            if (!valueFitsType(eap->valuep(), hintp)) {
                err(eap->valuep()->fileline(),
                    "An element of this aggregate must be of type " + hintp->prettyName()
                        + ", but this one is of type " + eap->valuep()->vhdlTypep()->prettyName());
            }
            checkValueInRange(eap->valuep(), hintp, "an element of this aggregate");
            // An element that is itself an array is as long as the element
            // subtype (nvc 1.22.1 and Questa 2025.2; GHDL 7.0.0-dev warns)
            if (hintp == elemTypep && !VN_IS(eap->valuep(), VhdlAggregate)) {
                checkArrayValue(eap->valuep(), hintp, "an element of this aggregate");
            }
        }
        // Where the context imposes no constraint -- an aggregate standing as
        // an operand, or returned where the result subtype is unconstrained
        // -- the aggregate's own elements fix its index range (9.3.3.3), and
        // settling that here means everything downstream sees a real subtype
        if (!wantp->isConstrainedArray() && wantp->dimensions() == 1) {
            if (VhdlType* const ownp = aggregateSubtype(nodep, wantp)) wantp = ownp;
        }
        // Whatever the context, an index named by a choice must be a value of
        // the array's index subtype: "(0 => 'a')" is not a STRING, which is
        // indexed by POSITIVE, however the aggregate is used (9.3.3.3)
        VhdlType* const baseIndexp = wantp->base()->indexTypes().empty()
                                         ? nullptr
                                         : wantp->base()->indexTypes()[0];
        if (baseIndexp && wantp->dimensions() == 1) {
            for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
                for (AstNode* cp = VN_AS(elemp, VhdlElemAssoc)->choicesp(); cp; cp = cp->nextp()) {
                    AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                    if (chp->isOthers() || chp->rangep() || !chp->valuep()) continue;
                    checkChoiceInRange(chp->valuep()->fileline(),
                                       V3VhdlEval::eval(chp->valuep()), baseIndexp);
                }
            }
        }
        return wantp;
    }
    // A record aggregate: positional elements take the element types in order.
    // The subtype's own element list is the one to ask: a subtype may
    // constrain an element the base record leaves unconstrained (5.3.3).
    size_t pos = 0;
    const std::vector<VhdlSymbol*>& elems
        = wantp->elements().size() == wantp->base()->elements().size()
              ? wantp->elements()
              : wantp->base()->elements();
    // Which elements each association stands for: its position, the
    // elements its choices name, or for "others" every one no other
    // association names (9.3.3.2)
    std::vector<std::vector<VhdlSymbol*>> covers;
    std::set<const VhdlSymbol*> given;
    bool unknownChoice = false;
    for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
        AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
        covers.emplace_back();
        if (!eap->choicesp()) {
            if (pos < elems.size()) covers.back().push_back(elems[pos]);
            ++pos;
        }
        for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
            const AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            const AstVhdlName* const namep = VN_CAST(chp->valuep(), VhdlName);
            if (!namep) {
                if (!chp->isOthers()) unknownChoice = true;
                continue;
            }
            bool found = false;
            for (VhdlSymbol* const symp : elems) {
                if (symp->name() != namep->name()) continue;
                covers.back().push_back(symp);
                found = true;
            }
            if (!found) {
                unknownChoice = true;
                // A choice of a record aggregate names one of its elements
                // (9.3.3.2)
                if (!m_inSpecialized) {
                    err(namep->fileline(), "Record type " + wantp->prettyName()
                                               + " has no element '" + namep->name() + "'");
                }
            }
        }
        for (const VhdlSymbol* const symp : covers.back()) given.insert(symp);
    }
    size_t idx = 0;
    for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp(), ++idx) {
        AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
        std::vector<VhdlSymbol*>& mine = covers[idx];
        bool isOthers = false;
        for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
            if (VN_AS(cp, VhdlChoice)->isOthers()) isOthers = true;
        }
        if (isOthers) {
            for (VhdlSymbol* const symp : elems) {
                if (!given.count(symp)) mine.push_back(symp);
            }
            if (mine.empty() && !unknownChoice) {
                err(eap->fileline(), "'others' in a record aggregate must stand for at least one"
                                     " element, but every element is already given");
            }
        }
        VhdlType* const fieldp = mine.empty() ? nullptr : mine.front()->typep();
        // One value is given to all of them, so they are all of one type
        const VhdlSymbol* otherp = nullptr;
        for (const VhdlSymbol* const symp : mine) {
            if (fieldp && symp->typep() && symp->typep()->base() != fieldp->base()) otherp = symp;
        }
        if (otherp) {
            err(eap->fileline(), "Elements given one value together must be of one type, but '"
                                     + mine.front()->name() + "' is of type "
                                     + fieldp->prettyName() + " and '" + otherp->name()
                                     + "' of type " + otherp->typep()->prettyName());
            analyzeExpr(eap->valuep(), nullptr);
            continue;
        }
        analyzeExpr(eap->valuep(), fieldp);
        if (fieldp && !valueFitsType(eap->valuep(), fieldp)) {
            err(eap->valuep()->fileline(),
                "Element '" + mine.front()->name() + "' of this aggregate is of type "
                    + fieldp->prettyName() + ", but the value is of type "
                    + eap->valuep()->vhdlTypep()->prettyName());
            continue;
        }
        // Each element given the value gets it whole, so it is as long as
        // each of them -- which for "others" may differ element to element
        for (const VhdlSymbol* const symp : mine) {
            checkArrayValue(eap->valuep(), symp->typep(),
                            "element '" + symp->name() + "' of this aggregate");
        }
    }
    return wantp;
}

// Work out which formal each actual is for. Positional associations come
// first and take the formals in order; a named one names its formal. Any
// formal left without an actual must have a default (6.5.7.1). Returns false
// if the actuals cannot be bound to this subprogram at all.
// Whether a subprogram only reads the parameter of that name. A parameter it
// writes, or one of the signal class, has to be the object itself.
bool VhdlSemVisitor::formalIsReadOnly(const VhdlSymbol* symp, const std::string& formal) {
    if (!symp) return true;
    const AstVhdlSubprogDecl* declp = symp->bodyp();
    if (!declp) declp = VN_CAST(symp->declp(), VhdlSubprogDecl);
    if (!declp) return true;
    AstNode* p = declp->paramsp();
    for (int g = 0; p && g < declp->genericCount(); ++g) p = p->nextp();
    for (; p; p = p->nextp()) {
        const AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        if (ifp->name() != formal) continue;
        if (ifp->kind() == VhdlIfaceKind::SIGNAL || ifp->kind() == VhdlIfaceKind::FILE) {
            return false;
        }
        return ifp->mode() == VhdlMode::IN;
    }
    return true;
}

// "f(r.x => 1, r.y => 2)" and "p(v(0) => a, v(1) => b)" name the parts of one
// parameter one at a time (6.5.7.1). Together they cover it, so they say what
// the whole of it is: the associations for each such formal are replaced by
// one whose actual is the aggregate they describe. A part named for an "out"
// parameter would need the value copied back out again, and is left alone.
void VhdlSemVisitor::mergeIndividualFormals(AstVhdlCall* nodep) {
    mergeIndividualIn(nodep->argsp(), nodep, false);
}

// The same for an instance's generic map. A generic is only ever read, so
// there is nothing to carry back out and no need to ask.
void VhdlSemVisitor::mergeIndividualGenerics(AstVhdlInstance* nodep) {
    mergeIndividualIn(nodep->genericMapp(), nullptr, true);
}

void VhdlSemVisitor::mergeIndividualIn(AstNode* assocsp, AstVhdlCall* callp, bool alwaysIn) {
    // Which formal each association names, where it names part of one
    std::vector<std::string> rootOf;
    bool any = false;
    for (AstNode* argp = assocsp; argp; argp = argp->nextp()) {
        const AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc);
        std::string root;
        if (asp && asp->formalp() && !VN_IS(asp->formalp(), VhdlName)) {
            const AstNodeVhdlExpr* rootp = asp->formalp();
            while (rootp) {
                if (const AstVhdlSelected* const selp = VN_CAST(rootp, VhdlSelected)) {
                    rootp = selp->prefixp();
                } else if (const AstVhdlCall* const callp = VN_CAST(rootp, VhdlCall)) {
                    rootp = callp->prefixp();
                } else {
                    break;
                }
            }
            if (const AstVhdlName* const namep = VN_CAST(rootp, VhdlName)) {
                root = namep->name();
                any = true;
            }
        }
        rootOf.push_back(root);
    }
    if (!any) return;
    // A part named for a parameter the subprogram writes -- an "out" or a
    // signal -- would need the value carried back out again, which an
    // aggregate cannot do. Those are left as they were written.
    std::vector<VhdlSymbol*> cands;
    if (!alwaysIn && callp) {
        std::vector<std::string> parts;
        if (flatten(callp->prefixp(), parts) && !parts.empty()) lookup(parts.back(), cands);
    }
    // Each formal named in parts, in the order its first part was written
    std::vector<std::string> order;
    for (const std::string& root : rootOf) {
        if (root.empty()) continue;
        if (std::find(order.begin(), order.end(), root) == order.end()) order.push_back(root);
    }
    for (const std::string& root : order) {
        bool writable = false;
        for (const VhdlSymbol* const candp : cands) {
            if (!formalIsReadOnly(candp->resolved(), root)) writable = true;
        }
        if (writable) continue;
        AstVhdlAggregate* aggp = nullptr;
        AstVhdlAssoc* firstp = nullptr;
        size_t i = 0;
        bool ok = true;
        std::vector<AstVhdlAssoc*> parts;
        for (AstNode* argp = assocsp; argp; argp = argp->nextp(), ++i) {
            if (rootOf[i] != root) continue;
            AstVhdlAssoc* const asp = VN_AS(argp, VhdlAssoc);
            if (!asp->actualp()) ok = false;
            parts.push_back(asp);
        }
        if (!ok || parts.empty()) continue;
        for (AstVhdlAssoc* const asp : parts) {
            // What part of the formal this names: an element by name, or an
            // index. Only one step deep, which is what a formal part is.
            AstVhdlChoice* choicep = nullptr;
            if (const AstVhdlSelected* const selp = VN_CAST(asp->formalp(), VhdlSelected)) {
                const AstVhdlName* const sufp = VN_CAST(selp->suffixp(), VhdlName);
                if (sufp && VN_IS(selp->prefixp(), VhdlName)) {
                    choicep = new AstVhdlChoice{
                        asp->fileline(), new AstVhdlName{asp->fileline(), sufp->name()}};
                }
            } else if (AstVhdlCall* const callp = VN_CAST(asp->formalp(), VhdlCall)) {
                AstVhdlAssoc* const idxp = VN_CAST(callp->argsp(), VhdlAssoc);
                AstNodeVhdlExpr* const idxValp
                    = idxp ? idxp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
                if (idxValp && VN_IS(callp->prefixp(), VhdlName) && callp->argsp()
                    && !callp->argsp()->nextp()) {
                    choicep = new AstVhdlChoice{asp->fileline(),
                                                idxValp->cloneTree(false)};
                }
            }
            if (!choicep) {
                ok = false;
                break;
            }
            AstNodeVhdlExpr* const valuep = asp->actualp()->cloneTree(false);
            AstVhdlElemAssoc* const elemp
                = new AstVhdlElemAssoc{asp->fileline(), choicep, valuep};
            if (!aggp) {
                aggp = new AstVhdlAggregate{asp->fileline()};
                firstp = asp;
            }
            aggp->addElemsp(elemp);
        }
        if (!ok || !aggp) {
            if (aggp) VL_DO_DANGLING(aggp->deleteTree(), aggp);
            continue;
        }
        // The first of them becomes the whole association; the rest go
        for (AstVhdlAssoc* const asp : parts) {
            if (asp == firstp) continue;
            VL_DO_DANGLING(asp->unlinkFrBack()->deleteTree(), asp);
        }
        AstNodeVhdlExpr* const oldFormalp = firstp->formalp()->unlinkFrBack();
        VL_DO_DANGLING(oldFormalp->deleteTree(), oldFormalp);
        firstp->formalp(new AstVhdlName{firstp->fileline(), root});
        AstNodeVhdlExpr* const oldActualp = firstp->actualp()->unlinkFrBack();
        VL_DO_DANGLING(oldActualp->deleteTree(), oldActualp);
        firstp->actualp(aggp);
    }
}

bool VhdlSemVisitor::bindActuals(const VhdlSymbol* symp,
                                 const std::vector<AstVhdlAssoc*>& assocs,
                                 std::vector<int>& formalOf,
                                 std::vector<std::string>* elemOf) {
    return vhdlBindActuals(symp, assocs, formalOf, elemOf);
}

// One element of a record type, by name
static const VhdlSymbol* vhdlElementNamed(const VhdlType* typep, const std::string& name) {
    if (!typep || typep->base()->kind() != VhdlTypeKind::RECORD) return nullptr;
    for (const VhdlSymbol* const elemp : typep->base()->elements()) {
        if (elemp && elemp->name() == name) return elemp;
    }
    return nullptr;
}

// A formal whose elements a call names one at a time takes its bounds from
// those elements: "p(v(0) => s)" hands over a parameter of exactly one, and
// "p(v(1) => a, v(0) => b)" one of two. The indices must run consecutively --
// an association that names some but not all of a range leaves the rest with
// no actual, and no default can supply an element (6.5.7.1).
VhdlType* VhdlSemVisitor::constrainArrayFromIndices(VhdlType* typep,
                                                    const std::vector<int64_t>& indices,
                                                    FileLine* flp) {
    if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return nullptr;
    if (indices.empty() || typep->base()->indexTypes().empty()) return nullptr;
    std::vector<int64_t> at = indices;
    std::sort(at.begin(), at.end());
    for (size_t i = 1; i < at.size(); ++i) {
        if (at[i] == at[i - 1]) return nullptr;  // The same element twice over
        if (at[i] != at[i - 1] + 1) return nullptr;  // A gap has no actual
    }
    const VhdlType* const declIdxp
        = typep->indexTypes().empty() ? typep->base()->indexTypes()[0] : typep->indexTypes()[0];
    // Which way round the formal's own range runs, where its subtype says
    const bool ascending = declIdxp && declIdxp->boundsp() ? declIdxp->ascending() : false;
    VhdlType* const outp = cloneSubtype(typep, flp);
    VhdlType* const newIndexp
        = cloneSubtype(const_cast<VhdlType*>(typep->base()->indexTypes()[0]), flp);
    if (ascending) {
        newIndexp->setBounds(at.front(), at.back(), true);
    } else {
        newIndexp->setBounds(at.back(), at.front(), false);
    }
    outp->clearIndexTypes();
    outp->addIndexType(newIndexp);
    outp->unconstrained(false);
    return outp;
}

// The part of a formal an association element names, written so that one
// string serves both shapes: an element of a record by its name, and an
// element of an array as its index in parentheses, which no identifier is.
std::string vhdlIndexPartName(int64_t at) { return "(" + std::to_string(at) + ")"; }

bool vhdlIsIndexPart(const std::string& part) {
    return part.size() > 2 && part.front() == '(' && part.back() == ')';
}

int64_t vhdlIndexPartAt(const std::string& part) {
    return std::stoll(part.substr(1, part.size() - 2));
}

// Whether a visible "??" takes this type, which is what lets a value of it
// stand as a condition (9.2.9)
bool VhdlSemVisitor::hasConditionOperator(const VhdlType* typep) {
    if (!typep) return false;
    std::vector<VhdlSymbol*> found;
    lookup("??", found);
    for (const VhdlSymbol* const symp : found) {
        const VhdlSymbol* const fp = symp->resolved();
        if (fp->kind() != VhdlSymKind::FUNCTION) continue;
        if (fp->paramTypes().size() != 1 || !fp->paramTypes()[0]) continue;
        if (fp->paramTypes()[0]->base() == typep->base()) return true;
    }
    return false;
}

VhdlType* vhdlFormalTypeAt(const VhdlSymbol* symp, int idx, const std::string& elem) {
    if (!symp || idx < 0 || static_cast<size_t>(idx) >= symp->paramTypes().size()) return nullptr;
    VhdlType* const typep = symp->paramTypes()[idx];
    if (elem.empty()) return typep;
    if (vhdlIsIndexPart(elem)) {
        if (!typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return nullptr;
        return typep->elementp() ? typep->elementp() : typep->base()->elementp();
    }
    const VhdlSymbol* const elemp = vhdlElementNamed(typep, elem);
    return elemp ? elemp->typep() : nullptr;
}

// "p(r.x => a, r.y => b)" and "p(v(0) => a, v(1) => b)" each name the parts of
// one parameter one at a time: the parameter, and the element of it this
// association element names (6.5.7.1)
static bool vhdlIndividualFormal(const AstNodeVhdlExpr* formalp, std::string& root,
                                 std::string& elem) {
    if (const AstVhdlSelected* const selp = VN_CAST(formalp, VhdlSelected)) {
        const AstVhdlName* const prefixp = VN_CAST(selp->prefixp(), VhdlName);
        const AstVhdlName* const suffixp = VN_CAST(selp->suffixp(), VhdlName);
        if (!prefixp || !suffixp) return false;
        root = prefixp->name();
        elem = suffixp->name();
        return true;
    }
    // An indexed name of the formal, whose index must be static: which
    // element it is has to be known where the call is written
    const AstVhdlCall* const callp = VN_CAST(formalp, VhdlCall);
    if (!callp || !callp->argsp() || callp->argsp()->nextp()) return false;
    const AstVhdlName* const prefixp = VN_CAST(callp->prefixp(), VhdlName);
    if (!prefixp) return false;
    const AstVhdlAssoc* const idxp = VN_CAST(callp->argsp(), VhdlAssoc);
    AstNodeVhdlExpr* const valp
        = idxp ? idxp->actualp() : VN_CAST(callp->argsp(), NodeVhdlExpr);
    if (!valp) return false;
    const VhdlValue val = V3VhdlEval::eval(valp);
    if (!val.isInt()) return false;
    root = prefixp->name();
    elem = vhdlIndexPartName(val.toInt());
    return true;
}

// Which formal each actual of a call names: positional up to the first named
// one, by name after that, and a formal with a default may be left out
// (6.5.7.1). False where the call cannot be read that way at all.
bool vhdlBindActuals(const VhdlSymbol* symp, const std::vector<AstVhdlAssoc*>& assocs,
                     std::vector<int>& formalOf, std::vector<std::string>* elemOf) {
    const size_t formals = symp->paramTypes().size();
    formalOf.assign(assocs.size(), -1);
    if (elemOf) elemOf->assign(assocs.size(), std::string{});
    std::vector<bool> filled(formals, false);
    // A formal whose elements are associated one at a time is named once per
    // element, and each of its elements exactly once (6.5.7.1)
    std::map<size_t, std::set<std::string>> individual;
    bool named = false;
    for (size_t i = 0; i < assocs.size(); ++i) {
        AstVhdlAssoc* const asp = assocs[i];
        const AstVhdlName* const formalNamep
            = asp ? VN_CAST(asp->formalp(), VhdlName) : nullptr;
        if (!asp || !asp->formalp()) {
            // Positional, and no positional may follow a named one
            if (named || i >= formals) return false;
            formalOf[i] = static_cast<int>(i);
            filled[i] = true;
            continue;
        }
        named = true;
        if (!formalNamep) {
            // The elements of one formal, associated one at a time (6.5.7.1)
            std::string root;
            std::string elem;
            if (!vhdlIndividualFormal(asp->formalp(), root, elem)) {
                return false;  // A conversion, a slice or an indexed formal
            }
            const int idx = symp->paramIndex(root);
            if (idx < 0) return false;
            // A record element must be one the type has; an array element is
            // named by an index, and which indices there are is what the call
            // is deciding, so there is nothing yet to check it against
            if (vhdlIsIndexPart(elem)) {
                const VhdlType* const ftypep = symp->paramTypes()[idx];
                if (!ftypep || ftypep->base()->kind() != VhdlTypeKind::ARRAY) return false;
            } else if (!vhdlElementNamed(symp->paramTypes()[idx], elem)) {
                return false;
            }
            if (filled[idx] && !individual.count(idx)) return false;
            if (!individual[idx].insert(elem).second) return false;  // Twice over
            formalOf[i] = idx;
            if (elemOf) (*elemOf)[i] = elem;
            filled[idx] = true;
            continue;
        }
        const int idx = symp->paramIndex(formalNamep->name());
        if (idx < 0 || filled[idx]) return false;
        formalOf[i] = idx;
        filled[idx] = true;
    }
    // Every element of a formal associated individually must be there: what
    // is left out has no actual at all, and no default can supply it
    for (const auto& pair : individual) {
        const VhdlType* const typep = symp->paramTypes()[pair.first];
        if (!typep) return false;
        // An array formal takes its own bounds from the indices the call
        // names, so there is no element list for them to be measured against
        if (typep->base()->kind() == VhdlTypeKind::ARRAY) {
            if (pair.second.empty()) return false;
            continue;
        }
        if (typep->base()->elements().size() != pair.second.size()) return false;
    }
    for (size_t i = 0; i < formals; ++i) {
        if (!filled[i] && !symp->paramHasDefault(i)) return false;
    }
    return true;
}

// The simple name a block configuration's specification gives, "" if none:
// "for arch", "for blk", "for gen(1 to 3)"
static std::string blockSpecName(const AstVhdlBlockConfig* bcp) {
    const AstVhdlEntitySpec* const specp = VN_CAST(bcp->specp(), VhdlEntitySpec);
    if (!specp || !specp->namesp() || !specp->entityClass().empty()) return "";
    const AstNode* namep = specp->namesp();
    if (const AstVhdlCall* const callp = VN_CAST(namep, VhdlCall)) namep = callp->prefixp();
    const AstVhdlName* const simplep = VN_CAST(namep, VhdlName);
    return simplep ? lower(simplep->name()) : "";
}

// A block configuration names what it configures (3.4.2): the outermost one
// an architecture of the entity, one within it a block or a generate
// statement of the region it stands in, and one within a component
// configuration the architecture the binding names.
void VhdlSemVisitor::checkConfiguration(const AstVhdlConfiguration* cfgp, const std::string& lib,
                                        const std::string& ent) {
    const AstVhdlBlockConfig* const bcp = cfgp->blockConfigp();
    if (!bcp || !m_primaryUnits.count(lib + "." + ent)) return;
    const std::string arch = blockSpecName(bcp);
    if (arch.empty()) return;
    const auto it = m_archByKey.find(lib + "." + ent + "(" + arch + ")");
    if (it == m_archByKey.end()) {
        err(bcp->fileline(), "Entity '" + ent + "' has no architecture '" + arch
                                 + "' for this configuration to configure");
        return;
    }
    checkBlockConfigItems(bcp, it->second->stmtsp());
}

// The first name in these expressions that denotes a signal declared around a
// statement -- in a block, a generate body, an architecture or its entity's
// ports -- or "" if none does. A configuration is analyzed apart from the
// architecture it configures, so the names are found by where they are
// declared rather than by looking them up.
std::string VhdlSemVisitor::signalNamedIn(const AstNode* exprsp, const AstNode* stmtp) {
    const auto declaredSignal = [&](const std::string& name, const AstNode* declsp) -> int {
        for (const AstNode* dp = declsp; dp; dp = dp->nextp()) {
            const AstVhdlObjectDecl* const odp = VN_CAST(dp, VhdlObjectDecl);
            if (odp && lower(odp->name()) == name) return odp->kind() == VhdlObjKind::SIGNAL;
        }
        return -1;  // Not declared here
    };
    std::string found;
    for (const AstNode* ep = exprsp; ep && found.empty(); ep = ep->nextp()) {
        ep->foreach([&](const AstVhdlName* namep) {
            if (!found.empty()) return;
            const std::string name = lower(namep->name());
            for (const AstNode* upp = parentOf(stmtp); upp; upp = parentOf(upp)) {
                int sig = -1;
                if (const AstVhdlBlock* const blkp = VN_CAST(upp, VhdlBlock)) {
                    sig = declaredSignal(name, blkp->declsp());
                } else if (const AstVhdlGenerateBody* const gbp = VN_CAST(upp, VhdlGenerateBody)) {
                    sig = declaredSignal(name, gbp->declsp());
                } else if (const AstVhdlArchitecture* const archp
                           = VN_CAST(upp, VhdlArchitecture)) {
                    sig = declaredSignal(name, archp->declsp());
                    std::vector<std::string> parts;
                    if (sig < 0 && flatten(archp->entityNamep(), parts) && !parts.empty()) {
                        const std::string lib
                            = archp->libname().empty() ? "work" : lower(archp->libname());
                        const auto it = m_primaryUnits.find(lib + "." + lower(parts.back()));
                        const AstVhdlEntity* const entp = it == m_primaryUnits.end()
                                                              ? nullptr
                                                              : VN_CAST(it->second, VhdlEntity);
                        for (const AstNode* pp = entp ? entp->portsp() : nullptr; pp;
                             pp = pp->nextp()) {
                            if (lower(pp->name()) == name) sig = 1;
                        }
                    }
                }
                if (sig < 0) continue;
                if (sig > 0) found = namep->name();
                break;
            }
        });
    }
    return found;
}

void VhdlSemVisitor::checkBlockConfigItems(const AstVhdlBlockConfig* bcp, const AstNode* stmtsp) {
    for (const AstNode* ip = bcp->itemsp(); ip; ip = ip->nextp()) {
        const AstVhdlBlockConfig* const itemp = VN_CAST(ip, VhdlBlockConfig);
        if (!itemp) continue;
        const AstVhdlEntitySpec* const specp = VN_CAST(itemp->specp(), VhdlEntitySpec);
        if (specp && specp->entityClass() == "component") {
            // Within a component configuration a block configuration names
            // the architecture of the entity the component is bound to
            const AstVhdlBinding* const bindp = itemp->bindingp();
            if (!bindp || bindp->kind() != "entity") continue;
            // "entity work.e(rtl)" reads as a call until something says
            // otherwise, so the architecture may be its one argument
            std::string bound = lower(bindp->archName());
            if (const AstVhdlCall* const callp = VN_CAST(bindp->unitp(), VhdlCall)) {
                const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
                const AstVhdlName* const archp = asp && !asp->nextp() && !asp->formalp()
                                                     ? VN_CAST(asp->actualp(), VhdlName)
                                                     : nullptr;
                if (bound.empty() && archp) bound = lower(archp->name());
            }
            const AstNodeVhdlExpr* const entp = VN_IS(bindp->unitp(), VhdlCall)
                                                    ? VN_AS(bindp->unitp(), VhdlCall)->prefixp()
                                                    : bindp->unitp();
            std::vector<std::string> parts;
            std::string lib = m_libname;
            if (entp && flatten(entp, parts) && !parts.empty()) {
                if (parts.size() >= 2 && parts[0] != "work") lib = parts[0];
                // The entity it binds to is one the library holds (7.3.2.2)
                if (lib == m_libname && !m_primaryUnits.count(lib + "." + parts.back())) {
                    err(bindp->fileline(), "The binding names entity '" + parts.back()
                                               + "', which is not in library " + lib);
                    continue;
                }
            }
            if (bound.empty()) continue;
            // The architecture itself, whose statements it goes on to name
            const AstVhdlArchitecture* boundArchp = nullptr;
            if (!parts.empty()) {
                const auto it = m_archByKey.find(lib + "." + parts.back() + "(" + bound + ")");
                if (it != m_archByKey.end()) boundArchp = it->second;
            }
            for (const AstNode* np = itemp->itemsp(); np; np = np->nextp()) {
                const AstVhdlBlockConfig* const nestedp = VN_CAST(np, VhdlBlockConfig);
                const std::string name = nestedp ? blockSpecName(nestedp) : "";
                if (name.empty()) continue;
                if (name != bound) {
                    err(nestedp->fileline(), "This block configuration configures architecture '"
                                                 + bound + "', which the binding names, not '"
                                                 + name + "'");
                } else if (boundArchp) {
                    checkBlockConfigItems(nestedp, boundArchp->stmtsp());
                }
            }
            continue;
        }
        const std::string label = blockSpecName(itemp);
        if (label.empty()) continue;
        const AstNode* foundp = nullptr;
        for (const AstNode* sp = stmtsp; sp; sp = sp->nextp()) {
            if ((VN_IS(sp, VhdlBlock) || VN_IS(sp, VhdlGenerate)) && lower(sp->name()) == label) {
                foundp = sp;
                break;
            }
        }
        if (!foundp) {
            err(itemp->fileline(), "'" + label
                                       + "' is not a block or generate statement of the region"
                                         " this block configuration is in");
            continue;
        }
        if (const AstVhdlBlock* const blkp = VN_CAST(foundp, VhdlBlock)) {
            checkBlockConfigItems(itemp, blkp->stmtsp());
        } else if (const AstVhdlGenerate* const genp = VN_CAST(foundp, VhdlGenerate)) {
            // Which values of the parameter it configures is locally static
            // (3.4.2), so no signal is named there
            if (const AstVhdlCall* const callp = VN_CAST(specp->namesp(), VhdlCall)) {
                if (const std::string sig = signalNamedIn(callp->argsp(), genp); !sig.empty()) {
                    err(callp->fileline(), "The index specification of a block configuration is"
                                           " locally static, and '"
                                               + sig + "' is a signal");
                }
            }
            // Which alternative a block configuration of an if or case
            // generate reaches is not said by its label alone
            const AstVhdlGenerateBody* const bodyp = genp->altsp();
            if (bodyp && !bodyp->nextp()) checkBlockConfigItems(itemp, bodyp->stmtsp());
        }
    }
}

// The prefix of an indexed name or a slice is a name or a function call
// (8.4, 8.5): a qualified expression and a type conversion are neither. A
// type mark given a range or a subtype is a constraint rather than a
// conversion -- "mem_t(0 to 3)(7 downto 0)" -- and is left alone.
void VhdlSemVisitor::checkIndexablePrefix(const AstVhdlCall* nodep) {
    const AstNodeVhdlExpr* const prefp = nodep->prefixp();
    const bool isSlice = nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc);
    const char* const what = isSlice ? "a slice" : "an indexed name";
    if (VN_IS(prefp, VhdlQualified)) {
        err(prefp->fileline(), std::string{"The prefix of "} + what
                                   + " is a name or a function call, and a qualified"
                                     " expression is neither");
        return;
    }
    const AstVhdlCall* const innerp = VN_CAST(prefp, VhdlCall);
    if (!innerp || !innerp->prefixp()) return;
    const VhdlSymbol* const psymp = innerp->prefixp()->vhdlSymp();
    if (!psymp || psymp->kind() != VhdlSymKind::TYPE) return;
    const AstVhdlAssoc* const asp = VN_CAST(innerp->argsp(), VhdlAssoc);
    if (!asp || asp->nextp() || asp->formalp() || !asp->actualp()) return;
    const AstNodeVhdlExpr* const actp = asp->actualp();
    if (const AstVhdlAttrName* const attrp = VN_CAST(actp, VhdlAttrName)) {
        if (attrp->name() == "range" || attrp->name() == "reverse_range") return;
    }
    if (actp->vhdlSymp() && actp->vhdlSymp()->kind() == VhdlSymKind::TYPE) return;
    err(prefp->fileline(), std::string{"The prefix of "} + what
                               + " is a name or a function call, and a type conversion"
                                 " is neither");
}

VhdlType* VhdlSemVisitor::analyzeCall(AstVhdlCall* nodep, VhdlType* wantp) {
    // "prefix ( ... )" is a function call, an indexed name, a slice or a type
    // conversion, and only now can we tell which (8.1)
    std::vector<std::string> parts;
    std::vector<VhdlSymbol*> found;
    const bool namedPrefix = flatten(nodep->prefixp(), parts) && !parts.empty();
    if (namedPrefix) {
        VhdlScope* scopep = m_scopep;
        bool viaScope = parts.size() > 1;
        for (size_t i = 0; i + 1 < parts.size() && viaScope; ++i) {
            VhdlScope* const nextp = scopep ? scopep->findNamed(parts[i]) : nullptr;
            if (!nextp) {
                viaScope = false;
            } else {
                scopep = nextp;
            }
        }
        if (viaScope && scopep) {
            scopep->lookupLocal(parts.back(), found);
        } else {
            lookup(parts.back(), found);
            if (conflictingPotentials(found) && !m_inSpecialized) {
                err(nodep->prefixp()->fileline(),
                    "'" + parts.back()
                        + "' is declared by more than one package a use clause makes visible"
                          " here, and one of them cannot be overloaded, so neither is visible");
                for (AstNode* ap = nodep->argsp(); ap; ap = ap->nextp()) {
                    if (AstVhdlAssoc* const asp = VN_CAST(ap, VhdlAssoc)) {
                        if (asp->actualp()) analyzeExpr(asp->actualp(), nullptr);
                    }
                }
                return V3VhdlStd::errorType();
            }
        }
    }

    // A method of a protected type, called through an object of that type
    // (5.6.2). The object is recorded on the prefix, where lowering reads it.
    if (AstVhdlSelected* const selp = VN_CAST(nodep->prefixp(), VhdlSelected)) {
        VhdlScope* methScopep = nullptr;
        VhdlSymbol* const objp = protectedObject(selp->prefixp(), methScopep);
        const AstVhdlName* const sufp = objp ? VN_CAST(selp->suffixp(), VhdlName) : nullptr;
        if (sufp) {
            std::vector<VhdlSymbol*> methods;
            methScopep->lookupLocal(sufp->name(), methods);
            if (!methods.empty()) {
                found = methods;
                parts.clear();
                parts.push_back(sufp->name());
                selp->prefixp()->vhdlSymp(objp);
            }
        }
    }

    dropUninstantiated(found);
    // Whether this call is a statement of its own decides between a
    // procedure and a function of the same profile; the arguments below are
    // expressions, so it applies to this call alone
    const bool wantProc = m_procCall;
    m_procCall = false;
    if (!found.empty()) {
        std::vector<VhdlSymbol*> kept;
        for (VhdlSymbol* const symp : found) {
            const VhdlSymKind kind = symp->resolved()->kind();
            if (kind == VhdlSymKind::PROCEDURE && !wantProc) continue;
            if (kind == VhdlSymKind::FUNCTION && wantProc) continue;
            kept.push_back(symp);
        }
        if (!kept.empty()) found = kept;
    }

    // A type mark in this position makes it a type conversion (9.3.6) or a
    // constrained subtype indication that the parser could not tell apart
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() == VhdlSymKind::TYPE) {
            nodep->prefixp()->vhdlSymp(symp);
            int operands = 0;
            for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
                if (AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc)) {
                    // A conversion has one operand, whose type is found from
                    // it alone: not a value that takes its type from its
                    // context, which here would be circular (9.3.6)
                    const AstVhdlLiteral* const litp = VN_CAST(asp->actualp(), VhdlLiteral);
                    bool reported = true;
                    if (litp
                        && (litp->kind() == VhdlLitKind::STRING
                            || litp->kind() == VhdlLitKind::BITSTRING
                            || litp->kind() == VhdlLitKind::NULLLIT)) {
                        err(asp->actualp()->fileline(),
                            std::string{"The operand of a type conversion cannot be "}
                                + (litp->kind() == VhdlLitKind::NULLLIT ? "null"
                                                                        : "a string literal")
                                + ", since its type would come only from the conversion;"
                                  " qualify it instead: "
                                + symp->name() + "'(...)");
                    } else if (VN_IS(asp->actualp(), VhdlAggregate)
                               || VN_IS(asp->actualp(), VhdlAllocator)) {
                        err(asp->actualp()->fileline(),
                            std::string{"The operand of a type conversion cannot be "}
                                + (VN_IS(asp->actualp(), VhdlAggregate) ? "an aggregate"
                                                                        : "an allocator")
                                + ", since its type would come only from the conversion;"
                                  " qualify it instead: "
                                + symp->name() + "'(...)");
                    } else {
                        reported = false;
                    }
                    if (asp->actualp()) ++operands;
                    if (operands == 2) {
                        err(asp->fileline(),
                            "A type conversion has exactly one operand, but this one to "
                                + symp->typep()->prettyName() + " is given more");
                    }
                    analyzeExpr(asp->actualp(), nullptr);
                    // Only between closely related types (9.3.6)
                    // "t(v'range)" is a subtype written where a call could be
                    const AstVhdlAttrName* const rangeAttrp
                        = VN_CAST(asp->actualp(), VhdlAttrName);
                    const bool isRange = rangeAttrp
                                         && (rangeAttrp->name() == "range"
                                             || rangeAttrp->name() == "reverse_range");
                    const VhdlType* const fromp = asp->actualp()->vhdlTypep();
                    // A copy of an instantiated package says again what its
                    // template said, with the actual types in the generics' place
                    if (fromp && (!fromp->isError() || fromp->isGenericFormal())
                        && !m_inSpecialized && !m_inPackageInstance && !isRange && !reported
                        && !closelyRelated(fromp, symp->typep())) {
                        err(asp->actualp()->fileline(),
                            "A type conversion changes a value between closely related types,"
                            " and "
                                + fromp->prettyName() + " and " + symp->typep()->prettyName()
                                + " are not");
                    }
                    // The result of a conversion must belong to the subtype
                    // the type mark names (9.3.6)
                    checkArrayValue(asp->actualp(), symp->typep(),
                                    "the conversion to " + symp->typep()->prettyName());
                }
            }
            const std::string what = "the conversion to " + symp->typep()->prettyName();
            const VhdlValue val = V3VhdlEval::eval(nodep);
            if (val.known() && !V3VhdlEval::inRange(val, symp->typep())) {
                err(nodep->fileline(), "Value " + val.prettyName()
                                           + " is outside the range of " + what + ", which is "
                                           + rangeText(symp->typep()));
            }
            // A conversion between closely related array types keeps the
            // operand's length (9.3.6), so "unsigned(v)" is as long as v even
            // though UNSIGNED itself is unconstrained
            return convertedType(symp->typep(), VN_CAST(nodep->argsp(), VhdlAssoc));
        }
    }

    // "f(r.x => 1, r.y => 2)" associates the parts of one parameter one at a
    // time (6.5.7.1). Together they are the whole of it, so they are gathered
    // into the aggregate they describe and the call has one actual again.
    mergeIndividualFormals(nodep);

    // Gather what each actual could be, without settling any of them, so the
    // whole set can filter the overloads (12.5)
    std::vector<CandSet> argCands;
    std::vector<AstVhdlAssoc*> assocs;
    for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
        argCands.emplace_back();
        AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc);
        assocs.push_back(asp);
        if (asp) {
            candidateTypes(asp->actualp(), argCands.back());
        } else {
            argCands.back().types.push_back(nullptr);  // A discrete range, in a slice
        }
    }
    // The first candidate of each, for diagnostics
    std::vector<VhdlType*> argTypes;
    for (const auto& cands : argCands) {
        argTypes.push_back(cands.types.empty() ? nullptr : cands.types[0]);
    }

    // A prefix that is not itself a name -- "p.all", or the result of another
    // call as in CORDIC(...)(1) -- denotes no declaration of its own, so its
    // type has to be worked out before it can be indexed (8.4)
    if (!namedPrefix) {
        VhdlType* typep = analyzeExpr(nodep->prefixp(), nullptr);
        if (!m_inSpecialized) checkIndexablePrefix(nodep);
        // An access value is dereferenced without saying so where what it
        // points at is what the name goes on to index (8.1)
        if (typep && typep->base()->kind() == VhdlTypeKind::ACCESS
            && typep->base()->designatedp()) {
            typep = typep->base()->designatedp();
        }
        if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY) {
            const bool isSlice = (nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc));
            return isSlice ? analyzeSliceName(nodep, typep) : analyzeIndexedName(nodep, typep);
        }
        if (typep && typep->isError()) return typep;
    }

    // An object prefix makes it an indexed name or a slice (8.4, 8.5).
    // Only where the prefix is a simple name: the search above looked the
    // last identifier of the prefix up in the enclosing region, and for
    // "r.data(0)" that finds whatever else is called "data" here rather than
    // the record's element. A selected prefix denotes what 8.3 says it
    // denotes, and the search below asks it.
    for (VhdlSymbol* const symp : VN_IS(nodep->prefixp(), VhdlSelected)
                                      ? std::vector<VhdlSymbol*>{}
                                      : found) {
        // An alias of an object is indexed like the object; an alias of a
        // subprogram is a call, which the search below settles
        if (!symp->isObject()) {
            if (symp->kind() != VhdlSymKind::ALIAS) continue;
            const VhdlSymKind rootKind = symp->resolved()->kind();
            if (rootKind == VhdlSymKind::FUNCTION || rootKind == VhdlSymKind::PROCEDURE) {
                continue;
            }
        }
        VhdlType* typep = symp->typep();
        if (typep && typep->kind() == VhdlTypeKind::ACCESS) typep = typep->designatedp();
        if (!typep || typep->kind() != VhdlTypeKind::ARRAY) continue;
        nodep->prefixp()->vhdlSymp(symp);
        // The prefix denotes the object, so it has the object's type; the
        // lowering reads it there to know the array's index range
        if (!nodep->prefixp()->vhdlTypep()) nodep->prefixp()->vhdlTypep(symp->typep());
        nodep->vhdlSymp(symp);
        // A slice keeps the array type; an index yields one element
        const bool isSlice = (nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc));
        return isSlice ? analyzeSliceName(nodep, typep) : analyzeIndexedName(nodep, typep);
    }

    // A prefix that is a selected name may denote an object the search above
    // does not reach -- an element of a record, or a name in another design
    // unit -- and indexing it is an indexed name or a slice (8.4, 8.5)
    if (namedPrefix && VN_IS(nodep->prefixp(), VhdlSelected)) {
        bool callable = false;
        // "r.elem(a to b)" of a record object selects its element, whatever
        // function of that name a search by simple name also finds (UVVM's
        // cov_bin.bin_vector beside its function bin_vector)
        bool ofRecord = false;
        if (parts.size() == 2) {
            std::vector<VhdlSymbol*> heads;
            lookup(parts[0], heads);
            for (const VhdlSymbol* const headp : heads) {
                const VhdlSymbol* const realp = headp->resolved();
                if (realp->isOverloadable() || !realp->typep()) continue;
                const VhdlSymKind kind = realp->kind();
                if (kind == VhdlSymKind::TYPE || kind == VhdlSymKind::PACKAGE) continue;
                ofRecord = realp->typep()->base()->kind() == VhdlTypeKind::RECORD;
                break;
            }
        }
        for (const VhdlSymbol* const symp : found) {
            if (ofRecord) break;
            // An alias of a subprogram is called like the subprogram (6.6.3),
            // so what it denotes is what says whether this is a call -- the
            // search above the loop asks the same question the same way
            const VhdlSymKind kind = symp->resolved()->kind();
            if (kind == VhdlSymKind::FUNCTION || kind == VhdlSymKind::PROCEDURE) {
                callable = true;
            }
        }
        if (!callable) {
            VhdlType* typep = analyzeExpr(nodep->prefixp(), nullptr);
            if (typep && typep->kind() == VhdlTypeKind::ACCESS) typep = typep->designatedp();
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY) {
                const bool isSlice = (nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc));
                return isSlice ? analyzeSliceName(nodep, typep) : analyzeIndexedName(nodep, typep);
            }
            if (typep && typep->isError()) return typep;
        }
    }

    // Otherwise a subprogram call, resolved by its arguments and its context
    std::vector<VhdlSymbol*> cands;
    std::vector<int> genOf;  // How many of each candidate's matched parameters
                             // were written with a generic type's name
    std::vector<int> formalOf;
    std::vector<std::string> elemOf;
    int best = -1;
    // Resolution is bidirectional: a subprogram overloads on its result as
    // well as on its arguments (12.5). So a candidate whose result is the
    // type the context asked for outranks one whose arguments merely match
    // more exactly -- otherwise "abs"(10) assigned to an enumeration means
    // the predefined operator on universal_integer, whose argument fits
    // without conversion, and the design's own is never considered.
    int bestFits = -1;
    // A discrete range is no actual of any subprogram, so a name given one
    // is a slice, of an object or of what a call with no arguments returns
    const bool anyRange
        = std::any_of(assocs.begin(), assocs.end(), [](const AstVhdlAssoc* asp) { return !asp; });
    for (VhdlSymbol* const symp : found) {
        // An alias of a subprogram is called like the subprogram it names
        const VhdlSymKind kind = symp->resolved()->kind();
        if (kind != VhdlSymKind::FUNCTION && kind != VhdlSymKind::PROCEDURE) continue;
        if (anyRange) continue;
        if (!bindActuals(symp, assocs, formalOf, &elemOf)) continue;
        bool ok = true;
        int score = 0;
        int viaGeneric = 0;
        for (size_t i = 0; i < argCands.size() && ok; ++i) {
            if (formalOf[i] < 0) continue;
            VhdlType* const paramp = vhdlFormalTypeAt(symp, formalOf[i], elemOf[i]);
            if (!anyMatches(argCands[i], paramp)) ok = false;
            if (exactMatch(argCands[i], paramp)) ++score;
            // A parameter written with the name of a generic type matched
            // nothing else where the unit was analyzed, so where a call could
            // mean either, it means the one that does not need it (4.7).
            // Counted apart from the score: such a parameter is a good match,
            // and where it is the only candidate it is the answer.
            if (symp->resolved()->paramFromGeneric(formalOf[i])) ++viaGeneric;
        }
        if (!ok) continue;
        VhdlType* const resultp = symp->returnTypep() ? symp->returnTypep() : symp->typep();
        const int fits = (wantp && !wantp->isError() && typesMatch(resultp, wantp)) ? 1 : 0;
        if (fits > bestFits || (fits == bestFits && score > best)) {
            bestFits = fits;
            best = score;
            cands.clear();
            genOf.clear();
        }
        if (fits == bestFits && score == best) {
            cands.push_back(symp);
            genOf.push_back(viaGeneric);
        }
    }
    // Among equals, the one that needs no parameter written with a generic
    // type's name -- but only for a call written *inside* the instantiated
    // unit, which the LRM analyzes against the formal generic type (4.7). A
    // call written where the package was instantiated sees the profiles the
    // instantiation made, and two of them alike is an ambiguous call there.
    if (m_inPackageInstance && cands.size() > 1 && cands.size() == genOf.size()) {
        int least = genOf[0];
        for (const int g : genOf) least = std::min(least, g);
        std::vector<VhdlSymbol*> keep;
        for (size_t i = 0; i < cands.size(); ++i) {
            if (genOf[i] == least) keep.push_back(cands[i]);
        }
        cands.swap(keep);
    }
    if (cands.empty()) {
        // A function whose parameters all have defaults may be called by
        // naming it (4.2.2.1), and then a following parenthesis indexes the
        // result rather than passing arguments: "cfg.get_val(3)" where
        // get_val takes nothing and returns an array (8.1).
        // Two such functions leave the name ambiguous, whatever follows it
        std::set<const VhdlSymbol*> noArgs;
        for (VhdlSymbol* const symp : found) {
            const VhdlSymbol* const fp = symp->resolved();
            if (fp->kind() != VhdlSymKind::FUNCTION) continue;
            if (!fp->paramTypes().empty() && !allParamsDefaulted(fp)) continue;
            const VhdlType* const retp = fp->returnTypep();
            if (retp && retp->base()->kind() == VhdlTypeKind::ARRAY)
                noArgs.insert(aliasRoot(symp));
        }
        if (noArgs.size() > 1 && !m_inSpecialized && !parts.empty()) {
            err(nodep->fileline(), "'" + parts.back() + "' names " + std::to_string(noArgs.size())
                                       + " functions called with no arguments, and nothing"
                                         " here decides which one's result this indexes");
            return V3VhdlStd::errorType();
        }
        for (VhdlSymbol* const symp : found) {
            if (symp->resolved()->kind() != VhdlSymKind::FUNCTION) continue;
            VhdlSymbol* const fp = symp->resolved();
            if (!fp->paramTypes().empty() && !allParamsDefaulted(fp)) continue;
            VhdlType* const retp = fp->returnTypep();
            if (!retp || retp->base()->kind() != VhdlTypeKind::ARRAY) continue;
            AstNodeVhdlExpr* const oldPrefixp = nodep->prefixp()->unlinkFrBack();
            AstVhdlCall* const innerp = new AstVhdlCall{oldPrefixp->fileline(), oldPrefixp};
            innerp->vhdlSymp(fp);
            innerp->vhdlTypep(retp);
            nodep->prefixp(innerp);
            const bool isSlice = (nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc));
            return isSlice ? analyzeSliceName(nodep, retp) : analyzeIndexedName(nodep, retp);
        }
        for (const VhdlType* const typep : argTypes) {
            if (typep && typep->isError()) return V3VhdlStd::errorType();
        }
        const std::string name = parts.empty() ? std::string{"<name>"} : parts.back();
        // Where exactly one declaration has the right formals, say which
        // argument it was that did not fit -- "of these types" on its own
        // leaves seven named associations to be checked by hand
        std::string detail;
        int arityOk = 0;
        for (VhdlSymbol* const symp : found) {
            const VhdlSymKind kind = symp->resolved()->kind();
            if (kind != VhdlSymKind::FUNCTION && kind != VhdlSymKind::PROCEDURE) continue;
            if (!bindActuals(symp, assocs, formalOf, &elemOf)) continue;
            ++arityOk;
            if (arityOk > 1) {
                detail.clear();
                break;
            }
            for (size_t i = 0; i < argCands.size(); ++i) {
                if (formalOf[i] < 0) continue;
                VhdlType* const paramp = vhdlFormalTypeAt(symp, formalOf[i], elemOf[i]);
                if (anyMatches(argCands[i], paramp)) continue;
                detail = ": argument '" + symp->paramName(formalOf[i])
                         + (elemOf[i].empty() ? "" : "." + elemOf[i]) + "' expects "
                         + (paramp ? paramp->prettyName() : std::string{"<unknown>"});
                if (i < argTypes.size() && argTypes[i]) {
                    detail += ", not " + argTypes[i]->prettyName();
                    // Two types of the same name is what a design that
                    // declares one STD.STANDARD also declares looks like, and
                    // saying the name twice tells no one anything (12.4)
                    if (paramp && paramp->prettyName() == argTypes[i]->prettyName()) {
                        detail += " -- two different types of that name, one at "
                                  + std::string{paramp->base()->fileline()->ascii()} + " and one at "
                                  + std::string{argTypes[i]->base()->fileline()->ascii()};
                    }
                }
                break;
            }
        }
        if (detail.empty() && arityOk > 1) {
            // Several declarations take that many arguments, so name the one
            // argument none of them accepts where there is exactly one such
            for (size_t i = 0; i < argCands.size() && detail.empty(); ++i) {
                bool anyOk = false;
                std::set<std::string> wanted;
                for (VhdlSymbol* const symp : found) {
                    const VhdlSymKind kind = symp->resolved()->kind();
                    if (kind != VhdlSymKind::FUNCTION && kind != VhdlSymKind::PROCEDURE) continue;
                    if (!bindActuals(symp, assocs, formalOf, &elemOf)) continue;
                    if (formalOf[i] < 0) continue;
                    VhdlType* const paramp = vhdlFormalTypeAt(symp, formalOf[i], elemOf[i]);
                    if (anyMatches(argCands[i], paramp)) {
                        anyOk = true;
                        break;
                    }
                    if (paramp) wanted.insert(paramp->prettyName());
                }
                if (anyOk || wanted.empty()) continue;
                detail = ": argument " + std::to_string(i + 1) + " is ";
                detail += (i < argTypes.size() && argTypes[i]) ? argTypes[i]->prettyName()
                                                              : std::string{"of no known type"};
                detail += ", and every declaration wants ";
                std::string list;
                for (const std::string& w : wanted) list += (list.empty() ? "" : " or ") + w;
                detail += list + " there";
            }
        }
        err(nodep->fileline(), "No visible subprogram '" + name + "' takes "
                                   + std::to_string(argCands.size())
                                   + " argument(s) of these types" + detail);
        for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
            if (AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc)) {
                analyzeExpr(asp->actualp(), nullptr);
            }
        }
        return V3VhdlStd::errorType();
    }
    if (cands.size() > 1) {
        // An argument whose own type is already in error settles nothing, and
        // saying the call is ambiguous on top of the mistake that made it so
        // is noise rather than news
        for (const VhdlType* const typep : argTypes) {
            if (!typep || !typep->isError()) continue;
            nodep->vhdlSymp(cands[0]);
            return cands[0]->returnTypep() ? cands[0]->returnTypep()
                                           : V3VhdlStd::errorType();
        }
    }
    VhdlType* const resultp
        = pick(nodep, cands, wantp,
               "subprogram '" + (parts.empty() ? std::string{"<name>"} : parts.back()) + "'");
    // The chosen signature now settles every actual
    if (nodep->vhdlSymp() && bindActuals(nodep->vhdlSymp(), assocs, formalOf, &elemOf)) {
        for (size_t i = 0; i < assocs.size(); ++i) {
            if (!assocs[i] || !assocs[i]->actualp() || formalOf[i] < 0) continue;
            VhdlType* const paramp = vhdlFormalTypeAt(nodep->vhdlSymp(), formalOf[i], elemOf[i]);
            analyzeExpr(assocs[i]->actualp(), paramp);
            if (elemOf[i].empty()) {
                checkActualClass(nodep->vhdlSymp(), formalOf[i], assocs[i]->actualp());
            }
            // A parameter written against an unconstrained array takes its
            // index range from the actual (5.3.2.2). Where the actual is a
            // value rather than an object -- a string literal, an aggregate,
            // a qualified expression -- nothing else has worked its bounds
            // out, and without them the call cannot be specialized on its
            // length and so cannot be emitted at all.
            if (paramp && paramp->base()->kind() == VhdlTypeKind::ARRAY
                && paramp->unconstrained()) {
                VhdlType* const actualp = assocs[i]->actualp()->vhdlTypep();
                if (!actualp || !actualp->isConstrainedArray()) {
                    VhdlType* const boundp = constrainFromValue(paramp, assocs[i]->actualp(),
                                                               assocs[i]->fileline());
                    if (boundp && boundp->isConstrainedArray()) {
                        assocs[i]->actualp()->vhdlTypep(boundp);
                    }
                }
            }
            // An actual must belong to the parameter's subtype (4.2.2.1)
            if (!nodep->vhdlSymp()->predefined()) {
                const std::string what = "parameter " + std::to_string(formalOf[i] + 1) + " of '"
                                         + nodep->vhdlSymp()->name() + "'";
                checkValueInRange(assocs[i]->actualp(), paramp, what);
                checkArrayValue(assocs[i]->actualp(), paramp, what);
            }
        }
    }
    // Some functions leave their result subtype open and then say the length
    // in the call itself. numeric_std's TO_SIGNED and TO_UNSIGNED take it as
    // an argument, RESIZE too, and the shifts and rotates give a result as
    // long as what they were handed. The frontend emits these as native
    // operations rather than compiling their bodies, so nothing else works
    // the length out -- and without it a call that passes the result on
    // cannot be specialized, and so cannot be emitted at all.
    if (VhdlType* const sizedp = intrinsicResultType(nodep->vhdlSymp(), assocs, resultp)) {
        return sizedp;
    }
    return resultp;
}

// Whether a length written as an expression is one the declaration it sizes
// can carry: it may name a port's bounds or a generic, which elaboration
// settles, but not a variable, whose value is not known until it runs
static bool sizeTravels(AstNodeVhdlExpr* nodep) {
    if (!nodep) return false;
    bool ok = true;
    nodep->foreach([&](AstNodeVhdlExpr* refp) {
        const VhdlSymbol* const symp = refp->vhdlSymp();
        if (!symp) return;
        switch (symp->kind()) {
        case VhdlSymKind::VARIABLE:
        case VhdlSymKind::SHARED_VARIABLE:
        case VhdlSymKind::PARAMETER:
        case VhdlSymKind::LOOP_PARAMETER: ok = false; break;
        default: break;
        }
    });
    return ok;
}

// The result subtype of one of the functions above, or null where this is
// not one of them or the length does not fold
VhdlType* VhdlSemVisitor::intrinsicResultType(VhdlSymbol* symp,
                                              const std::vector<AstVhdlAssoc*>& assocs,
                                              VhdlType* resultp) {
    if (!symp || !resultp) return nullptr;
    // Only where the frontend really is what carries the operation out. A
    // package that declares a RESIZE of its own means its own thing by it,
    // and its result is whatever its body returns (see V3VhdlStd)
    if (!V3VhdlStd::isIntrinsic(symp)) return nullptr;
    if (resultp->base()->kind() != VhdlTypeKind::ARRAY || resultp->isConstrainedArray()) {
        return nullptr;
    }
    if (resultp->base()->indexTypes().size() != 1) return nullptr;
    const std::string& name = symp->name();
    int64_t length = -1;
    bool fromOne = false;
    // An operand's own index range, where the result is numbered as it is
    const VhdlType* keepIndexp = nullptr;  // The result is numbered "1 to length", not down from it
    AstNodeVhdlExpr* sizeExprp = nullptr;  // Where the length is not a number here
    if (name == "to_signed" || name == "to_unsigned" || name == "resize") {
        if (assocs.size() != 2 || !assocs[1] || !assocs[1]->actualp()) return nullptr;
        const VhdlValue val = V3VhdlEval::eval(assocs[1]->actualp());
        if (val.isInt()) {
            length = val.toInt();
        } else if (sizeTravels(assocs[1]->actualp())) {
            // "to_signed(v, y'length)" where y is a port whose bounds the
            // instance settles: the length is not a number here, but it is
            // an expression the back end folds per instance
            sizeExprp = assocs[1]->actualp();
        } else {
            // "resize(arg, size_res)" says the length with another array
            const VhdlType* const sizep = assocs[1]->actualp()->vhdlTypep();
            if (!sizep || !sizep->isConstrainedArray()) return nullptr;
            length = sizep->dimLength(0);
        }
    } else if (name == "maximum" || name == "minimum") {
        // Of two arrays the answer is one of the two (5.3.2.4), so where they
        // are the same length that is the answer's length -- and where they
        // are not, which operand won is a question for running and the value
        // carries its own bounds instead
        if (assocs.size() != 2) return nullptr;
        int64_t agreed = -1;
        for (const AstVhdlAssoc* const asp : assocs) {
            if (!asp || !asp->actualp()) return nullptr;
            const VhdlType* const argp = asp->actualp()->vhdlTypep();
            if (!argp || !argp->isConstrainedArray() || argp->dimensions() != 1) return nullptr;
            const int64_t len = argp->dimLength(0);
            if (len < 0) return nullptr;
            if (agreed < 0) {
                agreed = len;
            } else if (agreed != len) {
                return nullptr;
            }
        }
        length = agreed;
        // The answer is one of the two operands, so it is numbered the way
        // an operand is and not the way numeric_std numbers a result
        keepIndexp = assocs[0]->actualp()->vhdlTypep()->indexTypes()[0];
    } else if (name == "shift_left" || name == "shift_right" || name == "rotate_left"
               || name == "rotate_right") {
        if (assocs.empty() || !assocs[0] || !assocs[0]->actualp()) return nullptr;
        const VhdlType* const argp = assocs[0]->actualp()->vhdlTypep();
        if (!argp || !argp->isConstrainedArray()) return nullptr;
        length = argp->dimLength(0);
    } else if (name == "to_01" || name == "to_bitvector" || name == "to_stdlogicvector"
               || name == "to_stdulogicvector" || name == "to_x01" || name == "to_ux01") {
        // These give back what they were handed, element for element, so the
        // result is as long as the argument. The frontend carries them out
        // itself, so nothing else measures the result -- and a value of no
        // length cannot be passed on to a subprogram written against an
        // unconstrained array, since the copy made for that call site is what
        // gives the parameter a width.
        //
        // How the result is numbered is each declaration's own business, and
        // the two families in the IEEE bodies do not agree: TO_01 and the
        // conversions between vector types count down from the length less
        // one, while TO_X01 and TO_UX01 count up from one.
        if (assocs.empty() || !assocs[0] || !assocs[0]->actualp()) return nullptr;
        const VhdlType* const argp = assocs[0]->actualp()->vhdlTypep();
        if (!argp || !argp->isConstrainedArray()) return nullptr;
        length = argp->dimLength(0);
        fromOne = name == "to_x01" || name == "to_ux01";
    } else {
        return nullptr;
    }
    if (!sizeExprp && (length < 0 || length > (1 << 20))) return nullptr;
    const VhdlType* const idxp
        = keepIndexp ? keepIndexp : resultp->base()->indexTypes()[0];
    if (!idxp) return nullptr;
    FileLine* const flp = resultp->fileline();
    VhdlType* const outp = cloneSubtype(resultp, flp);
    VhdlType* const newIndexp = cloneSubtype(const_cast<VhdlType*>(idxp), flp);
    if (keepIndexp) {
        outp->clearIndexTypes();
        outp->addIndexType(newIndexp);
        outp->unconstrained(false);
        return outp;
    }
    if (sizeExprp) {
        // numeric_std counts a vector down from its length less one (16.5),
        // written out so that elaboration works it out
        AstVhdlLiteral* const onep = new AstVhdlLiteral{flp, VhdlLitKind::INTEGER, "1"};
        onep->intVal(1);
        AstVhdlLiteral* const zerop = new AstVhdlLiteral{flp, VhdlLitKind::INTEGER, "0"};
        zerop->intVal(0);
        AstNodeVhdlExpr* const leftp = new AstVhdlBinary{
            flp, VhdlBinaryOp::SUB, sizeExprp->cloneTree(false), onep};
        // The range is what the expression says, not the index subtype's own
        newIndexp->unbound();
        newIndexp->boundsp(new AstVhdlRange{flp, leftp, VhdlDir::DOWNTO, zerop});
    } else if (fromOne) {
        newIndexp->setBounds(1, length, true);
    } else {
        // numeric_std counts a vector down from its length less one (16.5)
        newIndexp->setBounds(length - 1, 0, false);
    }
    outp->clearIndexTypes();
    outp->addIndexType(newIndexp);
    outp->unconstrained(false);
    return outp;
}

// A delay may not be negative (10.5.2.1, 10.2)
void VhdlSemVisitor::checkDelay(AstNodeVhdlExpr* nodep, const char* what) {
    if (!nodep) return;
    const VhdlValue val = V3VhdlEval::eval(nodep);
    if (!val.isInt() || val.toInt() >= 0) return;
    err(nodep->fileline(),
        std::string{"A "} + what + " may not be negative, but this one is "
            + val.prettyName());
}

// The elements of a waveform take effect in the order written, so their times
// must strictly increase and only the first may be immediate (10.5.2.1)
void VhdlSemVisitor::checkWaveform(AstNode* wavesp) {
    bool haveLast = false;
    int64_t last = 0;
    for (AstNode* wp = wavesp; wp; wp = wp->nextp()) {
        AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
        checkDelay(wfp->afterp(), "delay");
        int64_t here = 0;
        if (wfp->afterp()) {
            const VhdlValue val = V3VhdlEval::eval(wfp->afterp());
            if (!val.isInt()) return;  // Not static: nothing can be said
            here = val.toInt();
        }
        if (haveLast && here <= last) {
            err(wfp->fileline(), "This waveform element takes effect no later than the one "
                                 "before it, so it would never be seen");
            return;
        }
        haveLast = true;
        last = here;
    }
}

// What is assigned must belong to the target's subtype: in range for a
// scalar, and of the right length for a constrained array (10.5.2.2)
void VhdlSemVisitor::checkAssignedValue(AstNodeVhdlExpr* nodep, VhdlType* targetp) {
    if (m_unreached) return;  // A branch a settled condition closed off
    // Where the type is wrong, what its value or its length would be says
    // nothing more about it
    if (!checkAssignedType(nodep, targetp)) return;
    checkValueInRange(nodep, targetp, "the assignment target");
    checkArrayValue(nodep, targetp, "the assignment target");
}

// What is assigned must be of the target's type (10.5.2.1, 10.6.2). A literal
// or an aggregate was handed the target's type to be analyzed with and so
// already has it; what this catches is a value that has a type of its own and
// a different one. The universal types are the exception the language makes:
// they convert to any type of their own class (9.3.6).
bool VhdlSemVisitor::checkAssignedType(AstNodeVhdlExpr* nodep, VhdlType* targetp) {
    if (valueFitsType(nodep, targetp)) return true;
    err(nodep->fileline(), "Assignment target is of type " + targetp->prettyName()
                               + ", but the value is of type " + nodep->vhdlTypep()->prettyName());
    return false;
}

// Whether a value's type is the one a context requires, allowing what the
// language allows in place of it
bool VhdlSemVisitor::valueFitsType(AstNodeVhdlExpr* nodep, VhdlType* targetp) {
    if (m_inSpecialized) return true;  // The template was checked already
    if (!nodep || !targetp || targetp->isError()) return true;
    VhdlType* const valuep = nodep->vhdlTypep();
    if (!valuep || valuep->isError()) return true;
    if (valuep->base() == targetp->base()) return true;
    const VhdlTypeKind want = targetp->base()->kind();
    switch (valuep->base()->kind()) {
    case VhdlTypeKind::UNIVERSAL_INTEGER:
        if (want == VhdlTypeKind::INTEGER || want == VhdlTypeKind::PHYSICAL) return true;
        break;
    case VhdlTypeKind::UNIVERSAL_REAL:
        if (want == VhdlTypeKind::FLOATING || want == VhdlTypeKind::PHYSICAL) return true;
        break;
    // An incomplete type is one whose full declaration has not been seen, and
    // a protected type is reached through a name that is not its own
    case VhdlTypeKind::INCOMPLETE:
    case VhdlTypeKind::PROTECTED: return true;
    default: break;
    }
    if (want == VhdlTypeKind::INCOMPLETE || want == VhdlTypeKind::PROTECTED) return true;
    // "f(i)" where every parameter of f has a default reads two ways: a call
    // with one argument, or the defaulted call's result indexed by i (4.2.2.1,
    // 8.4). This analyzer only takes the first, so where the second would fit
    // the target it says nothing rather than reporting a type that another
    // reading of the same text does not have. nvc 1.22.1 and GHDL 7.0.0-dev
    // both accept such a design; Questa 2025.2 refuses it.
    return indexedCallWouldFit(nodep, valuep, targetp);
}

bool VhdlSemVisitor::indexedCallWouldFit(const AstNodeVhdlExpr* nodep, const VhdlType* valuep,
                                         const VhdlType* targetp) {
    const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall);
    if (!callp || !callp->argsp()) return false;
    if (valuep->base()->kind() != VhdlTypeKind::ARRAY) return false;
    const VhdlSymbol* const symp = callp->vhdlSymp() ? callp->vhdlSymp()->resolved() : nullptr;
    if (!symp || symp->kind() != VhdlSymKind::FUNCTION) return false;
    if (!allParamsDefaulted(symp)) return false;
    const VhdlType* const elemp
        = valuep->elementp() ? valuep->elementp() : valuep->base()->elementp();
    return elemp && elemp->base() == targetp->base();
}

void VhdlSemVisitor::analyzeChoice(AstVhdlChoice* chp, VhdlType* wantp) {
    // A choice is a value, a discrete range or "others" (9.3.3.1)
    if (!chp || chp->isOthers()) return;
    if (AstVhdlRange* const rp = chp->rangep()) {
        analyzeRange(rp, wantp);
        // A range of choices must lie within the subtype it chooses from
        if (rp->leftp()) checkValueInRange(rp->leftp(), wantp, "this choice");
        if (rp->rightp()) checkValueInRange(rp->rightp(), wantp, "this choice");
        return;
    }
    AstNodeVhdlExpr* const valuep = chp->valuep();
    if (!valuep) return;
    analyzeExpr(valuep, wantp);
    checkValueInRange(valuep, wantp, "this choice");
}

// One index per dimension, each within its own index range (8.4)
VhdlType* VhdlSemVisitor::analyzeIndexedName(AstVhdlCall* nodep, VhdlType* arrayp) {
    const std::vector<VhdlType*>& indexes = arrayp->indexTypes();
    size_t dim = 0;
    for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp(), ++dim) {
        AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc);
        if (!asp || !asp->actualp()) continue;
        VhdlType* const wantp = dim < indexes.size() ? indexes[dim] : nullptr;
        analyzeExpr(asp->actualp(), wantp);
        checkValueInRange(asp->actualp(), wantp, "the index of " + arrayp->prettyName());
    }
    if (!indexes.empty() && dim != indexes.size()) {
        err(nodep->fileline(), "Expected " + std::to_string(indexes.size()) + " index(es) for "
                                   + arrayp->prettyName() + ", but " + std::to_string(dim)
                                   + " were given");
    }
    // A subtype may constrain the element as well as the index --
    // "t(0 to 3)(7 downto 0)" -- and it is that element an index gives (5.3.3)
    return arrayp->elementp() ? arrayp->elementp() : arrayp->base()->elementp();
}

// A slice is of the array's type, constrained by the slice's own range, which
// must run the same way as the array's and lie within it (8.5)
VhdlType* VhdlSemVisitor::analyzeSliceName(AstVhdlCall* nodep, VhdlType* arrayp) {
    // A slice is a run of elements of a one-dimensional array, named by one
    // discrete range: a block of a matrix is not a slice of it
    if (nodep->argsp() && nodep->argsp()->nextp()) {
        err(nodep->fileline(), "A slice is named by one discrete range, but this one is"
                               " given several");
    } else if (arrayp->base()->indexTypes().size() > 1) {
        err(nodep->fileline(),
            "Only a one-dimensional array can be sliced, but " + arrayp->prettyName() + " has "
                + std::to_string(arrayp->base()->indexTypes().size()) + " dimensions");
    }
    VhdlType* const indexp = arrayp->indexTypes().empty() ? nullptr : arrayp->indexTypes()[0];
    const RangeInfo info = analyzeRange(nodep->argsp(), indexp);
    VhdlType* const outp = cloneSubtype(arrayp, nodep->fileline());
    outp->clearIndexTypes();
    VhdlType* sliceIndexp = info.typep ? info.typep : indexp;
    if (info.left.known() && info.right.known()) {
        sliceIndexp = makeRangeSubtype(info, nodep->fileline());
        // Only an array whose own range is known can say which way a slice
        // of it must run: an unconstrained type carries the index subtype,
        // and the object decides the direction, not the subtype (8.5)
        if (indexp && indexp->bounded() && arrayp->isConstrainedArray()
            && !sliceIndexp->isNull()) {
            if (info.ascending != indexp->ascending()) {
                err(nodep->fileline(), "Slice runs "
                                           + std::string{info.ascending ? "'to'" : "'downto'"}
                                           + " but " + arrayp->prettyName() + " is indexed "
                                           + (indexp->ascending() ? "'to'" : "'downto'"));
            } else if (!m_inSpecialized) {
                checkSubrange(sliceIndexp, indexp, nodep->fileline());
            }
        }
    } else if (sliceIndexp) {
        sliceIndexp = cloneSubtype(sliceIndexp, nodep->fileline());
        sliceIndexp->unbound();
        // A slice whose bounds did not fold is still that long: a bound
        // naming a generic is fixed once the instance is elaborated, so the
        // range is kept as written for the back end to fold then (8.5).
        // Only where every name in them is a generic: that value arrives
        // with the instance, where a variable's or a loop parameter's
        // arrives only while running.
        if (AstVhdlRange* const rp = VN_CAST(nodep->argsp(), VhdlRange)) {
            bool onlyGenerics = true;
            bool anyName = false;
            rp->foreach([&](AstVhdlName* namep) {
                const VhdlSymbol* const nsymp
                    = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
                anyName = true;
                if (!nsymp || nsymp->kind() != VhdlSymKind::GENERIC) onlyGenerics = false;
            });
            if (info.valid && !info.unbounded && rp->leftp() && rp->rightp() && anyName
                && onlyGenerics) {
                sliceIndexp->boundsp(rp);
            }
        }
    }
    outp->addIndexType(sliceIndexp ? sliceIndexp : V3VhdlStd::integerType());
    outp->unconstrained(!sliceIndexp || !sliceIndexp->bounded());
    // A dimension whose range is there but did not fold is not an open one:
    // it says how long the slice is, in terms an instance settles (8.5)
    if (sliceIndexp && !sliceIndexp->bounded() && sliceIndexp->boundsp()) {
        outp->openDimsKnown();
    }
    return outp;
}

// The number of elements a value has, where that is statically known, or -1
int64_t VhdlSemVisitor::staticLength(AstNodeVhdlExpr* nodep) {
    if (!nodep) return -1;
    if (AstVhdlAggregate* const aggp = VN_CAST(nodep, VhdlAggregate)) {
        return aggregateLength(aggp);
    }
    // "t'(...)" states a type, and the value inside is what has the length
    if (AstVhdlQualified* const qp = VN_CAST(nodep, VhdlQualified)) {
        return staticLength(qp->valuep());
    }
    // A concatenation is as long as its two operands together, counting an
    // operand of the element type as one element (9.2.5). That is what fixes
    // the length of "constant k : bit_vector := a & b", where neither the
    // subtype indication nor either operand alone says how long it is.
    if (AstVhdlBinary* const binp = VN_CAST(nodep, VhdlBinary)) {
        if (binp->op() == VhdlBinaryOp::CONCAT) {
            VhdlType* const arrayp = nodep->vhdlTypep();
            VhdlType* const elemp
                = arrayp && arrayp->base() ? arrayp->base()->elementp() : nullptr;
            const auto sideLength = [&](AstNodeVhdlExpr* sidep) -> int64_t {
                VhdlType* const sidetp = sidep ? sidep->vhdlTypep() : nullptr;
                if (elemp && sidetp && sidetp->base() == elemp->base()) return 1;
                return staticLength(sidep);
            };
            const int64_t lhsLen = sideLength(binp->lhsp());
            if (lhsLen < 0) return -1;
            const int64_t rhsLen = sideLength(binp->rhsp());
            if (rhsLen < 0) return -1;
            return lhsLen + rhsLen;
        }
    }
    const VhdlValue val = V3VhdlEval::eval(nodep);
    if (val.isStr()) return static_cast<int64_t>(val.toStr().size());
    VhdlType* const typep = nodep->vhdlTypep();
    if (typep && typep->isConstrainedArray() && typep->dimensions() == 1) {
        return typep->dimLength(0);
    }
    return -1;
}

// How many elements one positional association supplies. Usually one, but
// VHDL-2008 lets a value of the aggregate's own type stand for a whole slice
// of it, so that (c6, 4) is a four-element integer_vector when c6 has three
// (9.3.3.3).
int64_t VhdlSemVisitor::elementSpan(AstNodeVhdlExpr* valuep, VhdlType* arrayTypep) {
    if (!valuep || !arrayTypep) return 1;
    VhdlType* const typep = valuep->vhdlTypep();
    if (!typep || typep->base() != arrayTypep->base()) return 1;
    return staticLength(valuep);
}

// The subtype a type conversion yields: the type mark's own, unless that is
// an unconstrained array and the operand has a length to lend it (9.3.6)
VhdlType* VhdlSemVisitor::convertedType(VhdlType* top, AstVhdlAssoc* argp) {
    if (!top || top->base()->kind() != VhdlTypeKind::ARRAY || top->isConstrainedArray()) {
        return top;
    }
    if (!argp || !argp->actualp()) return top;
    VhdlType* const fromp = argp->actualp()->vhdlTypep();
    if (!fromp || fromp->base()->kind() != VhdlTypeKind::ARRAY
        || fromp->indexTypes().size() != top->base()->indexTypes().size()) {
        return top;
    }
    if (!fromp->isConstrainedArray()) {
        // A length that is not a number here but is fixed once the instance
        // is elaborated transfers just as a static one does
        for (const VhdlType* const idxp : fromp->indexTypes()) {
            if (!idxp || !idxp->boundsp()) return top;
        }
    }
    VhdlType* const outp = cloneSubtype(top, argp->fileline());
    outp->clearIndexTypes();
    for (VhdlType* const idxp : fromp->indexTypes()) outp->addIndexType(idxp);
    outp->unconstrained(false);
    return outp;
}

// The subtype an aggregate gives itself, or null where its own elements do
// not settle one -- which is what an "others" choice means (9.3.3.3)
VhdlType* VhdlSemVisitor::aggregateSubtype(AstVhdlAggregate* nodep, VhdlType* typep) {
    VhdlType* const indexp
        = typep->base()->indexTypes().empty() ? nullptr : typep->base()->indexTypes()[0];
    if (!indexp || !indexp->bounded()) return nullptr;
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
            if (span < 0) return nullptr;
            positional += span;
        }
        for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            if (chp->isOthers()) return nullptr;
            named = true;
            int64_t lo = 0;
            int64_t hi = 0;
            if (AstVhdlRange* const rp = chp->rangep()) {
                const RangeInfo info = analyzeRange(rp, indexp);
                if (!info.left.isInt() || !info.right.isInt()) return nullptr;
                lo = std::min(info.left.toInt(), info.right.toInt());
                hi = std::max(info.left.toInt(), info.right.toInt());
                // A choice written as a range says which way the aggregate's
                // own index runs, whatever the index subtype does (9.3.3.3)
                if (!sawRange) {
                    sawRange = true;
                    rangeAscending = info.left.toInt() <= info.right.toInt();
                }
            } else {
                const VhdlValue v = V3VhdlEval::eval(chp->valuep());
                if (!v.isInt()) return nullptr;
                lo = hi = v.toInt();
            }
            lowest = std::min(lowest, lo);
            highest = std::max(highest, hi);
        }
    }
    const bool ascending = sawRange ? rangeAscending : indexp->ascending();
    int64_t left = 0;
    int64_t right = 0;
    if (named) {
        if (lowest > highest) return nullptr;
        left = ascending ? lowest : highest;
        right = ascending ? highest : lowest;
    } else {
        if (positional <= 0) return nullptr;
        left = indexp->left();
        right = ascending ? left + positional - 1 : left - positional + 1;
    }
    VhdlType* const outp = cloneSubtype(typep, nodep->fileline());
    VhdlType* const newIndexp = cloneSubtype(indexp, nodep->fileline());
    newIndexp->setBounds(left, right, ascending);
    outp->clearIndexTypes();
    outp->addIndexType(newIndexp);
    outp->unconstrained(false);
    return outp;
}

// How many elements an aggregate has, or -1 where "others" or a non-static
// choice makes that undecidable. Named choices span from the lowest to the
// highest, which is not the same as counting them (9.3.3.3).
int64_t VhdlSemVisitor::aggregateLength(AstVhdlAggregate* nodep) {
    int64_t positional = 0;
    bool hasNamed = false;
    int64_t lowest = INT64_MAX;
    int64_t highest = INT64_MIN;
    for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
        AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
        if (!eap->choicesp()) {
            const int64_t span = elementSpan(eap->valuep(), nodep->vhdlTypep());
            if (span < 0) return -1;
            positional += span;
        }
        for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            if (chp->isOthers()) return -1;
            hasNamed = true;
            int64_t lo = 0;
            int64_t hi = 0;
            if (AstVhdlRange* const rp = chp->rangep()) {
                const RangeInfo info = analyzeRange(rp, nullptr);
                if (!info.left.isInt() || !info.right.isInt()) return -1;
                lo = info.ascending ? info.left.toInt() : info.right.toInt();
                hi = info.ascending ? info.right.toInt() : info.left.toInt();
                if (lo > hi) continue;
            } else {
                if (!chp->valuep()) return -1;
                const VhdlValue val = V3VhdlEval::eval(chp->valuep());
                if (!val.isInt()) return -1;
                lo = hi = val.toInt();
            }
            lowest = std::min(lowest, lo);
            highest = std::max(highest, hi);
        }
    }
    if (!hasNamed) return positional;
    if (lowest > highest) return 0;  // Every choice was a null range
    return highest - lowest + 1;
}

// An object declared with an unconstrained array subtype takes its constraint
// from its initial value (5.3.2.2, 6.4.2.2). Until that is done the object has
// no length, and neither has anything computed from its attributes.
// Whether a record type has no element left open. A record with an element
// of an unconstrained array type is not a type an object can have until the
// value it is given says how long that element is (5.3.3).
bool VhdlSemVisitor::fullyConstrainedRecord(const VhdlType* typep) {
    if (!typep || typep->base()->kind() != VhdlTypeKind::RECORD) return false;
    for (const VhdlSymbol* const ep : typep->elements()) {
        const VhdlType* const etp = ep->typep();
        if (!etp) continue;
        if (etp->base()->kind() == VhdlTypeKind::ARRAY && etp->unconstrained()) return false;
        if (etp->base()->kind() == VhdlTypeKind::RECORD && !fullyConstrainedRecord(etp)) {
            return false;
        }
    }
    return true;
}

// A record whose elements are constrained by the value an object of it is
// given. VHDL-2008 lets a record element be an unconstrained array, and then
// the object's own value is what says how long it is (5.3.3).
VhdlType* VhdlSemVisitor::constrainRecordFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                                   FileLine* flp) {
    // The subtype's own elements, not the base type's: a subtype written
    // "rec_t(x(1 to 5))" has already said how long x is, and an initial value
    // does not get to say it again (5.3.3)
    const std::vector<VhdlSymbol*>& elems
        = typep->elements().size() == typep->base()->elements().size()
              ? typep->elements()
              : typep->base()->elements();
    bool anyOpen = false;
    for (VhdlSymbol* const ep : elems) {
        if (ep->typep() && ep->typep()->base()->kind() == VhdlTypeKind::ARRAY
            && ep->typep()->unconstrained()) {
            anyOpen = true;
        }
    }
    if (!anyOpen) return typep;
    // Another value of the same record type hands over its element subtypes
    AstVhdlAggregate* const aggp = VN_CAST(valuep, VhdlAggregate);
    if (!aggp) {
        VhdlType* const valueTypep = valuep->vhdlTypep();
        if (valueTypep && valueTypep->base() == typep->base()
            && valueTypep->elements().size() == elems.size()) {
            bool settled = true;
            for (VhdlSymbol* const ep : valueTypep->elements()) {
                if (ep->typep() && ep->typep()->base()->kind() == VhdlTypeKind::ARRAY
                    && ep->typep()->unconstrained()) {
                    settled = false;
                }
            }
            if (settled) return valueTypep;
        }
        return typep;
    }
    // Which value each element is given, positionally or by name
    std::vector<AstNodeVhdlExpr*> byPos(elems.size(), nullptr);
    size_t positional = 0;
    for (AstVhdlElemAssoc* p = aggp->elemsp(); p; p = VN_AS(p->nextp(), VhdlElemAssoc)) {
        if (!p->choicesp()) {
            if (positional < elems.size()) byPos[positional] = p->valuep();
            ++positional;
            continue;
        }
        for (AstNode* cp = p->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            const AstVhdlName* const namep
                = chp->valuep() ? VN_CAST(chp->valuep(), VhdlName) : nullptr;
            if (!namep) continue;
            for (size_t i = 0; i < elems.size(); ++i) {
                if (elems[i]->name() == namep->name()) byPos[i] = p->valuep();
            }
        }
    }
    return constrainRecordFromParts(typep, byPos, flp);
}

// A record subtype whose open elements take their bounds from the values
// given for them, one per element in declaration order (5.3.3)
VhdlType* VhdlSemVisitor::constrainRecordFromParts(VhdlType* typep,
                                                   const std::vector<AstNodeVhdlExpr*>& byPos,
                                                   FileLine* flp) {
    const std::vector<VhdlSymbol*>& elems = typep->elements();
    VhdlType* outp = nullptr;
    for (size_t i = 0; i < elems.size() && i < byPos.size(); ++i) {
        VhdlType* const elemTypep = elems[i]->typep();
        if (!elemTypep || !byPos[i]) continue;
        if (elemTypep->base()->kind() != VhdlTypeKind::ARRAY || !elemTypep->unconstrained()) {
            continue;
        }
        // An element the record's own declaration constrained is not open,
        // even where the bound it was given names a generic and so does not
        // fold until the instance does (5.3.3). Taking its bounds from the
        // value instead would make an object of the type a different
        // structure from the type itself, and the two would no longer be
        // assignable to one another.
        if (elemTypep->lengthEmittable()) continue;
        VhdlType* const newTypep = constrainFromValue(elemTypep, byPos[i], flp);
        if (newTypep == elemTypep) continue;
        if (!outp) outp = cloneSubtype(typep, flp);
        VhdlSymbol* const newSymp = VhdlSymTab::newSymbol(
            VhdlSymKind::RECORD_ELEMENT, elems[i]->name(), elems[i]->declp());
        newSymp->typep(newTypep);
        newSymp->position(elems[i]->position());
        outp->replaceElement(i, newSymp);
    }
    return outp ? outp : typep;
}

VhdlType* VhdlSemVisitor::constrainFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                             FileLine* flp) {
    if (!typep || !valuep) return typep;
    // A record may have an element of an unconstrained array type, and that
    // element takes its bounds from the value the object is given (5.3.3)
    if (typep->base()->kind() == VhdlTypeKind::RECORD) {
        return constrainRecordFromValue(typep, valuep, flp);
    }
    if (typep->base()->kind() != VhdlTypeKind::ARRAY) return typep;
    // An array of arrays may have its index constrained and its element not
    // -- "t_slv_array(0 to n-1)" over an element type of std_logic_vector --
    // and then the element takes its bounds from the value too (5.3.2.2)
    if (!typep->unconstrained()) return constrainElementFromValue(typep, valuep, flp);
    // A value that is itself constrained hands over its index ranges
    // directly, whether it says its length as a number or as an expression
    // elaboration settles
    VhdlType* const valueTypep = valuep->vhdlTypep();
    if (valueTypep && valueTypep->base()->kind() == VhdlTypeKind::ARRAY
        && !valueTypep->unconstrained() && valueTypep->lengthEmittable()
        && valueTypep->dimensions() == typep->dimensions()) {
        VhdlType* const outp = cloneSubtype(typep, flp);
        outp->clearIndexTypes();
        for (VhdlType* const idxp : valueTypep->indexTypes()) outp->addIndexType(idxp);
        outp->unconstrained(false);
        return outp;
    }
    // Otherwise the value's length decides, starting at the index subtype's
    // own left bound (9.3.3.3). A value of more than one dimension says how
    // long each of them is: the outer level fixes the first, and its first
    // element the rest.
    const size_t dims = typep->dimensions();
    if (dims < 1) return typep;
    std::vector<int64_t> lengths;
    AstNodeVhdlExpr* levelp = valuep;
    for (size_t d = 0; d < dims; ++d) {
        const int64_t len = staticLength(levelp);
        if (len < 0) return typep;
        lengths.push_back(len);
        if (d + 1 == dims) break;
        AstVhdlAggregate* const aggp = VN_CAST(levelp, VhdlAggregate);
        AstVhdlElemAssoc* const firstp = aggp ? aggp->elemsp() : nullptr;
        if (!firstp || !firstp->valuep()) return typep;
        levelp = firstp->valuep();
    }
    std::vector<VhdlType*> newIndexes;
    for (size_t d = 0; d < dims; ++d) {
        VhdlType* const indexp = typep->indexTypes()[d];
        if (!indexp || !indexp->bounded()
            || indexp->base()->kind() == VhdlTypeKind::FLOATING) {
            return typep;
        }
        VhdlType* const newIndexp = cloneSubtype(indexp, flp);
        const int64_t left = indexp->left();
        const int64_t len = lengths[d];
        newIndexp->setBounds(left, indexp->ascending() ? left + len - 1 : left - len + 1,
                             indexp->ascending());
        newIndexes.push_back(newIndexp);
    }
    VhdlType* const outp = cloneSubtype(typep, flp);
    outp->clearIndexTypes();
    for (VhdlType* const idxp : newIndexes) outp->addIndexType(idxp);
    outp->unconstrained(false);
    return outp;
}

// The element of an array whose index is constrained but whose element is not
VhdlType* VhdlSemVisitor::constrainElementFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                                   FileLine* flp) {
    if (typep->lengthEmittable()) return typep;
    VhdlType* const elemp = typep->elementp() ? typep->elementp() : typep->base()->elementp();
    if (!elemp || elemp->base()->kind() != VhdlTypeKind::ARRAY || elemp->lengthEmittable()) {
        return typep;
    }
    VhdlType* const valueTypep = valuep->vhdlTypep();
    if (!valueTypep || valueTypep->base()->kind() != VhdlTypeKind::ARRAY) return typep;
    VhdlType* const valElemp
        = valueTypep->elementp() ? valueTypep->elementp() : valueTypep->base()->elementp();
    if (!valElemp || !valElemp->lengthEmittable()) return typep;
    VhdlType* const outp = cloneSubtype(typep, flp);
    outp->elementp(valElemp);
    return outp;
}

// A constant that an elaboration constant's value names has to be one too: a
// generic an instantiation supplied is emitted as a parameter, and a
// parameter's value may only name other parameters (4.9)
void VhdlSemVisitor::spreadElabConstants(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    // A constant the back end cannot fold is left alone: making it a
    // parameter would only move the failure to the parameter's value. What
    // hands a generic such a value is handed it as data instead, which
    // lowering decides.
    const auto markIn = [](AstNode* exprp, bool& changed, bool foldableOnly = false) {
        exprp->foreach([&](AstNodeVhdlExpr* refp) {
            const VhdlSymbol* const symp = refp->vhdlSymp();
            if (!symp || symp->kind() != VhdlSymKind::CONSTANT) return;
            if (foldableOnly) {
                const AstVhdlObjectDecl* const valuep = VN_CAST(symp->declp(), VhdlObjectDecl);
                if (!valuep || !V3VhdlEval::backEndFolds(valuep->defaultp())) return;
            }
            AstVhdlObjectDecl* const otherp = VN_CAST(symp->declp(), VhdlObjectDecl);
            // A deferred constant is written twice: once in the package with
            // no value and once in the body with one. Both say the same
            // constant, so both have to know it is settled by elaboration.
            AstVhdlObjectDecl* const fullp = VN_CAST(symp->fullDeclp(), VhdlObjectDecl);
            if ((!otherp || otherp->isElabConstant()) && (!fullp || fullp->isElabConstant())) {
                return;
            }
            if (otherp) otherp->elabConstant(true);
            if (fullp) fullp->elabConstant(true);
            changed = true;
        });
    };
    // A constant a constraint names decides how long an object is, and the
    // back end folds a length rather than reading one, so such a constant is
    // settled by elaboration too. Only a constraint: a loop's range and an
    // aggregate's choice are read while running, and a constant named there
    // may as well stay a variable -- which matters, because a constant whose
    // value is a call on a protected object cannot fold at all.
    {
        bool ignored = false;
        for (AstNodeVhdlUnit* const unitp : unitsp) {
            // The whole indication, not the constraint alone: an index
            // constraint is written the way a call is -- "bit_vector(n - 1
            // downto 0)" -- and the parser could not tell the two apart, so
            // the bounds arrive as the type mark's arguments
            unitp->foreach([&](AstVhdlSubtypeInd* indp) { markIn(indp, ignored); });
            unitp->foreach([&](AstVhdlTypeDecl* declp) { markIn(declp, ignored); });
            // What an instantiation hands a generic becomes a parameter's
            // value, so a constant named there is settled by elaboration as
            // well -- a record of settings passed down a hierarchy is the
            // usual shape (6.5.6.2)
            unitp->foreach([&](AstVhdlInstance* instp) {
                for (AstNode* p = instp->genericMapp(); p; p = p->nextp()) {
                    markIn(p, ignored, true);
                }
            });
            unitp->foreach([&](AstVhdlBinding* bindp) {
                for (AstNode* p = bindp->genericMapp(); p; p = p->nextp()) {
                    markIn(p, ignored, true);
                }
            });
        }
    }
    for (int round = 0; round < 16; ++round) {
        bool changed = false;
        for (AstNodeVhdlUnit* const unitp : unitsp) {
            unitp->foreach([&](AstVhdlObjectDecl* declp) {
                if (!declp->isElabConstant() || !declp->defaultp()) return;
                markIn(declp->defaultp(), changed);
            });
            // A component's generic may have a default the entity's has not,
            // and an instance that says nothing about it hands that default
            // over as a parameter of the module (6.5.6.2)
            unitp->foreach([&](AstVhdlComponentDecl* compp) {
                for (AstNode* p = compp->genericsp(); p; p = p->nextp()) {
                    AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
                    if (ifp->defaultp()) markIn(ifp->defaultp(), changed);
                }
            });
            // And the entity's own default is the value an instance that
            // says nothing about that generic is elaborated with, so what it
            // names is settled by elaboration too (6.5.6.2). A verification
            // component's configuration record arrives this way.
            unitp->foreach([&](AstVhdlEntity* entp) {
                for (AstNode* p = entp->genericsp(); p; p = p->nextp()) {
                    AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
                    if (ifp->defaultp()) markIn(ifp->defaultp(), changed);
                }
            });
        }
        if (!changed) break;
    }
}

// An object declared with an unconstrained array type takes its bounds from
// its initial value (6.4.2.2). Where that value is a call, the length only
// appears once the call has the copy made for its actuals, which is after the
// whole design has been analyzed -- so the declarations are looked at again.
bool VhdlSemVisitor::reconstrainDecls(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    std::map<const VhdlSymbol*, VhdlType*> settled;
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        unitp->foreach([&](AstVhdlObjectDecl* declp) {
            VhdlSymbol* const symp = declp->vhdlSymp();
            if (!symp || !declp->defaultp()) return;
            VhdlType* const declaredp = symp->typep();
            if (!declaredp || declaredp->lengthEmittable()) return;
            VhdlType* const newp
                = constrainFromValue(declaredp, declp->defaultp(), declp->fileline());
            if (!newp || newp == declaredp || !newp->lengthEmittable()) return;
            symp->typep(newp);
            settled.emplace(symp, newp);
        });
    }
    if (settled.empty()) return false;
    // Every name that reads the object was given the type it had before
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        unitp->foreach([&](AstNodeVhdlExpr* exprp) {
            const auto it = settled.find(exprp->vhdlSymp());
            if (it == settled.end()) return;
            if (exprp->vhdlTypep() && exprp->vhdlTypep()->lengthEmittable()) return;
            exprp->vhdlTypep(it->second);
        });
    }
    return true;
}

// An array value must have exactly as many elements as the constrained
// subtype it is given to (5.3.2.2)
void VhdlSemVisitor::checkArrayValue(AstNodeVhdlExpr* nodep, VhdlType* typep,
                                     const std::string& what) {
    if (!nodep || !typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return;
    if (!typep->isConstrainedArray()) return;
    // In a copy made for one call site the parameter's length is what that
    // call supplied, not something the design wrote, so a call the length
    // rules out is written anyway and guarded: "if v'length = 5 then f(v)"
    // is legal, and the match is a condition to be met while running (4.2.2.1)
    if (m_inSpecialized) return;
    if (AstVhdlAggregate* const aggp = VN_CAST(nodep, VhdlAggregate)) {
        checkAggregate(aggp, typep, what);
        return;
    }
    // Another array value must match dimension for dimension. A string or
    // bit string literal is the exception: it was handed the target's
    // subtype to be analyzed with, and its own length is its characters'
    VhdlType* const valuep = VN_IS(nodep, VhdlLiteral) ? nullptr : nodep->vhdlTypep();
    if (valuep && valuep->isConstrainedArray()
        && valuep->dimensions() == typep->dimensions()) {
        for (size_t dim = 0; dim < typep->dimensions(); ++dim) {
            const int64_t want = typep->dimLength(dim);
            const int64_t have = valuep->dimLength(dim);
            if (want == have) continue;
            err(nodep->fileline(), "Expected " + std::to_string(want) + " element(s) for "
                                       + what + dimensionText(dim, typep->dimensions())
                                       + ", but the value has " + std::to_string(have));
            return;
        }
        return;
    }
    if (typep->dimensions() != 1) return;
    const int64_t have = staticLength(nodep);
    if (have < 0) return;
    const int64_t want = typep->dimLength(0);
    if (have == want) return;
    err(nodep->fileline(), "Expected " + std::to_string(want) + " element(s) for " + what
                               + ", but the value has " + std::to_string(have));
}

// Which dimension a length mismatch is in, mentioned only where there is
// more than one
std::string VhdlSemVisitor::dimensionText(size_t dim, size_t dims) {
    if (dims < 2) return "";
    return " in dimension " + std::to_string(dim + 1);
}

// An aggregate given to a constrained array subtype must supply exactly its
// elements (9.3.3.3). Which rule applies turns on whether the aggregate has
// an "others" choice:
//
//   * with "others", the target's index range is the aggregate's, so every
//     named choice must be an index of the target;
//   * without it, the aggregate's own choices fix its index range, so the
//     choices need only be contiguous, distinct, and as many as the target
//     has elements -- which is why v(7 downto 4) := (3 downto 0 => '1') is
//     legal while adding "others => '0'" to it is not.
void VhdlSemVisitor::checkAggregate(AstVhdlAggregate* nodep, VhdlType* typep,
                                    const std::string& what) {
    VhdlType* const indexTypep = typep->indexTypes().empty() ? nullptr : typep->indexTypes()[0];
    std::vector<std::pair<int64_t, int64_t>> covered;  // Low..high of each choice
    std::vector<FileLine*> flps;
    int64_t positional = 0;
    bool hasOthers = false;
    bool hasNamed = false;
    bool allStatic = true;
    // A null range is a choice only of an aggregate with that one choice
    // (9.3.3.3)
    const bool lone = nodep->elemsp() && !nodep->elemsp()->nextp() && nodep->elemsp()->choicesp()
                      && !nodep->elemsp()->choicesp()->nextp();
    for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
        AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
        if (!eap->choicesp()) {
            const int64_t span = elementSpan(eap->valuep(), typep);
            if (span < 0) {
                allStatic = false;
            } else {
                positional += span;
            }
        }
        for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            if (chp->isOthers()) {
                hasOthers = true;
                continue;
            }
            hasNamed = true;
            int64_t lo = 0;
            int64_t hi = 0;
            FileLine* flp = chp->fileline();
            if (AstVhdlRange* const rp = chp->rangep()) {
                const RangeInfo info = analyzeRange(rp, indexTypep);
                if (!info.left.isInt() || !info.right.isInt()) {
                    allStatic = false;
                    continue;
                }
                lo = info.ascending ? info.left.toInt() : info.right.toInt();
                hi = info.ascending ? info.right.toInt() : info.left.toInt();
                flp = rp->fileline();
                if (lo > hi) {  // A null range names nothing
                    if (!lone && !m_inSpecialized) {
                        err(flp, "A null range is a choice only of an aggregate with a single"
                                 " element association and that one choice");
                    }
                    continue;
                }
            } else if (AstNodeVhdlExpr* const valuep = chp->valuep()) {
                const VhdlValue val = V3VhdlEval::eval(valuep);
                if (!val.isInt()) {
                    allStatic = false;
                    continue;
                }
                lo = hi = val.toInt();
                flp = valuep->fileline();
            } else {
                continue;
            }
            covered.emplace_back(lo, hi);
            flps.push_back(flp);
        }
    }
    // A multidimensional aggregate is an aggregate of rows, each an
    // aggregate (or a string) of the dimensions left (9.3.3.3): its
    // positional rows are counted against the first dimension, and each
    // row is checked against the rest
    if (typep->dimensions() > 1) {
        if (!typep->isConstrainedArray() || !typep->lengthEmittable()) return;
        VhdlType* const rowp = VhdlSymTab::newType(VhdlTypeKind::ARRAY, "", nodep->fileline());
        for (size_t i = 1; i < typep->indexTypes().size(); ++i) {
            rowp->addIndexType(typep->indexTypes()[i]);
        }
        rowp->elementp(typep->elementp() ? typep->elementp() : typep->base()->elementp());
        rowp->unconstrained(false);
        int64_t rows = 0;
        for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
            if (!eap->choicesp()) ++rows;
            if (eap->valuep()) checkArrayValue(eap->valuep(), rowp, what);
        }
        if (!hasNamed && !hasOthers && rows != typep->dimLength(0)) {
            err(nodep->fileline(), "Aggregate has " + std::to_string(rows) + " row(s), but " + what
                                       + " needs " + std::to_string(typep->dimLength(0)));
        }
        return;
    }
    if (!allStatic) return;
    const int64_t want = typep->dimLength(0);

    if (hasOthers) {
        // "others" fixes the aggregate to the target's range, so every named
        // choice must be one of the target's indexes
        for (size_t i = 0; i < covered.size(); ++i) {
            checkChoiceInRange(flps[i], VhdlValue::mkInt(covered[i].first, indexTypep),
                               indexTypep);
            if (covered[i].second != covered[i].first) {
                checkChoiceInRange(flps[i], VhdlValue::mkInt(covered[i].second, indexTypep),
                                   indexTypep);
            }
            // No index may be given a value twice, "others" or not
            for (size_t j = 0; j < i; ++j) {
                if (covered[i].first > covered[j].second) continue;
                if (covered[j].first > covered[i].second) continue;
                err(flps[i],
                    "Index "
                        + VhdlValue::mkInt(std::max(covered[i].first, covered[j].first),
                                           indexTypep)
                              .prettyName()
                        + " is given a value more than once in this aggregate");
                return;
            }
        }
        return;
    }
    if (!hasNamed) {
        if (positional == want) return;
        err(nodep->fileline(), "Aggregate has " + std::to_string(positional)
                                   + " element(s), but " + what + " needs "
                                   + std::to_string(want));
        return;
    }
    // Named choices: they must be distinct and contiguous, and together as
    // many as the target has elements
    std::vector<size_t> order(covered.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&covered](size_t a, size_t b) { return covered[a] < covered[b]; });
    int64_t elements = 0;
    for (size_t i = 0; i < order.size(); ++i) {
        const auto& here = covered[order[i]];
        elements += here.second - here.first + 1;
        if (i == 0) continue;
        const auto& prev = covered[order[i - 1]];
        if (here.first <= prev.second) {
            err(flps[order[i]], "Index " + VhdlValue::mkInt(here.first, indexTypep).prettyName()
                                    + " is given a value more than once in this aggregate");
            return;
        }
        if (here.first != prev.second + 1) {
            err(flps[order[i]], "This aggregate gives no value for index "
                                    + VhdlValue::mkInt(prev.second + 1, indexTypep).prettyName());
            return;
        }
    }
    if (elements == want) return;
    err(nodep->fileline(), "Aggregate has " + std::to_string(elements) + " element(s), but "
                               + what + " needs " + std::to_string(want));
}

VhdlType* VhdlSemVisitor::analyzeExpr(AstNodeVhdlExpr* nodep, VhdlType* wantp) {
    if (!nodep) return V3VhdlStd::errorType();
    if (nodep->vhdlTypep()) return nodep->vhdlTypep();  // Already settled

    VhdlType* resultp = V3VhdlStd::errorType();
    if (AstVhdlName* const p = VN_CAST(nodep, VhdlName)) {
        resultp = analyzeName(p, wantp);
    } else if (AstVhdlLiteral* const p = VN_CAST(nodep, VhdlLiteral)) {
        resultp = analyzeLiteral(p, wantp);
    } else if (AstVhdlUnary* const p = VN_CAST(nodep, VhdlUnary)) {
        resultp = analyzeUnary(p, wantp);
    } else if (AstVhdlBinary* const p = VN_CAST(nodep, VhdlBinary)) {
        resultp = analyzeBinary(p, wantp);
    } else if (AstVhdlCall* const p = VN_CAST(nodep, VhdlCall)) {
        resultp = analyzeCall(p, wantp);
    } else if (AstVhdlSelected* const p = VN_CAST(nodep, VhdlSelected)) {
        resultp = analyzeSelected(p, wantp);
    } else if (AstVhdlAttrName* const p = VN_CAST(nodep, VhdlAttrName)) {
        resultp = analyzeAttrName(p, wantp);
    } else if (AstVhdlAggregate* const p = VN_CAST(nodep, VhdlAggregate)) {
        resultp = analyzeAggregate(p, wantp);
    } else if (AstVhdlQualified* const p = VN_CAST(nodep, VhdlQualified)) {
        // A qualified expression states its own type (9.3.5), which is what
        // its operand must already be: it names a type, it converts nothing
        resultp = resolveTypeMark(p->typeMarkp());
        analyzeExpr(p->valuep(), resultp);
        if (p->valuep() && !valueFitsType(p->valuep(), resultp)) {
            err(p->valuep()->fileline(),
                "A qualified expression states the type of its operand, which must be "
                    + resultp->prettyName() + ", but this is of type "
                    + p->valuep()->vhdlTypep()->prettyName());
        }
        // Where the type mark names an unconstrained array, what is qualified
        // is what says how long the value is (5.3.2.2), just as it does for a
        // conversion: "integer_vector'(7, 8, 9)" is three elements even though
        // INTEGER_VECTOR itself says no length
        if (resultp && p->valuep()) {
            resultp = constrainFromValue(resultp, p->valuep(), p->fileline());
        }
    } else if (AstVhdlAllocator* const p = VN_CAST(nodep, VhdlAllocator)) {
        if (p->subtypep()) {
            resultp = resolveSubtype(p->subtypep());
            // An allocator creates an object of the designated type; the
            // access type is what it returns, not what it makes (9.3.7)
            if (resultp && wantp && wantp->kind() == VhdlTypeKind::ACCESS
                && resultp->base() == wantp->base() && !m_inSpecialized) {
                err(p->subtypep()->fileline(),
                    "An allocator names the type of the object it creates, but "
                        + resultp->prettyName() + " is the access type; name what it designates");
            }
        } else if (p->valuep()) {
            resultp = analyzeExpr(p->valuep(), nullptr);
        }
        // The allocator itself yields an access to that type; without the
        // access type declaration in hand, the context supplies it
        if (wantp && wantp->kind() == VhdlTypeKind::ACCESS) resultp = wantp;
    } else if (VN_IS(nodep, VhdlOpen) || VN_IS(nodep, VhdlDefault)) {
        resultp = wantp ? wantp : V3VhdlStd::errorType();
    } else if (AstVhdlExternalName* const p = VN_CAST(nodep, VhdlExternalName)) {
        resultp = resolveSubtype(p->subtypep());
    } else {
        iterateChildren(nodep);
    }

    nodep->vhdlTypep(resultp);
    return resultp;
}

//######################################################################
// Statements, where the context fixes the type each expression must have

// An aggregate target says nothing about its own type: it takes it from the
// value assigned to it (10.5.2.1). The value may be a string literal, whose
// type is settled by context and not by itself, so the first value that
// settles one is what the target and every other value are read against.
VhdlType* VhdlSemVisitor::aggregateTargetType(AstNodeVhdlExpr* valuep, AstNode* wavesp,
                                              AstNode* altsp) {
    std::vector<AstNodeVhdlExpr*> values;
    if (valuep) values.push_back(valuep);
    const auto addWaves = [&values](AstNode* wp) {
        for (; wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_CAST(wp, VhdlWaveform);
            if (wfp && wfp->valuep()) values.push_back(wfp->valuep());
        }
    };
    addWaves(wavesp);
    for (AstNode* ap = altsp; ap; ap = ap->nextp()) {
        addWaves(VN_AS(ap, VhdlAlternative)->wavesp());
    }
    bool anySelfTyped = false;
    for (AstNodeVhdlExpr* const vp : values) {
        CandSet cands;
        candidateTypes(vp, cands);
        if (cands.types.size() == 1 && cands.types[0] && !cands.types[0]->isError()) {
            return cands.types[0];
        }
        const AstVhdlLiteral* const litp = VN_CAST(vp, VhdlLiteral);
        const bool contextTyped = VN_IS(vp, VhdlAggregate)
                                  || (litp
                                      && (litp->kind() == VhdlLitKind::STRING
                                          || litp->kind() == VhdlLitKind::BITSTRING));
        if (!contextTyped) anySelfTyped = true;
    }
    // Nothing then says what type the target is: a string literal or an
    // aggregate takes its type from its context, and here that context is
    // the target, which takes its type from them (10.5.2.1, 10.6.2.1)
    if (!values.empty() && !anySelfTyped && !m_inSpecialized) {
        err(values[0]->fileline(), "The type of an aggregate target comes from the value"
                                   " assigned to it, which a string literal or an aggregate"
                                   " does not have on its own; qualify it");
        return V3VhdlStd::errorType();
    }
    return nullptr;
}

void VhdlSemVisitor::visit(AstVhdlSigAssign* nodep) {
    checkSequentialForm(nodep, nodep->kind());
    VhdlType* wantp = nullptr;
    if (VN_IS(nodep->targetp(), VhdlAggregate)) {
        wantp = aggregateTargetType(nullptr, nodep->wavesp(), nodep->altsp());
    }
    VhdlType* targetp = analyzeExpr(nodep->targetp(), wantp);
    if (wantp && (!targetp || targetp->isError())) targetp = wantp;
    checkAssignTarget(nodep->targetp(), true);
    checkNullWaveforms(nodep);
    // Every waveform element is of the target's type (10.5.2.2)
    for (AstNode* wp = nodep->wavesp(); wp; wp = wp->nextp()) {
        AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
        if (wfp->valuep()) {
            analyzeExpr(wfp->valuep(), targetp);
            checkAssignedValue(wfp->valuep(), targetp);
            checkOthersTarget(nodep->targetp(), wfp->valuep());
        }
        if (wfp->afterp()) analyzeExpr(wfp->afterp(), V3VhdlStd::timeType());
    }
    checkWaveform(nodep->wavesp());
    if (nodep->selectorp()) analyzeExpr(nodep->selectorp(), nullptr);
    bool settled = false;
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        if (altp->condp()) analyzeCondition(altp->condp());
        VL_RESTORER(m_unreached);
        if (altUnreached(altp, settled)) m_unreached = true;
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            analyzeChoice(chp, nodep->selectorp() ? nodep->selectorp()->vhdlTypep() : nullptr);
        }
        for (AstNode* wp = altp->wavesp(); wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
            if (wfp->valuep()) {
                analyzeExpr(wfp->valuep(), targetp);
                checkAssignedValue(wfp->valuep(), targetp);
                checkOthersTarget(nodep->targetp(), wfp->valuep());
            }
            if (wfp->afterp()) analyzeExpr(wfp->afterp(), V3VhdlStd::timeType());
        }
        checkWaveform(altp->wavesp());
    }
    if (nodep->selectorp()) {
        checkSelector(nodep->selectorp(), nodep->selectorp()->vhdlTypep(),
                      "a selected assignment");
        checkChoices(nodep->altsp(), nodep->matching(), nodep->fileline(),
                     nodep->selectorp()->vhdlTypep(), "this selected assignment");
    }
    checkGuardedTargets(nodep);
}

// The object a name denotes: through indexing, slicing and the selection of
// a record element to the object those belong to (8.1). An access value
// designates an object that is a variable whatever the name began with, and
// a call's result is no object, so for those the class is left UNKNOWN.
VhdlSemVisitor::ObjInfo VhdlSemVisitor::objectOfName(const AstNodeVhdlExpr* nodep) {
    for (int depth = 0; nodep && depth < 64; ++depth) {
        if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
            return objectOfSymbol(namep->vhdlSymp());
        }
        if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
            const VhdlType* const prefTypep = selp->prefixp()->vhdlTypep();
            if (selp->isAll() || !prefTypep || prefTypep->base()->kind() == VhdlTypeKind::ACCESS) {
                return {};
            }
            // An expanded name denotes the object itself; a record element
            // belongs to the object its prefix denotes
            const VhdlSymbol* const symp = selp->vhdlSymp();
            if (symp && symp->kind() != VhdlSymKind::RECORD_ELEMENT) return objectOfSymbol(symp);
            if (prefTypep->base()->kind() != VhdlTypeKind::RECORD) return {};
            nodep = selp->prefixp();
            continue;
        }
        if (const AstVhdlCall* const callp = VN_CAST(nodep, VhdlCall)) {
            // Indexed or sliced, the prefix is an array object; a call's is
            // a function, and a conversion's a type mark
            const AstNodeVhdlExpr* const prefp = callp->prefixp();
            const VhdlSymbol* const psymp = prefp->vhdlSymp();
            const VhdlType* const prefTypep = prefp->vhdlTypep();
            if (!prefTypep || prefTypep->base()->kind() != VhdlTypeKind::ARRAY) return {};
            if (psymp
                && (psymp->kind() == VhdlSymKind::FUNCTION
                    || psymp->kind() == VhdlSymKind::TYPE)) {
                return {};
            }
            nodep = prefp;
            continue;
        }
        return {};
    }
    return {};
}

VhdlSemVisitor::ObjInfo VhdlSemVisitor::objectOfSymbol(const VhdlSymbol* symp) {
    ObjInfo info;
    if (!symp) return info;
    symp = symp->resolved();
    info.symp = symp;
    // An interface object carries its class and its mode on the declaration
    const AstVhdlIfaceDecl* const ifp = VN_CAST(symp->declp(), VhdlIfaceDecl);
    switch (symp->kind()) {
    case VhdlSymKind::SIGNAL:
    case VhdlSymKind::PORT:
        info.cls = ObjClass::SIGNAL;
        info.modeIn = ifp && ifp->mode() == VhdlMode::IN;
        info.modeOut = ifp && ifp->mode() == VhdlMode::OUT;
        break;
    case VhdlSymKind::VARIABLE:
    case VhdlSymKind::SHARED_VARIABLE: info.cls = ObjClass::VARIABLE; break;
    case VhdlSymKind::FILE: info.cls = ObjClass::FILE; break;
    case VhdlSymKind::CONSTANT:
    case VhdlSymKind::GENERIC:
    case VhdlSymKind::LOOP_PARAMETER:
    case VhdlSymKind::GENERATE_PARAMETER: info.cls = ObjClass::CONSTANT; break;
    case VhdlSymKind::PARAMETER:
        if (!ifp) break;
        switch (ifp->kind()) {
        case VhdlIfaceKind::CONSTANT: info.cls = ObjClass::CONSTANT; break;
        case VhdlIfaceKind::VARIABLE: info.cls = ObjClass::VARIABLE; break;
        case VhdlIfaceKind::SIGNAL: info.cls = ObjClass::SIGNAL; break;
        case VhdlIfaceKind::FILE: info.cls = ObjClass::FILE; break;
        default: break;
        }
        info.modeIn = ifp->mode() == VhdlMode::IN;
        info.modeOut = ifp->mode() == VhdlMode::OUT;
        break;
    default: break;
    }
    return info;
}

// A signal assignment writes a signal and a variable assignment a variable,
// and neither writes an object of mode IN (10.5.2.1, 10.6.2.1, 6.5.2). An
// out or inout parameter given no class is a variable (4.2.2.1), so "x <= 1"
// inside the procedure that declares it is not a signal assignment to make.
// Whether a signal is guarded: declared bus or register (6.4.2.3)
static bool isGuardedSignal(const VhdlSymbol* symp) {
    if (!symp) return false;
    if (const AstVhdlObjectDecl* const odp = VN_CAST(symp->declp(), VhdlObjectDecl)) {
        return odp->signalKind() != VhdlSignalKind::NONE;
    }
    if (const AstVhdlIfaceDecl* const ifp = VN_CAST(symp->declp(), VhdlIfaceDecl)) {
        return ifp->signalKind() != VhdlSignalKind::NONE;
    }
    return false;
}

// An aggregate target names each object once, by a locally static name
// (10.5.2.1, 10.6.2.1)
void VhdlSemVisitor::checkAggregateTargetNames(const AstVhdlAggregate* aggp) {
    std::set<std::string> seen;
    for (const AstNode* ep = aggp->elemsp(); ep; ep = ep->nextp()) {
        AstNodeVhdlExpr* const valuep = VN_AS(ep, VhdlElemAssoc)->valuep();
        const ObjInfo info = objectOfName(valuep);
        if ((info.cls != ObjClass::SIGNAL && info.cls != ObjClass::VARIABLE) || !info.symp) {
            continue;
        }
        // A name whose index is a generic or an object is not locally
        // static, and two that fold alike name the same object
        bool localStatic = !nonStaticIndex(valuep);
        valuep->foreach([&](const AstVhdlName* namep) {
            const VhdlSymbol* const symp = namep->vhdlSymp();
            if (symp && symp->resolved()->kind() == VhdlSymKind::GENERIC) localStatic = false;
        });
        if (!localStatic) {
            err(valuep->fileline(), "A name in an aggregate target is locally static,"
                                    " and this one is not");
            continue;
        }
        std::string key = info.symp->name();
        if (const AstVhdlCall* const callp = VN_CAST(valuep, VhdlCall)) {
            const AstVhdlAssoc* const ip = VN_CAST(callp->argsp(), VhdlAssoc);
            const VhdlValue v
                = ip && ip->actualp() ? V3VhdlEval::eval(ip->actualp()) : VhdlValue{};
            if (!v.isInt() || callp->argsp()->nextp()) continue;
            key += "(" + std::to_string(v.toInt()) + ")";
        } else if (!VN_IS(valuep, VhdlName)) {
            continue;
        }
        if (!seen.insert(key).second) {
            err(valuep->fileline(), "An aggregate target names each object once, but '"
                                        + info.symp->name() + "' appears twice");
        }
    }
}

// An aggregate target names each object once, by a locally static name, and
// its names are all guarded or all not (10.5.2.1, 11.6). And outside a
// process, a signal that is guarded is assigned only by a guarded assignment,
// which is what disconnects it when the guard is false (11.6).
void VhdlSemVisitor::checkGuardedTargets(AstVhdlSigAssign* nodep) {
    if (m_inSpecialized) return;
    bool inProcess = false;
    for (const AstNode* upp = parentOf(nodep); upp; upp = parentOf(upp)) {
        if (VN_IS(upp, VhdlProcess) || VN_IS(upp, VhdlSubprogDecl)) inProcess = true;
    }
    if (const AstVhdlAggregate* const aggp = VN_CAST(nodep->targetp(), VhdlAggregate)) {
        checkAggregateTargetNames(aggp);
        int guarded = 0;
        int unguarded = 0;
        for (const AstNode* ep = aggp->elemsp(); ep; ep = ep->nextp()) {
            const ObjInfo info = objectOfName(VN_AS(ep, VhdlElemAssoc)->valuep());
            if (info.cls != ObjClass::SIGNAL || !info.symp) continue;
            (isGuardedSignal(info.symp) ? guarded : unguarded)++;
        }
        if (guarded && unguarded) {
            err(nodep->targetp()->fileline(), "The signals of an aggregate target are all"
                                              " guarded or none of them is");
        }
        return;
    }
    if (inProcess || nodep->guarded()) return;
    const ObjInfo info = objectOfName(nodep->targetp());
    if (info.cls == ObjClass::SIGNAL && isGuardedSignal(info.symp)) {
        err(nodep->fileline(), "'" + info.symp->name()
                                   + "' is a guarded signal, so a concurrent assignment to it"
                                     " must be guarded");
    }
}

// A null transaction turns a driver off, and only a guarded signal -- one
// declared bus or register -- has a driver that can be turned off (10.5.2.2)
void VhdlSemVisitor::checkNullWaveforms(AstVhdlSigAssign* nodep) {
    if (m_inSpecialized) return;
    const auto isNull = [](const AstNode* wp) { return VN_AS(wp, VhdlWaveform)->nullTxn(); };
    bool anyNull = false;
    for (AstNode* wp = nodep->wavesp(); wp; wp = wp->nextp()) anyNull = anyNull || isNull(wp);
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        for (AstNode* wp = VN_AS(ap, VhdlAlternative)->wavesp(); wp; wp = wp->nextp()) {
            anyNull = anyNull || isNull(wp);
        }
    }
    if (!anyNull) return;
    const ObjInfo info = objectOfName(nodep->targetp());
    if (info.cls != ObjClass::SIGNAL || !info.symp) return;
    VhdlSignalKind kind = VhdlSignalKind::NONE;
    if (const AstVhdlObjectDecl* const odp = VN_CAST(info.symp->declp(), VhdlObjectDecl)) {
        kind = odp->signalKind();
    } else if (const AstVhdlIfaceDecl* const ifp = VN_CAST(info.symp->declp(), VhdlIfaceDecl)) {
        kind = ifp->signalKind();
    } else {
        return;
    }
    if (kind != VhdlSignalKind::NONE) return;
    err(nodep->fileline(), "A null transaction turns a driver off, which only a guarded signal"
                           " -- one declared bus or register -- has, and '"
                               + info.symp->name() + "' is not one");
}

// An aggregate with "others" takes its index range from its context (9.3.3.3),
// and a target declared with an unconstrained array type -- a port, a
// parameter, or a record element of one -- gives it none to take: its bounds
// come from the actual, which the aggregate cannot see. A constraint the
// analysis gave a port to carry the actual's bounds is not one the design
// wrote, and it says so by leaving its direction unknown. A block's port is
// left alone: its port map is right there, and nvc's regress/issue1175 needs
// it taken.
// Whether a subtype indication is a bare type mark as the design wrote it,
// a constraint the analysis added to carry an actual's bounds aside
bool VhdlSemVisitor::writtenBare(const AstVhdlSubtypeInd* sip) {
    if (!sip || !sip->typeMarkp() || VN_IS(sip->typeMarkp(), VhdlCall)) return false;
    if (!sip->constraintp()) return true;
    bool synthesized = false;
    sip->constraintp()->foreach([&](const AstVhdlRange* rp) {
        if (rp->dirUnknown()) synthesized = true;
    });
    return synthesized;
}

// Whether a type mark, as its declarations write it, leaves the index range
// open: "bit_vector" and "std_logic_vector" do, a subtype declared with a
// constraint -- "ram_word" of a generic package -- does not, however little
// is known of its bounds
bool VhdlSemVisitor::markLeavesIndexOpen(const VhdlSymbol* markp, int depth) {
    if (!markp || depth > 16) return false;
    markp = markp->resolved();
    if (const AstVhdlSubtypeDecl* const sdp = VN_CAST(markp->declp(), VhdlSubtypeDecl)) {
        const AstVhdlSubtypeInd* const sip = sdp->subtypep();
        if (!writtenUnconstrained(sip)) return false;
        return markLeavesIndexOpen(sip->typeMarkp()->vhdlSymp(), depth + 1);
    }
    if (const AstVhdlTypeDecl* const tdp = VN_CAST(markp->declp(), VhdlTypeDecl)) {
        const AstVhdlArrayType* const arrp = VN_CAST(tdp->defp(), VhdlArrayType);
        if (!arrp || !arrp->indexp()) return false;
        for (const AstNode* rp = arrp->indexp()->rangesp(); rp; rp = rp->nextp()) {
            const AstVhdlSubtypeInd* const isp = VN_CAST(rp, VhdlSubtypeInd);
            if (isp && VN_IS(isp->constraintp(), VhdlBox)) return true;
        }
        return false;
    }
    // A type STD.STANDARD declares is built in code, with no declaration
    if (markp->declp()) return false;
    const VhdlType* const typep = markp->typep();
    return typep && typep->base()->kind() == VhdlTypeKind::ARRAY && typep->unconstrained();
}

bool VhdlSemVisitor::targetRangeOpen(const AstNodeVhdlExpr* targetp) {
    const auto declaredSubtype = [](const AstVhdlName* namep) -> const AstVhdlSubtypeInd* {
        // An alias names what it was declared for, whose subtype it may narrow
        if (!namep || !namep->vhdlSymp() || namep->vhdlSymp()->kind() == VhdlSymKind::ALIAS) {
            return nullptr;
        }
        const AstNode* const declp = namep->vhdlSymp()->declp();
        const AstVhdlIfaceDecl* const ifp = VN_CAST(declp, VhdlIfaceDecl);
        if (!ifp || VN_IS(parentOf(ifp), VhdlBlockHeader)) return nullptr;
        return ifp->subtypep();
    };
    if (const AstVhdlName* const namep = VN_CAST(targetp, VhdlName)) {
        const AstVhdlSubtypeInd* const sip = declaredSubtype(namep);
        if (!writtenBare(sip)) return false;
        return markLeavesIndexOpen(sip->typeMarkp()->vhdlSymp());
    }
    if (const AstVhdlSelected* const selp = VN_CAST(targetp, VhdlSelected)) {
        const AstVhdlSubtypeInd* const sip = declaredSubtype(VN_CAST(selp->prefixp(), VhdlName));
        const AstVhdlName* const elemNamep = VN_CAST(selp->suffixp(), VhdlName);
        if (!writtenBare(sip) || !elemNamep) return false;
        const VhdlSymbol* const markp = sip->typeMarkp()->vhdlSymp();
        const VhdlType* const typep = markp ? markp->typep() : nullptr;
        if (!typep || typep->base()->kind() != VhdlTypeKind::RECORD) return false;
        for (const VhdlSymbol* const ep : typep->base()->elements()) {
            if (ep->name() != elemNamep->name()) continue;
            const VhdlType* const etp = ep->typep();
            return etp && etp->base()->kind() == VhdlTypeKind::ARRAY && etp->unconstrained();
        }
    }
    return false;
}

void VhdlSemVisitor::checkOthersTarget(const AstNodeVhdlExpr* targetp,
                                       const AstNodeVhdlExpr* valuep) {
    const AstVhdlAggregate* const aggp = VN_CAST(valuep, VhdlAggregate);
    if (!aggp || m_inSpecialized || m_inPackageInstance) return;
    // GHDL 7.0.0-dev's default VHDL-93 mode is relaxed enough to take it, and
    // its own testsuite (gna/issue2422) needs it taken there
    if (aggp->fileline()->language() < V3LangCode::L1076_2008) return;
    bool others = false;
    for (const AstNode* ep = aggp->elemsp(); ep; ep = ep->nextp()) {
        for (const AstNode* cp = VN_AS(ep, VhdlElemAssoc)->choicesp(); cp; cp = cp->nextp()) {
            if (VN_AS(cp, VhdlChoice)->isOthers()) others = true;
        }
    }
    if (!others || !targetRangeOpen(targetp)) return;
    err(aggp->fileline(), "An aggregate with 'others' takes its index range from its target,"
                          " and this target's subtype is an unconstrained array type");
}

void VhdlSemVisitor::checkAssignTarget(const AstNodeVhdlExpr* targetp, bool signal) {
    if (m_inSpecialized || !targetp) return;
    if (const AstVhdlAggregate* const aggp = VN_CAST(targetp, VhdlAggregate)) {
        for (const AstNode* ep = aggp->elemsp(); ep; ep = ep->nextp()) {
            checkAssignTarget(VN_AS(ep, VhdlElemAssoc)->valuep(), signal);
        }
        return;
    }
    const ObjInfo info = objectOfName(targetp);
    if (info.cls == ObjClass::UNKNOWN) {
        // A value that is no name, and a name of a unit, a literal, a type or
        // a function's result, denotes no object at all (10.5.2.1, 10.6.2.1)
        bool notObject = VN_IS(targetp, VhdlLiteral) || VN_IS(targetp, VhdlBinary)
                         || VN_IS(targetp, VhdlUnary) || VN_IS(targetp, VhdlQualified)
                         || VN_IS(targetp, VhdlAllocator);
        const auto denotesNoObject = [](const VhdlSymbol* symp) {
            if (!symp) return false;
            const VhdlSymKind kind = symp->resolved()->kind();
            return kind == VhdlSymKind::PHYSICAL_UNIT || kind == VhdlSymKind::ENUM_LITERAL
                   || kind == VhdlSymKind::FUNCTION || kind == VhdlSymKind::TYPE;
        };
        if (VN_IS(targetp, VhdlName)) notObject = denotesNoObject(targetp->vhdlSymp());
        // "f(x)" as a whole, where f names a function or a type; a prefix
        // that selects -- "r.v(0 to 1)" -- names an element, whatever else
        // is called v
        if (const AstVhdlCall* const callp = VN_CAST(targetp, VhdlCall)) {
            notObject = VN_IS(callp->prefixp(), VhdlName)
                        && (denotesNoObject(callp->vhdlSymp())
                            || denotesNoObject(callp->prefixp()->vhdlSymp()));
        }
        if (notObject) {
            err(targetp->fileline(), std::string{"The target of a "}
                                         + (signal ? "signal" : "variable") + " assignment is "
                                         + (signal ? "a signal" : "a variable")
                                         + " or an aggregate of them, and this denotes no object");
        }
        return;
    }
    // The GUARD a guarded block declares is its guard expression's value, and
    // nothing else assigns it (11.2)
    if (info.symp && VN_IS(info.symp->declp(), VhdlBlock)) {
        err(targetp->fileline(), "The implicit signal GUARD is the value of its block's"
                                 " guard expression, and cannot be assigned");
        return;
    }
    // A procedure declared outside any process drives no signal of its own
    // accord: the signals it assigns are the ones it is given (10.5.2.1)
    if (signal && info.cls == ObjClass::SIGNAL && info.symp
        && info.symp->kind() != VhdlSymKind::PARAMETER
        && !(VN_IS(info.symp->declp(), VhdlIfaceDecl) && isSubprogramParam(info.symp->declp()))) {
        const AstVhdlSubprogDecl* subp = nullptr;
        bool inProcess = false;
        // A force or release overrides a signal rather than driving it, and
        // nvc 1.22.1, GHDL 7.0.0-dev and Questa 2025.2 all take one on any
        // signal the procedure can see
        bool forcing = false;
        for (const AstNode* upp = parentOf(targetp); upp; upp = parentOf(upp)) {
            if (!subp) {
                if (const AstVhdlSigAssign* const sap = VN_CAST(upp, VhdlSigAssign)) {
                    if (sap->force() || sap->release()) forcing = true;
                }
            }
            if (!subp) subp = VN_CAST(upp, VhdlSubprogDecl);
            if (VN_IS(upp, VhdlProcess)) inProcess = true;
        }
        if (subp && !inProcess && !forcing) {
            err(targetp->fileline(), std::string{"A "}
                                         + (subp->isFunction() ? "function" : "procedure")
                                         + " declared outside a process assigns only"
                                           " signals it is given as parameters, and '"
                                         + info.symp->name() + "' is not one");
            return;
        }
    }
    const ObjClass want = signal ? ObjClass::SIGNAL : ObjClass::VARIABLE;
    const std::string name = info.symp ? info.symp->name() : "";
    if (info.cls != want) {
        err(targetp->fileline(), std::string{"The target of a "} + (signal ? "signal" : "variable")
                                     + " assignment must be " + objClassName(want) + ", but '"
                                     + name + "' is " + objClassName(info.cls));
    } else if (info.modeIn) {
        err(targetp->fileline(), "'" + name + "' is of mode in, so nothing can assign it");
    }
}

// A signal parameter is given a name denoting a signal, and a variable
// parameter one denoting a variable: what the subprogram reads or writes
// through it is that object, not a copy of a value (4.2.2.2, 4.2.2.3). And a
// parameter the subprogram may write needs an object that may be written.
// A parameter of mode IN given no class is a constant, which takes any value.
void VhdlSemVisitor::checkActualClass(const VhdlSymbol* subp, int formal,
                                      const AstNodeVhdlExpr* actualp) {
    if (m_inSpecialized || !subp || !actualp || formal < 0) return;
    const AstVhdlSubprogDecl* const declp = VN_CAST(subp->resolved()->declp(), VhdlSubprogDecl);
    if (!declp) return;
    const AstVhdlIfaceDecl* ifp = nullptr;
    int idx = -declp->genericCount();
    for (const AstNode* p = declp->paramsp(); p; p = p->nextp(), ++idx) {
        if (idx == formal) {
            ifp = VN_CAST(p, VhdlIfaceDecl);
            break;
        }
    }
    if (!ifp) return;
    // A formal of mode in or inout reads what it is given, and before
    // VHDL-2008 nothing reads a port or parameter of mode out (4.2.2.1). A
    // function's arguments are read where they are written, which the rule
    // for reading names already covers.
    if ((ifp->mode() == VhdlMode::IN || ifp->mode() == VhdlMode::INOUT)
        && subp->resolved()->kind() == VhdlSymKind::PROCEDURE
        && actualp->fileline()->language() < V3LangCode::L1076_2008) {
        const ObjInfo readp = objectOfName(actualp);
        if (readp.modeOut && readp.symp) {
            err(actualp->fileline(),
                std::string{"A "}
                    + (readp.symp->kind() == VhdlSymKind::PORT ? "port" : "parameter")
                    + " of mode out cannot be read before VHDL-2008, and parameter '"
                    + ifp->prettyName() + "' of '" + subp->resolved()->name() + "' reads '"
                    + readp.symp->name() + "'");
            return;
        }
    }
    ObjClass want = ObjClass::UNKNOWN;
    if (ifp->kind() == VhdlIfaceKind::SIGNAL) {
        want = ObjClass::SIGNAL;
    } else if (ifp->kind() == VhdlIfaceKind::VARIABLE
               && (ifp->mode() != VhdlMode::IN || ifp->classExplicit())) {
        // Of mode in too, where the class was written: it still names the
        // variable it reads (4.2.2.3)
        want = ObjClass::VARIABLE;
    } else {
        return;
    }
    // A value that is no name at all denotes no object; a name reached
    // through an access value, a call or an attribute is left alone
    const bool notName = VN_IS(actualp, VhdlLiteral) || VN_IS(actualp, VhdlBinary)
                         || VN_IS(actualp, VhdlUnary) || VN_IS(actualp, VhdlAggregate)
                         || VN_IS(actualp, VhdlQualified);
    const ObjInfo info = notName ? ObjInfo{} : objectOfName(actualp);
    if (!notName && info.cls == ObjClass::UNKNOWN) return;
    // VHDL-87 passed a file through a variable parameter of its file type,
    // as Ashenden's examples still do
    if (want == ObjClass::VARIABLE && info.cls == ObjClass::FILE) return;
    const std::string formalText
        = "parameter '" + ifp->prettyName() + "' of '" + subp->resolved()->name() + "'";
    if (notName || info.cls != want) {
        err(actualp->fileline(),
            "What is given for " + formalText + " must name " + objClassName(want)
                + (notName ? ", but this is a value"
                           : ", but '" + info.symp->name() + "' is " + objClassName(info.cls)));
    } else if (ifp->mode() != VhdlMode::IN && info.modeIn) {
        err(actualp->fileline(), "'" + info.symp->name()
                                     + "' is of mode in, so it cannot be given"
                                       " for "
                                     + formalText + ", which may write it");
    } else if (want == ObjClass::SIGNAL) {
        // A signal parameter is bound to one signal for the whole call, so
        // the name that gives it is static (4.2.2.3)
        if (const VhdlSymbol* const varyingp
            = nonStaticIndex(const_cast<AstNodeVhdlExpr*>(actualp))) {
            err(actualp->fileline(), "What is given for " + formalText
                                         + " is a static signal name, but this one's index"
                                           " depends on '"
                                         + varyingp->name() + "'");
        }
    }
}

// Whether a settled condition closes off this alternative of a conditional
// assignment: its own condition folds to FALSE, or an earlier one's folded to
// TRUE. As for a branch of an if statement, what such an alternative assigns
// is checked for length only if it runs (10.5.2.1), and it never does.
bool VhdlSemVisitor::altUnreached(const AstVhdlAlternative* altp, bool& settled) {
    const bool unreached = settled;
    if (!altp->condp()) return unreached;
    // Only a BOOLEAN reads as its position: a STD_LOGIC condition, taken
    // through "??", is TRUE when it is '1', which sits at position three
    const VhdlType* const typep = altp->condp()->vhdlTypep();
    if (!typep || typep->base() != V3VhdlStd::booleanType()) return unreached;
    const VhdlValue cond = V3VhdlEval::eval(altp->condp());
    if (!cond.isInt()) return unreached;
    if (cond.toInt() != 0) settled = true;
    return unreached || cond.toInt() == 0;
}

// A conditional or selected assignment written as a sequential statement is
// VHDL-2008's (10.5.3, 10.5.4, 10.6.3, 10.6.4); before it they were
// concurrent statements only
void VhdlSemVisitor::checkSequentialForm(const AstNode* nodep, VhdlAssignKind kind) {
    if (kind != VhdlAssignKind::CONDITIONAL && kind != VhdlAssignKind::SELECTED) return;
    if (nodep->fileline()->language() >= V3LangCode::L1076_2008 || m_inSpecialized) return;
    for (const AstNode* upp = parentOf(nodep); upp; upp = parentOf(upp)) {
        if (VN_IS(upp, VhdlProcess) || VN_IS(upp, VhdlSubprogDecl)) {
            err(nodep->fileline(),
                std::string{"Before VHDL-2008 a "}
                    + (kind == VhdlAssignKind::CONDITIONAL ? "conditional" : "selected")
                    + " assignment is not a sequential statement");
            return;
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlVarAssign* nodep) {
    checkSequentialForm(nodep, nodep->kind());
    VhdlType* wantp = nullptr;
    if (VN_IS(nodep->targetp(), VhdlAggregate)) {
        wantp = aggregateTargetType(nodep->valuep(), nullptr, nodep->altsp());
    }
    VhdlType* targetp = analyzeExpr(nodep->targetp(), wantp);
    if (wantp && (!targetp || targetp->isError())) targetp = wantp;
    checkAssignTarget(nodep->targetp(), false);
    if (const AstVhdlAggregate* const aggp = VN_CAST(nodep->targetp(), VhdlAggregate)) {
        if (!m_inSpecialized) checkAggregateTargetNames(aggp);
    }
    if (nodep->valuep()) {
        analyzeExpr(nodep->valuep(), targetp);
        checkAssignedValue(nodep->valuep(), targetp);
        checkOthersTarget(nodep->targetp(), nodep->valuep());
    }
    if (nodep->selectorp()) analyzeExpr(nodep->selectorp(), nullptr);
    bool settled = false;
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        if (altp->condp()) analyzeCondition(altp->condp());
        VL_RESTORER(m_unreached);
        if (altUnreached(altp, settled)) m_unreached = true;
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            analyzeChoice(VN_AS(cp, VhdlChoice),
                          nodep->selectorp() ? nodep->selectorp()->vhdlTypep() : nullptr);
        }
        for (AstNode* wp = altp->wavesp(); wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
            if (wfp->valuep()) {
                analyzeExpr(wfp->valuep(), targetp);
                checkAssignedValue(wfp->valuep(), targetp);
                checkOthersTarget(nodep->targetp(), wfp->valuep());
            }
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlIf* nodep) {
    bool settled = false;
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        // A condition is of type BOOLEAN, or is converted to one by "??" (9.2.9)
        if (altp->condp()) analyzeCondition(altp->condp());
        // A copy made for one call site settles conditions the source left
        // open -- "value'length = target'length" where the two lengths now
        // differ -- and the branch behind such a condition is one nothing
        // reaches. What it assigns is checked for length when it runs
        // (10.5.2.1), so a branch that never runs is legal however it is
        // written, and reporting it would refuse code the design cannot
        // execute. It is still analyzed; only the run-time checks are held.
        VL_RESTORER(m_unreached);
        if (altUnreached(altp, settled)) m_unreached = true;
        iterateAndNextNull(altp->stmtsp());
    }
}

void VhdlSemVisitor::visit(AstVhdlCase* nodep) {
    VhdlType* const exprp = analyzeExpr(nodep->exprp(), nullptr);
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            // Every choice is of the case expression's type (10.9)
            analyzeChoice(VN_AS(cp, VhdlChoice), exprp);
        }
        iterateAndNextNull(altp->stmtsp());
    }
    checkCaseChoices(nodep, exprp);
}

// The choices of a case statement must name every value of the expression's
// subtype exactly once (10.9). Verilator needs this before lowering anyway:
// a case that is neither complete nor has an "others" has no defined
// behaviour to emit, and a duplicated choice has two.
void VhdlSemVisitor::checkCaseChoices(AstVhdlCase* nodep, VhdlType* typep) {
    checkSelector(nodep->exprp(), typep, "a case statement");
    // Its choices are locally static (10.9): a generic is fixed only when
    // the design is elaborated, and an object's value only when it runs
    for (AstNode* ap = nodep->altsp(); ap && !m_inSpecialized; ap = ap->nextp()) {
        for (AstNode* cp = VN_AS(ap, VhdlAlternative)->choicesp(); cp; cp = cp->nextp()) {
            const VhdlSymbol* genp = nullptr;
            cp->foreach([&](const AstVhdlName* namep) {
                const VhdlSymbol* const symp
                    = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
                if (genp || !symp) return;
                // An attribute of an object -- "v'length" -- asks about its
                // subtype, which may well be static
                const AstVhdlAttrName* const attrp = VN_CAST(parentOf(namep), VhdlAttrName);
                if (attrp && attrp->prefixp() == namep) return;
                switch (symp->kind()) {
                case VhdlSymKind::GENERIC:
                case VhdlSymKind::VARIABLE:
                case VhdlSymKind::SHARED_VARIABLE:
                case VhdlSymKind::SIGNAL:
                case VhdlSymKind::PORT: genp = symp; break;
                default: break;
                }
            });
            if (genp) {
                err(cp->fileline(), "A case choice is locally static, and this one depends on "
                                        + std::string{genp->kindName()} + " '" + genp->name()
                                        + "'");
            }
        }
    }
    checkChoices(nodep->altsp(), nodep->matching(), nodep->fileline(), typep,
                 "this case statement");
}

// What a case statement or a selected assignment chooses on is of a discrete
// type or a one-dimensional array of characters, whose values the choices
// can name one by one (10.9, 10.5.4)
void VhdlSemVisitor::checkSelector(AstNodeVhdlExpr* exprp, const VhdlType* typep,
                                   const std::string& what) {
    if (!exprp || !typep || typep->isError() || typep->isGenericFormal() || m_inSpecialized) {
        return;
    }
    const VhdlType* const basep = typep->base();
    if (basep->isDiscrete()) return;
    if (basep->kind() == VhdlTypeKind::ARRAY && basep->indexTypes().size() == 1) {
        const VhdlType* const elemp = basep->elementp();
        if (!elemp || elemp->base()->kind() == VhdlTypeKind::ENUM) {
            checkSelectorStatic(exprp, what);
            return;
        }
    }
    err(exprp->fileline(), "What " + what
                               + " chooses on is of a discrete type or a one-dimensional array"
                                 " of characters, but this is of type "
                               + typep->prettyName());
}

// Before VHDL-2008 an array selector names an object whose subtype is locally
// static (VHDL-93 8.8): not a parameter or port left unconstrained, and not
// one constrained by a generic or by a constant a function call gives its
// value. VHDL-2008 dropped the rule.
void VhdlSemVisitor::checkSelectorStatic(const AstNodeVhdlExpr* exprp, const std::string& what) {
    if (exprp->fileline()->language() >= V3LangCode::L1076_2008) return;
    const AstVhdlName* const namep = VN_CAST(exprp, VhdlName);
    const VhdlSymbol* const symp
        = namep && namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
    if (!symp) return;
    const AstVhdlSubtypeInd* sip = nullptr;
    if (const AstVhdlObjectDecl* const odp = VN_CAST(symp->declp(), VhdlObjectDecl)) {
        sip = odp->subtypep();
    } else if (const AstVhdlIfaceDecl* const ifp = VN_CAST(symp->declp(), VhdlIfaceDecl)) {
        sip = ifp->subtypep();
    }
    if (!sip) return;
    std::string why;
    if (writtenBare(sip) && sip->typeMarkp()
        && markLeavesIndexOpen(sip->typeMarkp()->vhdlSymp())) {
        why = "its subtype leaves the bounds to what it is given";
    } else if (const VhdlSymbol* const varyp = notLocallyStatic(sip)) {
        why = "its subtype depends on " + std::string{varyp->kindName()} + " '" + varyp->name()
              + "'";
    }
    if (why.empty()) return;
    err(exprp->fileline(), "Before VHDL-2008 what " + what
                               + " chooses on has a locally static subtype, and '" + symp->name()
                               + "' does not: " + why);
}

// The choices of a case statement or a selected assignment must name every
// value of the expression's subtype exactly once (10.9, 10.5.4). Verilator
// needs this before lowering anyway: a selection that is neither complete nor
// has an "others" has no defined behaviour to emit, and a duplicated choice
// has two.
void VhdlSemVisitor::checkChoices(AstNode* altsp, bool matching, FileLine* flp, VhdlType* typep,
                                  const std::string& what) {
    if (m_inSpecialized) return;
    // "others" stands for whatever is left, so it is the one choice of the
    // last alternative (10.9, 10.5.4)
    for (AstNode* ap = altsp; ap; ap = ap->nextp()) {
        const AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            if (!VN_AS(cp, VhdlChoice)->isOthers()) continue;
            if (ap->nextp() || altp->choicesp()->nextp()) {
                err(cp->fileline(),
                    "'others' must be the only choice of the last alternative of " + what);
                break;
            }
        }
    }
    if (!typep || typep->isError()) return;
    if (matching) {
        // A matching case compares with "?=", and two choices written the same
        // match the same values (10.9)
        std::set<std::string> seen;
        for (AstNode* ap = altsp; ap; ap = ap->nextp()) {
            AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
            for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
                AstNodeVhdlExpr* const valuep = VN_AS(cp, VhdlChoice)->valuep();
                const VhdlValue val = valuep ? V3VhdlEval::eval(valuep) : VhdlValue{};
                if (!val.isStr() || seen.insert(val.toStr()).second) continue;
                err(valuep->fileline(), "Choice " + val.prettyName()
                                            + " is given by more than one alternative of " + what);
            }
        }
        return;
    }
    // Duplicates are found among any discrete choices; whether they cover
    // every value can only be asked of a subtype with bounds
    const bool bounded = typep->isDiscrete() && typep->bounded();
    const bool discrete = typep->isDiscrete();
    const bool array = typep->base()->kind() == VhdlTypeKind::ARRAY;
    if (!discrete && !array) return;

    bool hasOthers = false;
    bool allStatic = true;
    std::vector<std::pair<int64_t, int64_t>> covered;  // Discrete choices
    std::vector<FileLine*> flps;
    std::map<std::string, FileLine*> seenStrings;  // Array choices
    for (AstNode* ap = altsp; ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            if (chp->isOthers()) {
                hasOthers = true;
                continue;
            }
            if (AstVhdlRange* const rp = chp->rangep()) {
                const RangeInfo info = analyzeRange(rp, typep);
                if (!info.left.isInt() || !info.right.isInt()) {
                    allStatic = false;
                    continue;
                }
                const int64_t lo = info.ascending ? info.left.toInt() : info.right.toInt();
                const int64_t hi = info.ascending ? info.right.toInt() : info.left.toInt();
                if (lo > hi) continue;  // A null range names nothing
                covered.emplace_back(lo, hi);
                flps.push_back(rp->fileline());
                continue;
            }
            AstNodeVhdlExpr* const valuep = chp->valuep();
            if (!valuep) continue;
            const VhdlValue val = V3VhdlEval::eval(valuep);
            if (val.isStr()) {
                // A choice is a value of the expression's subtype, and so
                // exactly as long as it (10.9)
                const int64_t have = static_cast<int64_t>(val.toStr().size());
                if (typep->isConstrainedArray() && typep->dimensions() == 1
                    && have != typep->dimLength(0)) {
                    err(valuep->fileline(), "Choice " + val.prettyName() + " has "
                                                + std::to_string(have)
                                                + " element(s), but what it is a choice for has "
                                                + std::to_string(typep->dimLength(0)));
                    continue;
                }
                // Where the subtype leaves the length open, the choices are
                // still values of one subtype, and so all of one length
                if (!typep->isConstrainedArray() && !seenStrings.empty()
                    && static_cast<int64_t>(seenStrings.begin()->first.size()) != have) {
                    err(valuep->fileline(), "Choice " + val.prettyName() + " has "
                                                + std::to_string(have)
                                                + " element(s), but the choices before it have "
                                                + std::to_string(seenStrings.begin()->first.size())
                                                + ", and all are values of one subtype");
                    continue;
                }
                const auto pair = seenStrings.emplace(val.toStr(), valuep->fileline());
                if (!pair.second) {
                    err(valuep->fileline(), "Choice " + val.prettyName()
                                                + " is given by more than one alternative of "
                                                + what);
                }
                continue;
            }
            if (!val.isInt()) {
                allStatic = false;
                continue;
            }
            covered.emplace_back(val.toInt(), val.toInt());
            flps.push_back(valuep->fileline());
        }
    }
    // Without "others" the choices of an array selector name each of its
    // values, which is every element value at every position: 2 ** 8 of them
    // for a BIT_VECTOR(7 downto 0)
    if (array && !hasOthers && allStatic && covered.empty() && typep->isConstrainedArray()
        && typep->dimensions() == 1) {
        const VhdlType* const elemp
            = typep->elementp() ? typep->elementp() : typep->base()->elementp();
        const int64_t len = typep->dimLength(0);
        if (elemp && elemp->isDiscrete() && elemp->bounded() && len >= 0) {
            const double perElem = static_cast<double>(elemp->high() - elemp->low() + 1);
            const double values = std::pow(perElem, static_cast<double>(len));
            if (static_cast<double>(seenStrings.size()) < values) {
                std::string subject = what;
                subject[0]
                    = static_cast<char>(std::toupper(static_cast<unsigned char>(subject[0])));
                err(flp, subject + " has no 'others', and its "
                             + std::to_string(seenStrings.size())
                             + " choice(s) do not name every one of the "
                             + std::to_string(elemp->high() - elemp->low() + 1) + " ** "
                             + std::to_string(len) + " values of what it chooses on");
            }
        }
    }
    if (!allStatic || !discrete) return;

    std::vector<size_t> order(covered.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&covered](size_t a, size_t b) { return covered[a] < covered[b]; });
    for (size_t i = 1; i < order.size(); ++i) {
        if (covered[order[i]].first > covered[order[i - 1]].second) continue;
        err(flps[order[i]], "Choice "
                                + VhdlValue::mkInt(covered[order[i]].first, typep).prettyName()
                                + " is given by more than one alternative of " + what);
        return;
    }
    if (hasOthers || !bounded) return;
    // Without "others" the alternatives must leave no value of the subtype
    // unaccounted for
    int64_t next = typep->low();
    const int64_t last = typep->high();
    for (const size_t idx : order) {
        if (covered[idx].first > next) break;
        if (covered[idx].second >= next) next = covered[idx].second + 1;
        if (next > last) break;
    }
    if (next > last) return;
    std::string subject = what;
    subject[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(subject[0])));
    err(flp, subject + " has no alternative for value "
                 + VhdlValue::mkInt(next, typep).prettyName() + " of " + typep->prettyName());
}

// What an assertion or a report statement reports is a STRING, and its
// severity a SEVERITY_LEVEL, both the ones STD.STANDARD declares (10.3, 10.4):
// a CHARACTER is not a one-character string, and a type of the design's own
// named SEVERITY_LEVEL is not the one the simulator knows how to act on
void VhdlSemVisitor::visit(AstVhdlAssert* nodep) {
    if (nodep->condp()) analyzeCondition(nodep->condp());
    const auto check = [&](AstNodeVhdlExpr* exprp, VhdlType* wantp, const char* what) {
        if (!exprp) return;
        VhdlType* const typep = analyzeExpr(exprp, wantp);
        if (!typep || typep->isError() || typep->base() == wantp->base()) return;
        // A type of the design's own may share the name, and then saying
        // both names would say nothing
        const bool sameName = typep->prettyName() == wantp->prettyName();
        err(exprp->fileline(), std::string{what} + " must be of type " + wantp->prettyName()
                                   + (sameName ? " from STD.STANDARD, but this is of another type"
                                                 " of that name"
                                               : ", but this is of type " + typep->prettyName()));
    };
    check(nodep->reportp(), V3VhdlStd::stringType(), "What is reported");
    check(nodep->severityp(), V3VhdlStd::severityType(), "A severity");
}

void VhdlSemVisitor::visit(AstVhdlWait* nodep) {
    for (AstNode* sp = nodep->sensp(); sp; sp = sp->nextp()) {
        analyzeExpr(VN_AS(sp, NodeVhdlExpr), nullptr);
        checkSensitivityName(VN_AS(sp, NodeVhdlExpr));
    }
    if (nodep->condp()) analyzeCondition(nodep->condp());
    if (nodep->timeoutp()) {
        analyzeExpr(nodep->timeoutp(), V3VhdlStd::timeType());
        // A timeout is a length of time, not a number of anything (10.2): a
        // bare integer names no unit, so it is not a TIME either
        const AstNodeVhdlExpr* barep = nodep->timeoutp();
        if (const AstVhdlUnary* const unp = VN_CAST(barep, VhdlUnary)) barep = unp->lhsp();
        const AstVhdlLiteral* const litp = VN_CAST(barep, VhdlLiteral);
        if (litp && (litp->kind() == VhdlLitKind::INTEGER || litp->kind() == VhdlLitKind::REAL)) {
            err(nodep->timeoutp()->fileline(),
                "A timeout is of type time, but this number has no unit");
        } else if (!valueFitsType(nodep->timeoutp(), V3VhdlStd::timeType())) {
            err(nodep->timeoutp()->fileline(), "A timeout is of type time, but this is of type "
                                                   + nodep->timeoutp()->vhdlTypep()->prettyName());
        } else {
            checkDelay(nodep->timeoutp(), "timeout");
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlReturn* nodep) {
    if (!nodep->valuep()) return;
    if (!m_returnTypep) {
        err(nodep->fileline(), "A procedure's 'return' may not have a value");
        return;
    }
    const int errsBefore = V3Error::errorCount();
    analyzeExpr(nodep->valuep(), m_returnTypep);
    // Of the function's result type (10.13)
    if (V3Error::errorCount() == errsBefore && !valueFitsType(nodep->valuep(), m_returnTypep)) {
        err(nodep->valuep()->fileline(), "The function returns " + m_returnTypep->prettyName()
                                             + ", but this value is of type "
                                             + nodep->valuep()->vhdlTypep()->prettyName());
    }
}

// An instance associates actuals with the formals of the component or entity
// it names (6.5.7.1). Resolving the unit first gives every actual the type its
// formal has, which is what lets the bounds of a port be checked here rather
// than after elaboration.
void VhdlSemVisitor::visit(AstVhdlInstance* nodep) {
    std::vector<std::string> parts;
    VhdlScope* unitScopep = nullptr;
    std::string entKey;
    // "entity work.reg4(behav)" names the architecture as well (7.3.2), which
    // the parser could not tell from a call; the entity is the prefix
    AstNodeVhdlExpr* unitNamep = nodep->unitp();
    if (AstVhdlCall* const callp = VN_CAST(unitNamep, VhdlCall)) {
        AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
        if (asp && !asp->nextp() && asp->actualp() && callp->prefixp()) {
            unitNamep = callp->prefixp();
        }
    }
    if (flatten(unitNamep, parts) && !parts.empty()) {
        if (parts.size() > 1) {
            // "entity lib.name" names the entity directly (11.7)
            std::string lib = parts[0];
            if (lib == "work") lib = m_libname;
            entKey = lib + "." + parts.back();
            const auto it = m_unitScopes.find(entKey);
            if (it != m_unitScopes.end()) unitScopep = it->second;
        } else {
            std::vector<VhdlSymbol*> found;
            lookup(parts.back(), found);
            for (VhdlSymbol* const symp : found) {
                if (symp->kind() == VhdlSymKind::COMPONENT
                    || symp->kind() == VhdlSymKind::ENTITY) {
                    unitNamep->vhdlSymp(symp);
                    unitScopep = symp->innerp();
                    break;
                }
            }
            // "entity e" names an entity visible here, as "use work.all" makes
            // them; anything else needs its library written (11.7.2)
            if (nodep->kind() == "entity" && !m_inSpecialized) {
                const auto isEntity
                    = [](const VhdlSymbol* symp) { return symp->kind() == VhdlSymKind::ENTITY; };
                const bool unitVisible = m_scopep->findNamed(parts.back())
                                         || m_scopep->findNamed("*all:" + m_libname);
                if (found.empty() && !unitVisible) {
                    err(unitNamep->fileline(), "No entity '" + parts.back()
                                                   + "' is visible here; name it with its"
                                                     " library, as in 'entity work."
                                                   + parts.back() + "'");
                } else if (!found.empty() && std::none_of(found.begin(), found.end(), isEntity)) {
                    const std::string kind = found[0]->kindName();
                    const bool vowel = std::string{"aeiou"}.find(kind[0]) != std::string::npos;
                    err(unitNamep->fileline(), "An entity instantiation names an entity, and '"
                                                   + parts.back() + "' here names "
                                                   + (vowel ? "an " : "a ") + kind);
                }
            }
            // Default binding is to the entity of the same name (7.3.3),
            // which is the entity a generic type would be settled on, whether
            // the instance named it directly or through a component. A
            // configuration specification may have named a different one.
            std::string entity = parts.back();
            const auto labelIt = m_specBindings.find(nodep->label());
            const auto compIt = m_specBindings.find(entity);
            if (!nodep->label().empty() && labelIt != m_specBindings.end()) {
                entity = labelIt->second;
            } else if (compIt != m_specBindings.end()) {
                entity = compIt->second;
            }
            entKey = m_libname + "." + entity;
            if (!unitScopep) {
                // The entity may not have been analyzed into this library yet
                const auto it = m_unitScopes.find(entKey);
                if (it != m_unitScopes.end()) unitScopep = it->second;
            }
        }
    }
    // Which port each position names, for a binding indication that says
    // the entity's ports and the component's go by different names (7.3.2.2)
    if (unitScopep) {
        std::map<int, std::string> byPosition;
        for (const auto& pair : unitScopep->direct()) {
            const VhdlSymbol* const symp = pair.second;
            if (symp->kind() != VhdlSymKind::PORT || symp->position() < 0) continue;
            byPosition[symp->position()] = symp->name();
        }
        std::vector<std::string> names;
        for (const auto& pair : byPosition) names.push_back(pair.second);
        nodep->formalPorts(names);
        // The same for the generics. One synthesized to carry another's
        // bounds has no place in the clause and is skipped, so a positional
        // generic map reaches what the design wrote.
        std::map<int, std::string> genByPosition;
        for (const auto& pair : unitScopep->direct()) {
            const VhdlSymbol* const symp = pair.second;
            if (symp->kind() != VhdlSymKind::GENERIC || symp->position() < 0) continue;
            genByPosition[symp->position()] = symp->name();
        }
        std::vector<std::string> genNames;
        for (const auto& pair : genByPosition) genNames.push_back(pair.second);
        nodep->formalGenerics(genNames);
    }
    // An entity with a generic type is copied per type the design gives it,
    // and this instance is of the copy rather than of the original (6.5.5)
    if (!entKey.empty()) {
        if (VhdlScope* const specp = specializeEntity(nodep, entKey)) unitScopep = specp;
    }
    bindOpenBounds(nodep, unitScopep);
    // "gB(2) => (...)" names part of one generic; together the parts are the
    // whole of it (6.5.7.1), and a generic is only ever read
    mergeIndividualGenerics(nodep);
    analyzeAssociations(nodep->genericMapp(), unitScopep, true);
    // A generic left unconstrained is bound from the actual's own subtype,
    // which is only known once the generic map has been analyzed -- and the
    // actual is often a literal, whose bounds come from how long it is
    bindOpenGenericBounds(nodep, unitScopep);
    analyzeAssociations(nodep->portMapp(), unitScopep);
    markDrivingPorts(nodep, unitScopep);
    if (!m_inSpecialized) checkPortAssociations(nodep, unitScopep);
    if (!m_inSpecialized) checkGenericAssociations(nodep, unitScopep);
}

// A port of mode out drives whatever signal the instance associates with it,
// and that is one driver of the signal like any other (6.5.6.3, 14.7.2). A
// signal an instance drives and a process drives too therefore needs
// resolving, and a signal two instances drive needs it as well.
void VhdlSemVisitor::markDrivingPorts(AstVhdlInstance* nodep, VhdlScope* unitScopep) {
    if (!unitScopep) return;
    int position = 0;
    for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
        AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
        const int here = position++;
        if (!asp->actualp()) continue;
        const VhdlSymbol* portp = nullptr;
        if (!asp->formalp()) {
            portp = portAt(unitScopep, here);
        } else if (const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName)) {
            const VhdlSymbol* const symp = namep->vhdlSymp();
            if (symp && symp->kind() == VhdlSymKind::PORT) portp = symp;
        }
        // A conversion written on the formal is a source of the actual in
        // its own right: the port's driving value goes out through it, and
        // the actual is not the port itself but a signal the conversion
        // drives (6.5.7.1) -- so an "inout" formal counts here as well
        if (!portp) {
            if (convertedFormalDrives(asp->formalp())) asp->drives(true);
            continue;
        }
        const AstVhdlIfaceDecl* const declp = VN_CAST(portp->declp(), VhdlIfaceDecl);
        if (!declp || declp->mode() != VhdlMode::OUT) continue;
        asp->drives(true);
    }
}

// What a port map may give a port (6.5.6.3). A formal the instance writes
// cannot be given a port of mode in, which nothing may write; before
// VHDL-2008 a formal of mode in cannot be given a port of mode out, which
// nothing may read; and a port of mode in with no default of its own must be
// given something, since otherwise it has no value at all.
void VhdlSemVisitor::checkPortAssociations(AstVhdlInstance* nodep, VhdlScope* unitScopep) {
    if (!unitScopep) return;
    std::set<const VhdlSymbol*> given;
    int position = 0;
    for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
        AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
        const int here = position++;
        const VhdlSymbol* portp = nullptr;
        if (!asp->formalp()) { portp = portAt(unitScopep, here); }
        const bool open = !asp->actualp() || VN_IS(asp->actualp(), VhdlOpen);
        if (asp->formalp()) {
            // Named whole, in part, or through a conversion: any mention of
            // the port associates it, unless what it is given is "open"
            asp->formalp()->foreach([&](const AstVhdlName* namep) {
                const VhdlSymbol* const symp = namep->vhdlSymp();
                if (symp && symp->kind() == VhdlSymKind::PORT) {
                    if (VN_IS(asp->formalp(), VhdlName)) portp = symp;
                    if (!open) given.insert(symp);
                }
            });
        }
        if (portp && !open) given.insert(portp);
        if (!portp || open) continue;
        const AstVhdlIfaceDecl* const formalp = VN_CAST(portp->declp(), VhdlIfaceDecl);
        if (!formalp) continue;
        // A port that takes its bounds from its actual cannot take them from a
        // conversion to a type that has none (6.5.6.3, 9.3.6): nvc 1.22.1 and
        // GHDL 7.0.0-dev refuse "foo => std_ulogic_vector(bar)", Questa 2025.2
        // takes it
        if (const AstVhdlCall* const cvp = VN_CAST(asp->actualp(), VhdlCall)) {
            // "x => f(s)" with s a signal is a conversion function on the
            // actual, whose one parameter is a constant (6.5.7.1): GHDL
            // 7.0.0-dev and Questa 2025.2 refuse a signal parameter there
            const VhdlSymbol* const fsymp
                = cvp->vhdlSymp() ? cvp->vhdlSymp()->resolved() : nullptr;
            const AstVhdlSubprogDecl* const fdeclp
                = fsymp && fsymp->kind() == VhdlSymKind::FUNCTION
                      ? VN_CAST(fsymp->declp(), VhdlSubprogDecl)
                      : nullptr;
            const AstVhdlAssoc* const onlyp = VN_CAST(cvp->argsp(), VhdlAssoc);
            const AstVhdlIfaceDecl* const fparamp = fdeclp ? fdeclp->paramsp() : nullptr;
            if (fparamp && !fparamp->nextp() && onlyp && !onlyp->nextp()
                && fparamp->kind() == VhdlIfaceKind::SIGNAL && !m_inSpecialized && onlyp->actualp()
                && objectOfName(onlyp->actualp()).cls == ObjClass::SIGNAL) {
                err(cvp->fileline(), "'" + fsymp->name()
                                         + "' stands as a conversion function in this port"
                                           " map, and its parameter must then be a constant,"
                                           " not signal '"
                                         + fparamp->prettyName() + "'");
            }
            const VhdlSymbol* const tsymp = cvp->prefixp() && cvp->prefixp()->vhdlSymp()
                                                ? cvp->prefixp()->vhdlSymp()->resolved()
                                                : nullptr;
            if (tsymp && tsymp->kind() == VhdlSymKind::TYPE && markLeavesIndexOpen(tsymp)
                && writtenBare(formalp->subtypep()) && formalp->subtypep()->typeMarkp()
                && markLeavesIndexOpen(formalp->subtypep()->typeMarkp()->vhdlSymp())
                && !m_inSpecialized) {
                err(cvp->fileline(), "Port '" + portp->name()
                                         + "' takes its bounds from its actual, and a"
                                           " conversion to "
                                         + tsymp->name() + ", which has none, gives it none");
            }
        }
        const ObjInfo info = objectOfName(asp->actualp());
        if (info.cls != ObjClass::SIGNAL || !info.symp) continue;
        const VhdlMode fmode = formalp->mode();
        const bool writes
            = fmode == VhdlMode::OUT || fmode == VhdlMode::INOUT || fmode == VhdlMode::BUFFER;
        if (writes && info.modeIn) {
            err(asp->actualp()->fileline(),
                "Port '" + info.symp->name() + "' is of mode in, so it cannot be given for port '"
                    + portp->name() + "', which writes it");
        } else if (asp->fileline()->language() < V3LangCode::L1076_2008
                   && (fmode == VhdlMode::INOUT || fmode == VhdlMode::BUFFER)
                   && info.symp->kind() == VhdlSymKind::PORT
                   && VN_IS(info.symp->declp(), VhdlIfaceDecl)
                   && VN_AS(info.symp->declp(), VhdlIfaceDecl)->mode() != fmode) {
            // Before VHDL-2008 an inout port is given only an inout port, and
            // a buffer port only a buffer port (VHDL-93 1.1.1.2)
            static const char* const modes[] = {"in", "out", "inout", "buffer", "linkage"};
            err(asp->actualp()->fileline(),
                "Port '" + info.symp->name() + "' is of mode "
                    + modes[static_cast<int>(VN_AS(info.symp->declp(), VhdlIfaceDecl)->mode())]
                    + ", and before VHDL-2008 a port of mode " + modes[static_cast<int>(fmode)]
                    + " is given only a port of its own mode: '" + portp->name() + "'");
        } else if (fmode == VhdlMode::IN && info.modeOut
                   && asp->fileline()->language() < V3LangCode::L1076_2008) {
            err(asp->actualp()->fileline(),
                "Port '" + info.symp->name()
                    + "' is of mode out, which cannot be read before VHDL-2008, so it cannot"
                      " be given for port '"
                    + portp->name() + "' of mode in");
        }
    }
    checkPartialAssociations(nodep);
    for (const auto& pair : unitScopep->direct()) {
        const VhdlSymbol* const symp = pair.second;
        if (symp->kind() != VhdlSymKind::PORT || given.count(symp)) continue;
        const AstVhdlIfaceDecl* const declp = VN_CAST(symp->declp(), VhdlIfaceDecl);
        if (!declp || declp->mode() != VhdlMode::IN || declp->defaultp()) continue;
        err(nodep->fileline(), "Port '" + symp->name()
                                   + "' of mode in has no default value, so it must be given"
                                     " something in the port map");
    }
}

// The object or generic an expression depends on that keeps it from being
// locally static (9.4.2), or nullptr. An attribute of an object asks about
// its subtype, which is as static as the declaration writes it.
const VhdlSymbol* VhdlSemVisitor::notLocallyStatic(const AstNode* exprsp) {
    const VhdlSymbol* foundp = nullptr;
    const auto varying = [](const VhdlSymbol* symp) {
        switch (symp->kind()) {
        case VhdlSymKind::GENERIC:
        case VhdlSymKind::GENERATE_PARAMETER:
        case VhdlSymKind::LOOP_PARAMETER:
        case VhdlSymKind::SIGNAL:
        case VhdlSymKind::PORT:
        case VhdlSymKind::VARIABLE:
        case VhdlSymKind::SHARED_VARIABLE: return true;
        case VhdlSymKind::CONSTANT: {
            // A constant is locally static only where its own value is, and
            // a call to a function of the design never is (9.4.2)
            const AstVhdlObjectDecl* const odp = VN_CAST(symp->declp(), VhdlObjectDecl);
            if (!odp || !odp->defaultp()) return false;
            bool called = false;
            odp->defaultp()->foreach([&](const AstVhdlCall* callp) {
                const VhdlSymbol* const fsymp
                    = callp->vhdlSymp() ? callp->vhdlSymp()->resolved() : nullptr;
                if (fsymp && fsymp->kind() == VhdlSymKind::FUNCTION && !fsymp->predefined()) {
                    called = true;
                }
            });
            return called;
        }
        default: return false;
        }
    };
    for (const AstNode* ep = exprsp; ep && !foundp; ep = ep->nextp()) {
        ep->foreach([&](const AstVhdlName* namep) {
            const VhdlSymbol* const symp
                = namep->vhdlSymp() ? namep->vhdlSymp()->resolved() : nullptr;
            if (foundp || !symp) return;
            const AstVhdlAttrName* const attrp = VN_CAST(parentOf(namep), VhdlAttrName);
            if (!(attrp && attrp->prefixp() == namep)) {
                if (varying(symp)) foundp = symp;
                return;
            }
            // "s'range": what the declaration of s constrains it with
            const AstNode* const declp = symp->declp();
            const AstVhdlSubtypeInd* sip = nullptr;
            if (const AstVhdlObjectDecl* const odp = VN_CAST(declp, VhdlObjectDecl)) {
                sip = odp->subtypep();
            } else if (const AstVhdlIfaceDecl* const ifp = VN_CAST(declp, VhdlIfaceDecl)) {
                sip = ifp->subtypep();
            }
            if (!sip) return;
            // An object whose bounds come from what it is given -- an
            // unconstrained parameter -- has no static range either
            if (writtenBare(sip) && sip->typeMarkp()
                && markLeavesIndexOpen(sip->typeMarkp()->vhdlSymp())) {
                foundp = symp;
                return;
            }
            sip->foreach([&](const AstVhdlName* innerp) {
                const VhdlSymbol* const isymp
                    = innerp->vhdlSymp() ? innerp->vhdlSymp()->resolved() : nullptr;
                if (!foundp && isymp && varying(isymp)) foundp = isymp;
            });
        });
    }
    return foundp;
}

// A port associated in parts -- "a(15 downto 0) => x, a(31 downto 16) => y"
// -- is associated once in every element, and no element twice (6.5.7.1):
// the parts together are the port's one association. Checked where the parts
// are indexes or slices with static bounds of a one-dimensional array.
void VhdlSemVisitor::checkPartialAssociations(AstVhdlInstance* nodep) {
    struct Part final {
        int64_t lo;
        int64_t hi;
        FileLine* flp;
    };
    // Per subelement written, by its text: the port and any record elements
    std::map<std::string, std::vector<Part>> parts;
    std::map<std::string, const VhdlType*> arrays;
    // Ports with a part written some other way -- "v(1)(3 downto 2)" -- whose
    // coverage this does not work out, so it says nothing about them
    std::set<std::string> unread;
    for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
        AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
        const AstVhdlCall* const callp = VN_CAST(asp->formalp(), VhdlCall);
        if (!callp || !callp->argsp() || callp->argsp()->nextp()) {
            if (asp->formalp() && !VN_IS(asp->formalp(), VhdlName)) {
                asp->formalp()->foreach([&](const AstVhdlName* namep) {
                    const VhdlSymbol* const symp = namep->vhdlSymp();
                    if (symp && symp->kind() == VhdlSymKind::PORT) unread.insert(symp->name());
                });
            }
            continue;
        }
        // The part's prefix is the port, or a record element of it
        std::string key;
        const AstNodeVhdlExpr* prefp = callp->prefixp();
        for (const AstNodeVhdlExpr* np = prefp; np;) {
            if (const AstVhdlName* const namep = VN_CAST(np, VhdlName)) {
                const VhdlSymbol* const symp = namep->vhdlSymp();
                if (!symp || symp->kind() != VhdlSymKind::PORT)
                    key.clear();
                else
                    key = symp->name() + key;
                break;
            }
            // An element of it at a static index: "v(1)(3 downto 2)"
            if (const AstVhdlCall* const idxp = VN_CAST(np, VhdlCall)) {
                const AstVhdlAssoc* const ap = VN_CAST(idxp->argsp(), VhdlAssoc);
                const VhdlValue iv = ap && !ap->nextp() && ap->actualp() && !ap->formalp()
                                         ? V3VhdlEval::eval(ap->actualp())
                                         : VhdlValue{};
                if (!iv.isInt()) {
                    key.clear();
                    break;
                }
                key = "(" + std::to_string(iv.toInt()) + ")" + key;
                np = idxp->prefixp();
                continue;
            }
            const AstVhdlSelected* const selp = VN_CAST(np, VhdlSelected);
            const AstVhdlName* const sufp = selp ? VN_CAST(selp->suffixp(), VhdlName) : nullptr;
            if (!sufp) {
                key.clear();
                break;
            }
            key = "." + sufp->name() + key;
            np = selp->prefixp();
        }
        // The part is named by a locally static name (6.5.7.1). A conversion
        // written on the formal, "real(o) => x", names no part at all.
        if (!key.empty() && !m_inSpecialized) {
            if (const VhdlSymbol* const varyp = notLocallyStatic(callp->argsp())) {
                err(callp->fileline(), "A port associated in parts names each part with a"
                                       " locally static name, and this one depends on "
                                           + std::string{varyp->kindName()} + " '" + varyp->name()
                                           + "'");
                unread.insert(key.substr(0, key.find_first_of(".(")));
                continue;
            }
        }
        const VhdlType* const arrayp = prefp->vhdlTypep();
        if (key.empty() || !arrayp || !arrayp->isConstrainedArray() || arrayp->dimensions() != 1) {
            callp->foreach([&](const AstVhdlName* namep) {
                const VhdlSymbol* const symp = namep->vhdlSymp();
                if (symp && symp->kind() == VhdlSymKind::PORT) unread.insert(symp->name());
            });
            continue;
        }
        // A part whose bounds do not fold here says nothing about coverage
        const auto unknownPart = [&]() { unread.insert(key.substr(0, key.find_first_of(".("))); };
        int64_t lo = 0;
        int64_t hi = 0;
        if (AstVhdlRange* const rp = VN_CAST(callp->argsp(), VhdlRange)) {
            // "d(s'range)" is a range too, written as an attribute
            const RangeInfo info = analyzeRange(rp, nullptr);
            if (!info.left.isInt() || !info.right.isInt()) {
                unknownPart();
                continue;
            }
            lo = std::min(info.left.toInt(), info.right.toInt());
            hi = std::max(info.left.toInt(), info.right.toInt());
        } else if (const AstVhdlAssoc* const ip = VN_CAST(callp->argsp(), VhdlAssoc)) {
            if (!ip->actualp()) continue;
            const VhdlValue v = V3VhdlEval::eval(ip->actualp());
            if (!v.isInt()) {
                unknownPart();
                continue;
            }
            lo = hi = v.toInt();
        } else {
            unknownPart();
            continue;
        }
        parts[key].push_back({lo, hi, asp->fileline()});
        arrays[key] = arrayp;
    }
    // An element associated in parts of its own -- "v(2)(7 downto 4)" -- is
    // one the array around it has associated, as far as that array goes
    std::vector<std::pair<std::string, Part>> inner;
    for (const auto& pair : parts) {
        const size_t open = pair.first.rfind('(');
        if (open == std::string::npos || pair.first.back() != ')') continue;
        const std::string outer = pair.first.substr(0, open);
        if (!parts.count(outer) || pair.second.empty()) continue;
        const int64_t idx = std::strtoll(pair.first.c_str() + open + 1, nullptr, 10);
        inner.emplace_back(outer, Part{idx, idx, pair.second.front().flp});
    }
    for (const auto& pair : inner) parts[pair.first].push_back(pair.second);
    for (auto& pair : parts) {
        if (unread.count(pair.first.substr(0, pair.first.find_first_of(".(")))) continue;
        std::vector<Part>& list = pair.second;
        std::sort(list.begin(), list.end(),
                  [](const Part& a, const Part& b) { return a.lo < b.lo; });
        const VhdlType* const indexp = arrays[pair.first]->indexTypes()[0];
        int64_t next = indexp->low();
        for (size_t i = 0; i < list.size(); ++i) {
            if (i && list[i].lo <= list[i - 1].hi) {
                err(list[i].flp, "Element " + std::to_string(list[i].lo) + " of port '"
                                     + pair.first + "' is associated more than once");
                next = indexp->high() + 1;  // One report is enough
                break;
            }
            if (list[i].lo > next) break;
            next = list[i].hi + 1;
        }
        if (next <= indexp->high()) {
            err(nodep->fileline(), "Port '" + pair.first + "' is associated in parts, but element "
                                       + std::to_string(next)
                                       + " is in none of them; every element must be");
        }
    }
}

// A generic with no default value of its own must be given one by the
// instance, since nothing else will (6.5.6.2)
void VhdlSemVisitor::checkGenericAssociations(AstVhdlInstance* nodep, VhdlScope* unitScopep) {
    if (!unitScopep) return;
    std::set<const VhdlSymbol*> given;
    std::set<int> positions;
    int position = 0;
    for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
        const AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
        if (!asp) continue;
        const bool open = !asp->actualp() || VN_IS(asp->actualp(), VhdlOpen);
        if (!asp->formalp()) {
            if (!open) positions.insert(position);
            ++position;
            continue;
        }
        if (open) continue;
        asp->formalp()->foreach([&](const AstVhdlName* namep) {
            if (namep->vhdlSymp()) given.insert(namep->vhdlSymp());
        });
        if (const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName)) {
            for (const auto& pair : unitScopep->direct()) {
                if (pair.second->kind() == VhdlSymKind::GENERIC
                    && pair.second->name() == namep->name()) {
                    given.insert(pair.second);
                }
            }
        }
    }
    for (const auto& pair : unitScopep->direct()) {
        const VhdlSymbol* const symp = pair.second;
        if (symp->kind() != VhdlSymKind::GENERIC || given.count(symp)) continue;
        // One synthesized to carry another's bounds is never written
        if (symp->position() < 0 || positions.count(symp->position())) continue;
        const AstVhdlIfaceDecl* const declp = VN_CAST(symp->declp(), VhdlIfaceDecl);
        if (!declp || declp->defaultp() || declp->kind() != VhdlIfaceKind::CONSTANT) continue;
        err(nodep->fileline(), "Generic '" + symp->name()
                                   + "' has no default value, so it must be given one in the"
                                     " generic map");
    }
}

// The port at that position of an entity or component's clause
const VhdlSymbol* VhdlSemVisitor::portAt(VhdlScope* scopep, int position) {
    for (const auto& pair : scopep->direct()) {
        VhdlSymbol* const symp = pair.second;
        if (symp->kind() == VhdlSymKind::PORT && symp->position() == position) return symp;
    }
    return nullptr;
}

// "for u1 : comp use entity work.thing" says which entity an instance is
// built from (7.3.1). Where that entity has a generic type, the copy made for
// this instance has to be of the right entity, so the binding is recorded as
// the architecture's declarations are analyzed -- which is before any of its
// statements, so every instance sees it.
// A configuration specification names a component, and instances of it in
// the region it is written in, each configured once: by label, or all, or
// the others, with "all" and "others" said once per component (7.3.1). What
// it binds them to is an entity, a configuration or open (7.3.2.2).
void VhdlSemVisitor::checkConfigSpecs(const AstNode* declsp, const AstNode* stmtsp) {
    if (m_inSpecialized) return;
    // The component each instance of the region instantiates
    std::map<std::string, std::string> instComp;
    std::set<std::string> instAny;
    for (const AstNode* sp = stmtsp; sp; sp = sp->nextp()) {
        const AstVhdlInstance* const instp = VN_CAST(sp, VhdlInstance);
        if (!instp || instp->label().empty()) continue;
        instAny.insert(instp->label());
        std::vector<std::string> parts;
        if ((instp->kind() == "component" || instp->kind().empty())
            && flatten(instp->unitp(), parts) && !parts.empty()) {
            instComp[instp->label()] = parts.back();
        }
    }
    std::set<std::string> configured;
    std::set<std::string> closed;  // "component" once all/others is said
    for (const AstNode* dp = declsp; dp; dp = dp->nextp()) {
        const AstVhdlConfigSpec* const csp = VN_CAST(dp, VhdlConfigSpec);
        const AstVhdlEntitySpec* const specp = csp ? csp->specp() : nullptr;
        if (!specp || specp->entityClass() != "component" || !specp->namesp()) continue;
        // The component is the last name; the labels come before it
        std::vector<const AstNode*> names;
        for (const AstNode* np = specp->namesp(); np; np = np->nextp()) names.push_back(np);
        const AstNodeVhdlExpr* const compp = VN_CAST(names.back(), NodeVhdlExpr);
        names.pop_back();
        std::vector<std::string> cparts;
        if (!compp || !flatten(compp, cparts) || cparts.empty()) continue;
        const std::string comp = cparts.back();
        std::vector<VhdlSymbol*> found;
        lookup(comp, found);
        bool isComp = false;
        for (const VhdlSymbol* const symp : found) {
            if (symp->resolved()->kind() == VhdlSymKind::COMPONENT) isComp = true;
        }
        if (!isComp) {
            err(compp->fileline(),
                "A configuration specification names a component, and '" + comp + "' is not one");
            continue;
        }
        if (specp->all() || specp->others()) {
            if (!closed.insert(comp).second) {
                err(csp->fileline(), "Component " + comp
                                         + " is already given an 'all' or"
                                           " 'others' configuration specification here");
            }
        }
        for (const AstNode* np : names) {
            const AstVhdlName* const lp = VN_CAST(np, VhdlName);
            if (!lp) continue;
            const std::string label = lp->name();
            const auto it = instComp.find(label);
            if (it == instComp.end()) {
                err(lp->fileline(),
                    "'" + label
                        + (instAny.count(label) ? "' is not an instance of a component"
                                                : "' labels no component instance in this"
                                                  " region"));
                continue;
            }
            if (it->second != comp) {
                err(lp->fileline(),
                    "Instance '" + label + "' is of component " + it->second + ", not " + comp);
                continue;
            }
            if (!configured.insert(label).second) {
                err(lp->fileline(), "Instance '" + label + "' is configured more than once");
            }
        }
        // A configuration aspect names a configuration it can see (7.3.2.2)
        const AstVhdlBinding* const bindp = csp->bindingp();
        if (bindp && bindp->kind() == "configuration") {
            const AstVhdlName* const cnamep = VN_CAST(bindp->unitp(), VhdlName);
            std::vector<VhdlSymbol*> cfound;
            if (cnamep) lookup(cnamep->name(), cfound);
            if (cnamep && cfound.empty()) {
                err(cnamep->fileline(), "No configuration '" + cnamep->name()
                                            + "' is visible here for this binding to name");
            }
        }
        if (bindp && bindp->kind() == "entity" && bindp->unitp()) {
            const AstNodeVhdlExpr* entp = bindp->unitp();
            if (const AstVhdlCall* const callp = VN_CAST(entp, VhdlCall)) entp = callp->prefixp();
            std::vector<std::string> eparts;
            if (entp && flatten(entp, eparts) && eparts.size() >= 2) {
                const std::string lib = eparts[0] == "work" ? m_libname : eparts[0];
                const auto uit = m_primaryUnits.find(lib + "." + eparts.back());
                if (uit != m_primaryUnits.end() && !VN_IS(uit->second, VhdlEntity)) {
                    err(entp->fileline(), "An entity aspect names an entity, and '" + eparts[0]
                                              + "." + eparts.back() + "' is not one");
                }
            }
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlConfigSpec* nodep) {
    AstVhdlEntitySpec* const specp = nodep->specp();
    AstVhdlBinding* const bindingp = nodep->bindingp();
    if (!specp || !bindingp || !bindingp->unitp()) return;
    std::vector<std::string> parts;
    if (!flatten(bindingp->unitp(), parts) || parts.empty()) return;
    const std::string entity = parts.back();
    std::vector<std::string> names;
    for (AstNode* p = specp->namesp(); p; p = p->nextp()) {
        if (const AstVhdlName* const namep = VN_CAST(p, VhdlName)) names.push_back(namep->name());
    }
    if (specp->entityClass() == "component" && !names.empty()) {
        m_specBindings[names.back()] = entity;
        names.pop_back();
    }
    if (specp->all() || specp->others()) return;
    for (const std::string& label : names) m_specBindings[label] = entity;
}

// An instance of an entity that was copied for its generic types names the
// copy. Every generic it still passes is named rather than positional: the
// generic types stay in the copy's clause, so that their names go on meaning
// something inside it, but they are not values, and a pin numbered by
// position would count them.
void VhdlSemVisitor::retargetInstance(AstVhdlInstance* nodep, const std::string& newName,
                                      const std::vector<AstVhdlIfaceDecl*>& generics,
                                      const std::vector<AstVhdlAssoc*>& assocs) {
    nodep->specModule(newName);
    for (size_t i = 0; i < generics.size(); ++i) {
        AstVhdlAssoc* const asp = assocs[i];
        if (!asp) continue;
        if (generics[i]->kind() == VhdlIfaceKind::TYPE
            || generics[i]->kind() == VhdlIfaceKind::PACKAGE
            || generics[i]->kind() == VhdlIfaceKind::SUBPROGRAM) {
            asp->unlinkFrBack();
            VL_DO_DANGLING(asp->deleteTree(), asp);
            continue;
        }
        if (!asp->formalp()) {
            asp->formalp(new AstVhdlName{asp->fileline(), generics[i]->name()});
        }
    }
}

// One design unit of a copy made for a generic type. An entity and the
// architecture bound to it are copied together, and each is analyzed exactly
// as the original was -- in a region built from its own context clause, so
// the copy sees the names the original saw and nothing else.
void VhdlSemVisitor::analyzeSpecializedUnit(AstNodeVhdlUnit* unitp) {
    VL_RESTORER(m_scopep);
    VL_RESTORER(m_rootp);
    VL_RESTORER(m_curUnitp);
    VL_RESTORER_COPY(m_libname);
    VL_RESTORER(m_ifacePos);
    setupUnitContext(unitp);
    const std::string lib = m_libname;
    VhdlScope* const unitScopep = pushScope(unitp->name());
    unitScopep->prefixes(lib + ":" + lower(unitp->name()), lib + ":" + lower(unitp->name()));
    if (AstVhdlEntity* const entp = VN_CAST(unitp, VhdlEntity)) {
        openPortGenerics(entp->genericsp(), entp, unitScopep);
        openPortGenerics(entp->portsp(), entp, unitScopep);
        m_ifacePos = 0;
        iterateAndNextNull(entp->genericsp());
        fillOpenGenericDefaults(unitScopep);
        m_ifacePos = 0;
        iterateAndNextNull(entp->portsp());
        iterateAndNextNull(entp->declsp());
        m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
        m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
        libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        return;
    }
    AstVhdlArchitecture* const archp = VN_AS(unitp, VhdlArchitecture);
    std::vector<std::string> parts;
    if (flatten(archp->entityNamep(), parts) && !parts.empty()) {
        const auto it = m_unitScopes.find(lib + "." + parts.back());
        if (it != m_unitScopes.end()) m_scopep->addUse(it->second);
        unitScopep->prefixes(parts.back() + "(" + lower(archp->name()) + ")", parts.back());
        m_unitScopes[lib + "." + parts.back() + "(" + lower(archp->name()) + ")"] = unitScopep;
    }
    iterateAndNextNull(archp->declsp());
    iterateAndNextNull(archp->stmtsp());
    analyzeDrivers(archp);
}

// "generic (type T)" is not a value an instance passes but a type it chooses,
// and a different choice is a different entity (6.5.5). One copy of the
// entity and of its architecture is made per set of types, so what reaches
// the back end is an ordinary entity whose ports have ordinary types --
// which is how VHDL elaboration describes it, and what keeps the generated
// model as fast as one written out by hand.
//
// Returns the copy's own region, so that the association checks that follow
// see the ports as the copy has them, or null when nothing was specialized.
VhdlScope* VhdlSemVisitor::specializeEntity(AstVhdlInstance* nodep, const std::string& entKey) {
    const auto unitIt = m_primaryUnits.find(entKey);
    if (unitIt == m_primaryUnits.end()) return nullptr;
    AstVhdlEntity* const entp = VN_CAST(unitIt->second, VhdlEntity);
    if (!entp) return nullptr;
    std::vector<AstVhdlIfaceDecl*> generics;
    bool anyOpen = false;
    for (AstNode* p = entp->genericsp(); p; p = p->nextp()) {
        AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        generics.push_back(ifp);
        const bool isKind = ifp->kind() == VhdlIfaceKind::TYPE
                            || ifp->kind() == VhdlIfaceKind::PACKAGE
                            || ifp->kind() == VhdlIfaceKind::SUBPROGRAM;
        if (isKind && !ifp->isBound()) anyOpen = true;
    }
    if (!anyOpen) return nullptr;
    // Which actual each generic is given, whether by name or by position
    std::vector<AstNodeVhdlExpr*> actuals(generics.size(), nullptr);
    std::vector<AstVhdlAssoc*> assocs(generics.size(), nullptr);
    size_t position = 0;
    for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
        AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
        if (!asp) continue;
        size_t index = generics.size();
        if (asp->formalp()) {
            std::vector<std::string> fparts;
            if (flatten(asp->formalp(), fparts) && !fparts.empty()) {
                for (size_t i = 0; i < generics.size(); ++i) {
                    if (generics[i]->name() == fparts.back()) index = i;
                }
            }
        } else {
            index = position++;
        }
        if (index < generics.size()) {
            actuals[index] = asp->actualp();
            assocs[index] = asp;
        }
    }
    std::vector<VhdlType*> actualTypes;
    std::vector<VhdlScope*> actualPackages;
    std::vector<VhdlSymbol*> actualSubs;
    for (size_t i = 0; i < generics.size(); ++i) {
        if (generics[i]->kind() == VhdlIfaceKind::SUBPROGRAM) {
            // A generic subprogram the instance says nothing about is
            // whatever is visible where the instance is written, which the
            // copy sees anyway ("is <>", 6.5.5)
            if (!actuals[i]) continue;
            VhdlSymbol* const subp = resolveSubprogramActual(actuals[i], generics[i]);
            if (!subp) return nullptr;  // Leaves the honest report
            actualSubs.push_back(subp);
            continue;
        }
        if (generics[i]->kind() == VhdlIfaceKind::TYPE) {
            if (!actuals[i]) {
                err(nodep->fileline(),
                    "Generic type '" + generics[i]->name() + "' of the instantiated entity '"
                        + entp->name() + "' is given no type");
                return nullptr;
            }
            VhdlType* const typep = resolveTypeActual(actuals[i]);
            if (!typep || typep->isError()) return nullptr;
            actualTypes.push_back(typep);
            continue;
        }
        if (generics[i]->kind() != VhdlIfaceKind::PACKAGE) continue;
        // A generic package is given a package that has been instantiated,
        // so that its types and constants are settled (6.5.5)
        VhdlScope* scopep = nullptr;
        if (actuals[i]) {
            std::vector<std::string> aparts;
            std::vector<VhdlSymbol*> found;
            if (flatten(actuals[i], aparts) && !aparts.empty()) lookup(aparts.back(), found);
            for (VhdlSymbol* const psymp : found) {
                if (psymp->kind() == VhdlSymKind::PACKAGE && psymp->innerp()) {
                    scopep = psymp->innerp();
                    break;
                }
            }
            // A package instantiated as a design unit of its own is not a
            // declaration of the region the instance is written in: it is
            // reached by its library name, "work.complex_real" (4.9, 13.2)
            if (!scopep && !aparts.empty()) {
                std::string lib = aparts.size() >= 2 ? lower(aparts[aparts.size() - 2])
                                                     : std::string{"work"};
                if (lib == "work") lib = m_libname;
                const auto unitIt2 = m_unitScopes.find(lib + "." + lower(aparts.back()));
                if (unitIt2 != m_unitScopes.end()) scopep = unitIt2->second;
            }
        }
        if (!scopep) return nullptr;  // Leaves the honest "cannot be lowered"
        actualPackages.push_back(scopep);
    }
    // The same actuals ask for the copy that was already made
    for (const VhdlEntitySpec& spec : m_entitySpecs) {
        if (spec.key != entKey || spec.types != actualTypes) continue;
        if (spec.packages != actualPackages) continue;
        if (spec.subprograms != actualSubs) continue;
        retargetInstance(nodep, spec.name, generics, assocs);
        return spec.scopep;
    }
    AstVhdlArchitecture* const archp = m_archUnits.count(entKey) ? m_archUnits[entKey] : nullptr;
    if (!archp) {
        err(nodep->fileline(), "Entity '" + entp->name()
                                   + "' has a generic type but no architecture to copy");
        return nullptr;
    }
    // A copy may instantiate an entity that is copied in its turn. VHDL has
    // no recursive instantiation, so the chain is finite; the bound is here
    // only so that a malformed design stops rather than filling memory, and
    // stopping leaves the ordinary "cannot be lowered yet" to be reported.
    if (m_specDepth > 32) return nullptr;
    const std::string newName = entp->name() + "__g" + std::to_string(++m_specUnitCount);
    AstVhdlEntity* const entCopyp = entp->cloneTree(false);
    entCopyp->name(newName);
    VhdlUnanalyzeVisitor{entCopyp};
    // A generic subprogram or package the instance said nothing about is
    // whatever is visible where the instance is written, which is what
    // "is <>" asks for. In the copy that is the ordinary visible one, so the
    // generic is settled and nothing declares it again (6.5.5).
    {
        size_t index = 0;
        for (AstNode* p = entCopyp->genericsp(); p; p = p->nextp(), ++index) {
            AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
            const bool isSub = ifp->kind() == VhdlIfaceKind::SUBPROGRAM;
            if (isSub && index < actuals.size() && !actuals[index]) ifp->bound(true);
        }
    }
    AstVhdlArchitecture* const archCopyp = archp->cloneTree(false);
    VhdlUnanalyzeVisitor{archCopyp};
    {
        AstNodeVhdlExpr* const oldp = archCopyp->entityNamep();
        AstVhdlName* const newp = new AstVhdlName{oldp->fileline(), newName};
        oldp->replaceWith(newp);
        VL_DO_DANGLING(oldp->deleteTree(), oldp);
    }
    entp->addNextHere(entCopyp);
    archp->addNextHere(archCopyp);
    V3VhdlSem::addSpecializedUnit(entCopyp);
    V3VhdlSem::addSpecializedUnit(archCopyp);
    VhdlScope* copyScopep = nullptr;
    {
        VL_RESTORER_COPY(m_boundGenTypes);
        VL_RESTORER_COPY(m_boundGenPackages);
        VL_RESTORER_COPY(m_boundGenSubs);
        m_boundGenTypes.clear();
        m_boundGenPackages.clear();
        m_boundGenSubs.clear();
        size_t nextType = 0;
        size_t nextPkg = 0;
        size_t nextSub = 0;
        for (size_t i = 0; i < generics.size(); ++i) {
            if (generics[i]->kind() == VhdlIfaceKind::TYPE) {
                m_boundGenTypes[generics[i]->name()] = actualTypes[nextType++];
            } else if (generics[i]->kind() == VhdlIfaceKind::PACKAGE) {
                m_boundGenPackages[generics[i]->name()] = actualPackages[nextPkg++];
            } else if (generics[i]->kind() == VhdlIfaceKind::SUBPROGRAM && actuals[i]) {
                m_boundGenSubs[generics[i]->name()] = actualSubs[nextSub++];
            }
        }
        ++m_specDepth;
        analyzeSpecializedUnit(entCopyp);
        const std::string copyKey
            = (entCopyp->libname().empty() ? "work" : lower(entCopyp->libname())) + "."
              + lower(newName);
        const auto copyIt = m_unitScopes.find(copyKey);
        if (copyIt != m_unitScopes.end()) copyScopep = copyIt->second;
        m_boundGenTypes.clear();
        m_boundGenPackages.clear();
        m_boundGenSubs.clear();
        analyzeSpecializedUnit(archCopyp);
        --m_specDepth;
    }
    m_entitySpecs.push_back(
        VhdlEntitySpec{entKey, actualTypes, actualPackages, actualSubs, newName, copyScopep});
    retargetInstance(nodep, newName, generics, assocs);
    return copyScopep;
}

// The bounds a default value gives an unconstrained object: its own, where it
// has them, and otherwise its length starting at the index subtype's own left
// bound (9.3.3.3)
bool VhdlSemVisitor::defaultBounds(VhdlSymbol* symp, AstNodeVhdlExpr* valuep, int dim,
                                   int64_t& leftr, int64_t& rightr) {
    VhdlType* const valueTypep = valuep->vhdlTypep();
    if (valueTypep && valueTypep->isConstrainedArray()
        && dim < static_cast<int>(valueTypep->indexTypes().size())) {
        const VhdlType* const idxp = valueTypep->indexTypes()[dim];
        if (idxp && idxp->bounded()) {
            leftr = idxp->left();
            rightr = idxp->right();
            return true;
        }
    }
    if (dim != 0) return false;
    VhdlType* const typep = symp->typep();
    if (!typep) return false;
    const std::vector<VhdlType*>& baseIndexes = typep->base()->indexTypes();
    if (baseIndexes.empty() || !baseIndexes[0] || !baseIndexes[0]->bounded()) return false;
    const int64_t len = staticLength(valuep);
    if (len < 0) return false;
    leftr = baseIndexes[0]->left();
    rightr = baseIndexes[0]->ascending() ? leftr + len - 1 : leftr - len + 1;
    return true;
}

// A generic left unconstrained but given a default takes its bounds from that
// default wherever an instance leaves it open -- including at the top, where
// there is no instance at all
void VhdlSemVisitor::fillOpenGenericDefaults(VhdlScope* scopep) {
    const auto it = m_openBounds.find(scopep);
    if (it == m_openBounds.end()) return;
    for (const OpenBound& bound : it->second) {
        if (!bound.isGeneric) continue;
        std::vector<VhdlSymbol*> found;
        scopep->lookupLocal(bound.port, found);
        if (found.empty()) continue;
        AstVhdlIfaceDecl* const declp = VN_CAST(found[0]->declp(), VhdlIfaceDecl);
        if (!declp || !declp->defaultp()) continue;
        int64_t left = 0;
        int64_t right = 0;
        if (!defaultBounds(found[0], declp->defaultp(), bound.dim, left, right)) continue;
        for (int side = 0; side < 2; ++side) {
            std::vector<VhdlSymbol*> gfound;
            scopep->lookupLocal(side == 0 ? bound.leftName : bound.rightName, gfound);
            if (gfound.empty()) continue;
            AstVhdlIfaceDecl* const genp = VN_CAST(gfound[0]->declp(), VhdlIfaceDecl);
            if (!genp || genp->defaultp()) continue;
            const int64_t value = side == 0 ? left : right;
            AstVhdlLiteral* const litp = new AstVhdlLiteral{
                genp->fileline(), VhdlLitKind::INTEGER, std::to_string(value)};
            litp->intVal(value);
            genp->defaultp(litp);
            analyzeExpr(litp, V3VhdlStd::integerType());
            // Deliberately not given to the symbol as a value: it is what an
            // instance that says nothing gets, and folding it here would fix
            // every instance to the default's bounds
        }
    }
}

// Whether the instance's generic map already names that formal, so that a
// bound worked out here is not written into it twice
static bool genericMapNames(const AstVhdlInstance* nodep, const std::string& name) {
    for (AstNode* p = nodep->genericMapp(); p; p = p->nextp()) {
        const AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
        const AstVhdlName* const namep = asp ? VN_CAST(asp->formalp(), VhdlName) : nullptr;
        if (namep && namep->name() == name) return true;
    }
    return false;
}

void VhdlSemVisitor::bindOpenGenericBounds(AstVhdlInstance* nodep, VhdlScope* unitScopep) {
    if (!unitScopep) return;
    const auto it = m_openBounds.find(unitScopep);
    if (it == m_openBounds.end()) return;
    for (const OpenBound& bound : it->second) {
        if (!bound.isGeneric) continue;
        std::vector<VhdlSymbol*> found;
        unitScopep->lookupLocal(bound.port, found);
        if (found.empty()) continue;
        const int wantPos = found[0]->position();
        AstNodeVhdlExpr* actualp = nullptr;
        int position = 0;
        for (AstNode* p = nodep->genericMapp(); p; p = p->nextp(), ++position) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (!asp->actualp()) continue;
            const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
            if (namep ? namep->name() == bound.port : position == wantPos) {
                actualp = asp->actualp();
                break;
            }
        }
        // Left open, so the default the entity gives it decides the bounds
        if (!actualp) {
            AstVhdlIfaceDecl* const declp = VN_CAST(found[0]->declp(), VhdlIfaceDecl);
            if (declp) actualp = declp->defaultp();
        }
        if (!actualp) continue;
        int64_t left = 0;
        int64_t right = 0;
        if (!defaultBounds(found[0], actualp, bound.dim, left, right)) continue;
        FileLine* const flp = nodep->fileline();
        if (genericMapNames(nodep, bound.leftName)) continue;
        for (int side = 0; side < 2; ++side) {
            const int64_t value = side == 0 ? left : right;
            AstVhdlLiteral* const litp
                = new AstVhdlLiteral{flp, VhdlLitKind::INTEGER, std::to_string(value)};
            litp->intVal(value);
            AstVhdlAssoc* const asp = new AstVhdlAssoc{
                flp, new AstVhdlName{flp, side == 0 ? bound.leftName : bound.rightName}, litp};
            nodep->addGenericMapp(asp);
            std::vector<VhdlSymbol*> gfound;
            unitScopep->lookupLocal(asp->formalp()->name(), gfound);
            if (!gfound.empty()) asp->formalp()->vhdlSymp(gfound[0]);
            analyzeExpr(litp, V3VhdlStd::integerType());
        }
    }
}

// A port left unconstrained takes the subtype of the actual it is connected
// to (6.5.6.3). Those bounds are not the same at every instance, so each open
// dimension becomes a generic of the entity and the port is constrained by
// it. V3Param then builds one module per set of bounds -- exactly what it
// does for a width generic written by hand, and what VHDL elaboration says
// should happen.
// A STRING is text here rather than a vector of characters (16.3)
static bool isStdStringType(const VhdlType* typep) {
    const VhdlType* const strp = V3VhdlStd::stringType();
    return typep && strp && typep->base() == strp->base();
}

// Whether an element type can live inside a packed array, which decides
// whether a record becomes one machine word or a C++ structure
static bool packableElement(const VhdlType* typep, int depth = 0) {
    if (!typep || depth > 16) return false;
    if (isStdStringType(typep)) return false;
    const VhdlType* const basep = typep->base();
    switch (basep->kind()) {
    case VhdlTypeKind::ENUM: return true;
    case VhdlTypeKind::ARRAY:
        return basep->elementp() && packableElement(basep->elementp(), depth + 1);
    default: return false;
    }
}

// Whether a record whose open elements an instance would settle can be a
// packed structure. It matters here and not only in the lowering: a packed
// one is a vector, so a port of it connects to the actual by width whatever
// the instance made its elements; an unpacked one is a named C++ structure,
// and a structure whose shape a generic decides is a different structure in
// the module that declares the port from the one that connects to it. That
// is the case this cannot do, so those ports keep their honest error.
static bool recordWouldPack(const VhdlType* typep, int depth = 0) {
    if (!typep || depth > 16) return false;
    for (const VhdlSymbol* const elemp : typep->base()->elements()) {
        const VhdlType* const etypep = elemp->typep();
        if (!etypep) return false;
        switch (etypep->base()->kind()) {
        case VhdlTypeKind::ENUM:
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::UNIVERSAL_INTEGER:
        case VhdlTypeKind::PHYSICAL: break;
        case VhdlTypeKind::ARRAY:
            // Not asked whether it is constrained: constraining it is what
            // this is for
            if (isStdStringType(etypep)) return false;
            if (!etypep->base()->elementp()) return false;
            if (!packableElement(etypep->base()->elementp(), depth + 1)) return false;
            break;
        case VhdlTypeKind::RECORD:
            if (!recordWouldPack(etypep, depth + 1)) return false;
            break;
        default: return false;
        }
    }
    return true;
}

void VhdlSemVisitor::openPortGenerics(AstNode* portsp, AstNodeVhdl* unitp, VhdlScope* scopep) {
    AstVhdlEntity* const entp = VN_CAST(unitp, VhdlEntity);
    AstVhdlComponentDecl* const compp = VN_CAST(unitp, VhdlComponentDecl);
    if (!entp && !compp) return;
    for (AstNode* p = portsp; p; p = p->nextp()) {
        AstVhdlIfaceDecl* const ifp = VN_CAST(p, VhdlIfaceDecl);
        if (!ifp
            || (ifp->kind() != VhdlIfaceKind::SIGNAL
                && ifp->kind() != VhdlIfaceKind::CONSTANT)) {
            continue;
        }
        AstVhdlSubtypeInd* const subp = ifp->subtypep();
        if (!subp || subp->constraintp() || !subp->typeMarkp()) continue;
        VhdlSymbol* const tsymp = lookupTypeQuiet(subp->typeMarkp());
        VhdlType* const typep = tsymp ? tsymp->typep() : nullptr;
        if (!typep) continue;
        // An array port, including an array of arrays, is a shape the back
        // end builds from a range and nothing more, so bounds an instance
        // settles reach it. A record is a structure with a name, and one
        // whose element length a generic decides is a different structure
        // per instance -- which the actual, whose length the text fixed, is
        // not. That one keeps the message it had.
        if (typep->base()->kind() != VhdlTypeKind::ARRAY
            && !(typep->base()->kind() == VhdlTypeKind::RECORD && recordWouldPack(typep))) {
            continue;
        }
        std::vector<std::string> path;
        if (AstVhdlIndexConstraint* const consp
            = openConstraintFor(typep, ifp, entp, compp, scopep, path)) {
            subp->constraintp(consp);
        }
    }
}

// The constraint a port needs so that every dimension inside its type that
// nothing has constrained takes its bounds from the actual. 5.3.2.2 e) says
// this of "an interface object of an array type, or a subelement of an
// interface object for which the subelement type is an array type", so a
// record element and an array's own element are reached the same way its top
// level is. Each open dimension gets a pair of integer generics, which
// bindOpenBounds then associates with the actual's bounds and V3Param folds
// per instance.
AstVhdlIndexConstraint* VhdlSemVisitor::openConstraintFor(VhdlType* typep,
                                                          AstVhdlIfaceDecl* ifp,
                                                          AstVhdlEntity* entp,
                                                          AstVhdlComponentDecl* compp,
                                                          VhdlScope* scopep,
                                                          std::vector<std::string>& path) {
    if (!typep || path.size() > 8) return nullptr;
    FileLine* const flp = ifp->fileline();
    VhdlType* const basep = typep->base();

    // A name for the generics that says where in the type they came from, so
    // that two open elements of one port do not collide
    const auto mangled = [&](size_t d) {
        std::string out = "__" + ifp->name();
        for (const std::string& step : path) out += "__" + (step.empty() ? "e" : step);
        return out + "__" + std::to_string(d);
    };
    const auto makeGeneric = [&](const std::string& name, const char* mark = "integer",
                                const char* dflt = nullptr) {
        AstVhdlIfaceDecl* const genp = new AstVhdlIfaceDecl{flp, name, VhdlIfaceKind::CONSTANT};
        genp->subtypep(new AstVhdlSubtypeInd{flp, new AstVhdlName{flp, mark}});
        // The two bounds have no sensible default -- an instance must say
        // them -- but the direction does: a vector is written "downto" far
        // more often than "to", which is the answer this had before an
        // instance was asked, and it keeps a binding this does not reach
        // giving no answer at all
        if (dflt) genp->defaultp(new AstVhdlName{flp, dflt});
        if (ifp->kind() == VhdlIfaceKind::CONSTANT) {
            // A generic constrained by these must see them declared first, so
            // they go in ahead of it
            ifp->addHereThisAsNext(genp);
        } else if (entp) {
            entp->addGenericsp(genp);
        } else {
            compp->addGenericsp(genp);
        }
    };

    if (basep->kind() == VhdlTypeKind::RECORD) {
        AstVhdlIndexConstraint* consp = nullptr;
        // The subtype's own elements, since a subtype may already have
        // constrained one of them and that one is not open (5.3.3)
        for (VhdlSymbol* const elemp : typep->elements()) {
            path.push_back(elemp->name());
            AstVhdlIndexConstraint* const innerp
                = openConstraintFor(elemp->typep(), ifp, entp, compp, scopep, path);
            path.pop_back();
            if (!innerp) continue;
            if (!consp) consp = new AstVhdlIndexConstraint{flp};
            // A record constraint names an element and how to constrain it,
            // which the parser records as a call (5.3.3)
            AstVhdlCall* const callp
                = new AstVhdlCall{flp, new AstVhdlName{flp, elemp->name()}};
            if (AstNode* const rangesp = innerp->rangesp()) {
                callp->addArgsp(VN_AS(rangesp->unlinkFrBackWithNext(), NodeVhdl));
            }
            VL_DO_DANGLING(innerp->deleteTree(), innerp);
            consp->addRangesp(callp);
        }
        return consp;
    }
    if (basep->kind() != VhdlTypeKind::ARRAY) return nullptr;

    AstVhdlIndexConstraint* consp = nullptr;
    // A STRING is text and is carried with its length, so what an actual
    // makes it is known from the value and needs no bounds passed beside it.
    // A generic of one has no instantiation to pass them anyway, when the
    // entity it belongs to is the top of the design.
    const VhdlType* const strp = V3VhdlStd::stringType();
    const bool isString = strp && basep == strp->base();
    if (typep->unconstrained() && !isString) {
        consp = new AstVhdlIndexConstraint{flp};
        const size_t dims = basep->indexTypes().size();
        for (size_t d = 0; d < dims; ++d) {
            const std::string leftName = "__vl" + mangled(d);
            const std::string rightName = "__vr" + mangled(d);
            const std::string ascName = "__va" + mangled(d);
            makeGeneric(leftName);
            makeGeneric(rightName);
            makeGeneric(ascName, "boolean", "false");
            AstVhdlRange* const rangep
                = new AstVhdlRange{flp, new AstVhdlName{flp, leftName}, VhdlDir::TO,
                                   new AstVhdlName{flp, rightName}};
            // Which way round the actual's range runs is not known here, so a
            // third generic carries it and the instance binds it. It folds
            // per instance like the other two, so it costs nothing to run.
            rangep->dirUnknown(true);
            rangep->dirp(new AstVhdlName{flp, ascName});
            consp->addRangesp(rangep);
            m_openBounds[scopep].push_back(OpenBound{ifp->name(), path, static_cast<int>(d),
                                                     leftName, rightName, ascName,
                                                     ifp->kind() == VhdlIfaceKind::CONSTANT});
        }
    }
    // The element may be open as well, or instead: "array (0 to 3) of
    // bit_vector" is constrained at its top level and not below it
    VhdlType* const elemp = typep->elementp() ? typep->elementp() : basep->elementp();
    path.push_back("");
    AstVhdlIndexConstraint* const innerp
        = openConstraintFor(elemp, ifp, entp, compp, scopep, path);
    path.pop_back();
    if (innerp) {
        if (!consp) consp = new AstVhdlIndexConstraint{flp};
        consp->addRangesp(innerp);
    }
    return consp;
}

// The bounds an instance gives each of its open ports, written as the
// associations the synthesized generics need
void VhdlSemVisitor::bindOpenBounds(AstVhdlInstance* nodep, VhdlScope* unitScopep) {
    if (!unitScopep) return;
    const auto it = m_openBounds.find(unitScopep);
    if (it == m_openBounds.end()) return;
    // "x(0) => a, x(1) => b" names the elements of the port one at a time,
    // and together they say how long it is (6.5.7.1). Which indices were
    // named has to be known before any one of them is looked at.
    std::map<std::string, std::pair<int64_t, int64_t>> indexed;  // Port -> lowest, highest
    for (AstNode* p = nodep->portMapp(); p; p = p->nextp()) {
        const AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
        const AstVhdlCall* const callp
            = asp->formalp() ? VN_CAST(asp->formalp(), VhdlCall) : nullptr;
        if (!callp || VN_IS(callp->argsp(), VhdlRange)) continue;
        const AstVhdlName* const rootp = VN_CAST(callp->prefixp(), VhdlName);
        const AstVhdlAssoc* const argp = VN_CAST(callp->argsp(), VhdlAssoc);
        if (!rootp || !argp || argp->nextp() || !argp->actualp()) continue;
        const VhdlValue at = V3VhdlEval::eval(argp->actualp());
        if (!at.isInt()) continue;
        const auto found = indexed.find(rootp->name());
        if (found == indexed.end()) {
            indexed.emplace(rootp->name(), std::make_pair(at.toInt(), at.toInt()));
        } else {
            found->second.first = std::min(found->second.first, at.toInt());
            found->second.second = std::max(found->second.second, at.toInt());
        }
    }
    for (const OpenBound& bound : it->second) {
        if (bound.isGeneric) continue;
        // Where the port is in the clause, so a positional map finds it
        std::vector<VhdlSymbol*> found;
        unitScopep->lookupLocal(bound.port, found);
        if (found.empty()) continue;
        const int wantPos = found[0]->position();
        AstNodeVhdlExpr* actualp = nullptr;
        AstVhdlRange* formalRangep = nullptr;
        size_t skipSteps = 0;  // How much of the path the formal already named
        int position = 0;
        for (AstNode* p = nodep->portMapp(); p; p = p->nextp(), ++position) {
            AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
            if (!asp->actualp() || VN_IS(asp->actualp(), VhdlOpen)) continue;
            // "p.a => x" associates one element of the port, and then it is
            // x that says how long p.a is, not how long p is (6.5.7.1)
            {
                std::vector<std::string> steps;  // Outermost first
                AstNodeVhdlExpr* rootp = asp->formalp();
                while (AstVhdlSelected* const selp = VN_CAST(rootp, VhdlSelected)) {
                    const AstVhdlName* const sufp = VN_CAST(selp->suffixp(), VhdlName);
                    if (!sufp) break;
                    steps.insert(steps.begin(), sufp->name());
                    rootp = selp->prefixp();
                }
                const AstVhdlName* const rnamep = VN_CAST(rootp, VhdlName);
                if (!steps.empty()) {
                    if (!rnamep || rnamep->name() != bound.port) continue;
                    if (steps.size() > bound.path.size()) continue;
                    if (!std::equal(steps.begin(), steps.end(), bound.path.begin())) continue;
                    actualp = asp->actualp();
                    skipSteps = steps.size();
                    break;
                }
            }
            // "value(data'range) => a" names part of the port, and it is that
            // part which says how long the port is
            if (AstVhdlCall* const callp = VN_CAST(asp->formalp(), VhdlCall)) {
                const AstVhdlName* const rootp = VN_CAST(callp->prefixp(), VhdlName);
                if (rootp && rootp->name() == bound.port) {
                    formalRangep = VN_CAST(callp->argsp(), VhdlRange);
                    if (formalRangep) {
                        actualp = asp->actualp();
                        break;
                    }
                }
                continue;
            }
            const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName);
            if (namep ? namep->name() == bound.port
                      : (!asp->formalp() && position == wantPos)) {
                actualp = asp->actualp();
                break;
            }
        }
        FileLine* const flp = nodep->fileline();
        // The elements the port map named one at a time are the whole of it,
        // and its index range runs the way its index subtype does (6.5.7.1)
        if (bound.path.empty() && bound.dim == 0) {
            const auto idxIt = indexed.find(bound.port);
            if (idxIt != indexed.end()) {
                // The predefined array types are indexed by NATURAL, which
                // runs upward, so the lowest index named is the left bound
                for (int side = 0; side < 2; ++side) {
                    const int64_t val
                        = side == 0 ? idxIt->second.first : idxIt->second.second;
                    AstVhdlLiteral* const litp = new AstVhdlLiteral{
                        flp, VhdlLitKind::INTEGER, std::to_string(val)};
                    litp->intVal(val);
                    nodep->addGenericMapp(new AstVhdlAssoc{
                        flp,
                        new AstVhdlName{flp, side == 0 ? bound.leftName : bound.rightName},
                        litp});
                }
                bindAscending(nodep, bound, nullptr, true, flp);
                continue;
            }
        }
        if (!actualp && !formalRangep) continue;
        // A formal that names a slice of the port says the bounds itself:
        // "value(data'range) => ..." gives the port that range (6.5.7.1),
        // and the actual is only part of what drives it
        if (formalRangep && bound.path.empty() && bound.dim == 0) {
            for (int side = 0; side < 2; ++side) {
                AstNodeVhdlExpr* endp = nullptr;
                if (formalRangep->leftp() && formalRangep->rightp()) {
                    endp = (side == 0 ? formalRangep->leftp() : formalRangep->rightp())
                               ->cloneTree(false);
                } else if (AstVhdlAttrName* const rattrp
                           = VN_CAST(formalRangep->attrp(), VhdlAttrName)) {
                    if (!rattrp->prefixp()) continue;
                    endp = new AstVhdlAttrName{flp, rattrp->prefixp()->cloneTree(false),
                                               side == 0 ? "left" : "right"};
                }
                if (!endp) continue;
                nodep->addGenericMapp(new AstVhdlAssoc{
                    flp, new AstVhdlName{flp, side == 0 ? bound.leftName : bound.rightName},
                    endp});
            }
            if (formalRangep->leftp() && formalRangep->rightp()) {
                bindAscending(nodep, bound, nullptr, formalRangep->dir() == VhdlDir::TO, flp);
            } else if (AstVhdlAttrName* const rattrp
                       = VN_CAST(formalRangep->attrp(), VhdlAttrName)) {
                if (rattrp->prefixp()) {
                    bindAscending(nodep, bound, rattrp->prefixp(), true, flp);
                }
            }
            continue;
        }
        for (int side = 0; side < 2; ++side) {
            // Where the open dimension is inside the port's type, the same
            // place inside the actual is what has its bounds
            AstNodeVhdlExpr* prefixp = actualp->cloneTree(false);
            for (size_t si = skipSteps; si < bound.path.size(); ++si) {
                const std::string& step = bound.path[si];
                prefixp = step.empty()
                              ? static_cast<AstNodeVhdlExpr*>(
                                    new AstVhdlAttrName{flp, prefixp, "element"})
                              : static_cast<AstNodeVhdlExpr*>(new AstVhdlSelected{
                                    flp, prefixp, new AstVhdlName{flp, step}});
            }
            AstVhdlAttrName* const attrp
                = new AstVhdlAttrName{flp, prefixp, side == 0 ? "left" : "right"};
            if (bound.dim > 0) {
                AstVhdlLiteral* const dimp = new AstVhdlLiteral{
                    flp, VhdlLitKind::INTEGER, std::to_string(bound.dim + 1)};
                dimp->intVal(bound.dim + 1);
                attrp->argp(dimp);
            }
            nodep->addGenericMapp(new AstVhdlAssoc{
                flp, new AstVhdlName{flp, side == 0 ? bound.leftName : bound.rightName},
                attrp});
        }
        // The same place inside the actual says which way its range runs
        {
            AstNodeVhdlExpr* prefixp = actualp->cloneTree(false);
            for (size_t si = skipSteps; si < bound.path.size(); ++si) {
                const std::string& step = bound.path[si];
                prefixp = step.empty()
                              ? static_cast<AstNodeVhdlExpr*>(
                                    new AstVhdlAttrName{flp, prefixp, "element"})
                              : static_cast<AstNodeVhdlExpr*>(new AstVhdlSelected{
                                    flp, prefixp, new AstVhdlName{flp, step}});
            }
            bindAscending(nodep, bound, prefixp, true, flp);
            VL_DO_DANGLING(prefixp->deleteTree(), prefixp);
        }
    }
}

// The generic that says which way an open dimension's range runs. Given a
// prefix, it is that object's 'ASCENDING; given none, the answer is already
// known here and goes in as TRUE or FALSE.
void VhdlSemVisitor::bindAscending(AstVhdlInstance* nodep, const OpenBound& bound,
                                   AstNodeVhdlExpr* prefixp, bool ascending, FileLine* flp) {
    if (bound.ascName.empty()) return;
    AstNodeVhdlExpr* valuep = nullptr;
    if (prefixp) {
        AstVhdlAttrName* const attrp
            = new AstVhdlAttrName{flp, prefixp->cloneTree(false), "ascending"};
        if (bound.dim > 0) {
            AstVhdlLiteral* const dimp = new AstVhdlLiteral{
                flp, VhdlLitKind::INTEGER, std::to_string(bound.dim + 1)};
            dimp->intVal(bound.dim + 1);
            attrp->argp(dimp);
        }
        valuep = attrp;
    } else {
        valuep = new AstVhdlName{flp, ascending ? "true" : "false"};
    }
    nodep->addGenericMapp(
        new AstVhdlAssoc{flp, new AstVhdlName{flp, bound.ascName}, valuep});
}

// Each actual takes the type of the formal it is associated with, by position
// or by name
void VhdlSemVisitor::analyzeAssociations(AstNode* assocsp, VhdlScope* unitScopep,
                                        bool generics) {
    int position = 0;
    for (AstNode* p = assocsp; p; p = p->nextp()) {
        AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
        if (!asp->actualp()) {
            ++position;
            continue;
        }
        VhdlType* formalTypep = nullptr;
        const VhdlSymbol* formalSymp = nullptr;
        // An individual association covers part of the formal, so the actual
        // is not the whole of it and the length check does not apply
        bool individual = false;
        if (unitScopep) {
            if (const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName)) {
                std::vector<VhdlSymbol*> found;
                unitScopep->lookupLocal(namep->name(), found);
                if (!found.empty()) {
                    asp->formalp()->vhdlSymp(found[0]);
                    formalTypep = found[0]->typep();
                    formalSymp = found[0];
                }
            } else if (!asp->formalp()) {
                formalSymp = formalSymAt(unitScopep, position, generics);
                formalTypep = formalSymp ? formalSymp->typep() : nullptr;
            } else {
                // An individual association names one part of the formal --
                // "r.x => v", "d(3 downto 0) => e" -- and several of them
                // together cover it (6.5.7.1). The leftmost name is the
                // formal; what follows says which part, and that part's
                // subtype is what the actual is checked against.
                AstNodeVhdlExpr* rootp = asp->formalp();
                std::vector<AstNodeVhdl*> steps;  // Innermost last
                while (rootp) {
                    if (AstVhdlSelected* const selp = VN_CAST(rootp, VhdlSelected)) {
                        steps.push_back(selp);
                        rootp = selp->prefixp();
                    } else if (AstVhdlCall* const callp = VN_CAST(rootp, VhdlCall)) {
                        steps.push_back(callp);
                        rootp = callp->prefixp();
                    } else {
                        break;
                    }
                }
                AstVhdlName* const namep = VN_CAST(rootp, VhdlName);
                std::vector<VhdlSymbol*> found;
                if (namep) unitScopep->lookupLocal(namep->name(), found);
                // A conversion written on the formal -- "f(port) => a" -- has
                // a function or a type mark at its root, and the port it
                // applies to is its argument (6.5.7.1). Naming that port here
                // is what lets the association be wired through the
                // conversion rather than straight to the actual.
                if (found.empty() || found[0]->kind() != VhdlSymKind::PORT) {
                    if (AstVhdlCall* const convp = VN_CAST(asp->formalp(), VhdlCall)) {
                        AstVhdlAssoc* const argp = VN_CAST(convp->argsp(), VhdlAssoc);
                        AstNodeVhdlExpr* const argValp
                            = argp ? argp->actualp() : VN_CAST(convp->argsp(), NodeVhdlExpr);
                        AstVhdlName* const pnamep
                            = convp->argsp() && !convp->argsp()->nextp()
                                  ? VN_CAST(argValp, VhdlName)
                                  : nullptr;
                        if (pnamep && !pnamep->vhdlSymp()) {
                            std::vector<VhdlSymbol*> pfound;
                            unitScopep->lookupLocal(pnamep->name(), pfound);
                            if (!pfound.empty()
                                && pfound[0]->kind() == VhdlSymKind::PORT) {
                                pnamep->vhdlSymp(pfound[0]);
                                pnamep->vhdlTypep(pfound[0]->typep());
                                // The conversion itself is a name of this
                                // region -- a type mark or a function of one
                                // parameter -- so it is resolved here
                                AstVhdlName* const cnamep
                                    = VN_CAST(convp->prefixp(), VhdlName);
                                if (cnamep && !cnamep->vhdlSymp()) {
                                    std::vector<VhdlSymbol*> cfound;
                                    lookup(cnamep->name(), cfound);
                                    for (VhdlSymbol* const csymp : cfound) {
                                        if (csymp->kind() == VhdlSymKind::TYPE) {
                                            cnamep->vhdlSymp(csymp);
                                            convp->vhdlTypep(csymp->typep());
                                            break;
                                        }
                                        if (csymp->kind() == VhdlSymKind::FUNCTION
                                            && csymp->paramTypes().size() == 1) {
                                            cnamep->vhdlSymp(csymp);
                                            convp->vhdlSymp(csymp);
                                            convp->vhdlTypep(csymp->returnTypep());
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (!found.empty() && (found[0]->kind() == VhdlSymKind::PORT
                                       || found[0]->kind() == VhdlSymKind::GENERIC)) {
                    namep->vhdlSymp(found[0]);
                    namep->vhdlTypep(found[0]->typep());
                    VhdlType* typep = found[0]->typep();
                    for (size_t i = steps.size(); i-- > 0 && typep;) {
                        if (AstVhdlSelected* const selp = VN_CAST(steps[i], VhdlSelected)) {
                            const AstVhdlName* const sufp
                                = VN_CAST(selp->suffixp(), VhdlName);
                            VhdlType* nextp = nullptr;
                            if (sufp) {
                                for (VhdlSymbol* const elemp : typep->elements()) {
                                    if (elemp->name() != sufp->name()) continue;
                                    nextp = elemp->typep();
                                    // Which element it is, so the lowering
                                    // can reach it
                                    selp->vhdlSymp(elemp);
                                }
                            }
                            selp->vhdlTypep(nextp);
                            typep = nextp;
                        } else {
                            // An index gives the element; a slice gives an
                            // array of its own, as long as its range says
                            AstVhdlCall* const callp = VN_AS(steps[i], VhdlCall);
                            AstVhdlRange* slicep = nullptr;
                            // The index or range is an expression of its own
                            for (AstNode* ap = callp->argsp(); ap; ap = ap->nextp()) {
                                if (AstVhdlRange* const rp = VN_CAST(ap, VhdlRange)) {
                                    slicep = rp;
                                    const RangeInfo info = analyzeRange(rp, nullptr);
                                    if (typep && info.valid && !info.unbounded
                                        && info.left.known() && info.right.known()) {
                                        VhdlType* const slicedp
                                            = cloneSubtype(typep, callp->fileline());
                                        slicedp->clearIndexTypes();
                                        slicedp->addIndexType(
                                            makeRangeSubtype(info, callp->fileline()));
                                        slicedp->unconstrained(false);
                                        typep = slicedp;
                                    }
                                } else if (AstVhdlAssoc* const asp2 = VN_CAST(ap, VhdlAssoc)) {
                                    if (asp2->actualp()) analyzeExpr(asp2->actualp(), nullptr);
                                } else if (AstNodeVhdlExpr* const ep
                                           = VN_CAST(ap, NodeVhdlExpr)) {
                                    analyzeExpr(ep, nullptr);
                                }
                            }
                            if (!slicep) {
                                typep = typep->elementp() ? typep->elementp()
                                                          : typep->base()->elementp();
                            }
                            callp->vhdlTypep(typep);
                        }
                    }
                    formalTypep = typep;
                    individual = true;
                }
            }
        }
        // A generic that is a type, a package or a subprogram is given a
        // *name*, not an expression: "t => std_logic_vector(7 downto 0)",
        // "p => work.complex_real", "f => falling_edge" (6.5.5). Reading
        // them as expressions asks for a value none of them has.
        if (formalSymp
            && (formalSymp->kind() == VhdlSymKind::TYPE
                || formalSymp->kind() == VhdlSymKind::PACKAGE
                || formalSymp->kind() == VhdlSymKind::FUNCTION
                || formalSymp->kind() == VhdlSymKind::PROCEDURE)) {
            ++position;
            continue;
        }
        analyzeExpr(asp->actualp(), formalTypep);
        // The actual is of the formal's type (6.5.6.3, 6.5.7.1). A formal of
        // a generic type takes whatever type the instance gives it.
        if (formalTypep && !individual && formalSymp
            && (formalSymp->kind() == VhdlSymKind::PORT
                || formalSymp->kind() == VhdlSymKind::GENERIC)
            && !formalTypep->isGenericFormal() && !formalTypep->base()->isGenericFormal()
            && !valueFitsType(asp->actualp(), formalTypep)) {
            err(asp->actualp()->fileline(),
                std::string{formalSymp->kind() == VhdlSymKind::PORT ? "Port '" : "Generic '"}
                    + formalSymp->name() + "' is of type " + formalTypep->prettyName()
                    + ", but its actual is of type " + asp->actualp()->vhdlTypep()->prettyName());
            ++position;
            continue;
        }
        // A port is connected once, when the design is elaborated, so a
        // signal given it is named statically (6.5.6.3). VHDL-2008 lets an
        // input take any expression, a signal made for it being the actual.
        const AstVhdlIfaceDecl* const fifp
            = formalSymp ? VN_CAST(formalSymp->declp(), VhdlIfaceDecl) : nullptr;
        if (formalSymp && formalSymp->kind() == VhdlSymKind::PORT && !m_inSpecialized
            && (asp->actualp()->fileline()->language() < V3LangCode::L1076_2008
                || (fifp && fifp->mode() != VhdlMode::IN))
            && objectOfName(asp->actualp()).cls == ObjClass::SIGNAL) {
            if (const VhdlSymbol* const varyingp = nonStaticIndex(asp->actualp())) {
                err(asp->actualp()->fileline(),
                    "What is given for port '" + formalSymp->name()
                        + "' is a static signal name, but this one's index depends on '"
                        + varyingp->name() + "'");
            }
        }
        if (formalTypep && !individual) {
            checkValueInRange(asp->actualp(), formalTypep, "this association");
            checkArrayValue(asp->actualp(), formalTypep, "this association");
        }
        ++position;
    }
}

// The formal at one position. A component's declarations are held in a map,
// so the order comes from the declaration node rather than the scope.
// A generic and a port may stand at the same position, since each clause is
// numbered from the start of itself, so which clause is being associated has
// to be said rather than guessed.
VhdlSymbol* VhdlSemVisitor::formalSymAt(VhdlScope* scopep, int position, bool generics) {
    for (const auto& pair : scopep->direct()) {
        VhdlSymbol* const symp = pair.second;
        if (symp->position() != position) continue;
        switch (symp->kind()) {
        case VhdlSymKind::PORT:
            if (!generics) return symp;
            break;
        case VhdlSymKind::GENERIC:
        case VhdlSymKind::TYPE:
        case VhdlSymKind::PACKAGE:
        case VhdlSymKind::FUNCTION:
        case VhdlSymKind::PROCEDURE:
            if (generics) return symp;
            break;
        default: break;
        }
    }
    return nullptr;
}

VhdlType* VhdlSemVisitor::formalAt(VhdlScope* scopep, int position) {
    VhdlSymbol* const symp = formalSymAt(scopep, position, false);
    if (!symp) return nullptr;
    return symp->typep();
}

void VhdlSemVisitor::visit(AstVhdlProcCall* nodep) {
    // A procedure call statement can only mean a procedure, and an
    // expression can only mean a function, so the two never compete (4.3)
    VL_RESTORER(m_procCall);
    m_procCall = true;
    analyzeExpr(nodep->callp(), nullptr);
}

void VhdlSemVisitor::visit(AstVhdlNext* nodep) {
    if (nodep->condp()) analyzeCondition(nodep->condp());
}
