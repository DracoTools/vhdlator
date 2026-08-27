// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL design libraries and analysis order
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

#ifndef VERILATOR_V3VHDLLIB_H_
#define VERILATOR_V3VHDLLIB_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"

#include <string>
#include <vector>

//============================================================================
// The design library, and the order in which its units may be analyzed.
//
// IEEE 1076-2008 clause 13.5 requires a design unit to be analyzed after
// everything it depends on: an architecture after its entity, a package body
// after its package, and any unit after the units its context clause names.
// Verilator reads every file before analyzing any of them, so the order is
// computed here rather than imposed on the command line.

class V3VhdlLib final {
public:
    // A design unit, its key, and what it must follow
    struct Unit final {
        AstNodeVhdlUnit* nodep = nullptr;
        std::string key;  // Unique: "lib.name", "lib.ent(arch)" or "lib.pkg%body"
        std::vector<std::string> dependsOn;  // Keys of units this must follow
        // Keys of units this must follow if they exist at all. A package
        // instantiation takes the body of what it copies, but an
        // uninstantiated package need not have one.
        std::vector<std::string> softDependsOn;
        // Libraries whose every primary unit this one must follow, which is
        // what "use work.all" asks for (12.4)
        std::vector<std::string> followsLibs;
    };

    // Register a parsed file's design units
    static void addFile(AstVhdlDesignFile* filep) VL_MT_DISABLED;

    // Compute the analysis order. Reports circular dependencies and design
    // units that are missing from the libraries given. Returns false if the
    // order could not be established.
    static bool computeOrder() VL_MT_DISABLED;

    // Units in analysis order, valid after a successful computeOrder()
    static const std::vector<AstNodeVhdlUnit*>& ordered() VL_MT_DISABLED;

    // Forget everything, releasing no nodes (the caller owns them)
    static void clear() VL_MT_DISABLED;

    // True if 'libname' is one Verilator supplies rather than one the user
    // compiled, so a missing unit in it is "not yet bundled" not "not found"
    static bool isPredefinedLib(const std::string& libname) VL_MT_DISABLED;

    static void selfTest() VL_MT_DISABLED;
};

#endif  // Guard
