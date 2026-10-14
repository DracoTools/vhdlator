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
VhdlScope* s_envp = nullptr;

// What is declared while one of these is alive is a VHDL-2008 addition to the
// predefined environment, which a unit written in VHDL-93 does not see
class Mark2008 final {
    const bool m_saved;

public:
    Mark2008()
        : m_saved{VhdlScope::mark2008()} {
        VhdlScope::mark2008(true);
    }
    ~Mark2008() { VhdlScope::mark2008(m_saved); }
    VL_UNCOPYABLE(Mark2008);
};

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

// One formal of a predefined subprogram. The name matters because VHDL lets
// any call use named association, and IEEE's own bodies do: NUMERIC_BIT calls
// READ (L => L, VALUE => ivalue, GOOD => GOOD).
struct Formal final {
    const char* namep;
    VhdlType* typep;
    bool hasDefault = false;
};

// The LRM names an operator's operands L and R (9.2)
const char* operandName(size_t i, size_t count) {
    if (count == 1) return "l";
    static const char* const NAMES[] = {"l", "r", "p3", "p4"};
    return i < 4 ? NAMES[i] : "p";
}

// Declare one predefined operator or function
// A character type is an enumeration whose literals are all character
// literals, which is what makes an array of it printable (5.2.2.1)
bool isCharacterType(const VhdlType* typep) {
    if (!typep) return false;
    const VhdlType* const basep = typep->base();
    if (basep->kind() != VhdlTypeKind::ENUM || basep->literals().empty()) return false;
    for (const VhdlSymbol* const litp : basep->literals()) {
        if (litp->name().empty() || litp->name()[0] != '\'') return false;
    }
    return true;
}

// STD_ULOGIC, recognised by its shape rather than by its name: the nine
// character literals 'U' 'X' '0' '1' 'Z' 'W' 'L' 'H' '-' in that order.
// The type is declared in STD_LOGIC_1164, which STANDARD cannot name, and
// a design may declare an equivalent of its own.
bool isNineValueLogic(const VhdlType* typep) {
    if (!typep) return false;
    const VhdlType* const basep = typep->base();
    if (basep->kind() != VhdlTypeKind::ENUM) return false;
    const std::vector<VhdlSymbol*>& lits = basep->literals();
    if (lits.size() != 9) return false;
    static const char* const NINE = "UX01ZWLH-";
    for (size_t i = 0; i < lits.size(); ++i) {
        const std::string& name = lits[i]->name();
        if (name.size() != 3 || name[1] != NINE[i]) return false;
    }
    return true;
}

void addFunc(VhdlScope* scopep, const std::string& name, VhdlType* returnp,
             const std::vector<VhdlType*>& params) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::FUNCTION, name, nullptr);
    for (size_t i = 0; i < params.size(); ++i) {
        symp->addParam(operandName(i, params.size()), params[i], false);
    }
    symp->returnTypep(returnp);
    symp->typep(returnp);
    symp->predefined(true);
    scopep->insert(symp);
}

// The same, for subprograms whose formals have names of their own
void addFuncN(VhdlScope* scopep, const std::string& name, VhdlType* returnp,
              const std::vector<Formal>& params) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::FUNCTION, name, nullptr);
    for (const Formal& f : params) symp->addParam(f.namep, f.typep, f.hasDefault);
    symp->returnTypep(returnp);
    symp->typep(returnp);
    symp->predefined(true);
    scopep->insert(symp);
}

void addProcN(VhdlScope* scopep, const std::string& name, const std::vector<Formal>& params) {
    VhdlSymbol* const symp = VhdlSymTab::newSymbol(VhdlSymKind::PROCEDURE, name, nullptr);
    for (const Formal& f : params) symp->addParam(f.namep, f.typep, f.hasDefault);
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
    for (int i = 32; i < 127; ++i) {
        out.emplace_back(vhdlCharLitName(std::string(1, static_cast<char>(i))));
    }
    out.emplace_back("del");
    for (int i = 0; i < 32; ++i) out.emplace_back(CTRL128[i]);
    for (int i = 160; i < 256; ++i) {
        out.emplace_back(vhdlCharLitName(std::string(1, static_cast<char>(i))));
    }
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
    s_envp = nullptr;
    s_booleanp = s_bitp = s_characterp = s_severityp = nullptr;
    s_integerp = s_realp = s_timep = s_delayp = nullptr;
    s_naturalp = s_positivep = s_stringp = nullptr;
    s_booleanVecp = s_bitVecp = s_integerVecp = s_realVecp = s_timeVecp = nullptr;
    s_fileOpenKindp = s_fileOpenStatusp = nullptr;
    s_univIntp = s_univRealp = s_errorp = nullptr;
}

//######################################################################
// Implicit operators (5.2.6, 9.2)

// Whether a type is STD_ULOGIC's shape, or an array of it
static bool stdUlogicShaped(const VhdlType* typep) {
    if (!typep) return false;
    const VhdlType* basep = typep->base();
    if (basep->kind() == VhdlTypeKind::ARRAY && basep->elementp()) {
        basep = basep->elementp()->base();
    }
    if (basep->kind() != VhdlTypeKind::ENUM || basep->literals().size() != 9) return false;
    static const char* const nine = "UX01ZWLH-";
    for (size_t i = 0; i < 9; ++i) {
        const std::string& name = basep->literals()[i]->name();
        if (name.size() != 3 || name[1] != nine[i]) return false;
    }
    return true;
}

bool V3VhdlStd::isIntrinsic(const VhdlSymbol* symp) {
    if (!symp) return false;
    if (!intrinsicName(symp->name())) return false;
    // One the language itself declares has no body that could be compiled
    if (symp->predefined()) return true;
    // Under --vhdl-metavalues a subprogram over the nine logic values is
    // compiled from its body, which says what each of them gives: the native
    // forms hold a logic value as one bit. An edge test is the exception: it
    // asks about a signal's history, which a body cannot do through a
    // parameter, and the lowering answers it over the nine values itself.
    if (v3Global.opt.vhdlMetavalues() && symp->name() != "rising_edge"
        && symp->name() != "falling_edge") {
        bool nine = stdUlogicShaped(symp->returnTypep());
        for (const VhdlType* const typep : symp->paramTypes()) {
            if (stdUlogicShaped(typep)) nine = true;
        }
        if (nine) return false;
    }
    // The packages the frontend stands in for. A subprogram declared anywhere
    // else is the design's own however it is named: IEEE's float_generic_pkg
    // declares a RESIZE that converts between floating point formats, which
    // has nothing to do with numeric_std's.
    static const std::vector<std::string> PKGS{
        "numeric_std",          "numeric_bit",      "numeric_std_unsigned",
        "numeric_bit_unsigned", "std_logic_1164",   "std_logic_arith",
        "std_logic_unsigned",   "std_logic_signed", "std_logic_misc",
        "std_logic_textio",     "textio",           "env",
        "standard"};
    for (const VhdlScope* scopep = symp->declScopep(); scopep; scopep = scopep->parentp()) {
        for (const std::string& candidate : PKGS) {
            if (candidate == scopep->name()) return true;
        }
    }
    return false;
}

bool V3VhdlStd::intrinsicName(const std::string& name) {
    static const std::vector<std::string> NAMES{
        "to_integer",     "to_unsigned",   "to_signed",       "resize",
        "shift_left",     "shift_right",   "rotate_left",     "rotate_right",
        "and_reduce",     "or_reduce",     "xor_reduce",      "nand_reduce",
        "nor_reduce",     "xnor_reduce",   "maximum",         "minimum",
        "to_stdlogicvector", "to_stdulogicvector", "to_bitvector", "to_bit",
        "to_stdulogic",   "to_stdlogic",   "to_x01",          "to_01",
        "to_ux01",        "rising_edge",   "falling_edge",    "now",
        "write",          "writeline",     "readline",        "read",
        "hwrite",         "owrite",        "bwrite",          "swrite",
        "hread",          "oread",         "bread",           "tee",
        "file_open",      "file_close",    "flush",           "endfile",
        "deallocate",     "stop",          "finish",
        "to_hstring",     "to_ostring",    "to_bstring",      "to_binarystring",
        "to_hex_string",  "to_octal_string", "to_binary_string"};
    for (const std::string& candidate : NAMES) {
        if (candidate == name) return true;
    }
    return false;
}

void V3VhdlStd::addImplicitOperators(VhdlScope* scopep, VhdlType* typep) {
    if (!typep || typep->isError()) return;
    VhdlType* const boolp = s_booleanp;
    if (!boolp) return;  // STANDARD is still being built

    // Every type has equality (5.2.6), but for a file type and a protected
    // type, whose values are not ones to compare (5.5, 5.6)
    if (typep->kind() != VhdlTypeKind::FILE && typep->kind() != VhdlTypeKind::PROTECTED) {
        addFunc(scopep, "=", boolp, {typep, typep});
        addFunc(scopep, "/=", boolp, {typep, typep});
    }

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
        const Mark2008 mark;
        addFunc(scopep, "minimum", typep, {typep, typep});
        addFunc(scopep, "maximum", typep, {typep, typep});
    }
    // A one-dimensional array of a scalar answers for the whole of itself,
    // giving one element back (5.3.2.4). That holds for any scalar element,
    // where the ordering operators above need a discrete one.
    if (typep->kind() == VhdlTypeKind::ARRAY && typep->dimensions() == 1 && typep->elementp()
        && typep->elementp()->isScalar()) {
        const Mark2008 mark;
        addFunc(scopep, "minimum", typep->elementp(), {typep});
        addFunc(scopep, "maximum", typep->elementp(), {typep});
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
        // "mod" and "rem" over a physical type give the same type (9.2.4).
        // VHDL-2008 added them, and V3VhdlSem says so where one is used.
        addFunc(scopep, "mod", typep, {typep, typep});
        addFunc(scopep, "rem", typep, {typep, typep});
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
            // Shifts (9.2.4), and the reduction operators VHDL-2008 added (9.2.2)
            VhdlType* const elemp = typep->elementp();
            for (const char* const op : {"sll", "srl", "sla", "sra", "rol", "ror"}) {
                addFunc(scopep, op, typep, {typep, s_integerp ? s_integerp : typep});
            }
            const Mark2008 mark;
            for (const char* const op : {"and", "or", "nand", "nor", "xor", "xnor"}) {
                addFunc(scopep, op, elemp, {typep});
            }
            // Every logical operator has element/array forms, not just "and"
            for (const char* const op : {"and", "or", "nand", "nor", "xor", "xnor"}) {
                addFunc(scopep, op, typep, {typep, elemp});
                addFunc(scopep, op, typep, {elemp, typep});
            }
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

    // The matching relational operators are predefined for BIT and for
    // STD_ULOGIC and for nothing else, and each gives back that same type
    // rather than a BOOLEAN -- which is what lets "?=" chain, and what
    // STD_LOGIC_1164 relies on where it leaves its own versions commented
    // out. The two equality ones are defined for a one-dimensional array of
    // either as well, and give back the element type (9.2.3).
    {
        VhdlType* matchp = nullptr;
        bool ordering = false;
        if (typep == s_bitp || isNineValueLogic(typep)) {
            matchp = typep;
            ordering = true;
        } else if (typep->kind() == VhdlTypeKind::ARRAY && typep->dimensions() == 1
                   && typep->elementp()
                   && (typep->elementp()->base() == (s_bitp ? s_bitp->base() : nullptr)
                       || isNineValueLogic(typep->elementp()))) {
            matchp = typep->elementp();
        }
        if (matchp) {
            const Mark2008 mark;
            addFunc(scopep, "?=", matchp, {typep, typep});
            addFunc(scopep, "?/=", matchp, {typep, typep});
            if (ordering) {
                for (const char* const op : {"?<", "?<=", "?>", "?>="}) {
                    addFunc(scopep, op, matchp, {typep, typep});
                }
            }
        }
    }

    // The condition operator, for anything that converts to a condition (9.2.9)
    if (typep == s_bitp || typep == s_booleanp) {
        const Mark2008 mark;
        addFunc(scopep, "??", boolp, {typep});
    }

    // TO_STRING is implicitly declared for a scalar type, and for a
    // one-dimensional array whose element type is a character type with only
    // character literals (5.2.6, 5.3.2.4). Not for INTEGER_VECTOR, say: a
    // design that writes its own TO_STRING for one would otherwise find both.
    if (s_stringp && typep != s_stringp
        && (typep->isScalar()
            || (typep->kind() == VhdlTypeKind::ARRAY && typep->dimensions() == 1
                && isCharacterType(typep->elementp())))) {
        const Mark2008 mark;
        addFunc(scopep, "to_string", s_stringp, {typep});
    }

    // A file type declaration implicitly declares the operations of 5.5.2.
    // They are declared here rather than written in VHDL because their
    // parameter is of the file type itself, which VHDL cannot express.
    if (typep->kind() == VhdlTypeKind::FILE && s_stringp && s_fileOpenKindp) {
        VhdlType* const valuep = typep->designatedp();
        addProcN(scopep, "file_open", {{"f", typep},
                                       {"external_name", s_stringp},
                                       {"open_kind", s_fileOpenKindp, true}});
        addProcN(scopep, "file_open", {{"status", s_fileOpenStatusp},
                                       {"f", typep},
                                       {"external_name", s_stringp},
                                       {"open_kind", s_fileOpenKindp, true}});
        addProcN(scopep, "file_close", {{"f", typep}});
        {
            const Mark2008 mark;
            addProcN(scopep, "flush", {{"f", typep}});
        }
        addFuncN(scopep, "endfile", boolp, {{"f", typep}});
        if (valuep) {
            addProcN(scopep, "read", {{"f", typep}, {"value", valuep}});
            addProcN(scopep, "write", {{"f", typep}, {"value", valuep}});
            // An unconstrained element type reads with a length out parameter
            if (valuep->kind() == VhdlTypeKind::ARRAY && s_naturalp) {
                addProcN(scopep, "read",
                         {{"f", typep}, {"value", valuep}, {"length", s_naturalp}});
            }
        }
    }

    // An access type declaration implicitly declares DEALLOCATE (5.4.2)
    if (typep->kind() == VhdlTypeKind::ACCESS) addProcN(scopep, "deallocate", {{"p", typep}});
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
    s_bitp = makeEnum(s_standardp, "bit", {vhdlCharLitName("0"), vhdlCharLitName("1")});
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
        symp->value(VhdlValue::mkInt(unit.value, s_timep));  // In femtoseconds
        symp->position(static_cast<int>(unit.value));
        symp->predefined(true);
        s_standardp->insert(symp);
    }
    s_delayp = makeSubtype(s_standardp, "delay_length", s_timep, 0, INT64_MAX);

    s_naturalp = makeSubtype(s_standardp, "natural", s_integerp, 0, 2147483647LL);
    s_positivep = makeSubtype(s_standardp, "positive", s_integerp, 1, 2147483647LL);

    s_stringp = makeArray(s_standardp, "string", s_positivep, s_characterp);
    s_bitVecp = makeArray(s_standardp, "bit_vector", s_naturalp, s_bitp);
    {
        const Mark2008 mark;
        s_booleanVecp = makeArray(s_standardp, "boolean_vector", s_naturalp, s_booleanp);
        s_integerVecp = makeArray(s_standardp, "integer_vector", s_naturalp, s_integerp);
        s_realVecp = makeArray(s_standardp, "real_vector", s_naturalp, s_realp);
        s_timeVecp = makeArray(s_standardp, "time_vector", s_naturalp, s_timep);
    }

    s_fileOpenKindp
        = makeEnum(s_standardp, "file_open_kind", {"read_mode", "write_mode", "append_mode"});
    s_fileOpenStatusp = makeEnum(s_standardp, "file_open_status",
                                 {"open_ok", "status_error", "name_error", "mode_error"});

    // NOW, and the 2008 conversions between TIME and REAL/INTEGER
    addFunc(s_standardp, "now", s_delayp, {});
    // VHDL-2008 gives BIT and BOOLEAN their own edge functions (16.3)
    if (s_bitp && s_booleanp) {
        const Mark2008 mark;
        addFunc(s_standardp, "rising_edge", s_booleanp, {s_bitp});
        addFunc(s_standardp, "falling_edge", s_booleanp, {s_bitp});
        addFunc(s_standardp, "rising_edge", s_booleanp, {s_booleanp});
        addFunc(s_standardp, "falling_edge", s_booleanp, {s_booleanp});
    }

    // Implicit operators for every predefined type
    for (VhdlType* const typep :
         {s_booleanp, s_bitp, s_characterp, s_severityp, s_integerp, s_realp, s_timep, s_stringp,
          s_bitVecp, s_fileOpenKindp, s_fileOpenStatusp, s_univIntp, s_univRealp}) {
        addImplicitOperators(s_standardp, typep);
    }
    {
        // What VHDL-2008 added to STANDARD (16.3)
        const Mark2008 mark;
        for (VhdlType* const typep : {s_booleanVecp, s_integerVecp, s_realVecp, s_timeVecp}) {
            addImplicitOperators(s_standardp, typep);
        }

        // The radix forms that come with BIT_VECTOR, and no other type (5.3.2.4)
        for (const char* const op : {"to_bstring", "to_binary_string", "to_ostring",
                                     "to_octal_string", "to_hstring", "to_hex_string"}) {
            addFuncN(s_standardp, op, s_stringp, {{"value", s_bitVecp}});
        }

        // The overloaded TO_STRING forms of 16.3, which take a second argument
        // saying how to write the value. The one-argument forms come with each
        // type, from addImplicitOperators.
        addFuncN(s_standardp, "to_string", s_stringp,
                 {{"value", s_realp}, {"digits", s_naturalp}});
        addFuncN(s_standardp, "to_string", s_stringp, {{"value", s_realp}, {"format", s_stringp}});
        addFuncN(s_standardp, "to_string", s_stringp, {{"value", s_timep}, {"unit", s_timep}});
    }

    // A literal's universal type must interoperate with the named types it
    // may be interpreted as (9.3.2)
    addFunc(s_standardp, "*", s_timep, {s_timep, s_univIntp});
    addFunc(s_standardp, "*", s_timep, {s_univIntp, s_timep});
    // A universal real may be scaled by a universal integer, which is
    // converted to universal_real for the operation (9.2.7)
    addFunc(s_standardp, "*", s_univRealp, {s_univRealp, s_univIntp});
    addFunc(s_standardp, "*", s_univRealp, {s_univIntp, s_univRealp});
    addFunc(s_standardp, "/", s_univRealp, {s_univRealp, s_univIntp});

    // The FOREIGN attribute (16.2), how a subprogram binds to C
    {
        VhdlSymbol* const symp
            = VhdlSymTab::newSymbol(VhdlSymKind::ATTRIBUTE, "foreign", nullptr);
        symp->typep(s_stringp);
        symp->predefined(true);
        s_standardp->insert(symp);
    }

    buildTextio();
    buildEnv();
    UINFO(4, "Built std.standard with " << s_standardp->direct().size() << " declarations");
}

//######################################################################
// Package TEXTIO (16.4)

// Package ENV of library STD (16.5). Built in code rather than shipped as a
// source, because its three subprograms are simulator control -- there is no
// VHDL that could implement them.
void V3VhdlStd::buildEnv() {
    s_envp = VhdlSymTab::newScope(nullptr, "env");
    s_stdLibp->addNamed("env", s_envp);
    addProcN(s_envp, "stop", {});
    addProcN(s_envp, "stop", {{"status", s_integerp}});
    addProcN(s_envp, "finish", {});
    addProcN(s_envp, "finish", {{"status", s_integerp}});
    addFuncN(s_envp, "resolution_limit", s_delayp ? s_delayp : s_timep, {});
    UINFO(4, "Built std.env with " << s_envp->direct().size() << " declarations");
}

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

    {
        const Mark2008 mark;
        addFuncN(s_textiop, "justify", s_stringp,
                 {{"value", s_stringp}, {"justified", s_sidep, true}, {"field", widthp, true}});
    }
    addObject(s_textiop, VhdlSymKind::FILE, "input", s_textp);
    addObject(s_textiop, VhdlSymKind::FILE, "output", s_textp);

    addProcN(s_textiop, "readline", {{"f", s_textp}, {"l", s_linep}});
    addProcN(s_textiop, "writeline", {{"f", s_textp}, {"l", s_linep}});
    {
        const Mark2008 mark;
        addProcN(s_textiop, "tee", {{"f", s_textp}, {"l", s_linep}});
    }
    // ENDFILE comes with the file type itself, so TEXT already has it (5.5.2)
    addFuncN(s_textiop, "endline", s_booleanp, {{"l", s_linep}});

    // READ, WRITE and their VHDL-2008 radix forms, over the predefined types
    // the LRM lists (16.4). Each has a form with and without a GOOD flag.
    for (VhdlType* const typep :
         {s_bitp, s_bitVecp, s_booleanp, s_characterp, s_integerp, s_realp, s_timep,
          s_stringp}) {
        addProcN(s_textiop, "read", {{"l", s_linep}, {"value", typep}});
        addProcN(s_textiop, "read", {{"l", s_linep}, {"value", typep}, {"good", s_booleanp}});
        // One declaration, not two: JUSTIFIED and FIELD have defaults, so a
        // two-argument call binds to this same procedure (16.4). REAL and
        // TIME are left out: theirs take one more parameter, and are
        // declared below rather than twice.
        if (typep != s_realp && typep != s_timep) {
            addProcN(s_textiop, "write", {{"l", s_linep},
                                          {"value", typep},
                                          {"justified", s_sidep, true},
                                          {"field", widthp, true}});
        }
        const Mark2008 mark;
        addProcN(s_textiop, "sread", {{"l", s_linep}, {"value", typep}, {"strlen", s_naturalp}});
    }
    // REAL and TIME take one more, saying how the value is to be written
    // (16.4). It has a default, so a call that leaves it out binds here too.
    addProcN(s_textiop, "write", {{"l", s_linep},
                                  {"value", s_realp},
                                  {"justified", s_sidep, true},
                                  {"field", widthp, true},
                                  {"digits", s_naturalp, true}});
    {
        const Mark2008 mark;
        addProcN(s_textiop, "write", {{"l", s_linep}, {"value", s_realp}, {"format", s_stringp}});
    }
    addProcN(s_textiop, "write", {{"l", s_linep},
                                  {"value", s_timep},
                                  {"justified", s_sidep, true},
                                  {"field", widthp, true},
                                  {"unit", s_timep, true}});
    // The radix operations are declared for BIT_VECTOR only (16.4). Declaring
    // them for STRING as well made every call with a literal argument
    // ambiguous, since a string literal fits either.
    const Mark2008 mark;
    {
        VhdlType* const typep = s_bitVecp;
        for (const char* const op : {"hread", "oread", "bread"}) {
            addProcN(s_textiop, op, {{"l", s_linep}, {"value", typep}});
            addProcN(s_textiop, op, {{"l", s_linep}, {"value", typep}, {"good", s_booleanp}});
        }
        // The write forms take an optional justification and field width
        for (const char* const op : {"hwrite", "owrite", "bwrite"}) {
            addProcN(s_textiop, op, {{"l", s_linep},
                                     {"value", typep},
                                     {"justified", s_sidep, true},
                                     {"field", widthp, true}});
        }
    }
    addProcN(s_textiop, "swrite", {{"l", s_linep},
                                   {"value", s_stringp},
                                   {"justified", s_sidep, true},
                                   {"field", widthp, true}});

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
