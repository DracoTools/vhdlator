// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL static expression evaluation
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
// Folding of static expressions, per IEEE 1076-2008 clause 9.4.
//
// VHDL needs this earlier than Verilog does. An index range, an array bound,
// a case choice and a physical unit are all defined in terms of the *value*
// of an expression, so a type is not fully known until its bounds have been
// computed. Everything downstream -- bounds checking here, and the choice
// between a packed array and a dynamic one in V3VhdlLower -- depends on it.
//
// Only already-analyzed expressions can be folded: names must carry the
// declaration they denote and operators the overload that was chosen, since
// only a *predefined* operator has a meaning known without executing code.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLEVAL_H_
#define VERILATOR_V3VHDLEVAL_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3VhdlSym.h"

#include <string>

class AstNodeVhdlExpr;

//============================================================================

class V3VhdlEval final {
public:
    // Fold an analyzed expression to its value, or return an UNKNOWN value if
    // it is not static or does not fold.
    //
    // Reports no diagnostic of its own: "not static" is the right answer
    // wherever a static value is merely useful, and only the caller knows
    // whether one was required. Where the fold fails for a reason that is an
    // error in itself -- division by zero, a negative integer exponent -- the
    // reason is left in *errp for the caller to report.
    static VhdlValue eval(AstNodeVhdlExpr* nodep, std::string* errp = nullptr) VL_MT_DISABLED;

    // The value as 'IMAGE would give it (16.2.6)
    static std::string image(const VhdlValue& val) VL_MT_DISABLED;
    // The inverse, as 'VALUE would give it. Returns UNKNOWN if the text does
    // not denote a value of that type.
    static VhdlValue fromImage(const std::string& text, VhdlType* typep) VL_MT_DISABLED;

    // Whether a value lies within a type's range. True when either the value
    // or the range is unknown, so that only real violations are reported.
    static bool inRange(const VhdlValue& val, const VhdlType* typep) VL_MT_DISABLED;

    static void selfTest() VL_MT_DISABLED;
};

#endif  // Guard
