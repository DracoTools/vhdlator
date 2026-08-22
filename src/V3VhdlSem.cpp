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
    std::string m_libname;  // Library of the unit being analyzed
    bool m_errored = false;

    VL_UNCOPYABLE(VhdlSemVisitor);

    // METHODS - scopes and libraries

    VhdlScope* libScope(const std::string& name) {
        const std::string lib = lower(name.empty() ? "work" : name);
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

    void setupUnitContext(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    static void dumpScope(const VhdlScope* scopep, const std::string& indent) VL_MT_DISABLED;
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
    iterateAndNextNull(nodep->defaultp());
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
    iterateAndNextNull(nodep->defaultp());
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

    // Declare the subprogram in the enclosing region, where it is visible
    m_scopep = outerp;
    nodep->vhdlSymp(symp);
    symp->innerp(innerp);
    declare(symp, nodep->fileline());

    if (nodep->hasBody()) {
        m_scopep = innerp;
        iterateAndNextNull(nodep->declsp());
        iterateAndNextNull(nodep->stmtsp());
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

    for (AstNode* itemp = unitp->contextp(); itemp; itemp = itemp->nextp()) {
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
            const auto it = m_unitScopes.find(lib + "." + parts[1]);
            if (it == m_unitScopes.end()) continue;  // Not one of ours, or not yet analyzed
            m_scopep->addNamed(parts[1], it->second);
            // "use pkg.all" makes everything potentially visible; a selective
            // use clause is treated the same, which is over-permissive but
            // never rejects a legal design
            m_scopep->addUse(it->second);
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
