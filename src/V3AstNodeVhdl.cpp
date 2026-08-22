// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL parse tree node methods
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
// Out-of-line methods for the VHDL parse tree nodes declared in
// V3AstNodeVhdl.h. Mostly dump() overrides, so that --dump-tree shows the
// attributes that distinguish otherwise identical nodes.
//
//*************************************************************************

#include "V3PchAstMT.h"

#include "V3Ast.h"

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Enum spellings

const char* vhdlUnaryOpName(VhdlUnaryOp op) {
    switch (op) {
    case VhdlUnaryOp::IDENTITY: return "+";
    case VhdlUnaryOp::NEGATE: return "-";
    case VhdlUnaryOp::ABS: return "abs";
    case VhdlUnaryOp::NOT: return "not";
    case VhdlUnaryOp::AND: return "and";
    case VhdlUnaryOp::NAND: return "nand";
    case VhdlUnaryOp::OR: return "or";
    case VhdlUnaryOp::NOR: return "nor";
    case VhdlUnaryOp::XOR: return "xor";
    case VhdlUnaryOp::XNOR: return "xnor";
    case VhdlUnaryOp::COND_CONV: return "??";
    }
    return "?";  // LCOV_EXCL_LINE
}

const char* vhdlBinaryOpName(VhdlBinaryOp op) {
    switch (op) {
    case VhdlBinaryOp::AND: return "and";
    case VhdlBinaryOp::OR: return "or";
    case VhdlBinaryOp::NAND: return "nand";
    case VhdlBinaryOp::NOR: return "nor";
    case VhdlBinaryOp::XOR: return "xor";
    case VhdlBinaryOp::XNOR: return "xnor";
    case VhdlBinaryOp::EQ: return "=";
    case VhdlBinaryOp::NE: return "/=";
    case VhdlBinaryOp::LT: return "<";
    case VhdlBinaryOp::LE: return "<=";
    case VhdlBinaryOp::GT: return ">";
    case VhdlBinaryOp::GE: return ">=";
    case VhdlBinaryOp::MATCH_EQ: return "?=";
    case VhdlBinaryOp::MATCH_NE: return "?/=";
    case VhdlBinaryOp::MATCH_LT: return "?<";
    case VhdlBinaryOp::MATCH_LE: return "?<=";
    case VhdlBinaryOp::MATCH_GT: return "?>";
    case VhdlBinaryOp::MATCH_GE: return "?>=";
    case VhdlBinaryOp::SLL: return "sll";
    case VhdlBinaryOp::SRL: return "srl";
    case VhdlBinaryOp::SLA: return "sla";
    case VhdlBinaryOp::SRA: return "sra";
    case VhdlBinaryOp::ROL: return "rol";
    case VhdlBinaryOp::ROR: return "ror";
    case VhdlBinaryOp::ADD: return "+";
    case VhdlBinaryOp::SUB: return "-";
    case VhdlBinaryOp::CONCAT: return "&";
    case VhdlBinaryOp::MUL: return "*";
    case VhdlBinaryOp::DIV: return "/";
    case VhdlBinaryOp::MOD: return "mod";
    case VhdlBinaryOp::REM: return "rem";
    case VhdlBinaryOp::POW: return "**";
    }
    return "?";  // LCOV_EXCL_LINE
}

const char* vhdlModeName(VhdlMode mode) {
    switch (mode) {
    case VhdlMode::IN: return "in";
    case VhdlMode::OUT: return "out";
    case VhdlMode::INOUT: return "inout";
    case VhdlMode::BUFFER: return "buffer";
    case VhdlMode::LINKAGE: return "linkage";
    }
    return "?";  // LCOV_EXCL_LINE
}

namespace {
const char* vhdlIfaceKindName(VhdlIfaceKind kind) {
    switch (kind) {
    case VhdlIfaceKind::CONSTANT: return "constant";
    case VhdlIfaceKind::SIGNAL: return "signal";
    case VhdlIfaceKind::VARIABLE: return "variable";
    case VhdlIfaceKind::FILE: return "file";
    case VhdlIfaceKind::TYPE: return "type";
    case VhdlIfaceKind::SUBPROGRAM: return "subprogram";
    case VhdlIfaceKind::PACKAGE: return "package";
    }
    return "?";  // LCOV_EXCL_LINE
}

const char* vhdlObjKindName(VhdlObjKind kind) {
    switch (kind) {
    case VhdlObjKind::CONSTANT: return "constant";
    case VhdlObjKind::SIGNAL: return "signal";
    case VhdlObjKind::VARIABLE: return "variable";
    case VhdlObjKind::SHARED_VARIABLE: return "shared variable";
    case VhdlObjKind::FILE: return "file";
    }
    return "?";  // LCOV_EXCL_LINE
}

const char* vhdlAssignKindName(VhdlAssignKind kind) {
    switch (kind) {
    case VhdlAssignKind::SIMPLE: return "simple";
    case VhdlAssignKind::CONDITIONAL: return "conditional";
    case VhdlAssignKind::SELECTED: return "selected";
    }
    return "?";  // LCOV_EXCL_LINE
}

const char* vhdlLitKindName(VhdlLitKind kind) {
    switch (kind) {
    case VhdlLitKind::INTEGER: return "integer";
    case VhdlLitKind::REAL: return "real";
    case VhdlLitKind::PHYSICAL: return "physical";
    case VhdlLitKind::CHARACTER: return "character";
    case VhdlLitKind::STRING: return "string";
    case VhdlLitKind::BITSTRING: return "bitstring";
    case VhdlLitKind::NULLLIT: return "null";
    }
    return "?";  // LCOV_EXCL_LINE
}
}  // namespace

//######################################################################
// dump() overrides

void AstVhdlContextItem::dump(std::ostream& str) const {
    this->AstNodeVhdl::dump(str);
    switch (m_kind) {
    case VhdlContextKind::LIBRARY: str << " [library]"; break;
    case VhdlContextKind::USE: str << " [use]"; break;
    case VhdlContextKind::CONTEXT_REF: str << " [context]"; break;
    }
}

void AstVhdlRange::dump(std::ostream& str) const {
    this->AstNodeVhdl::dump(str);
    if (attrp()) {
        str << " [attribute]";
    } else {
        str << " [" << (m_dir == VhdlDir::TO ? "to" : "downto") << "]";
    }
}

void AstVhdlIfaceDecl::dump(std::ostream& str) const {
    this->AstNodeVhdlDecl::dump(str);
    str << " [" << vhdlIfaceKindName(m_kind) << " " << vhdlModeName(m_mode);
    if (!m_modeExplicit) str << "(implicit)";
    str << "]";
}

void AstVhdlObjectDecl::dump(std::ostream& str) const {
    this->AstNodeVhdlDecl::dump(str);
    str << " [" << vhdlObjKindName(m_kind);
    if (m_signalKind == VhdlSignalKind::REGISTER) str << " register";
    if (m_signalKind == VhdlSignalKind::BUS) str << " bus";
    str << "]";
}

void AstVhdlSubprogDecl::dump(std::ostream& str) const {
    this->AstNodeVhdlDecl::dump(str);
    str << " [" << (m_isFunction ? "function" : "procedure");
    if (m_isFunction && !m_isPure) str << " impure";
    if (m_hasBody) str << " body";
    if (m_isOperator) str << " operator";
    str << "]";
}

void AstVhdlProcess::dump(std::ostream& str) const {
    this->AstNodeVhdlStmt::dump(str);
    str << " [";
    if (m_postponed) str << "postponed ";
    if (m_sensAll) {
        str << "sens=all";
    } else if (m_hasSensList) {
        str << "sens=list";
    } else {
        str << "sens=none";
    }
    str << "]";
}

void AstVhdlSigAssign::dump(std::ostream& str) const {
    this->AstNodeVhdlStmt::dump(str);
    str << " [" << vhdlAssignKindName(m_kind);
    switch (m_delay) {
    case VhdlDelayKind::TRANSPORT: str << " transport"; break;
    case VhdlDelayKind::INERTIAL: str << " inertial"; break;
    case VhdlDelayKind::REJECT_INERTIAL: str << " reject"; break;
    case VhdlDelayKind::NONE: break;
    }
    if (m_guarded) str << " guarded";
    if (m_postponed) str << " postponed";
    if (m_matching) str << " matching";
    if (m_force) str << " force";
    if (m_release) str << " release";
    str << "]";
}

void AstVhdlLiteral::dump(std::ostream& str) const {
    this->AstNodeVhdlExpr::dump(str);
    str << " [" << vhdlLitKindName(m_kind);
    if (m_kind == VhdlLitKind::BITSTRING && m_baseSpec) {
        str << " " << (m_signedBitStr ? "S" : "") << m_baseSpec;
    }
    str << "]";
}

void AstVhdlUnary::dump(std::ostream& str) const {
    this->AstNodeVhdlExpr::dump(str);
    str << " [" << vhdlUnaryOpName(m_op) << "]";
}

void AstVhdlBinary::dump(std::ostream& str) const {
    this->AstNodeVhdlExpr::dump(str);
    str << " [" << vhdlBinaryOpName(m_op) << "]";
}
