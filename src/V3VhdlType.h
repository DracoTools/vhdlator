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
    // Not const: an incomplete type becomes the full one when its
    // declaration arrives, and everything that already names it must see the
    // completion (5.4.2)
    VhdlTypeKind m_kind;
    bool m_genericFormal = false;  // A generic type nothing has bound yet
    std::string m_name;  // Declared name, or empty for an anonymous type
    VhdlType* m_base = nullptr;  // The type this constrains; self if a type
    FileLine* m_fileline = nullptr;  // Where it was declared

    // Scalar range, once it is known. Held the way VHDL states it -- left,
    // right and direction -- rather than as low and high, because a null
    // range such as (1 downto 10) has neither a low nor a high bound and
    // 'LEFT and 'RIGHT must still answer exactly (5.2.1, 16.2.3). Physical
    // types count in their primary unit; floating types use the real fields.
    int64_t m_left = 0;
    int64_t m_right = 0;
    double m_leftReal = 0.0;
    double m_rightReal = 0.0;
    bool m_bounded = false;  // The range is known
    bool m_ascending = true;  // Written "to" rather than "downto"

    // ENUM: its literals, in position order
    std::vector<VhdlSymbol*> m_literals;
    // ARRAY: index types and element type
    std::vector<VhdlType*> m_indexTypes;
    VhdlType* m_elementp = nullptr;
    bool m_unconstrained = false;  // Array with an unconstrained index
    // Which dimensions the "<>" was written for, one bit each. An array may
    // leave one dimension open and constrain another -- "array (t_two, t_enum)"
    // constrains both, yet t_two's bounds name a generic -- so knowing only
    // that the array is not fully constrained is not enough to tell which
    // index has no range of its own. Not every path that builds an array
    // records this, so until one does, every dimension of an unconstrained
    // array counts as open, which is what the whole type flag used to mean.
    uint32_t m_openDims = 0;
    bool m_openDimsKnown = false;
    // RECORD: its elements, in declaration order
    std::vector<VhdlSymbol*> m_elements;
    // ACCESS, FILE: the designated type
    VhdlType* m_designatedp = nullptr;
    // PHYSICAL: unit names and their values in the primary unit
    std::vector<std::pair<std::string, int64_t>> m_units;
    // PROTECTED: the scope holding its methods, and the body that defines
    // them, which is a separate declaration from the type's own (5.6.3)
    VhdlScope* m_scopep = nullptr;
    AstNode* m_bodyp = nullptr;
    // A range whose bounds did not fold, kept as written. A bound that names
    // a generic is fixed for each instance but not known during analysis, so
    // it is carried to the back end as an expression rather than a number.
    AstNode* m_boundsp = nullptr;
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
    // A generic type formal, before an instantiation says what it stands
    // for. It is a type of its own and matches nothing else (6.5.5), which
    // is what keeps two subprograms that differ only in it apart.
    bool isGenericFormal() const { return m_genericFormal; }
    void genericFormal(bool flag) { m_genericFormal = flag; }

    // Scalar range (5.2.1)
    bool bounded() const { return m_bounded; }
    void unbound() { m_bounded = false; }
    int64_t left() const { return m_left; }
    int64_t right() const { return m_right; }
    double leftReal() const { return m_leftReal; }
    double rightReal() const { return m_rightReal; }
    bool ascending() const { return m_ascending; }
    // A null range has no values, so asking for its bounds is meaningless;
    // callers check isNull() first
    bool isNull() const {
        if (!m_bounded) return false;
        if (m_kind == VhdlTypeKind::FLOATING) {
            return m_ascending ? m_leftReal > m_rightReal : m_leftReal < m_rightReal;
        }
        return m_ascending ? m_left > m_right : m_left < m_right;
    }
    int64_t low() const { return m_ascending ? m_left : m_right; }
    int64_t high() const { return m_ascending ? m_right : m_left; }
    double lowReal() const { return m_ascending ? m_leftReal : m_rightReal; }
    double highReal() const { return m_ascending ? m_rightReal : m_leftReal; }
    // Number of values in the range, for a discrete or physical type
    int64_t length() const {
        if (!m_bounded || isNull()) return 0;
        return high() - low() + 1;
    }
    void setBounds(int64_t left, int64_t right, bool ascending) {
        m_left = left;
        m_right = right;
        m_ascending = ascending;
        m_bounded = true;
    }
    void setRealBounds(double left, double right, bool ascending) {
        m_leftReal = left;
        m_rightReal = right;
        m_ascending = ascending;
        m_bounded = true;
    }
    // Copy the range of another type, as a subtype with no constraint of its
    // own inherits its base type's range
    void copyBounds(const VhdlType* fromp) {
        if (!fromp || !fromp->bounded()) return;
        m_left = fromp->m_left;
        m_right = fromp->m_right;
        m_leftReal = fromp->m_leftReal;
        m_rightReal = fromp->m_rightReal;
        m_ascending = fromp->m_ascending;
        m_bounded = true;
    }

    // An array whose every index range is known (5.3.2.1). Only such an array
    // has a length, and only such an array may be the subtype of an object.
    // The flag matters on its own: STRING is indexed by POSITIVE, which has
    // bounds of its own, yet STRING itself constrains nothing.
    bool isConstrainedArray() const {
        if (m_kind != VhdlTypeKind::ARRAY) return false;
        if (m_unconstrained) return false;
        if (m_indexTypes.empty()) return false;
        for (const VhdlType* const idxp : m_indexTypes) {
            if (!idxp || !idxp->bounded()) return false;
        }
        return true;
    }
    // Whether this array says how long it is -- as a number now, or as an
    // expression that folds once the instance is elaborated. A bound naming
    // a generic is a length; an index the text left open ("<>") is not one
    // until a value gives it one.
    bool lengthEmittable() const {
        if (base()->kind() != VhdlTypeKind::ARRAY) return true;
        if (m_indexTypes.empty()) return false;
        for (size_t i = 0; i < m_indexTypes.size(); ++i) {
            if (dimOpen(i)) return false;
            const VhdlType* const idxp = m_indexTypes[i];
            if (!idxp) return false;
            if (!idxp->bounded() && !idxp->boundsp()) return false;
        }
        const VhdlType* const elemp = m_elementp ? m_elementp : base()->m_elementp;
        if (elemp && elemp != this && elemp->base()->kind() == VhdlTypeKind::ARRAY) {
            return elemp->lengthEmittable();
        }
        return true;
    }
    // Length of one dimension, counting from zero
    int64_t dimLength(size_t dim) const {
        if (dim >= m_indexTypes.size() || !m_indexTypes[dim]) return 0;
        return m_indexTypes[dim]->length();
    }

    // Enumeration
    const std::vector<VhdlSymbol*>& literals() const { return m_literals; }
    void addLiteral(VhdlSymbol* symp) { m_literals.push_back(symp); }

    // Array
    const std::vector<VhdlType*>& indexTypes() const { return m_indexTypes; }
    void addIndexType(VhdlType* typep) { m_indexTypes.push_back(typep); }
    void clearIndexTypes() {
        m_indexTypes.clear();
        m_openDims = 0;
        m_openDimsKnown = false;
    }
    VhdlType* elementp() const { return m_elementp; }
    void elementp(VhdlType* typep) { m_elementp = typep; }
    bool unconstrained() const { return m_unconstrained; }
    void unconstrained(bool flag) { m_unconstrained = flag; }
    // Whether this dimension is the one nothing constrained
    bool dimOpen(size_t dim) const {
        if (!m_unconstrained) return false;
        if (!m_openDimsKnown) return true;  // Which dimension was never recorded
        return dim < 32 && (m_openDims & (1U << dim));
    }
    void setDimOpen(size_t dim) {
        if (dim < 32) m_openDims |= (1U << dim);
    }
    // Every open dimension of this array has now been named
    void openDimsKnown() { m_openDimsKnown = true; }
    size_t dimensions() const { return m_indexTypes.size(); }

    // Record
    const std::vector<VhdlSymbol*>& elements() const { return m_elements; }
    void addElement(VhdlSymbol* symp) { m_elements.push_back(symp); }
    // A subtype may constrain one element without touching the others (5.3.3)
    void replaceElement(size_t pos, VhdlSymbol* symp) {
        if (pos < m_elements.size()) m_elements[pos] = symp;
    }

    // Access and file
    VhdlType* designatedp() const { return m_designatedp; }
    void designatedp(VhdlType* typep) { m_designatedp = typep; }

    // Physical units
    const std::vector<std::pair<std::string, int64_t>>& units() const { return m_units; }
    void addUnit(const std::string& name, int64_t value) { m_units.emplace_back(name, value); }

    // Protected type body scope
    VhdlScope* scopep() const { return m_scopep; }
    void scopep(VhdlScope* scopep) { m_scopep = scopep; }
    AstNode* bodyp() const { return m_bodyp; }
    void bodyp(AstNode* nodep) { m_bodyp = nodep; }
    AstNode* boundsp() const { return m_boundsp; }
    void boundsp(AstNode* nodep) { m_boundsp = nodep; }

    // Take on everything another type is, keeping this object's identity so
    // that an access type declared against the incomplete name sees the full
    // declaration (5.4.2)
    void becomes(const VhdlType* otherp) {
        m_kind = otherp->m_kind;
        m_base = this;
        m_left = otherp->m_left;
        m_right = otherp->m_right;
        m_leftReal = otherp->m_leftReal;
        m_rightReal = otherp->m_rightReal;
        m_bounded = otherp->m_bounded;
        m_ascending = otherp->m_ascending;
        m_literals = otherp->m_literals;
        m_indexTypes = otherp->m_indexTypes;
        m_elementp = otherp->m_elementp;
        m_unconstrained = otherp->m_unconstrained;
        m_openDims = otherp->m_openDims;
        m_openDimsKnown = otherp->m_openDimsKnown;
        m_elements = otherp->m_elements;
        m_designatedp = otherp->m_designatedp;
        m_units = otherp->m_units;
        m_scopep = otherp->m_scopep;
        m_bodyp = otherp->m_bodyp;
        m_boundsp = otherp->m_boundsp;
        m_resolutionp = otherp->m_resolutionp;
    }

    // Resolution (6.3)
    VhdlSymbol* resolutionp() const { return m_resolutionp; }
    void resolutionp(VhdlSymbol* symp) { m_resolutionp = symp; }
    bool isResolved() const { return m_resolutionp != nullptr; }

    // For diagnostics
    std::string prettyName() const VL_MT_DISABLED;
};

//============================================================================
// The value of a static expression (9.4).
//
// Only what the frontend must know before elaboration is represented: enough
// to fold a range bound, size an array, and check a value against a subtype.
// Enumeration and physical values are integers -- a position number and a
// count of primary units -- because that is exactly what the LRM says they
// are, and it is also how V3VhdlLower will emit them.
//
// A value that is not static, or that the evaluator declines to fold, is
// UNKNOWN. UNKNOWN is not an error: it means "no static check is possible
// here", which is the right answer for anything depending on a generic.

enum class VhdlValKind : uint8_t {
    UNKNOWN,  // Not static, or not foldable
    INT,  // Integer, enumeration position, or physical value
    REAL,
    STR,  // String, bit string, or the result of 'IMAGE
};

class VhdlValue final {
    // MEMBERS
    VhdlValKind m_kind = VhdlValKind::UNKNOWN;
    int64_t m_int = 0;
    double m_real = 0.0;
    std::string m_str;
    VhdlType* m_typep = nullptr;  // The type the value belongs to, when known

public:
    // CONSTRUCTORS
    VhdlValue() = default;
    static VhdlValue mkInt(int64_t v, VhdlType* typep = nullptr) {
        VhdlValue out;
        out.m_kind = VhdlValKind::INT;
        out.m_int = v;
        out.m_typep = typep;
        return out;
    }
    static VhdlValue mkReal(double v, VhdlType* typep = nullptr) {
        VhdlValue out;
        out.m_kind = VhdlValKind::REAL;
        out.m_real = v;
        out.m_typep = typep;
        return out;
    }
    static VhdlValue mkStr(const std::string& v, VhdlType* typep = nullptr) {
        VhdlValue out;
        out.m_kind = VhdlValKind::STR;
        out.m_str = v;
        out.m_typep = typep;
        return out;
    }

    // METHODS
    VhdlValKind kind() const { return m_kind; }
    bool known() const { return m_kind != VhdlValKind::UNKNOWN; }
    bool isInt() const { return m_kind == VhdlValKind::INT; }
    bool isReal() const { return m_kind == VhdlValKind::REAL; }
    bool isStr() const { return m_kind == VhdlValKind::STR; }
    int64_t toInt() const { return m_int; }
    double toReal() const { return m_kind == VhdlValKind::REAL ? m_real : double(m_int); }
    const std::string& toStr() const { return m_str; }
    VhdlType* typep() const { return m_typep; }
    void typep(VhdlType* typep) { m_typep = typep; }

    // How the value reads back in a diagnostic
    std::string prettyName() const VL_MT_DISABLED;
};

#endif  // Guard
