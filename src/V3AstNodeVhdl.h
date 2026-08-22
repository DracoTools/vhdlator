// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL parse tree node definitions
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
// Nodes for constructs that VHDL has and Verilog does not, produced by
// V3VhdlParse and consumed by V3VhdlLower. They exist only between parsing and
// lowering; no core Verilator pass ever sees one.
//
// Everything here derives from AstNodeVhdl, so a single VN_IS check can assert
// that lowering left none behind.
//
// Do not edit the generated accessors: declare children with '@astgen op1..op4'
// and cross-links with '@astgen ptr', and astgen writes the rest.
//
//*************************************************************************

#ifndef VERILATOR_V3ASTNODEVHDL_H_
#define VERILATOR_V3ASTNODEVHDL_H_

#ifndef VERILATOR_V3AST_H_
#error "Use V3Ast.h as the include"
#include "V3Ast.h"  // This helps code analysis tools pick up symbols in V3Ast.h
#define VL_NOT_FINAL  // This #define fixes broken code folding in the IDE
#endif

class VhdlSymbol;
class VhdlType;

// === Attribute enums ========================================================

// Interface object class (6.5.2) and mode (6.5.2)
enum class VhdlIfaceKind : uint8_t { CONSTANT, SIGNAL, VARIABLE, FILE, TYPE, SUBPROGRAM, PACKAGE };
enum class VhdlMode : uint8_t { IN, OUT, INOUT, BUFFER, LINKAGE };

// Object declaration class (6.4.2)
enum class VhdlObjKind : uint8_t { CONSTANT, SIGNAL, VARIABLE, SHARED_VARIABLE, FILE };

// Signal kind (6.4.2.3)
enum class VhdlSignalKind : uint8_t { NONE, REGISTER, BUS };

// Delay mechanism (10.5.2.1)
enum class VhdlDelayKind : uint8_t { NONE, TRANSPORT, INERTIAL, REJECT_INERTIAL };

// Literal flavour (15.5 - 15.8, and the null literal of 9.3.1)
enum class VhdlLitKind : uint8_t { INTEGER, REAL, PHYSICAL, CHARACTER, STRING, BITSTRING, NULLLIT };

// Loop flavour (10.10)
enum class VhdlLoopKind : uint8_t { LOOP, WHILE, FOR };

// Concurrent and sequential signal assignment flavour (10.5, 11.6)
enum class VhdlAssignKind : uint8_t { SIMPLE, CONDITIONAL, SELECTED };

// Context item flavour (13.2, 13.4)
enum class VhdlContextKind : uint8_t { LIBRARY, USE, CONTEXT_REF };

// Range direction (5.2.1)
enum class VhdlDir : uint8_t { TO, DOWNTO };

// Unary operators (9.2)
enum class VhdlUnaryOp : uint8_t {
    IDENTITY,  // +
    NEGATE,  // -
    ABS,
    NOT,
    AND,  // Reduction operators, VHDL-2008 9.2.8
    NAND,
    OR,
    NOR,
    XOR,
    XNOR,
    COND_CONV,  // ?? , 9.2.9
};

// Binary operators (9.2), in precedence groups
enum class VhdlBinaryOp : uint8_t {
    // Logical, 9.2.2
    AND, OR, NAND, NOR, XOR, XNOR,
    // Relational, 9.2.3
    EQ, NE, LT, LE, GT, GE,
    // Matching relational, 9.2.3 (VHDL-2008)
    MATCH_EQ, MATCH_NE, MATCH_LT, MATCH_LE, MATCH_GT, MATCH_GE,
    // Shift, 9.2.4
    SLL, SRL, SLA, SRA, ROL, ROR,
    // Adding, 9.2.5
    ADD, SUB, CONCAT,
    // Multiplying, 9.2.7
    MUL, DIV, MOD, REM,
    // Misc, 9.2.8
    POW,
};

// Human-readable spellings, for dumps and error messages
const char* vhdlUnaryOpName(VhdlUnaryOp op) VL_MT_DISABLED;
const char* vhdlBinaryOpName(VhdlBinaryOp op) VL_MT_DISABLED;
const char* vhdlModeName(VhdlMode mode) VL_MT_DISABLED;

// === Abstract base classes ==================================================

class AstNodeVhdl VL_NOT_FINAL : public AstNode {
    // Any node of the VHDL parse tree
protected:
    AstNodeVhdl(VNType t, FileLine* fl)
        : AstNode{t, fl} {}

public:
    ASTGEN_MEMBERS_AstNodeVhdl;
};

class AstNodeVhdlDecl VL_NOT_FINAL : public AstNodeVhdl {
    // A declarative item (clause 6)
    std::string m_name;  // Declared simple name
    VhdlSymbol* m_vhdlSymp = nullptr;  // The symbol analysis created for it

protected:
    AstNodeVhdlDecl(VNType t, FileLine* fl, const std::string& name)
        : AstNodeVhdl{t, fl}
        , m_name{name} {}

public:
    ASTGEN_MEMBERS_AstNodeVhdlDecl;
    std::string name() const override VL_MT_STABLE { return m_name; }
    void name(const std::string& name) override { m_name = name; }
    VhdlSymbol* vhdlSymp() const { return m_vhdlSymp; }
    void vhdlSymp(VhdlSymbol* symp) { m_vhdlSymp = symp; }
};

class AstNodeVhdlExpr VL_NOT_FINAL : public AstNodeVhdl {
    // Anything that may appear where an expression is expected. VHDL cannot
    // tell a function call from an array index or a type conversion until
    // names are resolved (8.1), so several of these stay deliberately vague.
    //
    // Analysis fills in what the expression denotes and what type it has.
    // Neither is an Ast pointer, so astgen leaves them alone.
    VhdlType* m_vhdlTypep = nullptr;  // Type, once analysis has run
    VhdlSymbol* m_vhdlSymp = nullptr;  // What a name or call resolved to

protected:
    AstNodeVhdlExpr(VNType t, FileLine* fl)
        : AstNodeVhdl{t, fl} {}

public:
    ASTGEN_MEMBERS_AstNodeVhdlExpr;
    VhdlType* vhdlTypep() const { return m_vhdlTypep; }
    void vhdlTypep(VhdlType* typep) { m_vhdlTypep = typep; }
    VhdlSymbol* vhdlSymp() const { return m_vhdlSymp; }
    void vhdlSymp(VhdlSymbol* symp) { m_vhdlSymp = symp; }
};

class AstNodeVhdlStmt VL_NOT_FINAL : public AstNodeVhdl {
    // A concurrent or sequential statement, optionally labelled
    std::string m_label;  // Statement label, or empty

protected:
    AstNodeVhdlStmt(VNType t, FileLine* fl)
        : AstNodeVhdl{t, fl} {}

public:
    ASTGEN_MEMBERS_AstNodeVhdlStmt;
    std::string name() const override VL_MT_STABLE { return m_label; }
    void name(const std::string& name) override { m_label = name; }
    std::string label() const { return m_label; }
};

class AstNodeVhdlUnit VL_NOT_FINAL : public AstNodeVhdl {
    // A design unit (13.1): entity, architecture, package, package body,
    // configuration or context declaration
    // @astgen op1 := contextp : List[AstVhdlContextItem]  // Its context clause
    std::string m_name;  // Unit simple name
    std::string m_libname;  // Library it is analyzed into

protected:
    AstNodeVhdlUnit(VNType t, FileLine* fl, const std::string& name)
        : AstNodeVhdl{t, fl}
        , m_name{name} {}

public:
    ASTGEN_MEMBERS_AstNodeVhdlUnit;
    std::string name() const override VL_MT_STABLE { return m_name; }
    void name(const std::string& name) override { m_name = name; }
    std::string libname() const { return m_libname; }
    void libname(const std::string& name) { m_libname = name; }
};

// === Concrete node types ====================================================

// === AstNodeVhdl ===
class AstVhdlAccessType final : public AstNodeVhdl {
    // access_type_definition (5.4.1)
    // @astgen op1 := designatedp : AstVhdlSubtypeInd
public:
    AstVhdlAccessType(FileLine* fl, AstVhdlSubtypeInd* designatedp)
        : ASTGEN_SUPER_VhdlAccessType(fl) {
        this->designatedp(designatedp);
    }
    ASTGEN_MEMBERS_AstVhdlAccessType;
};

class AstVhdlAlternative final : public AstNodeVhdl {
    // A case alternative (10.9), a selected waveform (11.6), or a conditional
    // waveform (11.6). Exactly one of the payloads is used, per parent.
    // @astgen op1 := choicesp : List[AstVhdlChoice]   // Empty for a conditional "else"
    // @astgen op2 := condp : Optional[AstNodeVhdlExpr]  // Conditional forms
    // @astgen op3 := wavesp : List[AstVhdlWaveform]   // Assignment forms
    // @astgen op4 := stmtsp : List[AstNodeVhdlStmt]   // Case statement form
public:
    explicit AstVhdlAlternative(FileLine* fl)
        : ASTGEN_SUPER_VhdlAlternative(fl) {}
    ASTGEN_MEMBERS_AstVhdlAlternative;
};

class AstVhdlArrayType final : public AstNodeVhdl {
    // array_type_definition (5.3.2), constrained or unconstrained
    // @astgen op1 := indexp : AstVhdlIndexConstraint
    // @astgen op2 := elementp : AstVhdlSubtypeInd
public:
    AstVhdlArrayType(FileLine* fl, AstVhdlIndexConstraint* indexp, AstVhdlSubtypeInd* elementp)
        : ASTGEN_SUPER_VhdlArrayType(fl) {
        this->indexp(indexp);
        this->elementp(elementp);
    }
    ASTGEN_MEMBERS_AstVhdlArrayType;
};

class AstVhdlAssoc final : public AstNodeVhdl {
    // association_element (6.5.7): [formal =>] actual
    // @astgen op1 := formalp : Optional[AstNodeVhdlExpr]
    // @astgen op2 := actualp : Optional[AstNodeVhdlExpr]  // Absent means OPEN
    bool m_inertial = false;  // Actual was written "inertial expression"

public:
    AstVhdlAssoc(FileLine* fl, AstNodeVhdlExpr* formalp, AstNodeVhdlExpr* actualp)
        : ASTGEN_SUPER_VhdlAssoc(fl) {
        this->formalp(formalp);
        this->actualp(actualp);
    }
    ASTGEN_MEMBERS_AstVhdlAssoc;
    bool inertial() const { return m_inertial; }
    void inertial(bool flag) { m_inertial = flag; }
    bool isOpen() const { return !actualp(); }
};

class AstVhdlBinding final : public AstNodeVhdl {
    // binding_indication (7.3.2): use entity/configuration/open, plus maps
    // @astgen op1 := unitp : Optional[AstNodeVhdlExpr]  // Absent means OPEN
    // @astgen op2 := genericMapp : List[AstVhdlAssoc]
    // @astgen op3 := portMapp : List[AstVhdlAssoc]
    std::string m_kind;  // "entity", "configuration", "open"
    std::string m_archName;  // Architecture identifier for "entity x(arch)"

public:
    explicit AstVhdlBinding(FileLine* fl)
        : ASTGEN_SUPER_VhdlBinding(fl) {}
    ASTGEN_MEMBERS_AstVhdlBinding;
    std::string kind() const { return m_kind; }
    void kind(const std::string& s) { m_kind = s; }
    std::string archName() const { return m_archName; }
    void archName(const std::string& s) { m_archName = s; }
};

class AstVhdlBlockConfig final : public AstNodeVhdl {
    // block_configuration (3.4.2) and component_configuration (3.4.3)
    // @astgen op1 := specp : Optional[AstNodeVhdl]  // Block spec or component spec
    // @astgen op2 := usesp : List[AstVhdlContextItem]
    // @astgen op3 := bindingp : Optional[AstVhdlBinding]
    // @astgen op4 := itemsp : List[AstVhdlBlockConfig]
public:
    explicit AstVhdlBlockConfig(FileLine* fl)
        : ASTGEN_SUPER_VhdlBlockConfig(fl) {}
    ASTGEN_MEMBERS_AstVhdlBlockConfig;
};

class AstVhdlBlockHeader final : public AstNodeVhdl {
    // block_header (11.2), split out because a block already uses four operands
    // @astgen op1 := genericsp : List[AstVhdlIfaceDecl]
    // @astgen op2 := genericMapp : List[AstVhdlAssoc]
    // @astgen op3 := portsp : List[AstVhdlIfaceDecl]
    // @astgen op4 := portMapp : List[AstVhdlAssoc]
public:
    explicit AstVhdlBlockHeader(FileLine* fl)
        : ASTGEN_SUPER_VhdlBlockHeader(fl) {}
    ASTGEN_MEMBERS_AstVhdlBlockHeader;
};

class AstVhdlBox final : public AstNodeVhdl {
    // The box "<>", standing for an unconstrained index (5.3.2.1) or an
    // unspecified generic type/subprogram/package (6.5.5)
public:
    explicit AstVhdlBox(FileLine* fl)
        : ASTGEN_SUPER_VhdlBox(fl) {}
    ASTGEN_MEMBERS_AstVhdlBox;
};

class AstVhdlChoice final : public AstNodeVhdl {
    // choice (9.3.3.1): an expression, a discrete range, or "others"
    // @astgen op1 := exprp : Optional[AstNodeVhdlExpr]  // Absent means "others"
public:
    AstVhdlChoice(FileLine* fl, AstNodeVhdlExpr* exprp)
        : ASTGEN_SUPER_VhdlChoice(fl) {
        this->exprp(exprp);
    }
    ASTGEN_MEMBERS_AstVhdlChoice;
    bool isOthers() const { return !exprp(); }
};

class AstVhdlContextItem final : public AstNodeVhdl {
    // library_clause, use_clause or context_reference (13.2, 13.4)
    // @astgen op1 := namesp : List[AstNodeVhdlExpr]  // Libraries or selected names
    const VhdlContextKind m_kind;

public:
    AstVhdlContextItem(FileLine* fl, VhdlContextKind kind)
        : ASTGEN_SUPER_VhdlContextItem(fl)
        , m_kind{kind} {}
    ASTGEN_MEMBERS_AstVhdlContextItem;
    VhdlContextKind kind() const { return m_kind; }
    void dump(std::ostream& str) const override;
};

class AstVhdlDesignFile final : public AstNodeVhdl {
    // One analyzed source file, holding its design units in source order (13.1)
    // @astgen op1 := unitsp : List[AstNodeVhdlUnit]
    const std::string m_filename;  // Source this came from

public:
    AstVhdlDesignFile(FileLine* fl, const std::string& filename)
        : ASTGEN_SUPER_VhdlDesignFile(fl)
        , m_filename{filename} {}
    ASTGEN_MEMBERS_AstVhdlDesignFile;
    std::string name() const override VL_MT_STABLE { return m_filename; }
};

class AstVhdlElemAssoc final : public AstNodeVhdl {
    // element_association (9.3.3.1): [choices =>] expression
    // @astgen op1 := choicesp : List[AstVhdlChoice]
    // @astgen op2 := valuep : AstNodeVhdlExpr
public:
    AstVhdlElemAssoc(FileLine* fl, AstVhdlChoice* choicesp, AstNodeVhdlExpr* valuep)
        : ASTGEN_SUPER_VhdlElemAssoc(fl) {
        addChoicesp(choicesp);
        this->valuep(valuep);
    }
    ASTGEN_MEMBERS_AstVhdlElemAssoc;
};

class AstVhdlEntitySpec final : public AstNodeVhdl {
    // entity_specification (7.2) or component_specification (7.3.1):
    // the instantiation/entity name list plus its class
    // @astgen op1 := namesp : List[AstNodeVhdlExpr]  // Empty means "others" or "all"
    bool m_others = false;
    bool m_all = false;
    std::string m_entityClass;  // "signal", "component", ... or empty

public:
    explicit AstVhdlEntitySpec(FileLine* fl)
        : ASTGEN_SUPER_VhdlEntitySpec(fl) {}
    ASTGEN_MEMBERS_AstVhdlEntitySpec;
    bool others() const { return m_others; }
    void others(bool flag) { m_others = flag; }
    bool all() const { return m_all; }
    void all(bool flag) { m_all = flag; }
    std::string entityClass() const { return m_entityClass; }
    void entityClass(const std::string& s) { m_entityClass = s; }
};

class AstVhdlEnumType final : public AstNodeVhdl {
    // enumeration_type_definition (5.2.2)
    // @astgen op1 := literalsp : List[AstVhdlEnumLiteral]
public:
    explicit AstVhdlEnumType(FileLine* fl)
        : ASTGEN_SUPER_VhdlEnumType(fl) {}
    ASTGEN_MEMBERS_AstVhdlEnumType;
};

class AstVhdlFileType final : public AstNodeVhdl {
    // file_type_definition (5.5.1)
    // @astgen op1 := designatedp : AstNodeVhdlExpr
public:
    AstVhdlFileType(FileLine* fl, AstNodeVhdlExpr* designatedp)
        : ASTGEN_SUPER_VhdlFileType(fl) {
        this->designatedp(designatedp);
    }
    ASTGEN_MEMBERS_AstVhdlFileType;
};

class AstVhdlGenerateBody final : public AstNodeVhdl {
    // generate_statement_body (11.8) with its guarding condition or choices
    // @astgen op1 := condp : Optional[AstNodeVhdlExpr]  // if/elsif condition
    // @astgen op2 := choicesp : List[AstVhdlChoice]  // case-generate choices
    // @astgen op3 := declsp : List[AstNodeVhdl]
    // @astgen op4 := stmtsp : List[AstNodeVhdlStmt]
    std::string m_label;  // Alternative label, VHDL-2008

public:
    explicit AstVhdlGenerateBody(FileLine* fl)
        : ASTGEN_SUPER_VhdlGenerateBody(fl) {}
    ASTGEN_MEMBERS_AstVhdlGenerateBody;
    std::string name() const override VL_MT_STABLE { return m_label; }
    void name(const std::string& name) override { m_label = name; }
};

class AstVhdlIndexConstraint final : public AstNodeVhdl {
    // index_constraint (5.3.2.1), or an array type's index list. An unbounded
    // index is a box "<>", recorded as an absent range.
    // @astgen op1 := rangesp : List[AstNodeVhdl]
public:
    explicit AstVhdlIndexConstraint(FileLine* fl)
        : ASTGEN_SUPER_VhdlIndexConstraint(fl) {}
    ASTGEN_MEMBERS_AstVhdlIndexConstraint;
};

class AstVhdlNumericType final : public AstNodeVhdl {
    // integer_type_definition or floating_type_definition (5.2.3, 5.2.5).
    // Which one it is depends on the bounds, so the parser does not decide.
    // @astgen op1 := rangep : AstVhdlRange
public:
    AstVhdlNumericType(FileLine* fl, AstVhdlRange* rangep)
        : ASTGEN_SUPER_VhdlNumericType(fl) {
        this->rangep(rangep);
    }
    ASTGEN_MEMBERS_AstVhdlNumericType;
};

class AstVhdlPhysicalType final : public AstNodeVhdl {
    // physical_type_definition (5.2.4)
    // @astgen op1 := rangep : AstVhdlRange
    // @astgen op2 := unitsp : List[AstVhdlUnitDecl]
public:
    AstVhdlPhysicalType(FileLine* fl, AstVhdlRange* rangep)
        : ASTGEN_SUPER_VhdlPhysicalType(fl) {
        this->rangep(rangep);
    }
    ASTGEN_MEMBERS_AstVhdlPhysicalType;
};

class AstVhdlProtectedType final : public AstNodeVhdl {
    // protected_type_declaration or protected_type_body (5.6)
    // @astgen op1 := declsp : List[AstNodeVhdl]
    const bool m_isBody;

public:
    AstVhdlProtectedType(FileLine* fl, bool isBody)
        : ASTGEN_SUPER_VhdlProtectedType(fl)
        , m_isBody{isBody} {}
    ASTGEN_MEMBERS_AstVhdlProtectedType;
    bool isBody() const { return m_isBody; }
};

class AstVhdlRange final : public AstNodeVhdl {
    // range (5.2.1): either "left direction right" or a range attribute name
    // @astgen op1 := leftp : Optional[AstNodeVhdlExpr]
    // @astgen op2 := rightp : Optional[AstNodeVhdlExpr]
    // @astgen op3 := attrp : Optional[AstNodeVhdlExpr]  // e.g. t'range
    const VhdlDir m_dir;

public:
    AstVhdlRange(FileLine* fl, AstNodeVhdlExpr* leftp, VhdlDir dir, AstNodeVhdlExpr* rightp)
        : ASTGEN_SUPER_VhdlRange(fl)
        , m_dir{dir} {
        this->leftp(leftp);
        this->rightp(rightp);
    }
    AstVhdlRange(FileLine* fl, AstNodeVhdlExpr* attrp)
        : ASTGEN_SUPER_VhdlRange(fl)
        , m_dir{VhdlDir::TO} {
        this->attrp(attrp);
    }
    ASTGEN_MEMBERS_AstVhdlRange;
    VhdlDir dir() const { return m_dir; }
    void dump(std::ostream& str) const override;
};

class AstVhdlRecordConstraint final : public AstNodeVhdl {
    // record_constraint (5.3.3), VHDL-2008
    // @astgen op1 := elemsp : List[AstVhdlRecordElemConstraint]
public:
    explicit AstVhdlRecordConstraint(FileLine* fl)
        : ASTGEN_SUPER_VhdlRecordConstraint(fl) {}
    ASTGEN_MEMBERS_AstVhdlRecordConstraint;
};

class AstVhdlRecordElemConstraint final : public AstNodeVhdl {
    // record_element_constraint (5.3.3)
    // @astgen op1 := constraintp : AstNodeVhdl
    std::string m_name;  // Element simple name

public:
    AstVhdlRecordElemConstraint(FileLine* fl, const std::string& name, AstNodeVhdl* constraintp)
        : ASTGEN_SUPER_VhdlRecordElemConstraint(fl)
        , m_name{name} {
        this->constraintp(constraintp);
    }
    ASTGEN_MEMBERS_AstVhdlRecordElemConstraint;
    std::string name() const override VL_MT_STABLE { return m_name; }
    void name(const std::string& name) override { m_name = name; }
};

class AstVhdlRecordType final : public AstNodeVhdl {
    // record_type_definition (5.3.3)
    // @astgen op1 := elemsp : List[AstVhdlElementDecl]
public:
    explicit AstVhdlRecordType(FileLine* fl)
        : ASTGEN_SUPER_VhdlRecordType(fl) {}
    ASTGEN_MEMBERS_AstVhdlRecordType;
};

class AstVhdlSignature final : public AstNodeVhdl {
    // signature (4.5.3), disambiguating overloaded names in aliases and specs
    // @astgen op1 := paramTypesp : List[AstNodeVhdlExpr]
    // @astgen op2 := returnTypep : Optional[AstNodeVhdlExpr]
public:
    explicit AstVhdlSignature(FileLine* fl)
        : ASTGEN_SUPER_VhdlSignature(fl) {}
    ASTGEN_MEMBERS_AstVhdlSignature;
};

class AstVhdlSubtypeInd final : public AstNodeVhdl {
    // subtype_indication (6.3): [resolution] type_mark [constraint]
    // @astgen op1 := resolutionp : Optional[AstNodeVhdlExpr]
    // @astgen op2 := typeMarkp : Optional[AstNodeVhdlExpr]
    // @astgen op3 := constraintp : Optional[AstNodeVhdl]  // Range or index/record constraint
    VhdlType* m_vhdlTypep = nullptr;  // The subtype analysis derived

public:
    VhdlType* vhdlTypep() const { return m_vhdlTypep; }
    void vhdlTypep(VhdlType* typep) { m_vhdlTypep = typep; }
    AstVhdlSubtypeInd(FileLine* fl, AstNodeVhdlExpr* typeMarkp)
        : ASTGEN_SUPER_VhdlSubtypeInd(fl) {
        this->typeMarkp(typeMarkp);
    }
    ASTGEN_MEMBERS_AstVhdlSubtypeInd;
};

class AstVhdlWaveform final : public AstNodeVhdl {
    // waveform_element (10.5.2.1): value [after time]
    // @astgen op1 := valuep : Optional[AstNodeVhdlExpr]  // Absent means "unaffected"
    // @astgen op2 := afterp : Optional[AstNodeVhdlExpr]
public:
    AstVhdlWaveform(FileLine* fl, AstNodeVhdlExpr* valuep, AstNodeVhdlExpr* afterp)
        : ASTGEN_SUPER_VhdlWaveform(fl) {
        this->valuep(valuep);
        this->afterp(afterp);
    }
    ASTGEN_MEMBERS_AstVhdlWaveform;
};


// === AstNodeVhdlDecl ===
class AstVhdlAliasDecl final : public AstNodeVhdlDecl {
    // alias_declaration (6.6)
    // @astgen op1 := subtypep : Optional[AstVhdlSubtypeInd]
    // @astgen op2 := targetp : AstNodeVhdlExpr
    // @astgen op3 := signaturep : Optional[AstVhdlSignature]
public:
    AstVhdlAliasDecl(FileLine* fl, const std::string& name, AstNodeVhdlExpr* targetp)
        : ASTGEN_SUPER_VhdlAliasDecl(fl, name) {
        this->targetp(targetp);
    }
    ASTGEN_MEMBERS_AstVhdlAliasDecl;
};

class AstVhdlAttrDecl final : public AstNodeVhdlDecl {
    // attribute_declaration (6.7)
    // @astgen op1 := typeMarkp : AstNodeVhdlExpr
public:
    AstVhdlAttrDecl(FileLine* fl, const std::string& name, AstNodeVhdlExpr* typeMarkp)
        : ASTGEN_SUPER_VhdlAttrDecl(fl, name) {
        this->typeMarkp(typeMarkp);
    }
    ASTGEN_MEMBERS_AstVhdlAttrDecl;
};

class AstVhdlAttrSpec final : public AstNodeVhdlDecl {
    // attribute_specification (7.2)
    // @astgen op1 := specp : AstVhdlEntitySpec
    // @astgen op2 := valuep : AstNodeVhdlExpr
public:
    AstVhdlAttrSpec(FileLine* fl, const std::string& name, AstVhdlEntitySpec* specp,
                    AstNodeVhdlExpr* valuep)
        : ASTGEN_SUPER_VhdlAttrSpec(fl, name) {
        this->specp(specp);
        this->valuep(valuep);
    }
    ASTGEN_MEMBERS_AstVhdlAttrSpec;
};

class AstVhdlComponentDecl final : public AstNodeVhdlDecl {
    // component_declaration (6.8)
    // @astgen op1 := genericsp : List[AstVhdlIfaceDecl]
    // @astgen op2 := portsp : List[AstVhdlIfaceDecl]
public:
    AstVhdlComponentDecl(FileLine* fl, const std::string& name)
        : ASTGEN_SUPER_VhdlComponentDecl(fl, name) {}
    ASTGEN_MEMBERS_AstVhdlComponentDecl;
};

class AstVhdlConfigSpec final : public AstNodeVhdlDecl {
    // configuration_specification (7.3)
    // @astgen op1 := specp : AstVhdlEntitySpec
    // @astgen op2 := bindingp : Optional[AstVhdlBinding]
public:
    AstVhdlConfigSpec(FileLine* fl, AstVhdlEntitySpec* specp)
        : ASTGEN_SUPER_VhdlConfigSpec(fl, "") {
        this->specp(specp);
    }
    ASTGEN_MEMBERS_AstVhdlConfigSpec;
};

class AstVhdlDisconnectSpec final : public AstNodeVhdlDecl {
    // disconnection_specification (7.4)
    // @astgen op1 := specp : AstVhdlEntitySpec
    // @astgen op2 := afterp : AstNodeVhdlExpr
public:
    AstVhdlDisconnectSpec(FileLine* fl, AstVhdlEntitySpec* specp, AstNodeVhdlExpr* afterp)
        : ASTGEN_SUPER_VhdlDisconnectSpec(fl, "") {
        this->specp(specp);
        this->afterp(afterp);
    }
    ASTGEN_MEMBERS_AstVhdlDisconnectSpec;
};

class AstVhdlElementDecl final : public AstNodeVhdlDecl {
    // element_declaration of a record type (5.3.3)
    // @astgen op1 := subtypep : AstVhdlSubtypeInd
public:
    AstVhdlElementDecl(FileLine* fl, const std::string& name, AstVhdlSubtypeInd* subtypep)
        : ASTGEN_SUPER_VhdlElementDecl(fl, name) {
        this->subtypep(subtypep);
    }
    ASTGEN_MEMBERS_AstVhdlElementDecl;
};

class AstVhdlEnumLiteral final : public AstNodeVhdlDecl {
    // enumeration_literal (5.2.2): an identifier or a character literal
    const bool m_isChar;

public:
    AstVhdlEnumLiteral(FileLine* fl, const std::string& name, bool isChar)
        : ASTGEN_SUPER_VhdlEnumLiteral(fl, name)
        , m_isChar{isChar} {}
    ASTGEN_MEMBERS_AstVhdlEnumLiteral;
    bool isChar() const { return m_isChar; }
};

class AstVhdlGroupDecl final : public AstNodeVhdlDecl {
    // group_declaration (6.10)
    // @astgen op1 := templatep : AstNodeVhdlExpr
    // @astgen op2 := membersp : List[AstNodeVhdlExpr]
public:
    AstVhdlGroupDecl(FileLine* fl, const std::string& name, AstNodeVhdlExpr* templatep)
        : ASTGEN_SUPER_VhdlGroupDecl(fl, name) {
        this->templatep(templatep);
    }
    ASTGEN_MEMBERS_AstVhdlGroupDecl;
};

class AstVhdlGroupTemplate final : public AstNodeVhdlDecl {
    // group_template_declaration (6.9)
public:
    AstVhdlGroupTemplate(FileLine* fl, const std::string& name)
        : ASTGEN_SUPER_VhdlGroupTemplate(fl, name) {}
    ASTGEN_MEMBERS_AstVhdlGroupTemplate;
};

class AstVhdlIfaceDecl final : public AstNodeVhdlDecl {
    // interface_declaration (6.5.2): one generic, port or subprogram parameter.
    // The parser splits an identifier list into one node per identifier.
    // @astgen op1 := subtypep : Optional[AstVhdlSubtypeInd]
    // @astgen op2 := defaultp : Optional[AstNodeVhdlExpr]
    // @astgen op3 := genericsp : List[AstVhdlIfaceDecl]  // Generic package/subprogram
    const VhdlIfaceKind m_kind;
    VhdlMode m_mode = VhdlMode::IN;
    bool m_modeExplicit = false;  // Mode was written out
    VhdlSignalKind m_signalKind = VhdlSignalKind::NONE;

public:
    AstVhdlIfaceDecl(FileLine* fl, const std::string& name, VhdlIfaceKind kind)
        : ASTGEN_SUPER_VhdlIfaceDecl(fl, name)
        , m_kind{kind} {}
    ASTGEN_MEMBERS_AstVhdlIfaceDecl;
    VhdlIfaceKind kind() const { return m_kind; }
    VhdlMode mode() const { return m_mode; }
    void mode(VhdlMode mode) {
        m_mode = mode;
        m_modeExplicit = true;
    }
    bool modeExplicit() const { return m_modeExplicit; }
    VhdlSignalKind signalKind() const { return m_signalKind; }
    void signalKind(VhdlSignalKind kind) { m_signalKind = kind; }
    void dump(std::ostream& str) const override;
};

class AstVhdlObjectDecl final : public AstNodeVhdlDecl {
    // constant/signal/variable/file declaration (6.4.2)
    // @astgen op1 := subtypep : Optional[AstVhdlSubtypeInd]
    // @astgen op2 := defaultp : Optional[AstNodeVhdlExpr]
    // @astgen op3 := fileOpenp : Optional[AstNodeVhdlExpr]  // file ... open <expr>
    // @astgen op4 := fileNamep : Optional[AstNodeVhdlExpr]  // file ... is <expr>
    const VhdlObjKind m_kind;
    VhdlSignalKind m_signalKind = VhdlSignalKind::NONE;

public:
    AstVhdlObjectDecl(FileLine* fl, const std::string& name, VhdlObjKind kind)
        : ASTGEN_SUPER_VhdlObjectDecl(fl, name)
        , m_kind{kind} {}
    ASTGEN_MEMBERS_AstVhdlObjectDecl;
    VhdlObjKind kind() const { return m_kind; }
    VhdlSignalKind signalKind() const { return m_signalKind; }
    void signalKind(VhdlSignalKind kind) { m_signalKind = kind; }
    void dump(std::ostream& str) const override;
};

class AstVhdlSubprogDecl final : public AstNodeVhdlDecl {
    // subprogram_declaration or subprogram_body (4.2, 4.3)
    // @astgen op1 := paramsp : List[AstVhdlIfaceDecl]
    // @astgen op2 := returnTypep : Optional[AstNodeVhdlExpr]  // Function only
    // @astgen op3 := declsp : List[AstNodeVhdl]  // Body only
    // @astgen op4 := stmtsp : List[AstNodeVhdlStmt]  // Body only
    bool m_isFunction = false;
    bool m_isPure = true;  // Functions are pure unless declared impure
    bool m_hasBody = false;
    bool m_isOperator = false;  // Designator was a string, e.g. "and"

public:
    AstVhdlSubprogDecl(FileLine* fl, const std::string& name, bool isFunction)
        : ASTGEN_SUPER_VhdlSubprogDecl(fl, name)
        , m_isFunction{isFunction} {}
    ASTGEN_MEMBERS_AstVhdlSubprogDecl;
    bool isFunction() const { return m_isFunction; }
    // Named to avoid hiding AstNode::isPure(), which means something else:
    // this is VHDL's pure/impure function qualifier (4.2.1)
    bool vhdlPure() const { return m_isPure; }
    void vhdlPure(bool flag) { m_isPure = flag; }
    bool hasBody() const { return m_hasBody; }
    void hasBody(bool flag) { m_hasBody = flag; }
    bool isOperator() const { return m_isOperator; }
    void isOperator(bool flag) { m_isOperator = flag; }
    void dump(std::ostream& str) const override;
};

class AstVhdlSubprogInst final : public AstNodeVhdlDecl {
    // subprogram_instantiation_declaration (4.4), VHDL-2008
    // @astgen op1 := uninstantiatedp : AstNodeVhdlExpr
    // @astgen op2 := genericMapp : List[AstVhdlAssoc]
    // @astgen op3 := signaturep : Optional[AstVhdlSignature]
    const bool m_isFunction;

public:
    AstVhdlSubprogInst(FileLine* fl, const std::string& name, bool isFunction,
                       AstNodeVhdlExpr* uninstantiatedp)
        : ASTGEN_SUPER_VhdlSubprogInst(fl, name)
        , m_isFunction{isFunction} {
        this->uninstantiatedp(uninstantiatedp);
    }
    ASTGEN_MEMBERS_AstVhdlSubprogInst;
    bool isFunction() const { return m_isFunction; }
};

class AstVhdlSubtypeDecl final : public AstNodeVhdlDecl {
    // subtype_declaration (6.3)
    // @astgen op1 := subtypep : AstVhdlSubtypeInd
public:
    AstVhdlSubtypeDecl(FileLine* fl, const std::string& name, AstVhdlSubtypeInd* subtypep)
        : ASTGEN_SUPER_VhdlSubtypeDecl(fl, name) {
        this->subtypep(subtypep);
    }
    ASTGEN_MEMBERS_AstVhdlSubtypeDecl;
};

class AstVhdlTypeDecl final : public AstNodeVhdlDecl {
    // full_type_declaration or incomplete_type_declaration (6.2)
    // @astgen op1 := defp : Optional[AstNodeVhdl]  // Absent means incomplete
public:
    AstVhdlTypeDecl(FileLine* fl, const std::string& name, AstNodeVhdl* defp)
        : ASTGEN_SUPER_VhdlTypeDecl(fl, name) {
        this->defp(defp);
    }
    ASTGEN_MEMBERS_AstVhdlTypeDecl;
};

class AstVhdlUnitDecl final : public AstNodeVhdlDecl {
    // secondary_unit_declaration of a physical type (5.2.4)
    // @astgen op1 := valuep : Optional[AstNodeVhdlExpr]  // Absent for the primary unit
public:
    AstVhdlUnitDecl(FileLine* fl, const std::string& name, AstNodeVhdlExpr* valuep)
        : ASTGEN_SUPER_VhdlUnitDecl(fl, name) {
        this->valuep(valuep);
    }
    ASTGEN_MEMBERS_AstVhdlUnitDecl;
};


// === AstNodeVhdlExpr ===
class AstVhdlAggregate final : public AstNodeVhdlExpr {
    // aggregate (9.3.3)
    // @astgen op1 := elemsp : List[AstVhdlElemAssoc]
public:
    explicit AstVhdlAggregate(FileLine* fl)
        : ASTGEN_SUPER_VhdlAggregate(fl) {}
    ASTGEN_MEMBERS_AstVhdlAggregate;
};

class AstVhdlAllocator final : public AstNodeVhdlExpr {
    // allocator (9.3.7): new subtype_indication, or new qualified_expression
    // @astgen op1 := subtypep : Optional[AstVhdlSubtypeInd]
    // @astgen op2 := valuep : Optional[AstNodeVhdlExpr]
public:
    explicit AstVhdlAllocator(FileLine* fl)
        : ASTGEN_SUPER_VhdlAllocator(fl) {}
    ASTGEN_MEMBERS_AstVhdlAllocator;
};

class AstVhdlAttrName final : public AstNodeVhdlExpr {
    // attribute_name (8.6): prefix [signature] ' designator [ (expr) ]
    // @astgen op1 := prefixp : AstNodeVhdlExpr
    // @astgen op2 := argp : Optional[AstNodeVhdlExpr]
    // @astgen op3 := signaturep : Optional[AstVhdlSignature]
    std::string m_attrName;  // Attribute designator, lowercased

public:
    AstVhdlAttrName(FileLine* fl, AstNodeVhdlExpr* prefixp, const std::string& attrName)
        : ASTGEN_SUPER_VhdlAttrName(fl)
        , m_attrName{attrName} {
        this->prefixp(prefixp);
    }
    ASTGEN_MEMBERS_AstVhdlAttrName;
    std::string name() const override VL_MT_STABLE { return m_attrName; }
    void name(const std::string& name) override { m_attrName = name; }
};

class AstVhdlBinary final : public AstNodeVhdlExpr {
    // Binary operators (9.2)
    // @astgen op1 := lhsp : AstNodeVhdlExpr
    // @astgen op2 := rhsp : AstNodeVhdlExpr
    const VhdlBinaryOp m_op;

public:
    AstVhdlBinary(FileLine* fl, VhdlBinaryOp op, AstNodeVhdlExpr* lhsp, AstNodeVhdlExpr* rhsp)
        : ASTGEN_SUPER_VhdlBinary(fl)
        , m_op{op} {
        this->lhsp(lhsp);
        this->rhsp(rhsp);
    }
    ASTGEN_MEMBERS_AstVhdlBinary;
    VhdlBinaryOp op() const { return m_op; }
    void dump(std::ostream& str) const override;
};

class AstVhdlCall final : public AstNodeVhdlExpr {
    // prefix ( association_list ). Syntactically this is a function call, an
    // indexed name, a slice or a type conversion (8.1); which one it is cannot
    // be known until names are resolved, so the parser records it verbatim.
    // @astgen op1 := prefixp : AstNodeVhdlExpr
    // @astgen op2 := argsp : List[AstNodeVhdl]  // Assoc elements or a range
public:
    AstVhdlCall(FileLine* fl, AstNodeVhdlExpr* prefixp)
        : ASTGEN_SUPER_VhdlCall(fl) {
        this->prefixp(prefixp);
    }
    ASTGEN_MEMBERS_AstVhdlCall;
};

class AstVhdlDefault final : public AstNodeVhdlExpr {
    // The reserved word "default" used as a generic map actual (6.5.7.2)
public:
    explicit AstVhdlDefault(FileLine* fl)
        : ASTGEN_SUPER_VhdlDefault(fl) {}
    ASTGEN_MEMBERS_AstVhdlDefault;
};

class AstVhdlExternalName final : public AstNodeVhdlExpr {
    // external_name (8.7), VHDL-2008: << signal .path.to.thing : type >>
    // @astgen op1 := subtypep : AstVhdlSubtypeInd
    // @astgen op2 := pathp : List[AstNodeVhdlExpr]
    std::string m_objClass;  // "signal", "variable" or "constant"
    int m_upLevels = 0;  // Count of leading "^." in a relative pathname
    bool m_absolute = false;  // Pathname began with "."
    bool m_package = false;  // Pathname began with "@"

public:
    AstVhdlExternalName(FileLine* fl, const std::string& objClass, AstVhdlSubtypeInd* subtypep)
        : ASTGEN_SUPER_VhdlExternalName(fl)
        , m_objClass{objClass} {
        this->subtypep(subtypep);
    }
    ASTGEN_MEMBERS_AstVhdlExternalName;
    std::string objClass() const { return m_objClass; }
    int upLevels() const { return m_upLevels; }
    void upLevels(int n) { m_upLevels = n; }
    bool absolute() const { return m_absolute; }
    void absolute(bool flag) { m_absolute = flag; }
    bool package() const { return m_package; }
    void package(bool flag) { m_package = flag; }
};

class AstVhdlLiteral final : public AstNodeVhdlExpr {
    // Abstract, string, bit string, character, physical or null literal
    // (15.5 - 15.8, 9.3.2, 9.3.1)
    // @astgen op1 := unitp : Optional[AstNodeVhdlExpr]  // Physical literal unit name
    const VhdlLitKind m_kind;
    std::string m_text;  // Source text, or decoded contents for a string
    int64_t m_intVal = 0;
    double m_realVal = 0.0;
    char m_baseSpec = '\0';  // Bit string base: 'B', 'O', 'X' or 'D'
    bool m_signedBitStr = false;

public:
    AstVhdlLiteral(FileLine* fl, VhdlLitKind kind, const std::string& text)
        : ASTGEN_SUPER_VhdlLiteral(fl)
        , m_kind{kind}
        , m_text{text} {}
    ASTGEN_MEMBERS_AstVhdlLiteral;
    VhdlLitKind kind() const { return m_kind; }
    std::string name() const override VL_MT_STABLE { return m_text; }
    void name(const std::string& name) override { m_text = name; }
    std::string text() const { return m_text; }
    int64_t intVal() const { return m_intVal; }
    void intVal(int64_t v) { m_intVal = v; }
    double realVal() const { return m_realVal; }
    void realVal(double v) { m_realVal = v; }
    char baseSpec() const { return m_baseSpec; }
    void baseSpec(char c) { m_baseSpec = c; }
    bool signedBitStr() const { return m_signedBitStr; }
    void signedBitStr(bool flag) { m_signedBitStr = flag; }
    void dump(std::ostream& str) const override;
};

class AstVhdlName final : public AstNodeVhdlExpr {
    // simple_name, character literal used as a name, or operator symbol (8.2)
    std::string m_name;  // Lowercased for a basic identifier, verbatim otherwise
    const bool m_extendedId;  // Came from the \...\ form
    bool m_isChar = false;  // Was a character literal, e.g. type t is ('0','1')
    bool m_isOperator = false;  // Was an operator symbol, e.g. "and"

public:
    AstVhdlName(FileLine* fl, const std::string& name, bool extendedId = false)
        : ASTGEN_SUPER_VhdlName(fl)
        , m_name{name}
        , m_extendedId{extendedId} {}
    ASTGEN_MEMBERS_AstVhdlName;
    std::string name() const override VL_MT_STABLE { return m_name; }
    void name(const std::string& name) override { m_name = name; }
    bool extendedId() const { return m_extendedId; }
    bool isChar() const { return m_isChar; }
    void isChar(bool flag) { m_isChar = flag; }
    bool isOperator() const { return m_isOperator; }
    void isOperator(bool flag) { m_isOperator = flag; }
};

class AstVhdlOpen final : public AstNodeVhdlExpr {
    // The reserved word "open" used as an actual (6.5.7)
public:
    explicit AstVhdlOpen(FileLine* fl)
        : ASTGEN_SUPER_VhdlOpen(fl) {}
    ASTGEN_MEMBERS_AstVhdlOpen;
};

class AstVhdlQualified final : public AstNodeVhdlExpr {
    // qualified_expression (9.3.5): type_mark ' ( expression ) or ' aggregate
    // @astgen op1 := typeMarkp : AstNodeVhdlExpr
    // @astgen op2 := valuep : AstNodeVhdlExpr
public:
    AstVhdlQualified(FileLine* fl, AstNodeVhdlExpr* typeMarkp, AstNodeVhdlExpr* valuep)
        : ASTGEN_SUPER_VhdlQualified(fl) {
        this->typeMarkp(typeMarkp);
        this->valuep(valuep);
    }
    ASTGEN_MEMBERS_AstVhdlQualified;
};

class AstVhdlSelected final : public AstNodeVhdlExpr {
    // selected_name (8.3): prefix.suffix, where suffix may be "all"
    // @astgen op1 := prefixp : AstNodeVhdlExpr
    // @astgen op2 := suffixp : Optional[AstNodeVhdlExpr]  // Absent means ".all"
public:
    AstVhdlSelected(FileLine* fl, AstNodeVhdlExpr* prefixp, AstNodeVhdlExpr* suffixp)
        : ASTGEN_SUPER_VhdlSelected(fl) {
        this->prefixp(prefixp);
        this->suffixp(suffixp);
    }
    ASTGEN_MEMBERS_AstVhdlSelected;
    bool isAll() const { return !suffixp(); }
};

class AstVhdlUnary final : public AstNodeVhdlExpr {
    // Unary and reduction operators (9.2.1, 9.2.8, 9.2.9)
    // @astgen op1 := lhsp : AstNodeVhdlExpr
    const VhdlUnaryOp m_op;

public:
    AstVhdlUnary(FileLine* fl, VhdlUnaryOp op, AstNodeVhdlExpr* lhsp)
        : ASTGEN_SUPER_VhdlUnary(fl)
        , m_op{op} {
        this->lhsp(lhsp);
    }
    ASTGEN_MEMBERS_AstVhdlUnary;
    VhdlUnaryOp op() const { return m_op; }
    void dump(std::ostream& str) const override;
};


// === AstNodeVhdlStmt ===
class AstVhdlAssert final : public AstNodeVhdlStmt {
    // assertion_statement (10.3) and report_statement (10.4), plus their
    // concurrent forms (11.5)
    // @astgen op1 := condp : Optional[AstNodeVhdlExpr]  // Absent for a report
    // @astgen op2 := reportp : Optional[AstNodeVhdlExpr]
    // @astgen op3 := severityp : Optional[AstNodeVhdlExpr]
    bool m_postponed = false;

public:
    explicit AstVhdlAssert(FileLine* fl)
        : ASTGEN_SUPER_VhdlAssert(fl) {}
    ASTGEN_MEMBERS_AstVhdlAssert;
    bool postponed() const { return m_postponed; }
    void postponed(bool flag) { m_postponed = flag; }
    bool isReport() const { return !condp(); }
};

class AstVhdlBlock final : public AstNodeVhdlStmt {
    // block_statement (11.2)
    // @astgen op1 := guardp : Optional[AstNodeVhdlExpr]
    // @astgen op2 := headerp : Optional[AstVhdlBlockHeader]
    // @astgen op3 := declsp : List[AstNodeVhdl]
    // @astgen op4 := stmtsp : List[AstNodeVhdlStmt]
public:
    explicit AstVhdlBlock(FileLine* fl)
        : ASTGEN_SUPER_VhdlBlock(fl) {}
    ASTGEN_MEMBERS_AstVhdlBlock;
};

class AstVhdlCase final : public AstNodeVhdlStmt {
    // case_statement (10.9)
    // @astgen op1 := exprp : AstNodeVhdlExpr
    // @astgen op2 := altsp : List[AstVhdlAlternative]
    bool m_matching = false;  // VHDL-2008 "case?"

public:
    AstVhdlCase(FileLine* fl, AstNodeVhdlExpr* exprp)
        : ASTGEN_SUPER_VhdlCase(fl) {
        this->exprp(exprp);
    }
    ASTGEN_MEMBERS_AstVhdlCase;
    bool matching() const { return m_matching; }
    void matching(bool flag) { m_matching = flag; }
};

class AstVhdlGenerate final : public AstNodeVhdlStmt {
    // for/if/case generate_statement (11.8). The alternatives carry the
    // conditions or choices; a for-generate uses paramName and rangep.
    // @astgen op1 := rangep : Optional[AstVhdlRange]  // for-generate
    // @astgen op2 := exprp : Optional[AstNodeVhdlExpr]  // case-generate selector
    // @astgen op3 := altsp : List[AstVhdlGenerateBody]
    std::string m_paramName;  // for-generate parameter
    const VhdlLoopKind m_kind;  // FOR, or LOOP standing for if/case

public:
    AstVhdlGenerate(FileLine* fl, VhdlLoopKind kind)
        : ASTGEN_SUPER_VhdlGenerate(fl)
        , m_kind{kind} {}
    ASTGEN_MEMBERS_AstVhdlGenerate;
    VhdlLoopKind kind() const { return m_kind; }
    std::string paramName() const { return m_paramName; }
    void paramName(const std::string& s) { m_paramName = s; }
};

class AstVhdlIf final : public AstNodeVhdlStmt {
    // if_statement (10.8). Each alternative holds a condition and statements;
    // the final alternative may have no condition, meaning "else".
    // @astgen op1 := altsp : List[AstVhdlAlternative]
public:
    explicit AstVhdlIf(FileLine* fl)
        : ASTGEN_SUPER_VhdlIf(fl) {}
    ASTGEN_MEMBERS_AstVhdlIf;
};

class AstVhdlInstance final : public AstNodeVhdlStmt {
    // component_instantiation_statement (11.7)
    // @astgen op1 := unitp : AstNodeVhdlExpr  // Component, entity or configuration name
    // @astgen op2 := genericMapp : List[AstVhdlAssoc]
    // @astgen op3 := portMapp : List[AstVhdlAssoc]
    std::string m_kind;  // "component", "entity", "configuration" or "" if implicit
    std::string m_archName;  // Architecture identifier for "entity x(arch)"

public:
    AstVhdlInstance(FileLine* fl, AstNodeVhdlExpr* unitp)
        : ASTGEN_SUPER_VhdlInstance(fl) {
        this->unitp(unitp);
    }
    ASTGEN_MEMBERS_AstVhdlInstance;
    std::string kind() const { return m_kind; }
    void kind(const std::string& s) { m_kind = s; }
    std::string archName() const { return m_archName; }
    void archName(const std::string& s) { m_archName = s; }
};

class AstVhdlLoop final : public AstNodeVhdlStmt {
    // loop_statement (10.10), in its bare, while and for forms
    // @astgen op1 := condp : Optional[AstNodeVhdlExpr]  // WHILE
    // @astgen op2 := rangep : Optional[AstVhdlRange]  // FOR
    // @astgen op3 := stmtsp : List[AstNodeVhdlStmt]
    const VhdlLoopKind m_kind;
    std::string m_paramName;  // FOR loop parameter

public:
    AstVhdlLoop(FileLine* fl, VhdlLoopKind kind)
        : ASTGEN_SUPER_VhdlLoop(fl)
        , m_kind{kind} {}
    ASTGEN_MEMBERS_AstVhdlLoop;
    VhdlLoopKind kind() const { return m_kind; }
    std::string paramName() const { return m_paramName; }
    void paramName(const std::string& s) { m_paramName = s; }
};

class AstVhdlNext final : public AstNodeVhdlStmt {
    // next_statement (10.11) and exit_statement (10.12)
    // @astgen op1 := condp : Optional[AstNodeVhdlExpr]
    const bool m_isExit;
    std::string m_loopLabel;

public:
    AstVhdlNext(FileLine* fl, bool isExit)
        : ASTGEN_SUPER_VhdlNext(fl)
        , m_isExit{isExit} {}
    ASTGEN_MEMBERS_AstVhdlNext;
    bool isExit() const { return m_isExit; }
    std::string loopLabel() const { return m_loopLabel; }
    void loopLabel(const std::string& s) { m_loopLabel = s; }
};

class AstVhdlNull final : public AstNodeVhdlStmt {
    // null_statement (10.14)
public:
    explicit AstVhdlNull(FileLine* fl)
        : ASTGEN_SUPER_VhdlNull(fl) {}
    ASTGEN_MEMBERS_AstVhdlNull;
};

class AstVhdlProcCall final : public AstNodeVhdlStmt {
    // procedure_call_statement (10.7) and its concurrent form (11.4)
    // @astgen op1 := callp : AstNodeVhdlExpr
    bool m_postponed = false;

public:
    AstVhdlProcCall(FileLine* fl, AstNodeVhdlExpr* callp)
        : ASTGEN_SUPER_VhdlProcCall(fl) {
        this->callp(callp);
    }
    ASTGEN_MEMBERS_AstVhdlProcCall;
    bool postponed() const { return m_postponed; }
    void postponed(bool flag) { m_postponed = flag; }
};

class AstVhdlProcess final : public AstNodeVhdlStmt {
    // process_statement (11.3)
    // @astgen op1 := sensp : List[AstNodeVhdlExpr]  // Empty if none written
    // @astgen op2 := declsp : List[AstNodeVhdl]
    // @astgen op3 := stmtsp : List[AstNodeVhdlStmt]
    bool m_postponed = false;
    bool m_hasSensList = false;  // "process (...)" as opposed to "process"
    bool m_sensAll = false;  // VHDL-2008 "process (all)"

public:
    explicit AstVhdlProcess(FileLine* fl)
        : ASTGEN_SUPER_VhdlProcess(fl) {}
    ASTGEN_MEMBERS_AstVhdlProcess;
    bool postponed() const { return m_postponed; }
    void postponed(bool flag) { m_postponed = flag; }
    bool hasSensList() const { return m_hasSensList; }
    void hasSensList(bool flag) { m_hasSensList = flag; }
    bool sensAll() const { return m_sensAll; }
    void sensAll(bool flag) { m_sensAll = flag; }
    void dump(std::ostream& str) const override;
};

class AstVhdlReturn final : public AstNodeVhdlStmt {
    // return_statement (10.13)
    // @astgen op1 := valuep : Optional[AstNodeVhdlExpr]
public:
    explicit AstVhdlReturn(FileLine* fl)
        : ASTGEN_SUPER_VhdlReturn(fl) {}
    ASTGEN_MEMBERS_AstVhdlReturn;
};

class AstVhdlSigAssign final : public AstNodeVhdlStmt {
    // signal_assignment_statement (10.5) and its concurrent form (11.6).
    // SIMPLE uses wavesp; CONDITIONAL and SELECTED use altsp.
    // @astgen op1 := targetp : AstNodeVhdlExpr
    // @astgen op2 := wavesp : List[AstVhdlWaveform]
    // @astgen op3 := altsp : List[AstVhdlAlternative]
    // @astgen op4 := selectorp : Optional[AstNodeVhdlExpr]  // SELECTED
    const VhdlAssignKind m_kind;
    VhdlDelayKind m_delay = VhdlDelayKind::NONE;
    bool m_guarded = false;  // Concurrent form written "guarded"
    bool m_postponed = false;
    bool m_matching = false;  // VHDL-2008 "with ... select?"
    bool m_force = false;  // VHDL-2008 force assignment
    bool m_release = false;  // VHDL-2008 release
    std::string m_forceMode;  // "in", "out" or empty

public:
    AstVhdlSigAssign(FileLine* fl, VhdlAssignKind kind, AstNodeVhdlExpr* targetp)
        : ASTGEN_SUPER_VhdlSigAssign(fl)
        , m_kind{kind} {
        this->targetp(targetp);
    }
    ASTGEN_MEMBERS_AstVhdlSigAssign;
    VhdlAssignKind kind() const { return m_kind; }
    VhdlDelayKind delay() const { return m_delay; }
    void delay(VhdlDelayKind kind) { m_delay = kind; }
    bool guarded() const { return m_guarded; }
    void guarded(bool flag) { m_guarded = flag; }
    bool postponed() const { return m_postponed; }
    void postponed(bool flag) { m_postponed = flag; }
    bool matching() const { return m_matching; }
    void matching(bool flag) { m_matching = flag; }
    bool force() const { return m_force; }
    void force(bool flag) { m_force = flag; }
    bool release() const { return m_release; }
    void release(bool flag) { m_release = flag; }
    std::string forceMode() const { return m_forceMode; }
    void forceMode(const std::string& s) { m_forceMode = s; }
    void dump(std::ostream& str) const override;
};

class AstVhdlVarAssign final : public AstNodeVhdlStmt {
    // variable_assignment_statement (10.6)
    // @astgen op1 := targetp : AstNodeVhdlExpr
    // @astgen op2 := valuep : Optional[AstNodeVhdlExpr]  // SIMPLE
    // @astgen op3 := altsp : List[AstVhdlAlternative]  // CONDITIONAL, SELECTED
    // @astgen op4 := selectorp : Optional[AstNodeVhdlExpr]  // SELECTED
    const VhdlAssignKind m_kind;
    bool m_matching = false;

public:
    AstVhdlVarAssign(FileLine* fl, VhdlAssignKind kind, AstNodeVhdlExpr* targetp)
        : ASTGEN_SUPER_VhdlVarAssign(fl)
        , m_kind{kind} {
        this->targetp(targetp);
    }
    ASTGEN_MEMBERS_AstVhdlVarAssign;
    VhdlAssignKind kind() const { return m_kind; }
    bool matching() const { return m_matching; }
    void matching(bool flag) { m_matching = flag; }
};

class AstVhdlWait final : public AstNodeVhdlStmt {
    // wait_statement (10.2)
    // @astgen op1 := sensp : List[AstNodeVhdlExpr]  // wait on ...
    // @astgen op2 := condp : Optional[AstNodeVhdlExpr]  // wait until ...
    // @astgen op3 := timeoutp : Optional[AstNodeVhdlExpr]  // wait for ...
public:
    explicit AstVhdlWait(FileLine* fl)
        : ASTGEN_SUPER_VhdlWait(fl) {}
    ASTGEN_MEMBERS_AstVhdlWait;
};


// === AstNodeVhdlUnit ===
class AstVhdlArchitecture final : public AstNodeVhdlUnit {
    // architecture_body (3.3)
    // @astgen op2 := entityNamep : AstNodeVhdlExpr
    // @astgen op3 := declsp : List[AstNodeVhdl]
    // @astgen op4 := stmtsp : List[AstNodeVhdlStmt]
public:
    AstVhdlArchitecture(FileLine* fl, const std::string& name, AstNodeVhdlExpr* entityNamep)
        : ASTGEN_SUPER_VhdlArchitecture(fl, name) {
        this->entityNamep(entityNamep);
    }
    ASTGEN_MEMBERS_AstVhdlArchitecture;
};

class AstVhdlConfiguration final : public AstNodeVhdlUnit {
    // configuration_declaration (3.4)
    // @astgen op2 := entityNamep : AstNodeVhdlExpr
    // @astgen op3 := declsp : List[AstNodeVhdl]
    // @astgen op4 := blockConfigp : Optional[AstVhdlBlockConfig]
public:
    AstVhdlConfiguration(FileLine* fl, const std::string& name, AstNodeVhdlExpr* entityNamep)
        : ASTGEN_SUPER_VhdlConfiguration(fl, name) {
        this->entityNamep(entityNamep);
    }
    ASTGEN_MEMBERS_AstVhdlConfiguration;
};

class AstVhdlContextDecl final : public AstNodeVhdlUnit {
    // context_declaration (13.3), VHDL-2008. Its items are in op1 (contextp).
public:
    AstVhdlContextDecl(FileLine* fl, const std::string& name)
        : ASTGEN_SUPER_VhdlContextDecl(fl, name) {}
    ASTGEN_MEMBERS_AstVhdlContextDecl;
};

class AstVhdlEntity final : public AstNodeVhdlUnit {
    // entity_declaration (3.2)
    // @astgen op2 := genericsp : List[AstVhdlIfaceDecl]
    // @astgen op3 := portsp : List[AstVhdlIfaceDecl]
    // @astgen op4 := declsp : List[AstNodeVhdl]
public:
    AstVhdlEntity(FileLine* fl, const std::string& name)
        : ASTGEN_SUPER_VhdlEntity(fl, name) {}
    ASTGEN_MEMBERS_AstVhdlEntity;
};

class AstVhdlPackage final : public AstNodeVhdlUnit {
    // package_declaration (4.7), including its optional generic clause
    // @astgen op2 := genericsp : List[AstVhdlIfaceDecl]
    // @astgen op3 := genericMapp : List[AstVhdlAssoc]
    // @astgen op4 := declsp : List[AstNodeVhdl]
public:
    AstVhdlPackage(FileLine* fl, const std::string& name)
        : ASTGEN_SUPER_VhdlPackage(fl, name) {}
    ASTGEN_MEMBERS_AstVhdlPackage;
};

class AstVhdlPackageBody final : public AstNodeVhdlUnit {
    // package_body (4.8)
    // @astgen op2 := declsp : List[AstNodeVhdl]
public:
    AstVhdlPackageBody(FileLine* fl, const std::string& name)
        : ASTGEN_SUPER_VhdlPackageBody(fl, name) {}
    ASTGEN_MEMBERS_AstVhdlPackageBody;
};

class AstVhdlPackageInst final : public AstNodeVhdlUnit {
    // package_instantiation_declaration (4.9), VHDL-2008
    // @astgen op2 := uninstantiatedp : AstNodeVhdlExpr
    // @astgen op3 := genericMapp : List[AstVhdlAssoc]
public:
    AstVhdlPackageInst(FileLine* fl, const std::string& name, AstNodeVhdlExpr* uninstantiatedp)
        : ASTGEN_SUPER_VhdlPackageInst(fl, name) {
        this->uninstantiatedp(uninstantiatedp);
    }
    ASTGEN_MEMBERS_AstVhdlPackageInst;
};


#endif  // Guard
