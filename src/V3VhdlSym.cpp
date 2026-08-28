// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL symbols, scopes and visibility
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

#include "V3VhdlSym.h"

#include "V3Global.h"

#include <algorithm>
#include <memory>
#include <iomanip>
#include <sstream>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Arena

namespace {
std::vector<std::unique_ptr<VhdlType>> s_types;
std::vector<std::unique_ptr<VhdlSymbol>> s_symbols;
std::vector<std::unique_ptr<VhdlScope>> s_scopes;
}  // namespace

VhdlType* VhdlSymTab::newType(VhdlTypeKind kind, const std::string& name, FileLine* fl) {
    s_types.emplace_back(new VhdlType{kind, name, fl});
    return s_types.back().get();
}

VhdlSymbol* VhdlSymTab::newSymbol(VhdlSymKind kind, const std::string& name, AstNode* declp) {
    s_symbols.emplace_back(new VhdlSymbol{kind, name, declp});
    return s_symbols.back().get();
}

VhdlScope* VhdlSymTab::newScope(VhdlScope* parentp, const std::string& name) {
    s_scopes.emplace_back(new VhdlScope{parentp, name});
    return s_scopes.back().get();
}

void VhdlSymTab::clear() {
    s_scopes.clear();
    s_symbols.clear();
    s_types.clear();
}

//######################################################################
// VhdlType

std::string VhdlType::prettyName() const {
    if (!m_name.empty()) return m_name;
    // An anonymous subtype reads best as the type it constrains, so that a
    // diagnostic about my_vec1(1 to 10) names my_vec1
    if (isSubtype() && m_base) return m_base->prettyName();
    switch (m_kind) {
    case VhdlTypeKind::ERROR: return "<error>";
    case VhdlTypeKind::UNIVERSAL_INTEGER: return "universal_integer";
    case VhdlTypeKind::UNIVERSAL_REAL: return "universal_real";
    case VhdlTypeKind::ARRAY: {
        std::string out = "array of ";
        out += m_elementp ? m_elementp->prettyName() : "<unknown>";
        return out;
    }
    case VhdlTypeKind::RECORD: return "<anonymous record>";
    case VhdlTypeKind::ACCESS:
        return "access " + (m_designatedp ? m_designatedp->prettyName() : "<unknown>");
    case VhdlTypeKind::FILE:
        return "file of " + (m_designatedp ? m_designatedp->prettyName() : "<unknown>");
    default: return "<anonymous type>";
    }
}

//######################################################################
// VhdlValue

std::string VhdlValue::prettyName() const {
    std::ostringstream os;
    switch (m_kind) {
    case VhdlValKind::UNKNOWN: return "<not static>";
    case VhdlValKind::REAL:
        // Always with a decimal point, so a real never reads as an integer
        os << std::showpoint << m_real;
        return os.str();
    case VhdlValKind::STR: return '"' + m_str + '"';
    case VhdlValKind::INT: break;
    }
    // An enumeration value reads back as its literal, and a physical value
    // with the name of its primary unit, so that diagnostics quote what the
    // user wrote rather than an internal number
    if (m_typep && m_typep->base()->kind() == VhdlTypeKind::ENUM) {
        const std::vector<VhdlSymbol*>& lits = m_typep->base()->literals();
        if (m_int >= 0 && m_int < static_cast<int64_t>(lits.size())) return lits[m_int]->name();
    }
    os << m_int;
    if (m_typep && m_typep->base()->kind() == VhdlTypeKind::PHYSICAL
        && !m_typep->base()->units().empty()) {
        os << ' ' << m_typep->base()->units().front().first;
    }
    return os.str();
}

//######################################################################
// VhdlSymbol

const char* VhdlSymbol::kindName() const {
    switch (m_kind) {
    case VhdlSymKind::ERROR: return "erroneous declaration";
    case VhdlSymKind::TYPE: return "type";
    case VhdlSymKind::CONSTANT: return "constant";
    case VhdlSymKind::SIGNAL: return "signal";
    case VhdlSymKind::VARIABLE: return "variable";
    case VhdlSymKind::SHARED_VARIABLE: return "shared variable";
    case VhdlSymKind::FILE: return "file";
    case VhdlSymKind::GENERIC: return "generic";
    case VhdlSymKind::PORT: return "port";
    case VhdlSymKind::PARAMETER: return "parameter";
    case VhdlSymKind::ENUM_LITERAL: return "enumeration literal";
    case VhdlSymKind::PHYSICAL_UNIT: return "physical unit";
    case VhdlSymKind::FUNCTION: return "function";
    case VhdlSymKind::PROCEDURE: return "procedure";
    case VhdlSymKind::RECORD_ELEMENT: return "record element";
    case VhdlSymKind::ALIAS: return "alias";
    case VhdlSymKind::ATTRIBUTE: return "attribute";
    case VhdlSymKind::LABEL: return "label";
    case VhdlSymKind::LIBRARY: return "library";
    case VhdlSymKind::PACKAGE: return "package";
    case VhdlSymKind::ENTITY: return "entity";
    case VhdlSymKind::ARCHITECTURE: return "architecture";
    case VhdlSymKind::CONFIGURATION: return "configuration";
    case VhdlSymKind::COMPONENT: return "component";
    case VhdlSymKind::CONTEXT: return "context";
    case VhdlSymKind::LOOP_PARAMETER: return "loop parameter";
    case VhdlSymKind::GENERATE_PARAMETER: return "generate parameter";
    }
    return "declaration";  // LCOV_EXCL_LINE
}

bool VhdlSymbol::homographOf(const VhdlSymbol* otherp) const {
    if (m_name != otherp->name()) return false;
    // Only overloadables escape hiding, and only when their signatures differ
    if (!isOverloadable() || !otherp->isOverloadable()) return true;
    // An alias may rename a subprogram, and its signature is not known until
    // the alias target is resolved, so never call it a homograph (6.6.2)
    if (m_kind == VhdlSymKind::ALIAS || otherp->kind() == VhdlSymKind::ALIAS) return false;
    // A procedure has no result and a function has one, so the two can never
    // share a profile. An enumeration literal is a parameterless function
    // returning its own type (5.2.2), so it compares like one.
    const bool procedure = m_kind == VhdlSymKind::PROCEDURE;
    if (procedure != (otherp->kind() == VhdlSymKind::PROCEDURE)) return false;
    if (m_paramTypes.size() != otherp->paramTypes().size()) return false;
    for (size_t i = 0; i < m_paramTypes.size(); ++i) {
        const VhdlType* const ap = m_paramTypes[i];
        const VhdlType* const bp = otherp->paramTypes()[i];
        if (ap && bp && ap->base() != bp->base()) return false;
    }
    // A function's result type is part of its signature (12.3)
    if (m_returnTypep && otherp->returnTypep()
        && m_returnTypep->base() != otherp->returnTypep()->base()) {
        return false;
    }
    return true;
}

//######################################################################
// VhdlScope

VhdlSymbol* VhdlScope::insert(VhdlSymbol* symp) {
    VhdlSymbol* hiddenp = nullptr;
    const auto range = m_direct.equal_range(symp->name());
    for (auto it = range.first; it != range.second; ++it) {
        if (!symp->homographOf(it->second)) continue;
        // An implicit declaration of a predefined operation is hidden by an
        // explicit homograph in the same region (12.3). NUMERIC_STD relies on
        // this: it declares "=" for UNSIGNED itself, alongside the one the
        // array type declaration brought with it.
        if (it->second->predefined() && !symp->predefined()) {
            m_direct.erase(it);
            m_direct.emplace(symp->name(), symp);
            symp->declScopep(this);
            return nullptr;
        }
        if (symp->predefined() && !it->second->predefined()) return nullptr;  // Already hidden
        hiddenp = it->second;
        break;
    }
    m_direct.emplace(symp->name(), symp);
    symp->declScopep(this);
    return hiddenp;
}

void VhdlScope::lookupLocal(const std::string& name, std::vector<VhdlSymbol*>& out) const {
    const auto range = m_direct.equal_range(name);
    for (auto it = range.first; it != range.second; ++it) out.push_back(it->second);
}

// Whether two declarations of one name are homographs: overloadable ones are
// when their parameter and result type profiles agree, and a declaration is
// hidden by a homograph of it in an inner region (12.3).
static bool sameProfile(const VhdlSymbol* ap, const VhdlSymbol* bp) {
    if (ap->paramTypes().size() != bp->paramTypes().size()) return false;
    for (size_t i = 0; i < ap->paramTypes().size(); ++i) {
        const VhdlType* const atp = ap->paramTypes()[i];
        const VhdlType* const btp = bp->paramTypes()[i];
        if (!atp || !btp || atp->base() != btp->base()) return false;
    }
    const VhdlType* const arp = ap->returnTypep();
    const VhdlType* const brp = bp->returnTypep();
    if (!arp != !brp) return false;
    if (arp && brp && arp->base() != brp->base()) return false;
    return true;
}

// Add what one region offers, leaving out anything an inner region has
// already declared with the same profile
static void addUnhidden(const VhdlScope* scopep, const std::string& name,
                        std::vector<VhdlSymbol*>& out, size_t innerCount) {
    std::vector<VhdlSymbol*> here;
    scopep->lookupLocal(name, here);
    for (VhdlSymbol* const symp : here) {
        bool hidden = false;
        for (size_t i = 0; i < innerCount && !hidden; ++i) {
            if (out[i] != symp && sameProfile(out[i], symp)) hidden = true;
        }
        if (!hidden) out.push_back(symp);
    }
}

void VhdlScope::lookup(const std::string& name, std::vector<VhdlSymbol*>& out) const {
    // Directly visible declarations first. The innermost region that declares
    // the name with something not overloadable wins outright, since that
    // hides everything outside it (12.3). An overloadable declaration hides
    // only a homograph of itself, which is why the rest are gathered.
    for (const VhdlScope* scopep = this; scopep; scopep = scopep->parentp()) {
        const size_t before = out.size();
        addUnhidden(scopep, name, out, before);
        for (size_t i = before; i < out.size(); ++i) {
            if (!out[i]->isOverloadable()) return;
        }
    }
    const size_t directCount = out.size();

    // Then everything use clauses made potentially visible (12.4). These are
    // gathered from every enclosing region, not just the nearest one: a
    // package that declares its own "=" does not thereby hide the predefined
    // "=" of every other type, because overloads coexist. A directly visible
    // homograph does hide one, though.
    for (const VhdlScope* scopep = this; scopep; scopep = scopep->parentp()) {
        for (const VhdlScope* const usedp : scopep->m_usedp) {
            addUnhidden(usedp, name, out, directCount);
        }
    }

    // The same region may be reached by more than one route. Which one it
    // was found by must not decide the answer, so the order the regions were
    // looked at in is kept rather than sorting by address.
    std::set<const VhdlSymbol*> seen;
    std::vector<VhdlSymbol*> kept;
    for (VhdlSymbol* const symp : out) {
        if (seen.insert(symp).second) kept.push_back(symp);
    }
    out.swap(kept);

    // Two use clauses may each make a type of the same name potentially
    // visible, and 12.4 c) says neither is then directly visible. The common
    // case is a design that declares its own BOOLEAN_VECTOR or INTEGER_VECTOR
    // beside the one VHDL-2008 added to STD.STANDARD, and refusing that
    // outright helps no one: the design's own declaration is what it meant,
    // and STD.STANDARD's is still reachable by its selected name.
    if (out.size() > directCount + 1) {
        bool anyOwn = false;
        for (size_t i = directCount; i < out.size(); ++i) {
            if (!out[i]->isOverloadable() && !out[i]->predefined()) anyOwn = true;
        }
        if (anyOwn) {
            std::vector<VhdlSymbol*> pruned;
            for (size_t i = 0; i < out.size(); ++i) {
                if (i >= directCount && !out[i]->isOverloadable() && out[i]->predefined()) {
                    continue;
                }
                pruned.push_back(out[i]);
            }
            out.swap(pruned);
        }
    }
}

void VhdlScope::addUse(VhdlScope* scopep) {
    if (!scopep) return;
    if (std::find(m_usedp.begin(), m_usedp.end(), scopep) == m_usedp.end()) {
        m_usedp.push_back(scopep);
    }
}

void VhdlScope::addNamed(const std::string& name, VhdlScope* scopep) {
    m_named[name] = scopep;
}

VhdlScope* VhdlScope::findNamed(const std::string& name) const {
    for (const VhdlScope* scopep = this; scopep; scopep = scopep->parentp()) {
        const auto it = scopep->m_named.find(name);
        if (it != scopep->m_named.end()) return it->second;
        // A use clause makes what the used package declares potentially
        // visible too, so a package instantiated inside another package can
        // be the prefix of an expanded name (4.9, 12.4). The region's own
        // declarations are looked at first, so a local one still hides it,
        // and a used region's own uses are not followed: potential
        // visibility does not pass on.
        for (const VhdlScope* const usedp : scopep->m_usedp) {
            const auto uit = usedp->m_named.find(name);
            if (uit != usedp->m_named.end()) return uit->second;
        }
    }
    return nullptr;
}
