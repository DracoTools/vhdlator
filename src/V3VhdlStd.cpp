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

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlStd.h"

#include "V3Global.h"

#include <string>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// State

namespace {

bool s_built = false;
VhdlScope* s_stdLibp = nullptr;
VhdlScope* s_standardp = nullptr;
VhdlScope* s_textiop = nullptr;

VhdlType* s_booleanp = nullptr;
VhdlType* s_bitp = nullptr;
VhdlType* s_characterp = nullptr;
VhdlType* s_severityp = nullptr;
VhdlType* s_integerp = nullptr;
VhdlType* s_realp = nullptr;
VhdlType* s_timep = nullptr;
VhdlType* s_delayp = nullptr;
VhdlType* s_naturalp = nullptr;
VhdlType* s_positivep = nullptr;
VhdlType* s_stringp = nullptr;
VhdlType* s_booleanVecp = nullptr;
VhdlType* s_bitVecp = nullptr;
VhdlType* s_integerVecp = nullptr;
VhdlType* s_realVecp = nullptr;
VhdlType* s_timeVecp = nullptr;
VhdlType* s_fileOpenKindp = nullptr;
VhdlType* s_fileOpenStatusp = nullptr;
VhdlType* s_univIntp = nullptr;
VhdlType* s_univRealp = nullptr;
VhdlType* s_errorp = nullptr;
VhdlType* s_linep = nullptr;
VhdlType* s_textp = nullptr;
VhdlType* s_sidep = nullptr;

FileLine* stdFileline() {
    static FileLine* s_flp = nullptr;
    if (!s_flp) s_flp = new FileLine{"<std.standard>"};
    return s_flp;
}

// Declare an enumeration type and its literals, in position order
VhdlType* makeEnum(VhdlScope* scopep, const std::string& name,
                   const std::vector<std::string>& literals) {
    VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ENUM, name, stdFileline());
    typep->setBounds(0, static_cast<int64_t>(literals.size()) - 1, true);
    VhdlSymbol* const typeSymp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, name, nullptr);
    typeSymp->typep(typep);
    typeSymp->predefined(true);
    scopep->insert(typeSymp);
    int pos = 0;
    for (const std::string& lit : literals) {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::ENUM_LITERAL, lit, nullptr);
        symp->typep(typep);
        symp->returnTypep(typep);  // An enumeration literal is a parameterless function
        symp->position(pos++);
        symp->predefined(true);
        typep->addLiteral(symp);
        scopep->insert(symp);
    }
    return typep;
}

VhdlType* makeInteger(VhdlScope* scopep, const std::string& name, int64_t lo, int64_t hi) {
    VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::INTEGER, name, stdFileline());
    typep->setBounds(lo, hi, true);
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, name, nullptr);
    symp->typep(typep);
    symp->predefined(true);
    scopep->insert(symp);
    return typep;
}

// A subtype constrains a base type but is the same type for compatibility
VhdlType* makeSubtype(VhdlScope* scopep, const std::string& name, VhdlType* basep, int64_t lo,
                      int64_t hi) {
    VhdlType* const typep = VhdlSymTab::newType(basep->kind(), name, stdFileline());
    typep->base(basep->base());
    typep->setBounds(lo, hi, true);
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, name, nullptr);
    symp->typep(typep);
    symp->predefined(true);
    scopep->insert(symp);
    return typep;
}

VhdlType* makeArray(VhdlScope* scopep, const std::string& name, VhdlType* indexp,
                    VhdlType* elemp) {
    VhdlType* const typep = VhdlSymTab::newType(VhdlTypeKind::ARRAY, name, stdFileline());
    typep->addIndexType(indexp);
    typep->elementp(elemp);
    typep->unconstrained(true);
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, name, nullptr);
    symp->typep(typep);
    symp->predefined(true);
    scopep->insert(symp);
    return typep;
}

void addObject(VhdlScope* scopep, VhdlSymKind kind, const std::string& name, VhdlType* typep) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(kind, name, nullptr);
    symp->typep(typep);
    symp->predefined(true);
    scopep->insert(symp);
}

// Declare one predefined operator or function
void addFunc(VhdlScope* scopep, const std::string& name, VhdlType* returnp,
             const std::vector<VhdlType*>& params) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::FUNCTION, name, nullptr);
    for (VhdlType* const typep : params) symp->addParamType(typep);
    symp->returnTypep(returnp);
    symp->typep(returnp);
    symp->predefined(true);
    scopep->insert(symp);
}

void addProc(VhdlScope* scopep, const std::string& name,
             const std::vector<VhdlType*>& params) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::PROCEDURE, name, nullptr);
    for (VhdlType* const typep : params) symp->addParamType(typep);
    symp->predefined(true);
    scopep->insert(symp);
}

// The 256 values of CHARACTER (16.3). The control characters have names; the
// graphic characters are character literals, whose "name" is the character.
std::vector<std::string> characterLiterals() {
    static const char* const CTRL0[32]
        = {"nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel", "bs",  "ht",  "lf",
           "vt",  "ff",  "cr",  "so",  "si",  "dle", "dc1", "dc2", "dc3", "dc4", "nak",
           "syn", "etb", "can", "em",  "sub", "esc", "fsp", "gsp", "rsp", "usp"};
    static const char* const CTRL128[32]
        = {"c128", "c129", "c130", "c131", "c132", "c133", "c134", "c135", "c136", "c137", "c138",
           "c139", "c140", "c141", "c142", "c143", "c144", "c145", "c146", "c147", "c148", "c149",
           "c150", "c151", "c152", "c153", "c154", "c155", "c156", "c157", "c158", "c159"};
    std::vector<std::string> out;
    out.reserve(256);
    for (int i = 0; i < 32; ++i) out.emplace_back(CTRL0[i]);
    for (int i = 32; i < 127; ++i) out.emplace_back(1, static_cast<char>(i));
    out.emplace_back("del");
    for (int i = 0; i < 32; ++i) out.emplace_back(CTRL128[i]);
    for (int i = 160; i < 256; ++i) out.emplace_back(1, static_cast<char>(i));
    return out;
}

}  // namespace

//######################################################################
// Accessors

VhdlScope* V3VhdlStd::standardScope() { return s_standardp; }
VhdlScope* V3VhdlStd::stdLibScope() { return s_stdLibp; }
VhdlScope* V3VhdlStd::textioScope() { return s_textiop; }
VhdlType* V3VhdlStd::booleanType() { return s_booleanp; }
VhdlType* V3VhdlStd::bitType() { return s_bitp; }
VhdlType* V3VhdlStd::characterType() { return s_characterp; }
VhdlType* V3VhdlStd::severityType() { return s_severityp; }
VhdlType* V3VhdlStd::integerType() { return s_integerp; }
VhdlType* V3VhdlStd::realType() { return s_realp; }
VhdlType* V3VhdlStd::timeType() { return s_timep; }
VhdlType* V3VhdlStd::naturalType() { return s_naturalp; }
VhdlType* V3VhdlStd::positiveType() { return s_positivep; }
VhdlType* V3VhdlStd::stringType() { return s_stringp; }
VhdlType* V3VhdlStd::bitVectorType() { return s_bitVecp; }
VhdlType* V3VhdlStd::universalIntegerType() { return s_univIntp; }
VhdlType* V3VhdlStd::universalRealType() { return s_univRealp; }
VhdlType* V3VhdlStd::errorType() { return s_errorp; }

void V3VhdlStd::clear() {
    s_built = false;
    s_stdLibp = s_standardp = s_textiop = nullptr;
    s_linep = s_textp = s_sidep = nullptr;
    s_booleanp = s_bitp = s_characterp = s_severityp = nullptr;
    s_integerp = s_realp = s_timep = s_delayp = nullptr;
    s_naturalp = s_positivep = s_stringp = nullptr;
    s_booleanVecp = s_bitVecp = s_integerVecp = s_realVecp = s_timeVecp = nullptr;
    s_fileOpenKindp = s_fileOpenStatusp = nullptr;
    s_univIntp = s_univRealp = s_errorp = nullptr;
}

//######################################################################
// Implicit operators (5.2.6, 9.2)

void V3VhdlStd::addImplicitOperators(VhdlScope* scopep, VhdlType* typep) {
    if (!typep || typep->isError()) return;
    VhdlType* const boolp = s_booleanp;
    if (!boolp) return;  // STANDARD is still being built

    // Every type has equality (5.2.6)
    addFunc(scopep, "=", boolp, {typep, typep});
    addFunc(scopep, "/=", boolp, {typep, typep});

    // Scalar and discrete-array types are ordered
    const bool ordered
        = typep->isScalar()
          || (typep->kind() == VhdlTypeKind::ARRAY && typep->dimensions() == 1
              && typep->elementp() && typep->elementp()->isDiscrete());
    if (ordered) {
        addFunc(scopep, "<", boolp, {typep, typep});
        addFunc(scopep, "<=", boolp, {typep, typep});
        addFunc(scopep, ">", boolp, {typep, typep});
        addFunc(scopep, ">=", boolp, {typep, typep});
        addFunc(scopep, "minimum", typep, {typep, typep});
        addFunc(scopep, "maximum", typep, {typep, typep});
    }

    if (typep->isNumeric()) {
        addFunc(scopep, "+", typep, {typep});  // Identity
        addFunc(scopep, "-", typep, {typep});  // Negation
        addFunc(scopep, "abs", typep, {typep});
        addFunc(scopep, "+", typep, {typep, typep});
        addFunc(scopep, "-", typep, {typep, typep});
    }
    if (typep->kind() == VhdlTypeKind::INTEGER
        || typep->kind() == VhdlTypeKind::UNIVERSAL_INTEGER) {
        addFunc(scopep, "*", typep, {typep, typep});
        addFunc(scopep, "/", typep, {typep, typep});
        addFunc(scopep, "mod", typep, {typep, typep});
        addFunc(scopep, "rem", typep, {typep, typep});
        addFunc(scopep, "**", typep, {typep, s_integerp ? s_integerp : typep});
    } else if (typep->kind() == VhdlTypeKind::FLOATING
               || typep->kind() == VhdlTypeKind::UNIVERSAL_REAL) {
        addFunc(scopep, "*", typep, {typep, typep});
        addFunc(scopep, "/", typep, {typep, typep});
        addFunc(scopep, "**", typep, {typep, s_integerp ? s_integerp : typep});
    } else if (typep->kind() == VhdlTypeKind::PHYSICAL) {
        // A physical value scales by an integer or a real (9.2.7)
        if (s_integerp) {
            addFunc(scopep, "*", typep, {typep, s_integerp});
            addFunc(scopep, "*", typep, {s_integerp, typep});
            addFunc(scopep, "/", typep, {typep, s_integerp});
        }
        if (s_realp) {
            addFunc(scopep, "*", typep, {typep, s_realp});
            addFunc(scopep, "*", typep, {s_realp, typep});
            addFunc(scopep, "/", typep, {typep, s_realp});
        }
        if (s_univIntp) addFunc(scopep, "/", s_univIntp, {typep, typep});
    }

    // Logical operators over BOOLEAN, BIT and one-dimensional arrays of them
    const bool logical
        = (typep == s_booleanp || typep == s_bitp
           || (typep->kind() == VhdlTypeKind::ARRAY && typep->dimensions() == 1
               && typep->elementp()
               && (typep->elementp()->base() == (s_booleanp ? s_booleanp->base() : nullptr)
                   || typep->elementp()->base() == (s_bitp ? s_bitp->base() : nullptr))));
    if (logical) {
        for (const char* const op : {"and", "or", "nand", "nor", "xor", "xnor"}) {
            addFunc(scopep, op, typep, {typep, typep});
        }
        addFunc(scopep, "not", typep, {typep});
        if (typep->kind() == VhdlTypeKind::ARRAY) {
            // Reduction operators and shifts, VHDL-2008 9.2.8 and 9.2.4
            VhdlType* const elemp = typep->elementp();
            for (const char* const op : {"and", "or", "nand", "nor", "xor", "xnor"}) {
                addFunc(scopep, op, elemp, {typep});
            }
            for (const char* const op : {"sll", "srl", "sla", "sra", "rol", "ror"}) {
                addFunc(scopep, op, typep, {typep, s_integerp ? s_integerp : typep});
            }
            addFunc(scopep, "and", typep, {typep, elemp});
            addFunc(scopep, "and", typep, {elemp, typep});
        }
    }

    // Concatenation, in all four forms (9.2.5)
    if (typep->kind() == VhdlTypeKind::ARRAY && typep->dimensions() == 1) {
        VhdlType* const elemp = typep->elementp();
        addFunc(scopep, "&", typep, {typep, typep});
        addFunc(scopep, "&", typep, {typep, elemp});
        addFunc(scopep, "&", typep, {elemp, typep});
        addFunc(scopep, "&", typep, {elemp, elemp});
    }

    // The condition operator, for anything that converts to a condition (9.2.9)
    if (typep == s_bitp || typep == s_booleanp) {
        addFunc(scopep, "??", boolp, {typep});
    }
}

//######################################################################
// build()

void V3VhdlStd::build() {
    if (s_built) return;
    s_built = true;

    s_stdLibp = VhdlSymTab::newScope(nullptr, "std");
    s_standardp = VhdlSymTab::newScope(nullptr, "standard");
    s_stdLibp->addNamed("standard", s_standardp);

    s_errorp = VhdlSymTab::newType(VhdlTypeKind::ERROR, "", stdFileline());

    // The anonymous universal types of literals (9.3.2)
    s_univIntp
        = VhdlSymTab::newType(VhdlTypeKind::UNIVERSAL_INTEGER, "universal_integer", stdFileline());
    s_univRealp
        = VhdlSymTab::newType(VhdlTypeKind::UNIVERSAL_REAL, "universal_real", stdFileline());

    // Order matters: BOOLEAN first, since the implicit operators of every
    // other type return it
    s_booleanp = makeEnum(s_standardp, "boolean", {"false", "true"});
    s_bitp = makeEnum(s_standardp, "bit", {"0", "1"});
    s_characterp = makeEnum(s_standardp, "character", characterLiterals());
    s_severityp
        = makeEnum(s_standardp, "severity_level", {"note", "warning", "error", "failure"});

    s_integerp = makeInteger(s_standardp, "integer", -2147483648LL, 2147483647LL);
    s_realp = VhdlSymTab::newType(VhdlTypeKind::FLOATING, "real", stdFileline());
    s_realp->setRealBounds(-1.7976931348623157e308, 1.7976931348623157e308, true);
    {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, "real", nullptr);
        symp->typep(s_realp);
        symp->predefined(true);
        s_standardp->insert(symp);
    }

    // TIME, counted in its primary unit of femtoseconds (16.3)
    s_timep = VhdlSymTab::newType(VhdlTypeKind::PHYSICAL, "time", stdFileline());
    s_timep->setBounds(INT64_MIN, INT64_MAX, true);
    {
        VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::TYPE, "time", nullptr);
        symp->typep(s_timep);
        symp->predefined(true);
        s_standardp->insert(symp);
    }
    static const struct {
        const char* namep;
        int64_t value;
    } UNITS[] = {{"fs", 1},
                 {"ps", 1000LL},
                 {"ns", 1000000LL},
                 {"us", 1000000000LL},
                 {"ms", 1000000000000LL},
                 {"sec", 1000000000000000LL},
                 {"min", 60000000000000000LL},
                 {"hr", 3600000000000000000LL}};
    for (const auto& unit : UNITS) {
        s_timep->addUnit(unit.namep, unit.value);
        VhdlSymbol* const symp
            = VhdlSymTab::newSymbol(VhdlSymKind::PHYSICAL_UNIT, unit.namep, nullptr);
        symp->typep(s_timep);
        symp->position(static_cast<int>(unit.value));
        symp->predefined(true);
        s_standardp->insert(symp);
    }
    s_delayp = makeSubtype(s_standardp, "delay_length", s_timep, 0, INT64_MAX);

    s_naturalp = makeSubtype(s_standardp, "natural", s_integerp, 0, 2147483647LL);
    s_positivep = makeSubtype(s_standardp, "positive", s_integerp, 1, 2147483647LL);

    s_stringp = makeArray(s_standardp, "string", s_positivep, s_characterp);
    s_booleanVecp = makeArray(s_standardp, "boolean_vector", s_naturalp, s_booleanp);
    s_bitVecp = makeArray(s_standardp, "bit_vector", s_naturalp, s_bitp);
    s_integerVecp = makeArray(s_standardp, "integer_vector", s_naturalp, s_integerp);
    s_realVecp = makeArray(s_standardp, "real_vector", s_naturalp, s_realp);
    s_timeVecp = makeArray(s_standardp, "time_vector", s_naturalp, s_timep);

    s_fileOpenKindp
        = makeEnum(s_standardp, "file_open_kind", {"read_mode", "write_mode", "append_mode"});
    s_fileOpenStatusp = makeEnum(s_standardp, "file_open_status",
                                 {"open_ok", "status_error", "name_error", "mode_error"});

    // NOW, and the 2008 conversions between TIME and REAL/INTEGER
    addFunc(s_standardp, "now", s_delayp, {});

    // Implicit operators for every predefined type
    for (VhdlType* const typep :
         {s_booleanp, s_bitp, s_characterp, s_severityp, s_integerp, s_realp, s_timep,
          s_stringp, s_booleanVecp, s_bitVecp, s_integerVecp, s_realVecp, s_timeVecp,
          s_fileOpenKindp, s_fileOpenStatusp, s_univIntp, s_univRealp}) {
        addImplicitOperators(s_standardp, typep);
    }

    // A literal's universal type must interoperate with the named types it
    // may be interpreted as (9.3.2)
    addFunc(s_standardp, "*", s_timep, {s_timep, s_univIntp});
    addFunc(s_standardp, "*", s_timep, {s_univIntp, s_timep});
    addFunc(s_standardp, "*", s_realp, {s_realp, s_univIntp});
    addFunc(s_standardp, "*", s_realp, {s_univIntp, s_realp});

    // The FOREIGN attribute (16.2), how a subprogram binds to C
    {
        VhdlSymbol* const symp
            = VhdlSymTab::newSymbol(VhdlSymKind::ATTRIBUTE, "foreign", nullptr);
        symp->typep(s_stringp);
        symp->predefined(true);
        s_standardp->insert(symp);
    }

    buildTextio();
    UINFO(4, "Built std.standard with " << s_standardp->direct().size() << " declarations");
}

//######################################################################
// Package TEXTIO (16.4)

void V3VhdlStd::buildTextio() {
    s_textiop = VhdlSymTab::newScope(nullptr, "textio");
    s_stdLibp->addNamed("textio", s_textiop);

    // type LINE is access STRING;  type TEXT is file of STRING;
    s_linep = VhdlSymTab::newType(VhdlTypeKind::ACCESS, "line", stdFileline());
    s_linep->designatedp(s_stringp);
    addObject(s_textiop, VhdlSymKind::TYPE, "line", s_linep);
    s_textp = VhdlSymTab::newType(VhdlTypeKind::FILE, "text", stdFileline());
    s_textp->designatedp(s_stringp);
    addObject(s_textiop, VhdlSymKind::TYPE, "text", s_textp);
    s_sidep = makeEnum(s_textiop, "side", {"right", "left"});
    VhdlType* const widthp = makeSubtype(s_textiop, "width", s_naturalp, 0, 2147483647LL);
    for (VhdlType* const typep : {s_linep, s_textp, s_sidep}) {
        addImplicitOperators(s_textiop, typep);
    }

    addFunc(s_textiop, "justify", s_stringp, {s_stringp, s_sidep, widthp});
    addObject(s_textiop, VhdlSymKind::FILE, "input", s_textp);
    addObject(s_textiop, VhdlSymKind::FILE, "output", s_textp);

    addProc(s_textiop, "readline", {s_textp, s_linep});
    addProc(s_textiop, "writeline", {s_textp, s_linep});
    addProc(s_textiop, "tee", {s_textp, s_linep});
    addFunc(s_textiop, "endfile", s_booleanp, {s_textp});
    addFunc(s_textiop, "endline", s_booleanp, {s_linep});

    // READ, WRITE and their VHDL-2008 radix forms, over the predefined types
    // the LRM lists (16.4). Each has a form with and without a GOOD flag.
    for (VhdlType* const typep :
         {s_bitp, s_bitVecp, s_booleanp, s_characterp, s_integerp, s_realp, s_timep,
          s_stringp}) {
        addProc(s_textiop, "read", {s_linep, typep});
        addProc(s_textiop, "read", {s_linep, typep, s_booleanp});
        addProc(s_textiop, "write", {s_linep, typep});
        addProc(s_textiop, "write", {s_linep, typep, s_sidep, widthp});
        addProc(s_textiop, "sread", {s_linep, typep, s_naturalp});
    }
    for (VhdlType* const typep : {s_bitVecp, s_stringp}) {
        for (const char* const op : {"hread", "hwrite", "oread", "owrite", "bread", "bwrite"}) {
            addProc(s_textiop, op, {s_linep, typep});
            addProc(s_textiop, op, {s_linep, typep, s_booleanp});
        }
    }
    addProc(s_textiop, "swrite", {s_linep, s_stringp});
    addProc(s_textiop, "swrite", {s_linep, s_stringp, s_sidep, widthp});
    addFunc(s_textiop, "to_string", s_stringp, {s_timep});
    addFunc(s_textiop, "to_string", s_stringp, {s_realp});
    addFunc(s_textiop, "to_string", s_stringp, {s_realp, s_naturalp});
    addFunc(s_textiop, "to_string", s_stringp, {s_realp, s_stringp});

    UINFO(4, "Built std.textio with " << s_textiop->direct().size() << " declarations");
}

void V3VhdlStd::selfTest() {
    build();
    if (!s_booleanp || s_booleanp->literals().size() != 2) {
        v3fatalSrc("std.standard BOOLEAN is malformed");
    }
    if (!s_characterp || s_characterp->literals().size() != 256) {
        v3fatalSrc("std.standard CHARACTER must have 256 literals, has "
                   << (s_characterp ? s_characterp->literals().size() : 0));
    }
    // NATURAL is a subtype of INTEGER, so the two are the same type
    if (!s_naturalp->compatible(s_integerp)) {
        v3fatalSrc("std.standard NATURAL must be compatible with INTEGER");
    }
    if (s_naturalp->low() != 0) v3fatalSrc("std.standard NATURAL must start at zero");
    // "=" must be visible for BIT
    std::vector<VhdlSymbol*> found;
    s_standardp->lookup("=", found);
    if (found.empty()) v3fatalSrc("std.standard declares no equality operator");
}
