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
#include "V3VhdlStd.h"
#include "V3VhdlSym.h"

#include <map>
#include <string>
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
        }
        if (found.empty()) {
            err(namep->fileline(), std::string{"Cannot find "} + what + " '" + parts.back() + "'");
        } else {
            err(namep->fileline(), "'" + parts.back() + "' is a " + found[0]->kindName()
                                       + ", but a " + what + " is required here");
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

    // METHODS - types

    // Resolve a type mark to the type it names
    VhdlType* resolveTypeMark(AstNodeVhdlExpr* markp) {
        if (!markp) return V3VhdlStd::errorType();
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
        VhdlType* basep = nullptr;
        if (AstVhdlCall* const callp = VN_CAST(nodep->typeMarkp(), VhdlCall)) {
            // The parser could not tell "std_logic_vector(7 downto 0)" from a
            // call, so a constrained type mark arrives as one (8.1)
            basep = resolveTypeMark(callp->prefixp());
        } else {
            basep = resolveTypeMark(nodep->typeMarkp());
        }
        if (!basep || basep->isError()) {
            nodep->vhdlTypep(V3VhdlStd::errorType());
            return nodep->vhdlTypep();
        }
        VhdlType* typep = basep;
        const bool constrained = nodep->constraintp() || VN_IS(nodep->typeMarkp(), VhdlCall);
        if (constrained) {
            // A constrained subtype is a distinct subtype of the same type
            typep = VhdlSymTab::newType(basep->kind(), "", nodep->fileline());
            typep->base(basep->base());
            typep->elementp(basep->elementp());
            typep->designatedp(basep->designatedp());
            for (VhdlType* const idxp : basep->indexTypes()) typep->addIndexType(idxp);
            for (VhdlSymbol* const elemp : basep->elements()) typep->addElement(elemp);
            typep->resolutionp(basep->resolutionp());
        }
        if (nodep->resolutionp()) {
            VhdlSymbol* const resp
                = lookupOne(nodep->resolutionp(), VhdlSymKind::FUNCTION, "resolution function");
            if (resp) typep->resolutionp(resp);
        }
        nodep->vhdlTypep(typep);
        return typep;
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
                VhdlSymbol* const symp
                    = VhdlSymTab::newSymbol(VhdlSymKind::ENUM_LITERAL, elp->name(), elp);
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
            const bool isReal = rangeIsReal(nump->rangep());
            VhdlType* const typep = VhdlSymTab::newType(
                isReal ? VhdlTypeKind::FLOATING : VhdlTypeKind::INTEGER, name, flp);
            return typep;
        }
        if (AstVhdlPhysicalType* const physp = VN_CAST(defp, VhdlPhysicalType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::PHYSICAL, name, flp);
            int64_t primary = 1;
            bool first = true;
            for (AstNode* unitp = physp->unitsp(); unitp; unitp = unitp->nextp()) {
                AstVhdlUnitDecl* const udp = VN_AS(unitp, VhdlUnitDecl);
                VhdlSymbol* const symp
                    = VhdlSymTab::newSymbol(VhdlSymKind::PHYSICAL_UNIT, udp->name(), udp);
                symp->typep(typep);
                udp->vhdlSymp(symp);
                typep->addUnit(udp->name(), first ? primary : 0);
                first = false;
                declare(symp, udp->fileline());
            }
            return typep;
        }
        if (AstVhdlArrayType* const arrp = VN_CAST(defp, VhdlArrayType)) {
            VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ARRAY, name, flp);
            bool unconstrained = false;
            for (AstNode* idxp = arrp->indexp()->rangesp(); idxp; idxp = idxp->nextp()) {
                if (AstVhdlSubtypeInd* const sip = VN_CAST(idxp, VhdlSubtypeInd)) {
                    typep->addIndexType(resolveSubtype(sip));
                    if (VN_IS(sip->constraintp(), VhdlBox)) unconstrained = true;
                } else {
                    typep->addIndexType(V3VhdlStd::integerType());
                }
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

    void setupUnitContext(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
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
    };
    void candidateTypes(AstNodeVhdlExpr* nodep, CandSet& out) VL_MT_DISABLED;
    // True if the candidates could stand where 'wantp' is expected
    static bool anyMatches(const CandSet& cands, const VhdlType* wantp) VL_MT_DISABLED;
    // True if that match needs no implicit conversion (9.3.2)
    static bool exactMatch(const CandSet& cands, const VhdlType* wantp) VL_MT_DISABLED;
};

//######################################################################
// Declarations

void VhdlSemVisitor::visit(AstVhdlTypeDecl* nodep) {
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
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(kind, nodep->name(), nodep);
    symp->typep(resolveSubtype(nodep->subtypep()));
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
    if (nodep->defaultp()) analyzeExpr(nodep->defaultp(), symp->typep());
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
        symp->addParamType(typep);
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
    } else {
        nodep->vhdlSymp(symp);
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
    iterateAndNextNull(nodep->genericsp());
    iterateAndNextNull(nodep->portsp());
    popScope();
    symp->innerp(innerp);
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::visit(AstVhdlAliasDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::ALIAS, nodep->name(), nodep);
    if (nodep->subtypep()) symp->typep(resolveSubtype(nodep->subtypep()));
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

void VhdlSemVisitor::visit(AstVhdlAttrDecl* nodep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::ATTRIBUTE, nodep->name(), nodep);
    symp->typep(resolveTypeMark(nodep->typeMarkp()));
    nodep->vhdlSymp(symp);
    declare(symp, nodep->fileline());
}

//######################################################################
// Regions with their own declarations

void VhdlSemVisitor::visit(AstVhdlProcess* nodep) {
    pushScope(nodep->name().empty() ? "process" : nodep->name());
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlBlock* nodep) {
    pushScope(nodep->name().empty() ? "block" : nodep->name());
    if (nodep->headerp()) {
        iterateAndNextNull(nodep->headerp()->genericsp());
        iterateAndNextNull(nodep->headerp()->portsp());
    }
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlGenerate* nodep) {
    pushScope(nodep->name().empty() ? "generate" : nodep->name());
    if (nodep->kind() == VhdlLoopKind::FOR) {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::GENERATE_PARAMETER,
                                                       nodep->paramName(), nodep);
        symp->typep(V3VhdlStd::integerType());
        declare(symp, nodep->fileline());
    }
    iterateAndNextNull(nodep->rangep());
    iterateAndNextNull(nodep->exprp());
    iterateAndNextNull(nodep->altsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlGenerateBody* nodep) {
    pushScope(nodep->name().empty() ? "generate body" : nodep->name());
    iterateAndNextNull(nodep->condp());
    iterateAndNextNull(nodep->declsp());
    iterateAndNextNull(nodep->stmtsp());
    popScope();
}

void VhdlSemVisitor::visit(AstVhdlLoop* nodep) {
    if (nodep->kind() == VhdlLoopKind::FOR) {
        // The loop parameter is declared by the loop itself (10.10)
        pushScope("loop");
        VhdlSymbol* const symp
            = VhdlSymTab::newSymbol(VhdlSymKind::LOOP_PARAMETER, nodep->paramName(), nodep);
        symp->typep(V3VhdlStd::integerType());
        declare(symp, nodep->fileline());
        iterateChildren(nodep);
        popScope();
        return;
    }
    iterateChildren(nodep);
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

    for (AstNode* listp : contexts)
    for (AstNode* itemp = listp; itemp; itemp = itemp->nextp()) {
        AstVhdlContextItem* const ctxp = VN_AS(itemp, VhdlContextItem);
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
}

// Print a region and everything declared in it, for --dump-vhdl-syms
void VhdlSemVisitor::dumpScope(const VhdlScope* scopep, const std::string& indent) {
    for (const auto& pair : scopep->direct()) {
        const VhdlSymbol* const symp = pair.second;
        if (symp->predefined()) continue;  // std.standard would drown the rest
        std::cout << indent << symp->kindName() << " " << symp->name();
        if (symp->typep()) std::cout << " : " << symp->typep()->prettyName();
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
            iterateAndNextNull(entp->genericsp());
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
        } else if (AstVhdlPackage* const pkgp = VN_CAST(unitp, VhdlPackage)) {
            iterateAndNextNull(pkgp->genericsp());
            iterateAndNextNull(pkgp->declsp());
            m_unitScopes[lib + "." + lower(unitp->name())] = unitScopep;
            m_primaryUnits[lib + "." + lower(unitp->name())] = unitp;
            libScope(lib)->addNamed(lower(unitp->name()), unitScopep);
        } else if (AstVhdlPackageBody* const bodyp = VN_CAST(unitp, VhdlPackageBody)) {
            const auto it = m_unitScopes.find(lib + "." + lower(unitp->name()));
            if (it != m_unitScopes.end()) m_scopep->addUse(it->second);
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

bool V3VhdlSem::analyze(const std::vector<AstNodeVhdlUnit*>& unitsp) {
    V3VhdlStd::build();
    VhdlSemVisitor visitor;
    visitor.analyzeUnits(unitsp);
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
    if (top->kind() == VhdlTypeKind::UNIVERSAL_INTEGER
        && fromp->kind() == VhdlTypeKind::INTEGER) {
        return true;
    }
    if (top->kind() == VhdlTypeKind::UNIVERSAL_REAL
        && fromp->kind() == VhdlTypeKind::FLOATING) {
        return true;
    }
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
            lookup(litp->text(), found);
            for (const VhdlSymbol* const symp : found) {
                if (symp->kind() == VhdlSymKind::ENUM_LITERAL) out.types.push_back(symp->typep());
            }
            return;
        }
        case VhdlLitKind::STRING:
        case VhdlLitKind::BITSTRING:
            // A string literal is always an array, never a single element
            out.need = CandSet::Need::ARRAY;
            return;
        case VhdlLitKind::NULLLIT: out.need = CandSet::Need::ACCESS; return;
        default: break;
        }
    }
    if (VN_IS(nodep, VhdlAggregate)) {
        out.need = CandSet::Need::COMPOSITE;
        return;
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
        return wantp->kind() == VhdlTypeKind::ARRAY || wantp->isError();
    case CandSet::Need::ACCESS:
        return wantp->kind() == VhdlTypeKind::ACCESS || wantp->isError();
    case CandSet::Need::COMPOSITE: return wantp->isComposite() || wantp->isError();
    case CandSet::Need::ANY: return true;
    }
    return true;  // LCOV_EXCL_LINE
}

bool VhdlSemVisitor::exactMatch(const CandSet& cands, const VhdlType* wantp) {
    if (cands.types.empty()) return true;  // The context settles it either way
    for (const VhdlType* const typep : cands.types) {
        if (!typep || !wantp) continue;
        // A literal of universal type converts implicitly, so it is no less
        // exact a match than a named type would be (9.3.2)
        if (typep->isUniversal() && typesMatch(typep, wantp)) return true;
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
        // Several interpretations remain. Prefer an exact result-type match
        // over a universal one before giving up.
        std::vector<VhdlSymbol*> exact;
        for (VhdlSymbol* const symp : cands) {
            VhdlType* const resultp = symp->returnTypep() ? symp->returnTypep() : symp->typep();
            if (wantp && resultp && resultp->base() == wantp->base()) exact.push_back(symp);
        }
        if (exact.size() == 1) {
            cands.swap(exact);
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
    std::vector<VhdlSymbol*> found;
    lookup(nodep->name(), found);
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
        lookup(nodep->text(), found);
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
        if (wantp && wantp->kind() == VhdlTypeKind::ARRAY) return wantp;
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
        const bool isExact = exactMatch(ocands, symp->paramTypes()[0]);
        (isExact ? exact : loose).push_back(symp);
    }
    std::vector<VhdlSymbol*> cands = exact.empty() ? loose : exact;
    if (cands.empty()) {
        if (!ocands.types.empty() && ocands.types[0] && ocands.types[0]->isError()) {
            return V3VhdlStd::errorType();
        }
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
    std::vector<VhdlSymbol*> exact;
    std::vector<VhdlSymbol*> loose;
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() != VhdlSymKind::FUNCTION) continue;
        if (symp->paramTypes().size() != 2) continue;
        if (!anyMatches(lcands, symp->paramTypes()[0])) continue;
        if (!anyMatches(rcands, symp->paramTypes()[1])) continue;
        const bool isExact = exactMatch(lcands, symp->paramTypes()[0])
                             && exactMatch(rcands, symp->paramTypes()[1]);
        (isExact ? exact : loose).push_back(symp);
    }
    std::vector<VhdlSymbol*> cands = exact.empty() ? loose : exact;
    const bool sawError = (!lcands.types.empty() && lcands.types[0] && lcands.types[0]->isError())
                          || (!rcands.types.empty() && rcands.types[0]
                              && rcands.types[0]->isError());
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
                return pick(nodep, found, wantp, "declaration of '" + parts.back() + "'");
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
    if (nodep->argp()) analyzeExpr(nodep->argp(), nullptr);

    const std::string& attr = nodep->name();
    // Attributes yielding a value of the prefix type (16.2.3, 16.2.4)
    if (attr == "left" || attr == "right" || attr == "high" || attr == "low"
        || attr == "leftof" || attr == "rightof" || attr == "pred" || attr == "succ"
        || attr == "val" || attr == "value") {
        if (prefixTypep && prefixTypep->kind() == VhdlTypeKind::ARRAY) {
            return prefixTypep->indexTypes().empty() ? V3VhdlStd::integerType()
                                                     : prefixTypep->indexTypes()[0];
        }
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (attr == "pos" || attr == "length" || attr == "ascending_length") {
        return V3VhdlStd::integerType();
    }
    if (attr == "ascending" || attr == "event" || attr == "active" || attr == "quiet"
        || attr == "stable") {
        return V3VhdlStd::booleanType();
    }
    if (attr == "image" || attr == "instance_name" || attr == "path_name"
        || attr == "simple_name") {
        return V3VhdlStd::stringType();
    }
    if (attr == "last_value" || attr == "delayed" || attr == "driving_value") {
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
    if (attr == "last_event" || attr == "last_active") return V3VhdlStd::timeType();
    if (attr == "driving") return V3VhdlStd::booleanType();
    if (attr == "base" || attr == "element" || attr == "range" || attr == "reverse_range") {
        return prefixTypep ? prefixTypep : V3VhdlStd::errorType();
    }
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
        for (AstNode* elemp = nodep->elemsp(); elemp; elemp = elemp->nextp()) {
            AstVhdlElemAssoc* const eap = VN_AS(elemp, VhdlElemAssoc);
            for (AstNode* cp = eap->choicesp(); cp; cp = cp->nextp()) {
                AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
                if (chp->exprp()) analyzeExpr(chp->exprp(), nullptr);
            }
            // An element may itself be an array of the same type, as in
            // "(others => (others => '0'))", so try the element type first
            analyzeExpr(eap->valuep(), elemTypep);
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
            const AstVhdlName* const namep
                = chp->exprp() ? VN_CAST(chp->exprp(), VhdlName) : nullptr;
            if (namep) {
                for (VhdlSymbol* const symp : elems) {
                    if (symp->name() == namep->name()) fieldp = symp->typep();
                }
            } else if (!chp->exprp()) {
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

VhdlType* VhdlSemVisitor::analyzeCall(AstVhdlCall* nodep, VhdlType* wantp) {
    // "prefix ( ... )" is a function call, an indexed name, a slice or a type
    // conversion, and only now can we tell which (8.1)
    std::vector<std::string> parts;
    std::vector<VhdlSymbol*> found;
    if (flatten(nodep->prefixp(), parts) && !parts.empty()) {
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

    // A type mark in this position makes it a type conversion (9.3.6) or a
    // constrained subtype indication that the parser could not tell apart
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() == VhdlSymKind::TYPE) {
            nodep->prefixp()->vhdlSymp(symp);
            for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
                if (AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc)) {
                    analyzeExpr(asp->actualp(), nullptr);
                }
            }
            return symp->typep();
        }
    }

    // Gather what each actual could be, without settling any of them, so the
    // whole set can filter the overloads (12.5)
    std::vector<CandSet> argCands;
    for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp()) {
        argCands.emplace_back();
        if (AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc)) {
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

    // An object prefix makes it an indexed name or a slice (8.4, 8.5)
    for (VhdlSymbol* const symp : found) {
        if (!symp->isObject() && symp->kind() != VhdlSymKind::ALIAS) continue;
        VhdlType* typep = symp->typep();
        if (typep && typep->kind() == VhdlTypeKind::ACCESS) typep = typep->designatedp();
        if (!typep || typep->kind() != VhdlTypeKind::ARRAY) continue;
        nodep->prefixp()->vhdlSymp(symp);
        nodep->vhdlSymp(symp);
        // A slice keeps the array type; an index yields one element
        const bool isSlice = (nodep->argsp() && !VN_IS(nodep->argsp(), VhdlAssoc));
        return isSlice ? typep : typep->base()->elementp();
    }

    // Otherwise a subprogram call, resolved by its arguments and its context
    std::vector<VhdlSymbol*> exact;
    std::vector<VhdlSymbol*> loose;
    for (VhdlSymbol* const symp : found) {
        if (symp->kind() != VhdlSymKind::FUNCTION && symp->kind() != VhdlSymKind::PROCEDURE) {
            continue;
        }
        if (symp->paramTypes().size() != argCands.size()) continue;
        bool ok = true;
        bool isExact = true;
        for (size_t i = 0; i < argCands.size() && ok; ++i) {
            if (!anyMatches(argCands[i], symp->paramTypes()[i])) ok = false;
            if (!exactMatch(argCands[i], symp->paramTypes()[i])) isExact = false;
        }
        if (ok) (isExact ? exact : loose).push_back(symp);
    }
    std::vector<VhdlSymbol*> cands = exact.empty() ? loose : exact;
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
    if (nodep->vhdlSymp()) {
        size_t i = 0;
        for (AstNode* argp = nodep->argsp(); argp; argp = argp->nextp(), ++i) {
            AstVhdlAssoc* const asp = VN_CAST(argp, VhdlAssoc);
            if (!asp || !asp->actualp() || i >= nodep->vhdlSymp()->paramTypes().size()) continue;
            analyzeExpr(asp->actualp(), nodep->vhdlSymp()->paramTypes()[i]);
        }
    }
    return resultp;
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
        if (wfp->valuep()) analyzeExpr(wfp->valuep(), targetp);
        if (wfp->afterp()) analyzeExpr(wfp->afterp(), V3VhdlStd::timeType());
    }
    if (nodep->selectorp()) analyzeExpr(nodep->selectorp(), nullptr);
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        if (altp->condp()) analyzeExpr(altp->condp(), V3VhdlStd::booleanType());
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            if (chp->exprp()) analyzeExpr(chp->exprp(), nodep->selectorp()
                                                           ? nodep->selectorp()->vhdlTypep()
                                                           : nullptr);
        }
        for (AstNode* wp = altp->wavesp(); wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
            if (wfp->valuep()) analyzeExpr(wfp->valuep(), targetp);
            if (wfp->afterp()) analyzeExpr(wfp->afterp(), V3VhdlStd::timeType());
        }
    }
}

void VhdlSemVisitor::visit(AstVhdlVarAssign* nodep) {
    VhdlType* const targetp = analyzeExpr(nodep->targetp(), nullptr);
    if (nodep->valuep()) analyzeExpr(nodep->valuep(), targetp);
    if (nodep->selectorp()) analyzeExpr(nodep->selectorp(), nullptr);
    for (AstNode* ap = nodep->altsp(); ap; ap = ap->nextp()) {
        AstVhdlAlternative* const altp = VN_AS(ap, VhdlAlternative);
        if (altp->condp()) analyzeExpr(altp->condp(), V3VhdlStd::booleanType());
        for (AstNode* cp = altp->choicesp(); cp; cp = cp->nextp()) {
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            if (chp->exprp()) analyzeExpr(chp->exprp(), nullptr);
        }
        for (AstNode* wp = altp->wavesp(); wp; wp = wp->nextp()) {
            AstVhdlWaveform* const wfp = VN_AS(wp, VhdlWaveform);
            if (wfp->valuep()) analyzeExpr(wfp->valuep(), targetp);
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
            AstVhdlChoice* const chp = VN_AS(cp, VhdlChoice);
            // Every choice is of the case expression's type (10.9)
            if (chp->exprp()) analyzeExpr(chp->exprp(), exprp);
        }
        iterateAndNextNull(altp->stmtsp());
    }
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
    if (nodep->timeoutp()) analyzeExpr(nodep->timeoutp(), V3VhdlStd::timeType());
}

void VhdlSemVisitor::visit(AstVhdlReturn* nodep) {
    if (!nodep->valuep()) return;
    if (!m_returnTypep) {
        err(nodep->fileline(), "A procedure's 'return' may not have a value");
        return;
    }
    analyzeExpr(nodep->valuep(), m_returnTypep);
}

void VhdlSemVisitor::visit(AstVhdlProcCall* nodep) { analyzeExpr(nodep->callp(), nullptr); }

void VhdlSemVisitor::visit(AstVhdlNext* nodep) {
    if (nodep->condp()) analyzeExpr(nodep->condp(), V3VhdlStd::booleanType());
}
