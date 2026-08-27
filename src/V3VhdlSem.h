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
// Turns the VHDL parse tree into something with meaning: every declaration
// gets a symbol, every type mark a type, every name the declaration it
// denotes, and every expression a type.
//
// Runs over the design units in the order V3VhdlLib worked out, so that a
// package is always analyzed before whatever uses it.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLSEM_H_
#define VERILATOR_V3VHDLSEM_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"

#include <vector>

//============================================================================

class V3VhdlSem final {
public:
    // Analyze every design unit, in the given order. Diagnostics are reported
    // as they are found; returns false if any were.
    static bool analyze(const std::vector<AstNodeVhdlUnit*>& unitsp) VL_MT_DISABLED;

    // Release the symbol table
    static void clear() VL_MT_DISABLED;
    // Entity and architecture copies made for a generic type, which lowering
    // has to build alongside the units that were ordered before analysis
    static void addSpecializedUnit(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    static const std::vector<AstNodeVhdlUnit*>& specializedUnits() VL_MT_DISABLED;
};

#endif  // Guard
