// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL lowering into Verilator's AST
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
// Turns the analyzed VHDL tree into the AST the rest of Verilator works on.
//
// After this pass no AstNodeVhdl remains: an entity and its architecture have
// become an AstModule, a process an AstAlways, a signal assignment an
// AstAssignDly, and so on. Everything downstream -- V3LinkDot, V3Param,
// V3Width, V3Sched, V3EmitC -- then treats the design exactly as it treats
// one written in Verilog, which is the whole point of the approach.
//
// Two things here decide whether the result is fast.
//
// A process that tests a clock edge must become an edge-triggered AstAlways
// rather than a level-sensitive one, or every register in the design would be
// re-evaluated on every signal change.
//
// And the arithmetic of ieee.numeric_std must become native operations rather
// than calls into compiled VHDL. GHDL and NVC compile those packages as
// ordinary code; recognising them as intrinsics is what lets a VHDL adder
// emit the same C++ as a Verilog one.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLLOWER_H_
#define VERILATOR_V3VHDLLOWER_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"

#include <vector>

//============================================================================

class V3VhdlLower final {
public:
    // Lower every analyzed design unit into 'rootp'. Diagnostics are reported
    // as they are found; returns false if any were.
    static bool lower(const std::vector<AstNodeVhdlUnit*>& unitsp,
                      AstNetlist* rootp) VL_MT_DISABLED;
};

#endif  // Guard
