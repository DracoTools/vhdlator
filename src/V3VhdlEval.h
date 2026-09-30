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
// declaration they denote and operators the overload that was chosen. A call
// to a pure function whose actuals are static is static in its turn (9.4.3 i),
// and folds by running the body over those values.
//
//*************************************************************************

#ifndef VERILATOR_V3VHDLEVAL_H_
#define VERILATOR_V3VHDLEVAL_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3VhdlSym.h"

#include <map>
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

    // While a package's own declarations elaborate, a shared variable it
    // declared earlier holds its initial value and nothing else (14.4.2),
    // so a bound written with it -- UVVM's "(0 to
    // shared_spec_cov_config.max_requirement_lines)" -- is that value.
    // Anywhere else a shared variable's value is whatever running made it.
    static bool s_sharedInitValues;

    // Whether the back end can fold an expression to a value, which it does
    // by running what it names. Being static is not the same question: a
    // call on a pure function of the design folds, and one that builds a
    // value out of an access or a protected type -- a verification library's
    // handle -- does not, however static its arguments.
    static bool backEndFolds(const AstNode* nodep) VL_MT_DISABLED;

    // The same, for elaboration rather than for a value the emitted code is
    // built from: a body that is not pure may be run, and what it cannot work
    // out is left unknown inside an otherwise known value. What comes back
    // says how wide something is; it never stands in for the object itself.
    static VhdlValue evalElab(AstNodeVhdlExpr* nodep) VL_MT_DISABLED;
    // The same for a bound of an object's own subtype, where a number the
    // root entity's generic gives is left to the parameter it becomes
    static VhdlValue evalElabBound(AstNodeVhdlExpr* nodep) VL_MT_DISABLED;

    // The same, with the generics of an entity given the values one
    // instantiation hands them
    static VhdlValue
    evalElabWith(AstNodeVhdlExpr* nodep,
                 const std::map<const VhdlSymbol*, VhdlValue>& bindings) VL_MT_DISABLED;

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
