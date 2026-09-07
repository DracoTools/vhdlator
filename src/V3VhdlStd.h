// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: The predefined VHDL environment
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
// Package STANDARD of library STD, per IEEE 1076-2008 clause 16.3, together
// with the operators the language implicitly declares for every type it
// defines (5.2 - 5.5, 9.2).
//
// Built in code rather than parsed from VHDL source, as GHDL and NVC also do,
// because parts of it cannot be written in VHDL: the anonymous universal
// types of 9.3.2, and the implicit operators that every type declaration
// brings with it.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLSTD_H_
#define VERILATOR_V3VHDLSTD_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3VhdlSym.h"

//============================================================================

class V3VhdlStd final {
public:
    // Build STD.STANDARD, once per run. Idempotent.
    static void build() VL_MT_DISABLED;

    // The scope holding STANDARD's declarations
    static VhdlScope* standardScope() VL_MT_DISABLED;
    // The scope of library STD, in which STANDARD is named
    static VhdlScope* stdLibScope() VL_MT_DISABLED;
    static VhdlScope* textioScope() VL_MT_DISABLED;

    // The predefined types, for the analyzer to reach for directly
    static VhdlType* booleanType() VL_MT_DISABLED;
    static VhdlType* bitType() VL_MT_DISABLED;
    static VhdlType* characterType() VL_MT_DISABLED;
    static VhdlType* severityType() VL_MT_DISABLED;
    static VhdlType* integerType() VL_MT_DISABLED;
    static VhdlType* realType() VL_MT_DISABLED;
    static VhdlType* timeType() VL_MT_DISABLED;
    static VhdlType* naturalType() VL_MT_DISABLED;
    static VhdlType* positiveType() VL_MT_DISABLED;
    static VhdlType* stringType() VL_MT_DISABLED;
    static VhdlType* bitVectorType() VL_MT_DISABLED;
    static VhdlType* universalIntegerType() VL_MT_DISABLED;
    static VhdlType* universalRealType() VL_MT_DISABLED;
    static VhdlType* errorType() VL_MT_DISABLED;

    // Whether this subprogram is emitted as a native operation rather than
    // compiled. The frontend recognises the arithmetic and the text of the
    // IEEE packages instead of running them, so those bodies are never used
    // -- and specializing them per call site would be wasted work that also
    // renames them out of recognition.
    //
    // The name alone does not say which: RESIZE, MINIMUM and WRITE are
    // ordinary declarations in IEEE's fixed-point and floating-point
    // packages, and in any design that writes its own. Only a declaration
    // that came from one of the packages the frontend stands in for is one.
    static bool isIntrinsic(const VhdlSymbol* symp) VL_MT_DISABLED;

    // Declare the operators a type declaration implicitly brings with it
    // (5.2.6, 9.2). Called for every user-defined type as well.
    static void addImplicitOperators(VhdlScope* scopep, VhdlType* typep) VL_MT_DISABLED;

    static void clear() VL_MT_DISABLED;

private:
    // Whether the name is one of those the frontend stands in for
    static bool intrinsicName(const std::string& name) VL_MT_DISABLED;
    static void buildTextio() VL_MT_DISABLED;
    static void buildEnv() VL_MT_DISABLED;

public:
    static void selfTest() VL_MT_DISABLED;
};

#endif  // Guard
