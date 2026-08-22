// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL type model and symbol table
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
// The types and declarations that VHDL analysis works over, kept separate
// from Verilator's AstNodeDType because they answer different questions.
// AstNodeDType describes storage, which is what the back end needs. VhdlType
// describes a VHDL type in the sense of clause 5: its class, its base type,
// its constraint, whether it is resolved, and which literals belong to it.
// V3VhdlLower maps one onto the other once analysis is done.
//
// Types are owned by an arena that lives as long as the frontend does, and
// referred to by raw pointer everywhere else.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLTYPE_H_
#define VERILATOR_V3VHDLTYPE_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Error.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class AstNode;
class FileLine;
class VhdlScope;
class VhdlSymbol;

//============================================================================
// Type classes, per IEEE 1076-2008 clause 5

enum class VhdlTypeKind : uint8_t {
    ERROR,  // Stands in after a diagnostic, and matches anything
    ENUM,  // 5.2.2
    INTEGER,  // 5.2.3
    FLOATING,  // 5.2.5
    PHYSICAL,  // 5.2.4
    ARRAY,  // 5.3.2
    RECORD,  // 5.3.3
    ACCESS,  // 5.4
    FILE,  // 5.5
    PROTECTED,  // 5.6
    INCOMPLETE,  // 5.4.2, before its full declaration
    UNIVERSAL_INTEGER,  // The type of an integer literal (9.3.2)
    UNIVERSAL_REAL,  // The type of a real literal (9.3.2)
};

//============================================================================
// A type or subtype.
//
// A subtype shares its base type's class and shape and adds a constraint; a
// type is its own base. Two objects are of the same type when their base()
// pointers are equal, which is what type checking compares.

class VhdlType final {
    // MEMBERS
    const VhdlTypeKind m_kind;
    std::string m_name;  // Declared name, or empty for an anonymous type
    VhdlType* m_base = nullptr;  // The type this constrains; self if a type
    FileLine* m_fileline = nullptr;  // Where it was declared

    // Scalar bounds, once they are known. Physical types count in their
    // primary unit; floating types use m_lowReal/m_highReal.
    int64_t m_low = 0;
    int64_t m_high = 0;
    double m_lowReal = 0.0;
    double m_highReal = 0.0;
    bool m_bounded = false;  // Bounds are known
    bool m_ascending = true;  // Written "to" rather than "downto"

    // ENUM: its literals, in position order
    std::vector<VhdlSymbol*> m_literals;
    // ARRAY: index types and element type
    std::vector<VhdlType*> m_indexTypes;
    VhdlType* m_elementp = nullptr;
    bool m_unconstrained = false;  // Array with an unconstrained index
    // RECORD: its elements, in declaration order
    std::vector<VhdlSymbol*> m_elements;
    // ACCESS, FILE: the designated type
    VhdlType* m_designatedp = nullptr;
    // PHYSICAL: unit names and their values in the primary unit
    std::vector<std::pair<std::string, int64_t>> m_units;
    // PROTECTED: the scope holding its methods
    VhdlScope* m_scopep = nullptr;
    // Whether values of this type are resolved, and by what (6.3)
    VhdlSymbol* m_resolutionp = nullptr;

    VL_UNCOPYABLE(VhdlType);

public:
    // CONSTRUCTORS
    VhdlType(VhdlTypeKind kind, const std::string& name, FileLine* fl)
        : m_kind{kind}
        , m_name{name}
        , m_fileline{fl} {
        m_base = this;
    }
    ~VhdlType() = default;

    // METHODS
    VhdlTypeKind kind() const { return m_kind; }
    const std::string& name() const { return m_name; }
    void name(const std::string& n) { m_name = n; }
    FileLine* fileline() const { return m_fileline; }

    VhdlType* base() const { return m_base; }
    void base(VhdlType* typep) { m_base = typep; }
    bool isSubtype() const { return m_base != this; }

    // Two values are type-compatible when they share a base type. ERROR
    // matches anything, so one bad declaration does not cascade.
    bool compatible(const VhdlType* otherp) const {
        if (!otherp) return true;
        if (m_kind == VhdlTypeKind::ERROR || otherp->kind() == VhdlTypeKind::ERROR) return true;
        return base() == otherp->base();
    }

    bool isScalar() const {
        switch (m_kind) {
        case VhdlTypeKind::ENUM:
        case VhdlTypeKind::INTEGER:
        case VhdlTypeKind::FLOATING:
        case VhdlTypeKind::PHYSICAL:
        case VhdlTypeKind::UNIVERSAL_INTEGER:
        case VhdlTypeKind::UNIVERSAL_REAL: return true;
        default: return false;
        }
    }
    bool isDiscrete() const {
        return m_kind == VhdlTypeKind::ENUM || m_kind == VhdlTypeKind::INTEGER
               || m_kind == VhdlTypeKind::UNIVERSAL_INTEGER;
    }
    bool isNumeric() const {
        return m_kind == VhdlTypeKind::INTEGER || m_kind == VhdlTypeKind::FLOATING
               || m_kind == VhdlTypeKind::PHYSICAL
               || m_kind == VhdlTypeKind::UNIVERSAL_INTEGER
               || m_kind == VhdlTypeKind::UNIVERSAL_REAL;
    }
    bool isComposite() const {
        return m_kind == VhdlTypeKind::ARRAY || m_kind == VhdlTypeKind::RECORD;
    }
    bool isUniversal() const {
        return m_kind == VhdlTypeKind::UNIVERSAL_INTEGER
               || m_kind == VhdlTypeKind::UNIVERSAL_REAL;
    }
    bool isError() const { return m_kind == VhdlTypeKind::ERROR; }

    // Scalar bounds
    bool bounded() const { return m_bounded; }
    int64_t low() const { return m_low; }
    int64_t high() const { return m_high; }
    double lowReal() const { return m_lowReal; }
    double highReal() const { return m_highReal; }
    bool ascending() const { return m_ascending; }
    void setBounds(int64_t lo, int64_t hi, bool ascending) {
        m_low = lo;
        m_high = hi;
        m_ascending = ascending;
        m_bounded = true;
    }
    void setRealBounds(double lo, double hi, bool ascending) {
        m_lowReal = lo;
        m_highReal = hi;
        m_ascending = ascending;
        m_bounded = true;
    }

    // Enumeration
    const std::vector<VhdlSymbol*>& literals() const { return m_literals; }
    void addLiteral(VhdlSymbol* symp) { m_literals.push_back(symp); }

    // Array
    const std::vector<VhdlType*>& indexTypes() const { return m_indexTypes; }
    void addIndexType(VhdlType* typep) { m_indexTypes.push_back(typep); }
    VhdlType* elementp() const { return m_elementp; }
    void elementp(VhdlType* typep) { m_elementp = typep; }
    bool unconstrained() const { return m_unconstrained; }
    void unconstrained(bool flag) { m_unconstrained = flag; }
    size_t dimensions() const { return m_indexTypes.size(); }

    // Record
    const std::vector<VhdlSymbol*>& elements() const { return m_elements; }
    void addElement(VhdlSymbol* symp) { m_elements.push_back(symp); }

    // Access and file
    VhdlType* designatedp() const { return m_designatedp; }
    void designatedp(VhdlType* typep) { m_designatedp = typep; }

    // Physical units
    const std::vector<std::pair<std::string, int64_t>>& units() const { return m_units; }
    void addUnit(const std::string& name, int64_t value) { m_units.emplace_back(name, value); }

    // Protected type body scope
    VhdlScope* scopep() const { return m_scopep; }
    void scopep(VhdlScope* scopep) { m_scopep = scopep; }

    // Resolution (6.3)
    VhdlSymbol* resolutionp() const { return m_resolutionp; }
    void resolutionp(VhdlSymbol* symp) { m_resolutionp = symp; }
    bool isResolved() const { return m_resolutionp != nullptr; }

    // For diagnostics
    std::string prettyName() const VL_MT_DISABLED;
};

#endif  // Guard
