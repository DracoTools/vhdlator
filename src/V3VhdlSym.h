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
#include <set>
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
// An extended identifier is case sensitive and distinct from every basic one
// (15.4.3), but names are compared lowercased throughout. So the scanner keys
// one by a form lowercasing leaves alone: between backslashes, an uppercase
// letter as a backslash before its lowercase, and a backslash doubled.
// "\Foo\" is keyed "\\foo\", "\foo\" is "\foo\", and neither is "foo".
inline bool vhdlIsExtendedName(const std::string& name) {
    return name.size() >= 2 && name.front() == '\\' && name.back() == '\\';
}
inline std::string vhdlExtendedKey(const std::string& text) {
    std::string out = "\\";
    for (const char c : text) {
        if (c >= 'A' && c <= 'Z') {
            out += '\\';
            out += static_cast<char>(c - 'A' + 'a');
        } else if (c == '\\') {
            out += "\\\\";
        } else {
            out += c;
        }
    }
    return out + "\\";
}
// The identifier a key stands for, without its backslashes: what TO_STRING
// gives (5.7). A basic one is itself.
inline std::string vhdlExtendedText(const std::string& name) {
    if (!vhdlIsExtendedName(name)) return name;
    std::string out;
    for (size_t i = 1; i + 1 < name.size(); ++i) {
        if (name[i] == '\\' && i + 2 < name.size()) {
            const char c = name[++i];
            out += c == '\\' ? c : static_cast<char>(c - 'a' + 'A');
        } else {
            out += name[i];
        }
    }
    return out;
}
// A text names are written into -- a path, ":top(a):\\foo\:" -- with each key
// in it written as the source writes the identifier. Inside a key a backslash
// is always followed by another or by a lowercase letter, so where one is
// not, the key ends.
inline std::string vhdlDisplayText(const std::string& text) {
    if (text.find('\\') == std::string::npos) return text;
    std::string out;
    bool inKey = false;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '\\') {
            out += c;
            continue;
        }
        if (!inKey) {
            inKey = true;
            out += c;
            continue;
        }
        const char n = i + 1 < text.size() ? text[i + 1] : '\0';
        if (n == '\\') {
            out += "\\\\";
            ++i;
        } else if (n >= 'a' && n <= 'z') {
            out += static_cast<char>(n - 'a' + 'A');
            ++i;
        } else {
            inKey = false;
            out += c;
        }
    }
    return out;
}
// The identifier as the source writes it, backslashes and all: what 'IMAGE,
// 'SIMPLE_NAME and 'PATH_NAME give (16.2), and what a message shows
inline std::string vhdlDisplayName(const std::string& name) {
    if (!vhdlIsExtendedName(name)) return name;
    std::string out = "\\";
    for (const char c : vhdlExtendedText(name)) {
        out += c;
        if (c == '\\') out += c;
    }
    return out + "\\";
}

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
    std::vector<bool> m_paramGenerics;  // Written with a generic type's name
    // A copy made for one call declares each parameter with the subtype the call supplies;
    // these are the subtypes the subprogram declares, which an actual must belong to
    std::vector<VhdlType*> m_declaredParamTypes;
    VhdlType* m_returnTypep = nullptr;
    int m_position = 0;  // Enumeration literal position, or record element index
    bool m_predefined = false;  // Comes from std.standard rather than source
    bool m_hasBody = false;  // A subprogram whose body has been seen
    bool m_since2008 = false;  // Predefined by VHDL-2008, and by no earlier revision
    // The value of a constant, generic or physical unit, where it folds (9.4).
    // Unknown for anything whose value is not fixed until elaboration.
    VhdlValue m_value;
    VhdlValue m_elabValue;  // What elaboration worked out, where value() is not it
    bool m_deferred = false;  // A deferred constant, whose value is in the body
    // Where a deferred constant was given its value: the declaration in the
    // package body completes the one in the package, which has none (6.4.2.2)
    AstNode* m_fullDeclp = nullptr;
    VhdlSymbol* m_aliasp = nullptr;  // What an alias denotes (6.6)
    // A subprogram given the FOREIGN attribute (16.2) binds to a C symbol
    // instead of having a body here. The string says which, and which ABI.
    std::string m_foreignName;
    bool m_foreignDpi = false;  // Bound with the DPI-C ABI, not VHPIDIRECT
    bool m_objectAlias = false;  // An alias of an object, which is not overloadable
    // A signal driven from more than one place is resolved (6.5.2). Analysis
    // counts the drivers and, where the subtype names a resolution function,
    // gives it the copy that takes that many values.
    int m_driverCount = 0;
    VhdlSymbol* m_resolverp = nullptr;
    // The same function for *no* drivers at all, which is what a guarded
    // signal needs when every one of its drivers is disconnected: the
    // resolution function is then called with a null array (14.7.3.2)
    VhdlSymbol* m_resolverNullp = nullptr;
    // And one copy for every number of drivers in between, since a guarded
    // signal is resolved over those of its drivers that are connected and
    // any of them may be turned off on its own (14.7.3.2)
    std::map<int, VhdlSymbol*> m_resolverByCount;
    // A composite whose element subtype names the resolution function: the
    // drivers are resolved one element at a time rather than whole (14.7.3.2)
    bool m_resolvePerElement = false;
    // An unresolved signal with sources outside any generate, more than one: an error of
    // elaboration (14.7.2), which the analysis only warns of
    bool m_unresolvedSources = false;
    AstVhdlSubprogDecl* m_bodyp = nullptr;  // A subprogram's body, for copying
    // Values given by attribute specifications (7.2), by attribute name. The
    // expression stays owned by the specification.
    std::map<std::string, AstNodeVhdlExpr*> m_userAttrs;
    VhdlScope* m_declScopep = nullptr;  // Where it was declared, for 'path_name

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
    bool since2008() const { return m_since2008; }
    void since2008(bool flag) { m_since2008 = flag; }
    void hasBody(bool flag) { m_hasBody = flag; }
    bool deferred() const { return m_deferred; }
    void deferred(bool flag) { m_deferred = flag; }
    AstNode* fullDeclp() const { return m_fullDeclp; }
    void fullDeclp(AstNode* nodep) { m_fullDeclp = nodep; }

    AstVhdlSubprogDecl* bodyp() const { return m_bodyp; }
    void bodyp(AstVhdlSubprogDecl* nodep) { m_bodyp = nodep; }

    int driverCount() const { return m_driverCount; }
    void driverCount(int n) { m_driverCount = n; }
    VhdlSymbol* resolverp() const { return m_resolverp; }
    void resolverp(VhdlSymbol* symp) { m_resolverp = symp; }
    VhdlSymbol* resolverNullp() const { return m_resolverNullp; }
    void resolverNullp(VhdlSymbol* symp) { m_resolverNullp = symp; }
    VhdlSymbol* resolverForCount(int count) const {
        const auto it = m_resolverByCount.find(count);
        return it == m_resolverByCount.end() ? nullptr : it->second;
    }
    void resolverForCount(int count, VhdlSymbol* symp) { m_resolverByCount[count] = symp; }
    bool resolvePerElement() const { return m_resolvePerElement; }
    void resolvePerElement(bool flag) { m_resolvePerElement = flag; }
    bool unresolvedSources() const { return m_unresolvedSources; }
    void unresolvedSources(bool flag) { m_unresolvedSources = flag; }

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

    // The region the declaration appeared in, which is the path to it (16.2.7)
    VhdlScope* declScopep() const { return m_declScopep; }
    void declScopep(VhdlScope* scopep) { m_declScopep = scopep; }

    VhdlSymbol* aliasp() const { return m_aliasp; }
    void aliasp(VhdlSymbol* symp) { m_aliasp = symp; }
    bool objectAlias() const { return m_objectAlias; }
    void objectAlias(bool flag) { m_objectAlias = flag; }
    // An alias is transparent: it denotes whatever it was declared for (6.6.1)
    // What the name finally denotes. An alias may name another alias, so the
    // chain is followed to its end -- with a depth guard, since a malformed
    // design could tie one in a knot.
    const VhdlSymbol* resolved() const {
        const VhdlSymbol* p = this;
        for (int depth = 0; p->m_aliasp && depth < 32; ++depth) p = p->m_aliasp;
        return p;
    }
    VhdlSymbol* resolved() {
        VhdlSymbol* p = this;
        for (int depth = 0; p->m_aliasp && depth < 32; ++depth) p = p->m_aliasp;
        return p;
    }

    const VhdlValue& value() const { return m_value; }
    void value(const VhdlValue& val) { m_value = val; }
    // A composite value never answers this: it says how wide something is,
    // and an object holding one still needs its storage
    bool hasValue() const { return m_value.known() && !m_value.isComposite(); }
    // A value worked out for elaboration that the emitted code keeps its
    // object for: a record some of whose elements no one can work out, of
    // which the ones that *are* known are what a width is taken from. Unlike
    // value(), holding one says nothing about how the object is emitted.
    const VhdlValue& elabValue() const { return m_elabValue; }
    void elabValue(const VhdlValue& val) { m_elabValue = val; }

    const std::vector<VhdlType*>& paramTypes() const { return m_paramTypes; }
    const std::vector<VhdlType*>& declaredParamTypes() const {
        return m_declaredParamTypes.empty() ? m_paramTypes : m_declaredParamTypes;
    }
    void declaredParamTypes(const std::vector<VhdlType*>& typeps) {
        m_declaredParamTypes = typeps;
    }
    void addParamType(VhdlType* typep) {
        m_paramTypes.push_back(typep);
        m_paramNames.emplace_back();
        m_paramDefaults.push_back(false);
    }
    void addParam(const std::string& name, VhdlType* typep, bool hasDefault,
                  bool fromGeneric = false) {
        m_paramTypes.push_back(typep);
        m_paramNames.push_back(name);
        m_paramDefaults.push_back(hasDefault);
        m_paramGenerics.push_back(fromGeneric);
    }
    // Whether the parameter was written with the name of a generic type. In
    // the uninstantiated unit such a parameter matches nothing else; two
    // subprograms that differ only in one are told apart there, and an
    // instantiation that makes their profiles alike must not make the call
    // ambiguous (4.7)
    bool paramFromGeneric(size_t i) const {
        return i < m_paramGenerics.size() && m_paramGenerics[i];
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
        // An alias is overloadable only where what it denotes is. An alias
        // of a subprogram or of an enumeration literal shares its designator
        // with the others; an alias of an object is a homograph of anything
        // of that name, and hides it (6.6.1, 12.3) -- which is what lets a
        // design alias something STANDARD already declares, "alias ff is f"
        // beside the format-feed character literal FF.
        if (m_kind == VhdlSymKind::ALIAS) return !m_objectAlias;
        return m_kind == VhdlSymKind::FUNCTION || m_kind == VhdlSymKind::PROCEDURE
               || m_kind == VhdlSymKind::ENUM_LITERAL;
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
    VhdlSymbol* m_ownerp = nullptr;  // The subprogram this is the region of
    // Where a design unit's own path starts, which is not just its name: a
    // package is reached through its library and an architecture is written
    // with the entity it belongs to (16.2.7)
    std::string m_instPrefix;
    std::string m_pathPrefix;
    bool m_designRoot = false;  // An entity's or architecture's region: its paths run on
                                // through the instances above it, one per instance
    // Declared here. A multimap because a name may denote several overloads.
    std::multimap<std::string, VhdlSymbol*> m_direct;
    // Made potentially visible by a use clause (12.4). Searched only when the
    // enclosing regions declare nothing of that name.
    std::vector<VhdlScope*> m_usedp;
    // Of those, the ones a use clause named only some declarations of --
    // "use work.pkg.f" rather than "use work.pkg.all" -- and which (12.4)
    std::map<const VhdlScope*, std::set<std::string>> m_useOnly;
    // What VHDL-2008 added to the predefined environment is declared once, for
    // every revision, and marked as it is inserted; a unit written in an
    // earlier revision is then analyzed without it (16.3)
    static bool s_mark2008;  // Insertions now are VHDL-2008 additions
    static bool s_hide2008;  // Lookups now leave VHDL-2008 additions out
    // Regions this one continues (12.1): an architecture continues its
    // entity's, a package body its package's. What they declare is directly
    // visible here, not merely potentially visible.
    std::vector<VhdlScope*> m_extendsp;
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
    // The subprogram whose declarative region this is, where it is one. A
    // path names a subprogram with its profile, which the region's name
    // alone does not carry (16.2.7)
    VhdlSymbol* ownerp() const { return m_ownerp; }
    void ownerp(VhdlSymbol* symp) { m_ownerp = symp; }
    const std::multimap<std::string, VhdlSymbol*>& direct() const { return m_direct; }
    const std::string& instPrefix() const { return m_instPrefix; }
    const std::string& pathPrefix() const { return m_pathPrefix; }
    void prefixes(const std::string& inst, const std::string& path) {
        m_instPrefix = inst;
        m_pathPrefix = path;
    }
    bool designRoot() const { return m_designRoot; }
    void designRoot(bool flag) { m_designRoot = flag; }

    static bool mark2008() { return s_mark2008; }
    static void mark2008(bool flag) { s_mark2008 = flag; }
    static bool hide2008() { return s_hide2008; }
    static void hide2008(bool flag) { s_hide2008 = flag; }

    // Declare a symbol here. Returns the symbol it hides, if any, so the
    // caller can complain about an illegal redeclaration (12.3).
    VhdlSymbol* insert(VhdlSymbol* symp) VL_MT_DISABLED;
    // A declaration of another region made visible by this one's name too,
    // as an instantiated package makes visible what its generics stand for.
    // The symbol stays the other region's own.
    void reexport(VhdlSymbol* symp) { m_direct.emplace(symp->name(), symp); }

    // Symbols declared directly in this region with that name
    void lookupLocal(const std::string& name, std::vector<VhdlSymbol*>& out) const VL_MT_DISABLED;

    // Every declaration that name denotes here, following 12.3 and 12.4: the
    // innermost region that declares it wins, and only if no region declares
    // it are use clauses consulted.
    void lookup(const std::string& name, std::vector<VhdlSymbol*>& out) const VL_MT_DISABLED;
    // Only what use clauses of this region and those enclosing it make potentially visible
    void lookupUsed(const std::string& name, std::vector<VhdlSymbol*>& out) const VL_MT_DISABLED;

    // Make another region's declarations potentially visible (use ... .all)
    void addUse(VhdlScope* scopep) VL_MT_DISABLED;
    // Make only the declarations of these names potentially visible, unless
    // a use clause has made them all visible already (use ... .name)
    void addUseOf(VhdlScope* scopep, const std::set<std::string>& names) VL_MT_DISABLED;

    // Continue another region here, so that its declarations are directly
    // visible in this one (12.1)
    void addExtends(VhdlScope* scopep) VL_MT_DISABLED;
    const std::vector<VhdlScope*>& extends() const { return m_extendsp; }

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
