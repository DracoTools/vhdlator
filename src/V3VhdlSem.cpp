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
#include <map>
#include <string>
#include <utility>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

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

class VhdlSemVisitor final : public VNVisitor {
    // STATE
    VhdlScope* m_scopep = nullptr;  // Region being filled
    VhdlScope* m_rootp = nullptr;  // Outermost region, holding library names
    std::map<std::string, VhdlScope*> m_libs;  // Library name -> its region
    std::map<std::string, VhdlScope*> m_unitScopes;  // "lib.unit" -> its region
    std::map<std::string, AstNodeVhdlUnit*> m_primaryUnits;  // "lib.unit" -> the unit
    std::string m_libname;  // Library of the unit being analyzed
    VhdlType* m_returnTypep = nullptr;  // Result type of the enclosing function
    // A package body continues the package declaration's declarative region
    // (12.1), so what it declares completes an earlier declaration rather
    // than making a second one
    bool m_inPackageBody = false;
    int m_ifacePos = 0;  // Position of the interface element being declared
    bool m_errored = false;

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

    // Declare 'symp', complaining if it illegally hides something declared in
    // the same region (12.3)
    void declare(VhdlSymbol* symp, FileLine* flp) {
        VhdlSymbol* const hiddenp = m_scopep->insert(symp);
        if (hiddenp && !hiddenp->predefined()) {
            err(flp, "Declaration of '" + symp->name() + "' conflicts with the "
                         + hiddenp->kindName() + " of the same name declared in this region");
        }
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
            if (!symp->isObject()) continue;
            VhdlType* typep = symp->typep();
            if (typep && typep->kind() == VhdlTypeKind::ACCESS) typep = typep->designatedp();
            if (!typep || typep->base()->kind() != VhdlTypeKind::PROTECTED) continue;
            if (!typep->base()->scopep()) continue;
            methScopep = typep->base()->scopep();
            return symp;
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
        AstVhdlRange* const rp = VN_CAST(rangep, VhdlRange);
        if (!rp) return info;
        info.valid = true;
        if (AstNodeVhdlExpr* const namep = rp->attrp()) {
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
        // already fix. Analyze the bound that can stand alone first, so a
        // universal literal on the other side takes its type from it.
        VhdlType* leftp = analyzeExpr(rp->leftp(), wantp);
        VhdlType* rightp
            = analyzeExpr(rp->rightp(), wantp ? wantp : (leftp && !leftp->isUniversal() ? leftp
                                                                                       : nullptr));
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
        if (typep->base()->kind() != VhdlTypeKind::ARRAY) return typep;
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
    void checkChoiceInRange(FileLine* flp, const VhdlValue& val, VhdlType* typep) {
        if (!typep || !typep->bounded() || !val.known()) return;
        if (V3VhdlEval::inRange(val, typep)) return;
        err(flp, "Choice " + val.prettyName() + " is outside the index range, which is "
                     + rangeText(typep));
    }

    // Complain when a static value cannot belong to the subtype it is given
    // to (5.2.1). Silent whenever either the value or the range is unknown,
    // so that only a certain violation is reported.
    void checkValueInRange(AstNodeVhdlExpr* nodep, VhdlType* typep, const std::string& what) {
        if (!nodep || !typep || !typep->bounded() || !typep->isScalar()) return;
        const VhdlValue val = V3VhdlEval::eval(nodep);
        if (!val.known() || V3VhdlEval::inRange(val, typep)) return;
        err(nodep->fileline(), "Value " + val.prettyName() + " is outside the range of " + what
                                   + ", which is " + rangeText(typep));
    }

    // METHODS - types

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
        // The parser could not tell "std_logic_vector(7 downto 0)" from a
        // call, so a constrained type mark arrives as one (8.1)
        // "t(3 downto 0)(7 downto 0)" constrains the array and then its
        // element, and arrives as a call of a call (5.3.2.1)
        std::vector<AstNode*> argLists;
        AstNodeVhdlExpr* markp = nodep->typeMarkp();
        while (AstVhdlCall* const cp = VN_CAST(markp, VhdlCall)) {
            argLists.push_back(cp->argsp());
            markp = cp->prefixp();
        }
        std::reverse(argLists.begin(), argLists.end());  // Outermost first
        VhdlType* const basep = resolveTypeMark(markp);
        if (!basep || basep->isError()) {
            nodep->vhdlTypep(V3VhdlStd::errorType());
            return nodep->vhdlTypep();
        }
        AstNodeVhdl* const constraintp = nodep->constraintp();
        if (!constraintp && argLists.empty() && !nodep->resolutionp()) {
            nodep->vhdlTypep(basep);
            return basep;
        }
        // A constrained subtype is a distinct subtype of the same type
        VhdlType* const typep = cloneSubtype(basep, nodep->fileline());
        // Set before applying the constraint, so that a subtype indication
        // mentioning itself stops rather than recursing
        nodep->vhdlTypep(typep);
        VhdlType* curp = typep;
        VhdlType* curBasep = basep;
        for (size_t i = 0; i < argLists.size(); ++i) {
            if (i > 0) {
                VhdlType* const elemp = curp->elementp();
                if (!elemp) break;
                VhdlType* const newElemp = cloneSubtype(elemp, nodep->fileline());
                curp->elementp(newElemp);
                curBasep = elemp;
                curp = newElemp;
            }
            applyConstraint(curp, curBasep, nullptr, argLists[i], nodep->fileline());
        }
        if (constraintp) {
            applyConstraint(typep, basep, constraintp, nullptr, nodep->fileline());
        }
        if (nodep->resolutionp()) {
            VhdlSymbol* const resp
                = lookupOne(nodep->resolutionp(), VhdlSymKind::FUNCTION, "resolution function");
            if (resp) typep->resolutionp(resp);
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
            const RangeInfo info = analyzeRange(rp, basep->isScalar() ? basep : nullptr);
            applyRange(typep, info);
            checkSubrange(typep, basep, flp);
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
        std::vector<VhdlType*> indexes;
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
                    }
                }
            }
            if (!info.valid) continue;
            VhdlType* idxp = info.typep ? info.typep : wantp;
            if (!info.unbounded && info.left.known() && info.right.known()) {
                idxp = makeRangeSubtype(info, flp);
                checkSubrange(idxp, wantp, flp);
            } else if (!info.unbounded && info.valid) {
                // A range whose bounds did not fold constrains nothing
                // static, but it is still a range: a bound that names a
                // generic is fixed once the instance is elaborated, so keep
                // it as written for the back end to fold then (10.3)
                idxp = cloneSubtype(idxp ? idxp : V3VhdlStd::integerType(), flp);
                idxp->unbound();
                if (VN_IS(rp, VhdlRange)) idxp->boundsp(rp);
            }
            indexes.push_back(idxp ? idxp : V3VhdlStd::integerType());
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
        for (VhdlType* const idxp : indexes) {
            typep->addIndexType(idxp);
            if (!idxp->bounded()) unconstrained = true;
        }
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
            AstVhdlCall* const callp = VN_CAST(itemp, VhdlCall);
            const AstVhdlName* const namep
                = callp ? VN_CAST(callp->prefixp(), VhdlName) : nullptr;
            if (!namep) {
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
            applyConstraint(newTypep, elemTypep, nullptr, callp->argsp(), flp);
            VhdlSymbol* const newSymp = VhdlSymTab::newSymbol(VhdlSymKind::RECORD_ELEMENT,
                                                             elems[pos]->name(),
                                                             elems[pos]->declp());
            newSymp->typep(newTypep);
            newSymp->position(elems[pos]->position());
            typep->replaceElement(pos, newSymp);
        }
    }

    // A subtype's range must lie within the range it narrows (5.2.1)
    void checkSubrange(const VhdlType* typep, const VhdlType* basep, FileLine* flp) {
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
                    if (val.isInt()) {
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
                const RangeInfo info = analyzeRange(idxp, nullptr);
                VhdlType* indexTypep = info.typep;
                if (!info.unbounded && info.left.known() && info.right.known()) {
                    // "array (0 to 7)" gives a constrained array type, whose
                    // index subtype is anonymous (5.3.2.1)
                    indexTypep = makeRangeSubtype(info, flp);
                }
                if (!indexTypep) indexTypep = V3VhdlStd::integerType();
                if (info.unbounded || !indexTypep->bounded()) unconstrained = true;
                typep->addIndexType(indexTypep);
            }
            typep->unconstrained(unconstrained);
            typep->elementp(resolveSubtype(arrp->elementp()));
            return typep;
        }
        if (AstVhdlRecordType* const recp = VN_CAST(defp, VhdlRecordType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::RECORD, name, flp);
            int pos = 0;
            for (AstNode* elemp = recp->elemsp(); elemp; elemp = elemp->nextp()) {
                AstVhdlElementDecl* const edp = VN_AS(elemp, VhdlElementDecl);
                VhdlSymbol* const symp
                    = VhdlSymTab::newSymbol(VhdlSymKind::RECORD_ELEMENT, edp->name(), edp);
                symp->typep(resolveSubtype(edp->subtypep()));
                symp->position(pos++);
                edp->vhdlSymp(symp);
                typep->addElement(symp);
            }
            return typep;
        }
        if (AstVhdlAccessType* const accp = VN_CAST(defp, VhdlAccessType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ACCESS, name, flp);
            typep->designatedp(resolveSubtype(accp->designatedp()));
            return typep;
        }
        if (AstVhdlFileType* const filep = VN_CAST(defp, VhdlFileType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::FILE, name, flp);
            typep->designatedp(resolveTypeMark(filep->designatedp()));
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

    // Give a subprogram the parameter subtypes one call site supplies, and
    // analyze it as if it had been written that way (see specialize())
    VhdlSymbol* specializeSubprogram(AstVhdlSubprogDecl* originalp, VhdlScope* regionp,
                                     const std::vector<VhdlType*>& actualTypes,
                                     const std::string& name) VL_MT_DISABLED;

    // Give every resolved signal's resolution function the copy that takes as
    // many driver values as it has drivers
    void specializeResolvers() VL_MT_DISABLED;

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
    void visit(AstVhdlAttrDecl* nodep) override;
    void visit(AstVhdlAttrSpec* nodep) override;
    bool parseForeign(const std::string& text, FileLine* flp, std::string& cnamer,
                      bool& dpir);
    void visit(AstVhdlContextItem* nodep) override;
    // Regions
    void visit(AstVhdlProcess* nodep) override;
    void visit(AstVhdlBlock* nodep) override;
    void visit(AstVhdlGenerate* nodep) override;
    void visit(AstVhdlGenerateBody* nodep) override;
    void visit(AstVhdlLoop* nodep) override;
    // Statements, where the expected type of each expression is known
    void visit(AstVhdlSigAssign* nodep) override;
    void visit(AstVhdlVarAssign* nodep) override;
    void visit(AstVhdlIf* nodep) override;
    void visit(AstVhdlCase* nodep) override;
    void visit(AstVhdlAssert* nodep) override;
    void visit(AstVhdlWait* nodep) override;
    void visit(AstVhdlReturn* nodep) override;
    void visit(AstVhdlProcCall* nodep) override;
    void visit(AstVhdlNext* nodep) override;
    void visit(AstVhdlInstance* nodep) override;

    VhdlType* loopParamType(const RangeInfo& info, FileLine* flp) VL_MT_DISABLED;
    void analyzeChoice(AstVhdlChoice* chp, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeIndexedName(AstVhdlCall* nodep, VhdlType* arrayp) VL_MT_DISABLED;
    VhdlType* analyzeSliceName(AstVhdlCall* nodep, VhdlType* arrayp) VL_MT_DISABLED;
    int64_t staticLength(AstNodeVhdlExpr* nodep) VL_MT_DISABLED;
    void checkArrayValue(AstNodeVhdlExpr* nodep, VhdlType* typep,
                         const std::string& what) VL_MT_DISABLED;
    void checkAssignedValue(AstNodeVhdlExpr* nodep, VhdlType* targetp) VL_MT_DISABLED;
    void checkCaseChoices(AstVhdlCase* nodep, VhdlType* typep) VL_MT_DISABLED;
    void checkWaveform(AstNode* wavesp) VL_MT_DISABLED;
    void checkDelay(AstNodeVhdlExpr* nodep, const char* what) VL_MT_DISABLED;
    void checkAggregate(AstVhdlAggregate* nodep, VhdlType* typep,
                        const std::string& what) VL_MT_DISABLED;
    int64_t aggregateLength(AstVhdlAggregate* nodep) VL_MT_DISABLED;
    int64_t elementSpan(AstNodeVhdlExpr* valuep, VhdlType* arrayTypep) VL_MT_DISABLED;
    VhdlType* aggregateSubtype(AstVhdlAggregate* nodep, VhdlType* typep) VL_MT_DISABLED;
    VhdlType* convertedType(VhdlType* top, AstVhdlAssoc* argp) VL_MT_DISABLED;
    VhdlType* constrainFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                 FileLine* flp) VL_MT_DISABLED;
    static std::string dimensionText(size_t dim, size_t dims) VL_MT_DISABLED;
    void setupUnitContext(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    void applyContextItem(AstVhdlContextItem* ctxp) VL_MT_DISABLED;
    void analyzeAssociations(AstNode* assocsp, VhdlScope* unitScopep) VL_MT_DISABLED;
    void analyzeDrivers(AstVhdlArchitecture* archp) VL_MT_DISABLED;
    static bool isLogicValued(const VhdlType* typep) VL_MT_DISABLED;
    struct NeedResolve final {
        VhdlSymbol* symp;
        VhdlSymbol* resolvep;
        int count;
        FileLine* flp;
    };
    std::vector<NeedResolve> m_needResolve;  // Signals awaiting a sized resolver
    static VhdlType* formalAt(VhdlScope* scopep, int position) VL_MT_DISABLED;
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
    VhdlType* analyzeUnary(AstVhdlUnary* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeBinary(AstVhdlBinary* nodep, VhdlType* wantp) VL_MT_DISABLED;
    VhdlType* analyzeCall(AstVhdlCall* nodep, VhdlType* wantp) VL_MT_DISABLED;
    static bool bindActuals(const VhdlSymbol* symp, const std::vector<AstVhdlAssoc*>& assocs,
                            std::vector<int>& formalOf) VL_MT_DISABLED;
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
            m_scopep = declp->typep()->scopep();
            iterateAndNextNull(protp->declsp());
            return;
        }
    }
    VhdlType* const typep = buildType(nodep->defp(), nodep->name(), nodep->fileline());
    typep->name(nodep->name());
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, nodep->name(), nodep);
    symp->typep(typep);
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
    // A type declaration implicitly declares its operators (5.2.6)
    V3VhdlStd::addImplicitOperators(m_scopep, typep);
}

void VhdlSemVisitor::visit(AstVhdlSubtypeDecl* nodep) {
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
    VhdlSymKind kind = VhdlSymKind::CONSTANT;
    switch (nodep->kind()) {
    case VhdlObjKind::CONSTANT: kind = VhdlSymKind::CONSTANT; break;
    case VhdlObjKind::SIGNAL: kind = VhdlSymKind::SIGNAL; break;
    case VhdlObjKind::VARIABLE: kind = VhdlSymKind::VARIABLE; break;
    case VhdlObjKind::SHARED_VARIABLE: kind = VhdlSymKind::SHARED_VARIABLE; break;
    case VhdlObjKind::FILE: kind = VhdlSymKind::FILE; break;
    }
    VhdlSymbol* symp = nullptr;
    // A package body gives a deferred constant its value; that completes the
    // declaration in the package, it does not make a new one (6.4.2.2)
    if (m_inPackageBody && kind == VhdlSymKind::CONSTANT && nodep->defaultp()) {
        std::vector<VhdlSymbol*> found;
        m_scopep->lookup(nodep->name(), found);
        for (VhdlSymbol* const otherp : found) {
            if (otherp->kind() == VhdlSymKind::CONSTANT && otherp->deferred()) {
                symp = otherp;
                symp->deferred(false);
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
    }
    if (nodep->defaultp()) {
        analyzeExpr(nodep->defaultp(), symp->typep());
        symp->typep(constrainFromValue(symp->typep(), nodep->defaultp(), nodep->fileline()));
        checkValueInRange(nodep->defaultp(), symp->typep(),
                          std::string{symp->kindName()} + " '" + nodep->name() + "'");
        checkArrayValue(nodep->defaultp(), symp->typep(),
                        std::string{symp->kindName()} + " '" + nodep->name() + "'");
        // A constant with a static value is what every later fold builds on
        if (kind == VhdlSymKind::CONSTANT) symp->value(V3VhdlEval::eval(nodep->defaultp()));
    } else if (kind == VhdlSymKind::CONSTANT) {
        // A constant declared with no value in a package declaration is
        // deferred; its value comes with the package body (6.4.2.2)
        symp->deferred(true);
    }
}

void VhdlSemVisitor::visit(AstVhdlIfaceDecl* nodep) {
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
    // Its place in the clause, so that a positional association finds it
    symp->position(m_ifacePos++);
    if (nodep->kind() == VhdlIfaceKind::TYPE) {
        // A generic type stands for whatever the instantiation supplies, so
        // give it a type of its own that matches nothing else yet (6.5.5)
        VhdlType* const typep
            = VhdlSymTab::newType(VhdlTypeKind::ERROR, nodep->name(), nodep->fileline());
        symp->typep(typep);
    } else {
        symp->typep(resolveSubtype(nodep->subtypep()));
    }
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
    if (nodep->defaultp()) analyzeExpr(nodep->defaultp(), symp->typep());
}

void VhdlSemVisitor::visit(AstVhdlSubprogDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(
        nodep->isFunction() ? VhdlSymKind::FUNCTION : VhdlSymKind::PROCEDURE, nodep->name(),
        nodep);

    // Parameters live in the subprogram's own region, but their types are
    // named in the enclosing one, so resolve them before pushing
    VhdlScope* const outerp = m_scopep;
    VhdlScope* const innerp = pushScope(nodep->name());
    for (AstNode* paramp = nodep->paramsp(); paramp; paramp = paramp->nextp()) {
        AstVhdlIfaceDecl* const ifp = VN_AS(paramp, VhdlIfaceDecl);
        m_scopep = outerp;
        VhdlType* const typep = resolveSubtype(ifp->subtypep());
        m_scopep = innerp;
        VhdlSymbol* const psymp = VhdlSymTab::newSymbol(
            ifp->kind() == VhdlIfaceKind::SIGNAL ? VhdlSymKind::SIGNAL : VhdlSymKind::PARAMETER,
            ifp->name(), ifp);
        psymp->typep(typep);
        ifp->vhdlSymp(psymp);
        symp->addParam(ifp->name(), typep, ifp->defaultp() != nullptr);
        declare(psymp, ifp->fileline());
    }
    if (nodep->isFunction()) {
        m_scopep = outerp;
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
        // declaration, one region out (12.1)
        if (local.empty() && m_inPackageBody) outerp->lookup(nodep->name(), local);
        for (VhdlSymbol* const otherp : local) {
            if (!otherp->hasBody() && symp->homographOf(otherp)) {
                reusep = otherp;
                break;
            }
        }
    }
    if (reusep) {
        reusep->hasBody(true);
        reusep->innerp(innerp);
        nodep->vhdlSymp(reusep);
        if (nodep->hasBody()) reusep->bodyp(nodep);
    } else {
        nodep->vhdlSymp(symp);
        if (nodep->hasBody()) symp->bodyp(nodep);
        declare(symp, nodep->fileline());
    }

    if (nodep->hasBody()) {
        m_scopep = innerp;
        VhdlType* const oldReturnp = m_returnTypep;
        m_returnTypep = symp->returnTypep();
        iterateAndNextNull(nodep->declsp());
        iterateAndNextNull(nodep->stmtsp());
        m_returnTypep = oldReturnp;
    }
    m_scopep = outerp;
}

void VhdlSemVisitor::visit(AstVhdlComponentDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::COMPONENT, nodep->name(), nodep);
    VhdlScope* const innerp = pushScope(nodep->name());
    m_ifacePos = 0;
    iterateAndNextNull(nodep->genericsp());
    m_ifacePos = 0;
    iterateAndNextNull(nodep->portsp());
    popScope();
    symp->innerp(innerp);
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::visit(AstVhdlAliasDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::ALIAS, nodep->name(), nodep);
    if (nodep->subtypep()) symp->typep(resolveSubtype(nodep->subtypep()));
    // Record what the alias denotes, so that an alias of a type may be used
    // as a type mark and an alias of an object as an object (6.6)
    std::vector<std::string> parts;
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
        std::vector<VhdlSymbol*> found;
        if (viaScope && scopep) {
            scopep->lookupLocal(parts.back(), found);
        } else {
            lookup(parts.back(), found);
        }
        // An overloaded name aliases every one of its declarations at once,
        // so only an unambiguous target is worth recording here
        if (found.size() == 1) {
            symp->aliasp(found[0]);
            if (!symp->typep() && found[0]->typep()) symp->typep(found[0]->typep());
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
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

// An attribute specification gives an attribute a value for named entities
// (7.2). Most attributes are of interest only to the tool that defined them
// and are carried no further, but FOREIGN is the language's own way of saying
// that a subprogram is implemented outside VHDL (16.2), so it is acted on.
void VhdlSemVisitor::visit(AstVhdlAttrSpec* nodep) {
    iterateAndNextNull(nodep->valuep());
    if (nodep->name() != "foreign") return;
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
    if (kind == "VHPIDIRECT") {
        dpir = false;
    } else if (kind == "DPI-C" || kind == "DPI") {
        dpir = true;
    } else {
        // A bare name, which GHDL also accepts
        dpir = false;
        first = 0;
    }
    // GHDL allows "VHPIDIRECT <library> <symbol>". Verilator links statically,
    // so the library name has nothing to name and is ignored.
    if (words.size() - first > 2) {
        err(flp, "The FOREIGN attribute's value has more words than 'VHPIDIRECT [library] "
                 "symbol'");
        return false;
    }
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
    }
    pushScope(nodep->name().empty() ? "process" : nodep->name());
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlBlock* nodep) {
    // The maps are written outside the block's own region, since they name
    // what is visible where the block appears (11.2)
    if (nodep->headerp()) {
        analyzeAssociations(nodep->headerp()->genericMapp(), nullptr);
        analyzeAssociations(nodep->headerp()->portMapp(), nullptr);
    }
    pushScope(nodep->name().empty() ? "block" : nodep->name());
    if (nodep->headerp()) {
        m_ifacePos = 0;
        iterateAndNextNull(nodep->headerp()->genericsp());
        m_ifacePos = 0;
        iterateAndNextNull(nodep->headerp()->portsp());
    }
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlGenerate* nodep) {
    // The range is named outside the generate's own region, but the parameter
    // it declares is inside it (11.8)
    RangeInfo info;
    if (nodep->kind() == VhdlLoopKind::FOR) info = analyzeRange(nodep->rangep(), nullptr);
    pushScope(nodep->name().empty() ? "generate" : nodep->name());
    if (nodep->kind() == VhdlLoopKind::FOR) {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::GENERATE_PARAMETER,
                                                       nodep->paramName(), nodep);
        symp->typep(loopParamType(info, nodep->fileline()));
        nodep->vhdlSymp(symp);
        declare(symp, nodep->fileline());
    }
    iterateAndNextNull(nodep->exprp());
    iterateAndNextNull(nodep->altsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlGenerateBody* nodep) {
    // The condition is an expression, not a statement, so it needs analyzing
    // rather than iterating: iterating would leave its names unresolved
    if (nodep->condp()) analyzeExpr(nodep->condp(), V3VhdlStd::booleanType());
    for (AstNode* p = nodep->choicesp(); p; p = p->nextp()) {
        analyzeChoice(VN_AS(p, VhdlChoice), nullptr);
    }
    pushScope(nodep->name().empty() ? "generate body" : nodep->name());
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlLoop* nodep) {
    if (nodep->kind() == VhdlLoopKind::FOR) {
        // The loop parameter is declared by the loop itself, and its subtype
        // is that of the discrete range (10.10)
        const RangeInfo info = analyzeRange(nodep->rangep(), nullptr);
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
    iterateChildren(nodep);
}

// The subtype of a loop or generate parameter: the range's type, narrowed to
// the range itself where it is static (10.10, 11.8)
VhdlType* VhdlSemVisitor::loopParamType(const RangeInfo& info, FileLine* flp) {
    if (!info.typep || info.typep->isError()) return V3VhdlStd::integerType();
    if (!info.left.known() || !info.right.known()) return info.typep;
    return makeRangeSubtype(info, flp);
}

//######################################################################
// Design units

void VhdlSemVisitor::setupUnitContext(AstNodeVhdlUnit* unitp) {
    m_libname = unitp->libname().empty() ? "work" : lower(unitp->libname());

    // Every design unit begins as if it said
    //   library STD, WORK;  use STD.STANDARD.all;      (13.2)
    m_scopep = VhdlSymTab::newScope(nullptr, unitp->name() + "$context");
    m_rootp = m_scopep;
    m_scopep->addUse(V3VhdlStd::standardScope());
    m_scopep->addNamed("std", V3VhdlStd::stdLibScope());
    m_scopep->addNamed("work", libScope(m_libname));
    m_scopep->addNamed(m_libname, libScope(m_libname));

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

    for (AstNode* listp : contexts) {
        for (AstNode* itemp = listp; itemp; itemp = itemp->nextp()) {
            applyContextItem(VN_AS(itemp, VhdlContextItem));
        }
    }
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
            if (lnp) m_scopep->addNamed(lnp->name(), libScope(lnp->name()));
            continue;
        }
        // A use clause: "use lib.unit.all" or "use lib.unit.item"
        if (!flattenUse(exprp, parts) || parts.size() < 2) continue;
        std::string lib = parts[0];
        if (lib == "work") lib = m_libname;
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
        // "use pkg.all" makes everything potentially visible; a selective
        // use clause is treated the same, which is over-permissive but
        // never rejects a legal design
        m_scopep->addUse(targetp);
    }
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
    for (AstNodeVhdlUnit* const unitp : unitsp) {
        setupUnitContext(unitp);
        const std::string lib = m_libname;
        VhdlScope* const unitScopep = pushScope(unitp->name());

        if (AstVhdlEntity* const entp = VN_CAST(unitp, VhdlEntity)) {
            m_ifacePos = 0;
            iterateAndNextNull(entp->genericsp());
            m_ifacePos = 0;
            iterateAndNextNull(entp->portsp());
            iterateAndNextNull(entp->declsp());
            m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
            m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
            libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        } else if (AstVhdlArchitecture* const archp = VN_CAST(unitp, VhdlArchitecture)) {
            // An architecture sees its entity's declarations (3.3)
            std::vector<std::string> parts;
            if (flatten(archp->entityNamep(), parts) && !parts.empty()) {
                const auto it = m_unitScopes.find(lib + "." + parts.back());
                if (it != m_unitScopes.end()) m_scopep->addUse(it->second);
            }
            iterateAndNextNull(archp->declsp());
            iterateAndNextNull(archp->stmtsp());
            analyzeDrivers(archp);
        } else if (AstVhdlPackage* const pkgp = VN_CAST(unitp, VhdlPackage)) {
            iterateAndNextNull(pkgp->genericsp());
            iterateAndNextNull(pkgp->declsp());
            m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
            m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
            libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        } else if (AstVhdlPackageBody* const bodyp = VN_CAST(unitp, VhdlPackageBody)) {
            const auto it = m_unitScopes.find(lib + "." + lower(unitp->name()));
            if (it != m_unitScopes.end()) m_scopep->addUse(it->second);
            VL_RESTORER(m_inPackageBody);
            m_inPackageBody = true;
            iterateAndNextNull(bodyp->declsp());
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

VhdlSymbol* VhdlSemVisitor::specializeSubprogram(AstVhdlSubprogDecl* originalp,
                                                 VhdlScope* regionp,
                                                 const std::vector<VhdlType*>& actualTypes,
                                                 const std::string& name) {
    AstVhdlSubprogDecl* const copyp = originalp->cloneTree(false);
    copyp->name(name);
    VhdlUnanalyzeVisitor{copyp};
    // Each parameter is declared with the subtype the call supplies. Setting
    // the resolved type on the indication is enough: resolveSubtype takes it
    // as already worked out, which is exactly what a written-out constraint
    // would have produced.
    size_t i = 0;
    for (AstNode* p = copyp->paramsp(); p; p = p->nextp(), ++i) {
        AstVhdlIfaceDecl* const ifp = VN_AS(p, VhdlIfaceDecl);
        if (i >= actualTypes.size() || !actualTypes[i]) continue;
        if (!ifp->subtypep()) continue;
        VhdlType* const paramp = originalp->paramsp()
                                     ? nullptr
                                     : nullptr;  // Unused; kept for clarity
        (void)paramp;
        ifp->subtypep()->vhdlTypep(actualTypes[i]);
    }
    // Analyze the copy in the region the original was declared in, so it sees
    // exactly the names the original did
    VhdlScope* const savedp = m_scopep;
    VhdlType* const savedReturnp = m_returnTypep;
    m_scopep = regionp;
    m_returnTypep = nullptr;
    iterate(copyp);
    m_scopep = savedp;
    m_returnTypep = savedReturnp;
    // The copy belongs beside the original, so lowering finds it
    originalp->addNextHere(copyp);
    return copyp->vhdlSymp();
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

class VhdlSpecVisitor final : public VNVisitor {
    // STATE
    VhdlSemVisitor& m_sem;
    // Subprogram and actual types -> the copy made for them
    std::map<std::string, VhdlSymbol*> m_specializations;
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
    void passOver(AstNode* nodep) {
        m_madeOne = false;
        iterateAndNextNull(nodep);
    }
    bool madeOne() const { return m_madeOne; }

private:
    void visit(AstNode* nodep) override { iterateChildren(nodep); }
    void visit(AstVhdlCall* nodep) override {
        iterateChildren(nodep);
        trySpecialize(nodep, nodep->vhdlSymp(), nodep->argsp());
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
        if (V3VhdlStd::isIntrinsicName(symp->name())) return false;
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY
                && !typep->isConstrainedArray()) {
                return true;
            }
        }
        return false;
    }

    void trySpecialize(AstVhdlCall* nodep, VhdlSymbol* symp, AstNode* argsp) {
        if (!needsSpecializing(symp)) return;
        // The actual types, which must all be known for the copy to be made
        std::vector<VhdlType*> actualTypes;
        size_t i = 0;
        for (AstNode* p = argsp; p; p = p->nextp(), ++i) {
            AstVhdlAssoc* const asp = VN_CAST(p, VhdlAssoc);
            if (!asp || !asp->actualp() || i >= symp->paramTypes().size()) return;
            VhdlType* typep = asp->actualp()->vhdlTypep();
            // Only a length that is known is worth specializing on
            if (symp->paramTypes()[i]
                && symp->paramTypes()[i]->base()->kind() == VhdlTypeKind::ARRAY) {
                if (!typep || !typep->isConstrainedArray()) return;
                // A length of two billion is an index subtype's range, not an
                // object's; specializing on it would be meaningless and would
                // make the copy's own bounds checks fire
                const int64_t length = typep->dimLength(0);
                if (length <= 0 || length > (1 << 20)) return;
            }
            actualTypes.push_back(typep);
        }
        if (actualTypes.size() != symp->paramTypes().size()) return;

        std::string key = symp->name();
        for (const VhdlType* const typep : actualTypes) {
            key += "|" + std::to_string(reinterpret_cast<uintptr_t>(typep));
        }
        const auto it = m_specializations.find(key);
        if (it != m_specializations.end()) {
            nodep->vhdlSymp(it->second);
            return;
        }
        AstVhdlSubprogDecl* const bodyp = symp->bodyp();
        VhdlScope* const regionp = symp->innerp() ? symp->innerp()->parentp() : nullptr;
        if (!bodyp || !regionp) return;
        const std::string name = symp->name() + "__V" + std::to_string(++m_count);
        VhdlSymbol* const newp = m_sem.specializeSubprogram(bodyp, regionp, actualTypes, name);
        m_specializations.emplace(key, newp);
        if (newp) {
            nodep->vhdlSymp(newp);
            m_madeOne = true;
        }
    }
};

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
void collectDrivenSyms(AstNode* nodep, std::set<const VhdlSymbol*>& whole,
                       std::set<const VhdlSymbol*>& partial, bool follow) {
    if (!nodep) return;
    if (const AstVhdlSigAssign* const asgp = VN_CAST(nodep, VhdlSigAssign)) {
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

void VhdlSemVisitor::analyzeDrivers(AstVhdlArchitecture* archp) {
    std::map<const VhdlSymbol*, int> counts;
    std::map<const VhdlSymbol*, FileLine*> where;
    std::set<const VhdlSymbol*> partial;
    for (AstNode* p = archp->stmtsp(); p; p = p->nextp()) {
        std::set<const VhdlSymbol*> whole;
        collectDrivenSyms(p, whole, partial, false);
        for (const VhdlSymbol* const symp : whole) {
            ++counts[symp];
            if (!where.count(symp)) where[symp] = p->fileline();
        }
    }
    for (const auto& pair : counts) {
        VhdlSymbol* const symp = const_cast<VhdlSymbol*>(pair.first);
        if (partial.count(symp)) continue;  // Its elements are driven, not it
        symp->driverCount(pair.second);
        if (pair.second < 2) continue;
        VhdlType* const typep = symp->typep();
        VhdlSymbol* const resolvep = typep ? typep->resolutionp() : nullptr;
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
    for (const NeedResolve& need : m_needResolve) {
        VhdlSymbol* const resolvep = need.resolvep;
        if (resolvep->paramTypes().size() != 1) continue;
        VhdlType* const paramp = resolvep->paramTypes()[0];
        if (!paramp || paramp->base()->kind() != VhdlTypeKind::ARRAY) continue;
        if (paramp->isConstrainedArray()) continue;  // Already a fixed size
        AstVhdlSubprogDecl* const bodyp = resolvep->bodyp();
        VhdlScope* const regionp = resolvep->innerp() ? resolvep->innerp()->parentp() : nullptr;
        if (!bodyp || !regionp) continue;  // Predefined, as ieee's own is
        if (!bodyp->backp()) continue;  // Not in the tree, so nothing to copy beside
        // The drivers as an array indexed from zero
        VhdlType* const argp = cloneSubtype(paramp->base(), need.flp);
        VhdlType* const idxp = VhdlSymTab::newType(VhdlTypeKind::INTEGER, "", need.flp);
        idxp->base(V3VhdlStd::integerType()->base());
        idxp->setBounds(0, need.count - 1, true);
        argp->clearIndexTypes();
        argp->addIndexType(idxp);
        argp->unconstrained(false);
        const std::string name
            = resolvep->name() + "__V" + std::to_string(need.count) + "drv";
        std::vector<VhdlType*> actualTypes{argp};
        if (VhdlSymbol* const newp
            = specializeSubprogram(bodyp, regionp, actualTypes, name)) {
            need.symp->resolverp(newp);
        }
    }
}

bool V3VhdlSem::analyze(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    V3VhdlStd::build();
    VhdlSemVisitor visitor;
    visitor.analyzeUnits(unitsp);
    // Give every call to an unconstrained subprogram a copy with the lengths
    // it supplies. A copy may itself call one, so repeat until nothing new
    // appears; the bound is defensive, since a chain that never settles is a
    // recursion on data-dependent lengths, which has no static answer.
    VhdlSpecVisitor specializer{visitor};
    for (int round = 0; round < 20; ++round) {
        if (!specializer.pass(unitsp)) break;
    }
    // A resolution function is called once per resolved signal rather than
    // from anywhere in the source, so its copy is made here rather than by
    // the pass above
    visitor.specializeResolvers();
    return !visitor.errored();
}

void V3VhdlSem::clear() {
    V3VhdlStd::clear();
    VhdlSymTab::clear();
}

//######################################################################
// Expressions (clauses 8, 9, 12.5)

bool VhdlSemVisitor::typesMatch(const VhdlType* fromp, const VhdlType* top) {
    if (!fromp || !top) return true;
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

void VhdlSemVisitor::candidateTypes(AstNodeVhdlExpr* nodep, CandSet& out) {
    if (!nodep) return;
    if (nodep->vhdlTypep()) {
        out.types.push_back(nodep->vhdlTypep());
        return;
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
            if (symp->isOverloadable() && !symp->paramTypes().empty()) continue;
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

VhdlType* VhdlSemVisitor::pick(AstNodeVhdlExpr* nodep, std::vector<VhdlSymbol*>& cands,
                               VhdlType* wantp, const std::string& what) {
    if (cands.empty()) {
        err(nodep->fileline(), "No visible " + what + " matches this use");
        return V3VhdlStd::errorType();
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
        if (preferred.size() == 1) {
            cands.swap(preferred);
        } else {
            err(nodep->fileline(),
                "Ambiguous " + what + ": " + std::to_string(cands.size())
                    + " visible declarations match, and the context does not decide between them");
            nodep->vhdlSymp(cands[0]);
            return cands[0]->returnTypep() ? cands[0]->returnTypep() : cands[0]->typep();
        }
    }
    nodep->vhdlSymp(cands[0]);
    VhdlType* const resultp = cands[0]->returnTypep() ? cands[0]->returnTypep() : cands[0]->typep();
    return resultp ? resultp : V3VhdlStd::errorType();
}

VhdlType* VhdlSemVisitor::analyzeName(AstVhdlName* nodep, VhdlType* wantp) {
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
        if (symp->isOverloadable() && !symp->paramTypes().empty()) continue;  // Needs arguments
        if (symp->isOverloadable()) {
            callables.push_back(symp);
        } else {
            objects.push_back(symp);
        }
    }
    if (!objects.empty()) {
        nodep->vhdlSymp(objects[0]);
        return objects[0]->typep() ? objects[0]->typep() : V3VhdlStd::errorType();
    }
    if (callables.empty()) {
        err(nodep->fileline(), "'" + nodep->name() + "' needs arguments here");
        return V3VhdlStd::errorType();
    }
    return pick(nodep, callables, wantp, "declaration of '" + nodep->name() + "'");
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
        if (wantp && wantp->base()->kind() == VhdlTypeKind::ARRAY) return wantp;
        return nodep->kind() == VhdlLitKind::STRING ? V3VhdlStd::stringType()
                                                    : V3VhdlStd::bitVectorType();
    case VhdlLitKind::NULLLIT:
        if (wantp && wantp->kind() == VhdlTypeKind::ACCESS) return wantp;
        return V3VhdlStd::errorType();
    }
    return V3VhdlStd::errorType();  // LCOV_EXCL_LINE
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
            if (!methods.empty()) {
                nodep->prefixp()->vhdlSymp(objp);
                return pick(nodep, methods, wantp, "method '" + sufp->name() + "'");
            }
        }
    }

    // Otherwise a record element (8.3)
    VhdlType* const prefixp = analyzeExpr(nodep->prefixp(), nullptr);
    if (!prefixp || prefixp->isError()) return V3VhdlStd::errorType();
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
    for (VhdlSymbol* const elemp : recp->base()->elements()) {
        if (elemp->name() == suffixp->name()) {
            nodep->vhdlSymp(elemp);
            return elemp->typep();
        }
    }
    err(nodep->fileline(), "Record type " + recp->prettyName() + " has no element '"
                               + suffixp->name() + "'");
    return V3VhdlStd::errorType();
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
    if (!prefixTypep) prefixTypep = analyzeExpr(nodep->prefixp(), nullptr);
    // Record what the prefix denotes, so that folding an attribute later does
    // not have to work it out again
    if (nodep->prefixp() && !nodep->prefixp()->vhdlTypep()) {
        nodep->prefixp()->vhdlTypep(prefixTypep);
    }

    const std::string& attr = nodep->name();
    const bool isArray = prefixTypep && prefixTypep->base()->kind() == VhdlTypeKind::ARRAY;

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
            return isArray && prefixTypep->base()->elementp() ? prefixTypep->base()->elementp()
                                                              : V3VhdlStd::errorType();
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
    if (attr == "event" || attr == "active" || attr == "quiet" || attr == "stable"
        || attr == "driving") {
        return V3VhdlStd::booleanType();
    }
    if (attr == "instance_name" || attr == "path_name" || attr == "simple_name") {
        return V3VhdlStd::stringType();
    }
    if (attr == "last_value" || attr == "delayed" || attr == "driving_value"
        || attr == "transaction" || attr == "base") {
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (attr == "last_event" || attr == "last_active") return V3VhdlStd::timeType();
    // A user-defined attribute (7.2)
    std::vector<VhdlSymbol*> found;
    lookup(attr, found);
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() == VhdlSymKind::ATTRIBUTE) {
            nodep->vhdlSymp(symp);
            return symp->typep();
        }
    }
    err(nodep->fileline(), "Unknown attribute '" + attr + "'");
    return V3VhdlStd::errorType();
}

VhdlType* VhdlSemVisitor::analyzeAggregate(AstVhdlAggregate* nodep, VhdlType* wantp) {
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
        VhdlType* elemTypep = wantp->base()->elementp();
        if (wantp->base()->dimensions() > 1) {
            // In a multidimensional aggregate each element is itself an
            // aggregate covering the remaining dimensions (9.3.3.3), so hand
            // the elements an array type with one fewer index
            VhdlType* const subp = VhdlSymTab::newType(VhdlTypeKind::ARRAY, "",
                                                       nodep->fileline());
            const std::vector<VhdlType*>& idx = wantp->base()->indexTypes();
            for (size_t i = 1; i < idx.size(); ++i) subp->addIndexType(idx[i]);
            subp->elementp(wantp->base()->elementp());
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
            analyzeExpr(eap->valuep(), elemTypep);
            checkValueInRange(eap->valuep(), elemTypep, "an element of this aggregate");
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
    // A record aggregate: positional elements take the element types in order
    size_t pos = 0;
    const std::vector<VhdlSymbol*>& elems = wantp->base()->elements();
    for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
        AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
        VhdlType* fieldp = nullptr;
        if (eap->choicesp()) {
            const AstVhdlChoice* const chp = eap->choicesp();
            const AstVhdlName* const namep = VN_CAST(chp->valuep(), VhdlName);
            if (namep) {
                for (VhdlSymbol* const symp : elems) {
                    if (symp->name() == namep->name()) fieldp = symp->typep();
                }
            } else if (chp->isOthers()) {
                // "others => x" covers whatever is left
                fieldp = elems.empty() ? nullptr : elems.back()->typep();
            }
        } else if (pos < elems.size()) {
            fieldp = elems[pos]->typep();
        }
        ++pos;
        analyzeExpr(eap->valuep(), fieldp);
    }
    return wantp;
}

// Work out which formal each actual is for. Positional associations come
// first and take the formals in order; a named one names its formal. Any
// formal left without an actual must have a default (6.5.7.1). Returns false
// if the actuals cannot be bound to this subprogram at all.
bool VhdlSemVisitor::bindActuals(const VhdlSymbol* symp,
                                 const std::vector<AstVhdlAssoc*>& assocs,
                                 std::vector<int>& formalOf) {
    const size_t formals = symp->paramTypes().size();
    formalOf.assign(assocs.size(), -1);
    std::vector<bool> filled(formals, false);
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
        if (!formalNamep) return false;  // A conversion or an indexed formal
        const int idx = symp->paramIndex(formalNamep->name());
        if (idx < 0 || filled[idx]) return false;
        formalOf[i] = idx;
        filled[idx] = true;
    }
    for (size_t i = 0; i < formals; ++i) {
        if (!filled[i] && !symp->paramHasDefault(i)) return false;
    }
    return true;
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

    // A type mark in this position makes it a type conversion (9.3.6) or a
    // constrained subtype indication that the parser could not tell apart
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() == VhdlSymKind::TYPE) {
            nodep->prefixp()->vhdlSymp(symp);
            for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
                if (AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc)) {
                    analyzeExpr(asp->actualp(), nullptr);
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
        VhdlType* const typep = analyzeExpr(nodep->prefixp(), nullptr);
        if (typep && typep->base()->kind() == VhdlTypeKind::ARRAY) {
            const bool isSlice = (nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc));
            return isSlice ? analyzeSliceName(nodep, typep) : analyzeIndexedName(nodep, typep);
        }
        if (typep && typep->isError()) return typep;
    }

    // An object prefix makes it an indexed name or a slice (8.4, 8.5)
    for (VhdlSymbol* const symp : found) {
        if (!symp->isObject() && symp->kind() != VhdlSymKind::ALIAS) continue;
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
        for (const VhdlSymbol* const symp : found) {
            if (symp->kind() == VhdlSymKind::FUNCTION
                || symp->kind() == VhdlSymKind::PROCEDURE) {
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
    std::vector<int> formalOf;
    int best = -1;
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() != VhdlSymKind::FUNCTION && symp->kind() != VhdlSymKind::PROCEDURE) {
            continue;
        }
        if (!bindActuals(symp, assocs, formalOf)) continue;
        bool ok = true;
        int score = 0;
        for (size_t i = 0; i < argCands.size() && ok; ++i) {
            if (formalOf[i] < 0) continue;
            VhdlType* const paramp = symp->paramTypes()[formalOf[i]];
            if (!anyMatches(argCands[i], paramp)) ok = false;
            if (exactMatch(argCands[i], paramp)) ++score;
        }
        if (!ok) continue;
        if (score > best) {
            best = score;
            cands.clear();
        }
        if (score == best) cands.push_back(symp);
    }
    if (cands.empty()) {
        for (const VhdlType* const typep : argTypes) {
            if (typep && typep->isError()) return V3VhdlStd::errorType();
        }
        const std::string name = parts.empty() ? std::string{"<name>"} : parts.back();
        err(nodep->fileline(), "No visible subprogram '" + name + "' takes "
                                   + std::to_string(argCands.size())
                                   + " argument(s) of these types");
        for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
            if (AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc)) {
                analyzeExpr(asp->actualp(), nullptr);
            }
        }
        return V3VhdlStd::errorType();
    }
    VhdlType* const resultp
        = pick(nodep, cands, wantp,
               "subprogram '" + (parts.empty() ? std::string{"<name>"} : parts.back()) + "'");
    // The chosen signature now settles every actual
    if (nodep->vhdlSymp() && bindActuals(nodep->vhdlSymp(), assocs, formalOf)) {
        for (size_t i = 0; i < assocs.size(); ++i) {
            if (!assocs[i] || !assocs[i]->actualp() || formalOf[i] < 0) continue;
            VhdlType* const paramp = nodep->vhdlSymp()->paramTypes()[formalOf[i]];
            analyzeExpr(assocs[i]->actualp(), paramp);
            // An actual must belong to the parameter's subtype (4.2.2.1)
            if (!nodep->vhdlSymp()->predefined()) {
                const std::string what = "parameter " + std::to_string(formalOf[i] + 1) + " of '"
                                         + nodep->vhdlSymp()->name() + "'";
                checkValueInRange(assocs[i]->actualp(), paramp, what);
                checkArrayValue(assocs[i]->actualp(), paramp, what);
            }
        }
    }
    return resultp;
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
    checkValueInRange(nodep, targetp, "the assignment target");
    checkArrayValue(nodep, targetp, "the assignment target");
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
    return arrayp->base()->elementp();
}

// A slice is of the array's type, constrained by the slice's own range, which
// must run the same way as the array's and lie within it (8.5)
VhdlType* VhdlSemVisitor::analyzeSliceName(AstVhdlCall* nodep, VhdlType* arrayp) {
    VhdlType* const indexp = arrayp->indexTypes().empty() ? nullptr : arrayp->indexTypes()[0];
    const RangeInfo info = analyzeRange(nodep->argsp(), indexp);
    VhdlType* const outp = cloneSubtype(arrayp, nodep->fileline());
    outp->clearIndexTypes();
    VhdlType* sliceIndexp = info.typep ? info.typep : indexp;
    if (info.left.known() && info.right.known()) {
        sliceIndexp = makeRangeSubtype(info, nodep->fileline());
        if (indexp && indexp->bounded() && !sliceIndexp->isNull()) {
            if (info.ascending != indexp->ascending()) {
                err(nodep->fileline(), "Slice runs "
                                           + std::string{info.ascending ? "'to'" : "'downto'"}
                                           + " but " + arrayp->prettyName() + " is indexed "
                                           + (indexp->ascending() ? "'to'" : "'downto'"));
            } else {
                checkSubrange(sliceIndexp, indexp, nodep->fileline());
            }
        }
    } else if (sliceIndexp) {
        sliceIndexp = cloneSubtype(sliceIndexp, nodep->fileline());
        sliceIndexp->unbound();
    }
    outp->addIndexType(sliceIndexp ? sliceIndexp : V3VhdlStd::integerType());
    outp->unconstrained(!sliceIndexp || !sliceIndexp->bounded());
    return outp;
}

// The number of elements a value has, where that is statically known, or -1
int64_t VhdlSemVisitor::staticLength(AstNodeVhdlExpr* nodep) {
    if (!nodep) return -1;
    if (AstVhdlAggregate* const aggp = VN_CAST(nodep, VhdlAggregate)) {
        return aggregateLength(aggp);
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
    if (!fromp || !fromp->isConstrainedArray()
        || fromp->dimensions() != top->base()->indexTypes().size()) {
        return top;
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
            } else {
                const VhdlValue v = V3VhdlEval::eval(chp->valuep());
                if (!v.isInt()) return nullptr;
                lo = hi = v.toInt();
            }
            lowest = std::min(lowest, lo);
            highest = std::max(highest, hi);
        }
    }
    int64_t left = 0;
    int64_t right = 0;
    if (named) {
        if (lowest > highest) return nullptr;
        left = indexp->ascending() ? lowest : highest;
        right = indexp->ascending() ? highest : lowest;
    } else {
        if (positional <= 0) return nullptr;
        left = indexp->left();
        right = indexp->ascending() ? left + positional - 1 : left - positional + 1;
    }
    VhdlType* const outp = cloneSubtype(typep, nodep->fileline());
    VhdlType* const newIndexp = cloneSubtype(indexp, nodep->fileline());
    newIndexp->setBounds(left, right, indexp->ascending());
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
VhdlType* VhdlSemVisitor::constrainFromValue(VhdlType* typep, AstNodeVhdlExpr* valuep,
                                             FileLine* flp) {
    if (!typep || !valuep) return typep;
    if (typep->base()->kind() != VhdlTypeKind::ARRAY || !typep->unconstrained()) return typep;
    // A value that is itself constrained hands over its index ranges directly
    VhdlType* const valueTypep = valuep->vhdlTypep();
    if (valueTypep && valueTypep->isConstrainedArray()
        && valueTypep->dimensions() == typep->dimensions()) {
        VhdlType* const outp = cloneSubtype(typep, flp);
        outp->clearIndexTypes();
        for (VhdlType* const idxp : valueTypep->indexTypes()) outp->addIndexType(idxp);
        outp->unconstrained(false);
        return outp;
    }
    // Otherwise the value's length decides, starting at the index subtype's
    // own left bound (9.3.3.3)
    if (typep->dimensions() != 1) return typep;
    const int64_t len = staticLength(valuep);
    if (len < 0) return typep;
    VhdlType* const indexp = typep->indexTypes()[0];
    if (!indexp || !indexp->bounded() || indexp->base()->kind() == VhdlTypeKind::FLOATING) {
        return typep;
    }
    VhdlType* const newIndexp = cloneSubtype(indexp, flp);
    const int64_t left = indexp->left();
    newIndexp->setBounds(left, indexp->ascending() ? left + len - 1 : left - len + 1,
                         indexp->ascending());
    VhdlType* const outp = cloneSubtype(typep, flp);
    outp->clearIndexTypes();
    outp->addIndexType(newIndexp);
    outp->unconstrained(false);
    return outp;
}

// An array value must have exactly as many elements as the constrained
// subtype it is given to (5.3.2.2)
void VhdlSemVisitor::checkArrayValue(AstNodeVhdlExpr* nodep, VhdlType* typep,
                                     const std::string& what) {
    if (!nodep || !typep || typep->base()->kind() != VhdlTypeKind::ARRAY) return;
    if (!typep->isConstrainedArray()) return;
    if (AstVhdlAggregate* const aggp = VN_CAST(nodep, VhdlAggregate)) {
        checkAggregate(aggp, typep, what);
        return;
    }
    // Another array value must match dimension for dimension
    VhdlType* const valuep = nodep->vhdlTypep();
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
                if (lo > hi) continue;  // A null range names nothing
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
    if (!allStatic || typep->dimensions() != 1) return;
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
        // A qualified expression states its own type (9.3.5)
        resultp = resolveTypeMark(p->typeMarkp());
        analyzeExpr(p->valuep(), resultp);
    } else if (AstVhdlAllocator* const p = VN_CAST(nodep, VhdlAllocator)) {
        if (p->subtypep()) {
            resultp = resolveSubtype(p->subtypep());
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

void VhdlSemVisitor::visit(AstVhdlSigAssign* nodep) {
    VhdlType* const targetp = analyzeExpr(nodep->targetp(), nullptr);
    // Every waveform element is of the target's type (10.5.2.2)
    for (AstNode* wp = nodep->wavesp(); wp; wp = wp->nextp()) {
        AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
        if (wfp->valuep()) {
            analyzeExpr(wfp->valuep(), targetp);
            checkAssignedValue(wfp->valuep(), targetp);
        }
        if (wfp->afterp()) analyzeExpr(wfp->afterp(), V3VhdlStd::timeType());
    }
    checkWaveform(nodep->wavesp());
    if (nodep->selectorp()) analyzeExpr(nodep->selectorp(), nullptr);
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        if (altp->condp()) analyzeExpr(altp->condp(), V3VhdlStd::booleanType());
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            analyzeChoice(chp, nodep->selectorp() ? nodep->selectorp()->vhdlTypep() : nullptr);
        }
        for (AstNode* wp = altp->wavesp(); wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
            if (wfp->valuep()) {
                analyzeExpr(wfp->valuep(), targetp);
                checkAssignedValue(wfp->valuep(), targetp);
            }
            if (wfp->afterp()) analyzeExpr(wfp->afterp(), V3VhdlStd::timeType());
        }
        checkWaveform(altp->wavesp());
    }
}

void VhdlSemVisitor::visit(AstVhdlVarAssign* nodep) {
    VhdlType* const targetp = analyzeExpr(nodep->targetp(), nullptr);
    if (nodep->valuep()) {
        analyzeExpr(nodep->valuep(), targetp);
        checkAssignedValue(nodep->valuep(), targetp);
    }
    if (nodep->selectorp()) analyzeExpr(nodep->selectorp(), nullptr);
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        if (altp->condp()) analyzeExpr(altp->condp(), V3VhdlStd::booleanType());
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            analyzeChoice(VN_AS(cp, VhdlChoice),
                          nodep->selectorp() ? nodep->selectorp()->vhdlTypep() : nullptr);
        }
        for (AstNode* wp = altp->wavesp(); wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
            if (wfp->valuep()) {
                analyzeExpr(wfp->valuep(), targetp);
                checkAssignedValue(wfp->valuep(), targetp);
            }
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlIf* nodep) {
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        // A condition is of type BOOLEAN, or is converted to one by "??" (9.2.9)
        if (altp->condp()) analyzeExpr(altp->condp(), V3VhdlStd::booleanType());
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
    if (!typep || typep->isError() || nodep->matching()) return;
    const bool discrete = typep->isDiscrete() && typep->bounded();
    const bool array = typep->base()->kind() == VhdlTypeKind::ARRAY;
    if (!discrete && !array) return;

    bool hasOthers = false;
    bool allStatic = true;
    std::vector<std::pair<int64_t, int64_t>> covered;  // Discrete choices
    std::vector<FileLine*> flps;
    std::map<std::string, FileLine*> seenStrings;  // Array choices
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
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
                const auto pair = seenStrings.emplace(val.toStr(), valuep->fileline());
                if (!pair.second) {
                    err(valuep->fileline(), "Choice " + val.prettyName()
                                                + " is given by more than one alternative of "
                                                  "this case statement");
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
    if (!allStatic || !discrete) return;

    std::vector<size_t> order(covered.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(),
              [&covered](size_t a, size_t b) { return covered[a] < covered[b]; });
    for (size_t i = 1; i < order.size(); ++i) {
        if (covered[order[i]].first > covered[order[i - 1]].second) continue;
        err(flps[order[i]],
            "Choice " + VhdlValue::mkInt(covered[order[i]].first, typep).prettyName()
                + " is given by more than one alternative of this case statement");
        return;
    }
    if (hasOthers) return;
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
    err(nodep->fileline(), "This case statement has no alternative for value "
                               + VhdlValue::mkInt(next, typep).prettyName() + " of "
                               + typep->prettyName());
}

void VhdlSemVisitor::visit(AstVhdlAssert* nodep) {
    if (nodep->condp()) analyzeExpr(nodep->condp(), V3VhdlStd::booleanType());
    if (nodep->reportp()) analyzeExpr(nodep->reportp(), V3VhdlStd::stringType());
    if (nodep->severityp()) analyzeExpr(nodep->severityp(), V3VhdlStd::severityType());
}

void VhdlSemVisitor::visit(AstVhdlWait* nodep) {
    for (AstNode* sp = nodep->sensp(); sp; sp = sp->nextp()) {
        analyzeExpr(VN_AS(sp, NodeVhdlExpr), nullptr);
    }
    if (nodep->condp()) analyzeExpr(nodep->condp(), V3VhdlStd::booleanType());
    if (nodep->timeoutp()) {
        analyzeExpr(nodep->timeoutp(), V3VhdlStd::timeType());
        checkDelay(nodep->timeoutp(), "timeout");
    }
}

void VhdlSemVisitor::visit(AstVhdlReturn* nodep) {
    if (!nodep->valuep()) return;
    if (!m_returnTypep) {
        err(nodep->fileline(), "A procedure's 'return' may not have a value");
        return;
    }
    analyzeExpr(nodep->valuep(), m_returnTypep);
}

// An instance associates actuals with the formals of the component or entity
// it names (6.5.7.1). Resolving the unit first gives every actual the type its
// formal has, which is what lets the bounds of a port be checked here rather
// than after elaboration.
void VhdlSemVisitor::visit(AstVhdlInstance* nodep) {
    std::vector<std::string> parts;
    VhdlScope* unitScopep = nullptr;
    if (flatten(nodep->unitp(), parts) && !parts.empty()) {
        if (parts.size() > 1) {
            // "entity lib.name" names the entity directly (11.7)
            std::string lib = parts[0];
            if (lib == "work") lib = m_libname;
            const auto it = m_unitScopes.find(lib + "." + parts.back());
            if (it != m_unitScopes.end()) unitScopep = it->second;
        } else {
            std::vector<VhdlSymbol*> found;
            lookup(parts.back(), found);
            for (VhdlSymbol* const symp : found) {
                if (symp->kind() == VhdlSymKind::COMPONENT
                    || symp->kind() == VhdlSymKind::ENTITY) {
                    nodep->unitp()->vhdlSymp(symp);
                    unitScopep = symp->innerp();
                    break;
                }
            }
            if (!unitScopep) {
                // Default binding is to the entity of the same name (7.3.3),
                // which may not have been analyzed into this library yet
                const auto it = m_unitScopes.find(m_libname + "." + parts.back());
                if (it != m_unitScopes.end()) unitScopep = it->second;
            }
        }
    }
    analyzeAssociations(nodep->genericMapp(), unitScopep);
    analyzeAssociations(nodep->portMapp(), unitScopep);
}

// Each actual takes the type of the formal it is associated with, by position
// or by name
void VhdlSemVisitor::analyzeAssociations(AstNode* assocsp, VhdlScope* unitScopep) {
    int position = 0;
    for (AstNode* p = assocsp; p; p = p->nextp()) {
        AstVhdlAssoc* const asp = VN_AS(p, VhdlAssoc);
        if (!asp->actualp()) {
            ++position;
            continue;
        }
        VhdlType* formalTypep = nullptr;
        if (unitScopep) {
            if (const AstVhdlName* const namep = VN_CAST(asp->formalp(), VhdlName)) {
                std::vector<VhdlSymbol*> found;
                unitScopep->lookupLocal(namep->name(), found);
                if (!found.empty()) {
                    asp->formalp()->vhdlSymp(found[0]);
                    formalTypep = found[0]->typep();
                }
            } else if (!asp->formalp()) {
                formalTypep = formalAt(unitScopep, position);
            }
        }
        analyzeExpr(asp->actualp(), formalTypep);
        if (formalTypep) {
            checkValueInRange(asp->actualp(), formalTypep, "this association");
            checkArrayValue(asp->actualp(), formalTypep, "this association");
        }
        ++position;
    }
}

// The formal at one position. A component's declarations are held in a map,
// so the order comes from the declaration node rather than the scope.
VhdlType* VhdlSemVisitor::formalAt(VhdlScope* scopep, int position) {
    for (const auto& pair : scopep->direct()) {
        VhdlSymbol* const symp = pair.second;
        if (symp->position() == position
            && (symp->kind() == VhdlSymKind::PORT || symp->kind() == VhdlSymKind::GENERIC)) {
            return symp->typep();
        }
    }
    return nullptr;
}

void VhdlSemVisitor::visit(AstVhdlProcCall* nodep) { analyzeExpr(nodep->callp(), nullptr); }

void VhdlSemVisitor::visit(AstVhdlNext* nodep) {
    if (nodep->condp()) analyzeExpr(nodep->condp(), V3VhdlStd::booleanType());
}
