// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Reading of VHDL files
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

#ifndef VERILATOR_V3VHDL_H_
#define VERILATOR_V3VHDL_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Error.h"

#include <set>
#include <string>

class AstNetlist;
class AstNodeVhdlUnit;
class FileLine;
class VInFilter;

//============================================================================
// Entry points into the VHDL frontend.
//
// VHDL differs from Verilog in that a file cannot be analyzed in isolation: a
// design unit may depend on packages declared in other files, and the LRM
// requires those to be analyzed first (IEEE 1076-2008 clause 13.5). So reading
// is split in two. parseFile() lexes and parses one file into the design unit
// database without resolving anything across files; analyzeAndLower() then runs
// once over the whole database, in dependency order, and emits Verilator AST.

class V3Vhdl final {
public:
    // Lex and parse one VHDL file into the design unit database
    // The libraries Verilator ships are the IEEE 2008 editions, whatever
    // revision the design that uses them is written against
    static void parseFile(FileLine* fl, const string& filename, const string& libname,
                          VInFilter* filterp, const string& errmsg) VL_MT_DISABLED;

    // True if any VHDL source has been read this run
    static bool anyRead() VL_MT_DISABLED;

    // Analyze every design unit read so far and lower it into the Verilator
    // AST under rootp. No-op when no VHDL was read.
    static void analyzeAndLower(AstNetlist* rootp, VInFilter* filterp) VL_MT_DISABLED;

private:
    // Read the packages of the libraries Verilator ships that the design asks
    // for, and whatever those need in turn
    static void loadBundledLibraries(VInFilter* filterp) VL_MT_DISABLED;
    static void gatherWanted(const AstNodeVhdlUnit* unitp, const std::string& libname,
                             std::set<std::string>& wanted) VL_MT_DISABLED;
};

#endif  // Guard
