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
//
// Declarations and the regions they are visible in, per IEEE 1076-2008
// clause 12.
//
// Two things make this more than a stack of maps. Subprograms and enumeration
// literals may be overloaded, so a name can denote several declarations at
// once and the choice is made later by overload resolution (12.5). And a use
// clause makes declarations *potentially* visible, which a directly visible
// homograph then hides (12.4), so the two must be kept apart.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLSYM_H_
#define VERILATOR_V3VHDLSYM_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Error.h"
#include "V3VhdlType.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class AstNode;
class AstNodeVhdlExpr;
class AstVhdlSubprogDecl;
class FileLine;

//============================================================================
// A character literal and an identifier are different lexical elements (15.6),
// so 'c' must not collide with a variable named c. Keying character literals
// with their quotes keeps the two apart in one table, and has the side
// benefit that a literal's name is already how 'IMAGE spells it.
inline std::string vhdlCharLitName(const std::string& text) { return "'" + text + "'"; }

//============================================================================
// What a name denotes

enum class VhdlSymKind : uint8_t {
    ERROR,
    TYPE,  // A type or subtype declaration
    CONSTANT,
    SIGNAL,
    VARIABLE,
    SHARED_VARIABLE,
    FILE,
    GENERIC,  // An interface constant of a generic clause
    PORT,  // An interface signal of a port clause
    PARAMETER,  // A subprogram parameter
    ENUM_LITERAL,  // Overloadable
    PHYSICAL_UNIT,
    FUNCTION,  // Overloadable
    PROCEDURE,  // Overloadable
    RECORD_ELEMENT,
    ALIAS,
    ATTRIBUTE,
    LABEL,
    LIBRARY,
    PACKAGE,
    ENTITY,
    ARCHITECTURE,
    CONFIGURATION,
    COMPONENT,
    CONTEXT,
    LOOP_PARAMETER,
    GENERATE_PARAMETER,
};

//============================================================================
// One declaration

class VhdlSymbol final {
    // MEMBERS
    const VhdlSymKind m_kind;
    const std::string m_name;  // Lowercased for a basic identifier
    AstNode* const m_declp;  // The node that declared it
    VhdlType* m_typep = nullptr;  // Its type, where it has one
    VhdlScope* m_innerp = nullptr;  // Packages, entities, protected types
    // Overloadable subprograms: their signature. The names and defaults are
    // needed as well as the types, because an actual may be given by name and
    // a formal with a default may be left out altogether (6.5.7.1).
    std::vector<VhdlType*> m_paramTypes;
    std::vector<std::string> m_paramNames;
    std::vector<bool> m_paramDefaults;
    VhdlType* m_returnTypep = nullptr;
    int m_position = 0;  // Enumeration literal position, or record element index
    bool m_predefined = false;  // Comes from std.standard rather than source
    bool m_hasBody = false;  // A subprogram whose body has been seen
    // The value of a constant, generic or physical unit, where it folds (9.4).
    // Unknown for anything whose value is not fixed until elaboration.
    VhdlValue m_value;
    bool m_deferred = false;  // A deferred constant, whose value is in the body
    VhdlSymbol* m_aliasp = nullptr;  // What an alias denotes (6.6)
    // A subprogram given the FOREIGN attribute (16.2) binds to a C symbol
    // instead of having a body here. The string says which, and which ABI.
    std::string m_foreignName;
    bool m_foreignDpi = false;  // Bound with the DPI-C ABI, not VHPIDIRECT
    // A signal driven from more than one place is resolved (6.5.2). Analysis
    // counts the drivers and, where the subtype names a resolution function,
    // gives it the copy that takes that many values.
    int m_driverCount = 0;
    VhdlSymbol* m_resolverp = nullptr;
    AstVhdlSubprogDecl* m_bodyp = nullptr;  // A subprogram's body, for copying
    // Values given by attribute specifications (7.2), by attribute name. The
    // expression stays owned by the specification.
    std::map<std::string, AstNodeVhdlExpr*> m_userAttrs;

    VL_UNCOPYABLE(VhdlSymbol);

public:
    // CONSTRUCTORS
    VhdlSymbol(VhdlSymKind kind, const std::string& name, AstNode* declp)
        : m_kind{kind}
        , m_name{name}
        , m_declp{declp} {}
    ~VhdlSymbol() = default;

    // METHODS
    VhdlSymKind kind() const { return m_kind; }
    const std::string& name() const { return m_name; }
    AstNode* declp() const { return m_declp; }
    VhdlType* typep() const { return m_typep; }
    void typep(VhdlType* typep) { m_typep = typep; }
    VhdlScope* innerp() const { return m_innerp; }
    void innerp(VhdlScope* scopep) { m_innerp = scopep; }
    int position() const { return m_position; }
    void position(int pos) { m_position = pos; }
    bool predefined() const { return m_predefined; }
    void predefined(bool flag) { m_predefined = flag; }
    bool hasBody() const { return m_hasBody; }
    void hasBody(bool flag) { m_hasBody = flag; }
    bool deferred() const { return m_deferred; }
    void deferred(bool flag) { m_deferred = flag; }

    AstVhdlSubprogDecl* bodyp() const { return m_bodyp; }
    void bodyp(AstVhdlSubprogDecl* nodep) { m_bodyp = nodep; }

    int driverCount() const { return m_driverCount; }
    void driverCount(int n) { m_driverCount = n; }
    VhdlSymbol* resolverp() const { return m_resolverp; }
    void resolverp(VhdlSymbol* symp) { m_resolverp = symp; }

    const std::string& foreignName() const { return m_foreignName; }
    void foreignName(const std::string& name) { m_foreignName = name; }
    bool isForeign() const { return !m_foreignName.empty(); }
    bool foreignDpi() const { return m_foreignDpi; }
    void foreignDpi(bool flag) { m_foreignDpi = flag; }

    // A user-defined attribute's value for this entity, or nullptr
    AstNodeVhdlExpr* userAttr(const std::string& name) const {
        const auto it = m_userAttrs.find(name);
        return it == m_userAttrs.end() ? nullptr : it->second;
    }
    void userAttr(const std::string& name, AstNodeVhdlExpr* valuep) {
        m_userAttrs.emplace(name, valuep);
    }

    VhdlSymbol* aliasp() const { return m_aliasp; }
    void aliasp(VhdlSymbol* symp) { m_aliasp = symp; }
    // An alias is transparent: it denotes whatever it was declared for (6.6.1)
    const VhdlSymbol* resolved() const { return m_aliasp ? m_aliasp : this; }
    VhdlSymbol* resolved() { return m_aliasp ? m_aliasp : this; }

    const VhdlValue& value() const { return m_value; }
    void value(const VhdlValue& val) { m_value = val; }
    bool hasValue() const { return m_value.known(); }

    const std::vector<VhdlType*>& paramTypes() const { return m_paramTypes; }
    void addParamType(VhdlType* typep) {
        m_paramTypes.push_back(typep);
        m_paramNames.emplace_back();
        m_paramDefaults.push_back(false);
    }
    void addParam(const std::string& name, VhdlType* typep, bool hasDefault) {
        m_paramTypes.push_back(typep);
        m_paramNames.push_back(name);
        m_paramDefaults.push_back(hasDefault);
    }
    const std::string& paramName(size_t i) const {
        static const std::string empty;
        return i < m_paramNames.size() ? m_paramNames[i] : empty;
    }
    // Which formal that name is, or -1
    int paramIndex(const std::string& name) const {
        for (size_t i = 0; i < m_paramNames.size(); ++i) {
            if (m_paramNames[i] == name) return static_cast<int>(i);
        }
        return -1;
    }
    bool paramHasDefault(size_t i) const {
        return i < m_paramDefaults.size() && m_paramDefaults[i];
    }
    VhdlType* returnTypep() const { return m_returnTypep; }
    void returnTypep(VhdlType* typep) { m_returnTypep = typep; }

    // A subprogram or enumeration literal may share its name with others, and
    // so may an alias of one (6.6.2)
    bool isOverloadable() const {
        return m_kind == VhdlSymKind::FUNCTION || m_kind == VhdlSymKind::PROCEDURE
               || m_kind == VhdlSymKind::ENUM_LITERAL || m_kind == VhdlSymKind::ALIAS;
    }
    // Something an expression can read
    bool isObject() const {
        switch (m_kind) {
        case VhdlSymKind::CONSTANT:
        case VhdlSymKind::SIGNAL:
        case VhdlSymKind::VARIABLE:
        case VhdlSymKind::SHARED_VARIABLE:
        case VhdlSymKind::FILE:
        case VhdlSymKind::GENERIC:
        case VhdlSymKind::PORT:
        case VhdlSymKind::PARAMETER:
        case VhdlSymKind::LOOP_PARAMETER:
        case VhdlSymKind::GENERATE_PARAMETER: return true;
        default: return false;
        }
    }

    // Two declarations are homographs when one hides the other (12.3): same
    // name, and either not overloadable or with the same signature
    bool homographOf(const VhdlSymbol* otherp) const VL_MT_DISABLED;

    const char* kindName() const VL_MT_DISABLED;
};

//============================================================================
// A declarative region (12.1)

class VhdlScope final {
    // MEMBERS
    VhdlScope* const m_parentp;  // Enclosing region, or null at the root
    const std::string m_name;  // For diagnostics
    // Declared here. A multimap because a name may denote several overloads.
    std::multimap<std::string, VhdlSymbol*> m_direct;
    // Made potentially visible by a use clause (12.4). Searched only when the
    // enclosing regions declare nothing of that name.
    std::vector<VhdlScope*> m_usedp;
    // Selected-name targets: libraries and packages reachable as "lib.pkg"
    std::map<std::string, VhdlScope*> m_named;

    VL_UNCOPYABLE(VhdlScope);

public:
    // CONSTRUCTORS
    VhdlScope(VhdlScope* parentp, const std::string& name)
        : m_parentp{parentp}
        , m_name{name} {}
    ~VhdlScope() = default;

    // METHODS
    VhdlScope* parentp() const { return m_parentp; }
    const std::string& name() const { return m_name; }
    const std::multimap<std::string, VhdlSymbol*>& direct() const { return m_direct; }

    // Declare a symbol here. Returns the symbol it hides, if any, so the
    // caller can complain about an illegal redeclaration (12.3).
    VhdlSymbol* insert(VhdlSymbol* symp) VL_MT_DISABLED;

    // Symbols declared directly in this region with that name
    void lookupLocal(const std::string& name, std::vector<VhdlSymbol*>& out) const VL_MT_DISABLED;

    // Every declaration that name denotes here, following 12.3 and 12.4: the
    // innermost region that declares it wins, and only if no region declares
    // it are use clauses consulted.
    void lookup(const std::string& name, std::vector<VhdlSymbol*>& out) const VL_MT_DISABLED;

    // Make another region's declarations potentially visible (use ... .all)
    void addUse(VhdlScope* scopep) VL_MT_DISABLED;

    // Register a region reachable by selection, e.g. a library or package
    void addNamed(const std::string& name, VhdlScope* scopep) VL_MT_DISABLED;
    VhdlScope* findNamed(const std::string& name) const VL_MT_DISABLED;

    // What this region's own use clauses and library clauses brought in, so
    // that a copy of a design unit can be given the same context it had
    const std::vector<VhdlScope*>& uses() const { return m_usedp; }
    const std::map<std::string, VhdlScope*>& named() const { return m_named; }
};

//============================================================================
// Owns every type, symbol and scope for one run

class VhdlSymTab final {
public:
    static VhdlType* newType(VhdlTypeKind kind, const std::string& name,
                             FileLine* fl) VL_MT_DISABLED;
    static VhdlSymbol* newSymbol(VhdlSymKind kind, const std::string& name,
                                 AstNode* declp) VL_MT_DISABLED;
    static VhdlScope* newScope(VhdlScope* parentp, const std::string& name) VL_MT_DISABLED;
    // Release everything; invalidates every pointer handed out
    static void clear() VL_MT_DISABLED;
};

#endif  // Guard
