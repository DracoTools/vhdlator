// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL lexical scanner
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
// Lexical scanner for VHDL, implementing IEEE 1076-2008 clause 15.
//
// Hand written rather than generated, for two reasons. First, the apostrophe
// is genuinely ambiguous between a character literal and an attribute tick and
// is resolved from the preceding token (see tickIsAttribute), which a
// generated scanner expresses awkwardly. Second, a bit string literal is an
// optional integer, then a base specifier that also looks like an identifier,
// then a string -- again easier to decide by hand.
//
// The scanner interprets only what clause 15 requires. It does not build
// V3Numbers and knows nothing of types or widths; that is the parser's and the
// analyzer's business.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlScan.h"

#include "V3Global.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <unordered_map>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Character classification, per 15.2 (ISO/IEC 8859-1)

namespace {

bool isBasicLetter(unsigned char c) VL_PURE {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
// Latin-1 letters. 0xD7 (multiplication) and 0xF7 (division) are not letters.
bool isLatin1Letter(unsigned char c) VL_PURE {
    return c >= 0xC0 && c != 0xD7 && c != 0xF7;
}
bool isLetter(unsigned char c) VL_PURE { return isBasicLetter(c) || isLatin1Letter(c); }
bool isDigitChar(unsigned char c) VL_PURE { return c >= '0' && c <= '9'; }
bool isLetterOrDigit(unsigned char c) VL_PURE { return isLetter(c) || isDigitChar(c); }
// Graphic characters: basic and Latin-1 printables
bool isGraphic(unsigned char c) VL_PURE {
    return (c >= 0x20 && c <= 0x7E) || (c >= 0xA0);
}
// A format effector other than horizontal tab ends a line (15.3)
bool isLineEnd(unsigned char c) VL_PURE {
    return c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

char lowerChar(unsigned char c) VL_PURE {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    // Latin-1 uppercase to lowercase; 0xD7 is multiplication, not a letter
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return static_cast<char>(c + 0x20);
    return static_cast<char>(c);
}

std::string lowerString(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) out += lowerChar(static_cast<unsigned char>(c));
    return out;
}

// Value of an extended digit, or -1
int extendedDigitValue(unsigned char c) VL_PURE {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Decode a base specifier (15.8). Returns false if not one.
bool parseBaseSpecifier(const std::string& word, char* baseOutp, bool* signedOutp) {
    const std::string up = [&]() {
        std::string s;
        for (const char c : word) {
            const unsigned char u = static_cast<unsigned char>(c);
            s += (u >= 'a' && u <= 'z') ? static_cast<char>(u - 'a' + 'A') : c;
        }
        return s;
    }();
    if (up == "B" || up == "O" || up == "X" || up == "D") {
        *baseOutp = up[0];
        *signedOutp = false;
        return true;
    }
    if (up.size() == 2 && (up[0] == 'U' || up[0] == 'S')
        && (up[1] == 'B' || up[1] == 'O' || up[1] == 'X')) {
        *baseOutp = up[1];
        *signedOutp = (up[0] == 'S');
        return true;
    }
    return false;
}

// Convert a decimal digit string of any length to binary, per 15.8's D
// specifier. Repeated division keeps this exact for values wider than 64 bits.
std::string decimalToBinary(const std::string& digits) {
    std::string value = digits;
    // Strip leading zeros, but keep one digit
    size_t firstNonZero = value.find_first_not_of('0');
    if (firstNonZero == std::string::npos) return "0";
    value.erase(0, firstNonZero);

    std::string bits;
    while (!(value.size() == 1 && value[0] == '0')) {
        // Divide 'value' by two, recording the remainder as the next bit
        std::string quotient;
        int carry = 0;
        for (const char c : value) {
            const int cur = carry * 10 + (c - '0');
            const int q = cur / 2;
            carry = cur % 2;
            if (!quotient.empty() || q != 0) quotient += static_cast<char>('0' + q);
        }
        bits += static_cast<char>('0' + carry);
        value = quotient.empty() ? "0" : quotient;
    }
    // Bits were produced least significant first
    return std::string{bits.rbegin(), bits.rend()};
}

//######################################################################
// Reserved words (15.10), with the revision that first reserved each

struct VhdlKeyword final {
    const char* wordp;
    VhdlTokType tok;
    V3LangCode::en sinceRev;  // Not reserved before this revision
};

// Words added by VHDL-2008 are not reserved in 1993/2000/2002, so legacy code
// may still use e.g. "context" or "parameter" as an ordinary identifier.
const VhdlKeyword VHDL_KEYWORDS[] = {
    {"abs", VhdlTokType::KWD_ABS, V3LangCode::L1076_1993},
    {"access", VhdlTokType::KWD_ACCESS, V3LangCode::L1076_1993},
    {"after", VhdlTokType::KWD_AFTER, V3LangCode::L1076_1993},
    {"alias", VhdlTokType::KWD_ALIAS, V3LangCode::L1076_1993},
    {"all", VhdlTokType::KWD_ALL, V3LangCode::L1076_1993},
    {"and", VhdlTokType::KWD_AND, V3LangCode::L1076_1993},
    {"architecture", VhdlTokType::KWD_ARCHITECTURE, V3LangCode::L1076_1993},
    {"array", VhdlTokType::KWD_ARRAY, V3LangCode::L1076_1993},
    {"assert", VhdlTokType::KWD_ASSERT, V3LangCode::L1076_1993},
    {"assume", VhdlTokType::KWD_ASSUME, V3LangCode::L1076_2008},
    {"assume_guarantee", VhdlTokType::KWD_ASSUME_GUARANTEE, V3LangCode::L1076_2008},
    {"attribute", VhdlTokType::KWD_ATTRIBUTE, V3LangCode::L1076_1993},
    {"begin", VhdlTokType::KWD_BEGIN, V3LangCode::L1076_1993},
    {"block", VhdlTokType::KWD_BLOCK, V3LangCode::L1076_1993},
    {"body", VhdlTokType::KWD_BODY, V3LangCode::L1076_1993},
    {"buffer", VhdlTokType::KWD_BUFFER, V3LangCode::L1076_1993},
    {"bus", VhdlTokType::KWD_BUS, V3LangCode::L1076_1993},
    {"case", VhdlTokType::KWD_CASE, V3LangCode::L1076_1993},
    {"component", VhdlTokType::KWD_COMPONENT, V3LangCode::L1076_1993},
    {"configuration", VhdlTokType::KWD_CONFIGURATION, V3LangCode::L1076_1993},
    {"constant", VhdlTokType::KWD_CONSTANT, V3LangCode::L1076_1993},
    {"context", VhdlTokType::KWD_CONTEXT, V3LangCode::L1076_2008},
    {"cover", VhdlTokType::KWD_COVER, V3LangCode::L1076_2008},
    {"default", VhdlTokType::KWD_DEFAULT, V3LangCode::L1076_2008},
    {"disconnect", VhdlTokType::KWD_DISCONNECT, V3LangCode::L1076_1993},
    {"downto", VhdlTokType::KWD_DOWNTO, V3LangCode::L1076_1993},
    {"else", VhdlTokType::KWD_ELSE, V3LangCode::L1076_1993},
    {"elsif", VhdlTokType::KWD_ELSIF, V3LangCode::L1076_1993},
    {"end", VhdlTokType::KWD_END, V3LangCode::L1076_1993},
    {"entity", VhdlTokType::KWD_ENTITY, V3LangCode::L1076_1993},
    {"exit", VhdlTokType::KWD_EXIT, V3LangCode::L1076_1993},
    {"fairness", VhdlTokType::KWD_FAIRNESS, V3LangCode::L1076_2008},
    {"file", VhdlTokType::KWD_FILE, V3LangCode::L1076_1993},
    {"for", VhdlTokType::KWD_FOR, V3LangCode::L1076_1993},
    {"force", VhdlTokType::KWD_FORCE, V3LangCode::L1076_2008},
    {"function", VhdlTokType::KWD_FUNCTION, V3LangCode::L1076_1993},
    {"generate", VhdlTokType::KWD_GENERATE, V3LangCode::L1076_1993},
    {"generic", VhdlTokType::KWD_GENERIC, V3LangCode::L1076_1993},
    {"group", VhdlTokType::KWD_GROUP, V3LangCode::L1076_1993},
    {"guarded", VhdlTokType::KWD_GUARDED, V3LangCode::L1076_1993},
    {"if", VhdlTokType::KWD_IF, V3LangCode::L1076_1993},
    {"impure", VhdlTokType::KWD_IMPURE, V3LangCode::L1076_1993},
    {"in", VhdlTokType::KWD_IN, V3LangCode::L1076_1993},
    {"inertial", VhdlTokType::KWD_INERTIAL, V3LangCode::L1076_1993},
    {"inout", VhdlTokType::KWD_INOUT, V3LangCode::L1076_1993},
    {"is", VhdlTokType::KWD_IS, V3LangCode::L1076_1993},
    {"label", VhdlTokType::KWD_LABEL, V3LangCode::L1076_1993},
    {"library", VhdlTokType::KWD_LIBRARY, V3LangCode::L1076_1993},
    {"linkage", VhdlTokType::KWD_LINKAGE, V3LangCode::L1076_1993},
    {"literal", VhdlTokType::KWD_LITERAL, V3LangCode::L1076_1993},
    {"loop", VhdlTokType::KWD_LOOP, V3LangCode::L1076_1993},
    {"map", VhdlTokType::KWD_MAP, V3LangCode::L1076_1993},
    {"mod", VhdlTokType::KWD_MOD, V3LangCode::L1076_1993},
    {"nand", VhdlTokType::KWD_NAND, V3LangCode::L1076_1993},
    {"new", VhdlTokType::KWD_NEW, V3LangCode::L1076_1993},
    {"next", VhdlTokType::KWD_NEXT, V3LangCode::L1076_1993},
    {"nor", VhdlTokType::KWD_NOR, V3LangCode::L1076_1993},
    {"not", VhdlTokType::KWD_NOT, V3LangCode::L1076_1993},
    {"null", VhdlTokType::KWD_NULL, V3LangCode::L1076_1993},
    {"of", VhdlTokType::KWD_OF, V3LangCode::L1076_1993},
    {"on", VhdlTokType::KWD_ON, V3LangCode::L1076_1993},
    {"open", VhdlTokType::KWD_OPEN, V3LangCode::L1076_1993},
    {"or", VhdlTokType::KWD_OR, V3LangCode::L1076_1993},
    {"others", VhdlTokType::KWD_OTHERS, V3LangCode::L1076_1993},
    {"out", VhdlTokType::KWD_OUT, V3LangCode::L1076_1993},
    {"package", VhdlTokType::KWD_PACKAGE, V3LangCode::L1076_1993},
    {"parameter", VhdlTokType::KWD_PARAMETER, V3LangCode::L1076_2008},
    {"port", VhdlTokType::KWD_PORT, V3LangCode::L1076_1993},
    {"postponed", VhdlTokType::KWD_POSTPONED, V3LangCode::L1076_1993},
    {"private", VhdlTokType::KWD_PRIVATE, V3LangCode::L1076_2019},
    {"procedure", VhdlTokType::KWD_PROCEDURE, V3LangCode::L1076_1993},
    {"process", VhdlTokType::KWD_PROCESS, V3LangCode::L1076_1993},
    {"property", VhdlTokType::KWD_PROPERTY, V3LangCode::L1076_2008},
    {"protected", VhdlTokType::KWD_PROTECTED, V3LangCode::L1076_2000},
    {"pure", VhdlTokType::KWD_PURE, V3LangCode::L1076_1993},
    {"range", VhdlTokType::KWD_RANGE, V3LangCode::L1076_1993},
    {"record", VhdlTokType::KWD_RECORD, V3LangCode::L1076_1993},
    {"register", VhdlTokType::KWD_REGISTER, V3LangCode::L1076_1993},
    {"reject", VhdlTokType::KWD_REJECT, V3LangCode::L1076_1993},
    {"release", VhdlTokType::KWD_RELEASE, V3LangCode::L1076_2008},
    {"rem", VhdlTokType::KWD_REM, V3LangCode::L1076_1993},
    {"report", VhdlTokType::KWD_REPORT, V3LangCode::L1076_1993},
    {"restrict", VhdlTokType::KWD_RESTRICT, V3LangCode::L1076_2008},
    {"restrict_guarantee", VhdlTokType::KWD_RESTRICT_GUARANTEE, V3LangCode::L1076_2008},
    {"return", VhdlTokType::KWD_RETURN, V3LangCode::L1076_1993},
    {"rol", VhdlTokType::KWD_ROL, V3LangCode::L1076_1993},
    {"ror", VhdlTokType::KWD_ROR, V3LangCode::L1076_1993},
    {"select", VhdlTokType::KWD_SELECT, V3LangCode::L1076_1993},
    {"sequence", VhdlTokType::KWD_SEQUENCE, V3LangCode::L1076_2008},
    {"severity", VhdlTokType::KWD_SEVERITY, V3LangCode::L1076_1993},
    {"shared", VhdlTokType::KWD_SHARED, V3LangCode::L1076_1993},
    {"signal", VhdlTokType::KWD_SIGNAL, V3LangCode::L1076_1993},
    {"sla", VhdlTokType::KWD_SLA, V3LangCode::L1076_1993},
    {"sll", VhdlTokType::KWD_SLL, V3LangCode::L1076_1993},
    {"sra", VhdlTokType::KWD_SRA, V3LangCode::L1076_1993},
    {"srl", VhdlTokType::KWD_SRL, V3LangCode::L1076_1993},
    {"strong", VhdlTokType::KWD_STRONG, V3LangCode::L1076_2008},
    {"subtype", VhdlTokType::KWD_SUBTYPE, V3LangCode::L1076_1993},
    {"then", VhdlTokType::KWD_THEN, V3LangCode::L1076_1993},
    {"to", VhdlTokType::KWD_TO, V3LangCode::L1076_1993},
    {"transport", VhdlTokType::KWD_TRANSPORT, V3LangCode::L1076_1993},
    {"type", VhdlTokType::KWD_TYPE, V3LangCode::L1076_1993},
    {"unaffected", VhdlTokType::KWD_UNAFFECTED, V3LangCode::L1076_1993},
    {"units", VhdlTokType::KWD_UNITS, V3LangCode::L1076_1993},
    {"until", VhdlTokType::KWD_UNTIL, V3LangCode::L1076_1993},
    {"use", VhdlTokType::KWD_USE, V3LangCode::L1076_1993},
    {"variable", VhdlTokType::KWD_VARIABLE, V3LangCode::L1076_1993},
    {"view", VhdlTokType::KWD_VIEW, V3LangCode::L1076_2019},
    {"vmode", VhdlTokType::KWD_VMODE, V3LangCode::L1076_2008},
    {"vprop", VhdlTokType::KWD_VPROP, V3LangCode::L1076_2008},
    {"vunit", VhdlTokType::KWD_VUNIT, V3LangCode::L1076_2008},
    {"wait", VhdlTokType::KWD_WAIT, V3LangCode::L1076_1993},
    {"when", VhdlTokType::KWD_WHEN, V3LangCode::L1076_1993},
    {"while", VhdlTokType::KWD_WHILE, V3LangCode::L1076_1993},
    {"with", VhdlTokType::KWD_WITH, V3LangCode::L1076_1993},
    {"xnor", VhdlTokType::KWD_XNOR, V3LangCode::L1076_1993},
    {"xor", VhdlTokType::KWD_XOR, V3LangCode::L1076_1993},
};

}  // namespace

//######################################################################
// Token names

const char* vhdlTokName(VhdlTokType tok) {
    switch (tok) {
    case VhdlTokType::END_OF_FILE: return "<end of file>";
    case VhdlTokType::SCAN_ERROR: return "<bad character>";
    case VhdlTokType::IDENTIFIER: return "identifier";
    case VhdlTokType::CHARACTER_LIT: return "character literal";
    case VhdlTokType::STRING_LIT: return "string literal";
    case VhdlTokType::BITSTRING_LIT: return "bit string literal";
    case VhdlTokType::INTEGER_LIT: return "integer literal";
    case VhdlTokType::REAL_LIT: return "real literal";
    case VhdlTokType::TOOL_DIRECTIVE: return "tool directive";
    case VhdlTokType::AMPERSAND: return "&";
    case VhdlTokType::TICK: return "'";
    case VhdlTokType::LPAREN: return "(";
    case VhdlTokType::RPAREN: return ")";
    case VhdlTokType::STAR: return "*";
    case VhdlTokType::PLUS: return "+";
    case VhdlTokType::COMMA: return ",";
    case VhdlTokType::MINUS: return "-";
    case VhdlTokType::DOT: return ".";
    case VhdlTokType::SLASH: return "/";
    case VhdlTokType::COLON: return ":";
    case VhdlTokType::SEMICOLON: return ";";
    case VhdlTokType::LT: return "<";
    case VhdlTokType::EQ: return "=";
    case VhdlTokType::GT: return ">";
    case VhdlTokType::BAR: return "|";
    case VhdlTokType::LBRACKET: return "[";
    case VhdlTokType::RBRACKET: return "]";
    case VhdlTokType::QUERY: return "?";
    case VhdlTokType::AT: return "@";
    case VhdlTokType::CIRCUMFLEX: return "^";
    case VhdlTokType::ARROW: return "=>";
    case VhdlTokType::DOUBLE_STAR: return "**";
    case VhdlTokType::VAR_ASSIGN: return ":=";
    case VhdlTokType::NE: return "/=";
    case VhdlTokType::GE: return ">=";
    case VhdlTokType::LE: return "<=";
    case VhdlTokType::BOX: return "<>";
    case VhdlTokType::COND_CONV: return "??";
    case VhdlTokType::MATCH_EQ: return "?=";
    case VhdlTokType::MATCH_NE: return "?/=";
    case VhdlTokType::MATCH_LT: return "?<";
    case VhdlTokType::MATCH_LE: return "?<=";
    case VhdlTokType::MATCH_GT: return "?>";
    case VhdlTokType::MATCH_GE: return "?>=";
    case VhdlTokType::LTLT: return "<<";
    case VhdlTokType::GTGT: return ">>";
    default: break;  // Reserved words, from the table below
    }
    for (const VhdlKeyword& kw : VHDL_KEYWORDS) {
        if (kw.tok == tok) return kw.wordp;
    }
    return "<unknown token>";  // LCOV_EXCL_LINE
}

//######################################################################
// V3VhdlScan

V3VhdlScan::V3VhdlScan(FileLine* fileline, const std::string& text, V3LangCode lang)
    : m_text{text}
    , m_fileline{fileline}
    , m_lang{lang} {}

VhdlTokType V3VhdlScan::keywordLookup(const std::string& word, V3LangCode lang) {
    static const std::unordered_map<std::string, const VhdlKeyword*> s_map = []() {
        std::unordered_map<std::string, const VhdlKeyword*> map;
        for (const VhdlKeyword& kw : VHDL_KEYWORDS) map.emplace(kw.wordp, &kw);
        return map;
    }();
    const auto it = s_map.find(word);
    if (it == s_map.end()) return VhdlTokType::IDENTIFIER;
    // A word reserved only by a later revision is an ordinary identifier here,
    // which is what lets legacy code declare e.g. a signal named "context"
    if (lang < it->second->sinceRev) return VhdlTokType::IDENTIFIER;
    return it->second->tok;
}

char V3VhdlScan::advance() {
    const char c = m_text[m_pos++];
    if (isLineEnd(static_cast<unsigned char>(c))) {
        // Treat CRLF as one line ending
        if (c == '\r' && m_pos < m_text.size() && m_text[m_pos] == '\n') ++m_pos;
        ++m_lineno;
        m_column = 1;
    } else {
        ++m_column;
    }
    return c;
}

FileLine* V3VhdlScan::newFileline() const {
    FileLine* const flp = new FileLine{m_fileline};
    flp->lineno(m_lineno);
    flp->column(m_column, m_column);
    return flp;
}

bool V3VhdlScan::replacementChars() const {
    // VHDL-93 clause 13.10 lets '%' stand for '"', '!' for '|' and ':' for
    // '#'. VHDL-2008 removed them, and honouring that keeps the diagnostic
    // for a stray '%' in modern source precise rather than reporting a
    // string literal that runs to the end of the line.
    return m_lang < V3LangCode::L1076_2008;
}

bool V3VhdlScan::tickIsAttribute() const {
    // A "'" after something that can end a name is an attribute tick or the
    // tick of a qualified expression; otherwise it opens a character literal.
    switch (m_prevType) {
    case VhdlTokType::IDENTIFIER:
    case VhdlTokType::RPAREN:
    case VhdlTokType::RBRACKET:
    case VhdlTokType::CHARACTER_LIT:
    case VhdlTokType::STRING_LIT:
    case VhdlTokType::KWD_ALL: return true;  // e.g. accessValue.all'length
    case VhdlTokType::GTGT: return true;  // The end of an external name: << signal s : t >>'length
    case VhdlTokType::KWD_RANGE:
    case VhdlTokType::KWD_SUBTYPE:
        // These are attribute names only when they followed a tick, as in
        // t'range'left. As reserved words they introduce a constraint, and
        // "range '0' to '1'" must see a character literal.
        return m_beforePrevType == VhdlTokType::TICK;
    default: return false;
    }
}

void V3VhdlScan::skipSeparatorsAndComments() {
    while (!atEnd()) {
        const unsigned char c = static_cast<unsigned char>(peek());
        if (c == ' ' || c == '\t' || c == 0xA0 /*NBSP*/ || isLineEnd(c)) {
            advance();
        } else if (c == '-' && peek(1) == '-') {
            // Single-line comment (15.9), to end of line
            while (!atEnd() && !isLineEnd(static_cast<unsigned char>(peek()))) advance();
        } else if (c == '/' && peek(1) == '*' && m_lang >= V3LangCode::L1076_2008) {
            // Delimited comment (15.9), VHDL-2008 on. Does not nest.
            FileLine* const startp = newFileline();
            advance();  // '/'
            advance();  // '*'
            bool closed = false;
            while (!atEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) startp->v3error("Unterminated delimited comment, expected '*/'");
        } else {
            return;
        }
    }
}

VhdlToken V3VhdlScan::next() {
    while (true) {
        const VhdlToken tok = nextOrError();
        // scanDelimiter consumed the offending character before reporting, so
        // this always makes progress and cannot spin
        if (tok.type != VhdlTokType::SCAN_ERROR) return tok;
    }
}

VhdlToken V3VhdlScan::nextOrError() {
    skipSeparatorsAndComments();
    FileLine* const flp = newFileline();
    VhdlToken tok;
    tok.flp = flp;

    if (atEnd()) {
        tok.type = VhdlTokType::END_OF_FILE;
        m_beforePrevType = m_prevType;
        m_prevType = tok.type;
        return tok;
    }

    const unsigned char c = static_cast<unsigned char>(peek());
    if (isLetter(c)) {
        tok = scanIdentifierOrKeyword(flp);
    } else if (isDigitChar(c)) {
        tok = scanAbstractLiteral(flp);
    } else if (c == '\\') {
        tok = scanExtendedIdentifier(flp);
    } else if (c == '"' || (c == '%' && replacementChars())) {
        tok = scanStringLiteral(flp, static_cast<char>(c));
    } else if (c == '\'') {
        if (tickIsAttribute()) {
            advance();
            tok.type = VhdlTokType::TICK;
        } else {
            tok = scanCharacterLiteral(flp);
        }
    } else if (c == '`') {
        tok = scanToolDirective(flp);
    } else {
        tok = scanDelimiter(flp);
    }

    m_beforePrevType = m_prevType;
    m_prevType = tok.type;
    return tok;
}

VhdlToken V3VhdlScan::scanIdentifierOrKeyword(FileLine* flp) {
    VhdlToken tok;
    tok.flp = flp;
    std::string raw;
    while (!atEnd()) {
        const unsigned char c = static_cast<unsigned char>(peek());
        if (isLetterOrDigit(c)) {
            raw += advance();
        } else if (c == '_') {
            // 15.4.2: an underline must separate two letters or digits. We can
            // only get here after a letter, so a leading underline is
            // impossible; report each offence exactly once.
            const unsigned char nextc = static_cast<unsigned char>(peek(1));
            if (raw.back() == '_') {
                // Already reported at the first underline of the run
            } else if (nextc == '_') {
                flp->v3error("Identifier may not contain two consecutive underlines");
            } else if (!isLetterOrDigit(nextc)) {
                flp->v3error("Identifier may not end with an underline");
            }
            raw += advance();
        } else {
            break;
        }
    }

    // A base specifier immediately followed by a quote is a bit string
    // literal, not an identifier (15.8), e.g. X"FF"
    char base = '\0';
    bool isSigned = false;
    if ((peek() == '"' || (peek() == '%' && replacementChars()))
        && parseBaseSpecifier(raw, &base, &isSigned)) {
        return scanBitStringLiteral(flp, raw, false, 0, peek());
    }

    tok.text = lowerString(raw);
    tok.type = keywordLookup(tok.text, m_lang);
    return tok;
}

VhdlToken V3VhdlScan::scanExtendedIdentifier(FileLine* flp) {
    VhdlToken tok;
    tok.flp = flp;
    tok.type = VhdlTokType::IDENTIFIER;
    tok.extendedId = true;
    advance();  // opening backslash
    bool closed = false;
    while (!atEnd()) {
        const unsigned char c = static_cast<unsigned char>(peek());
        if (isLineEnd(c)) break;  // 15.3: a lexical element fits on one line
        if (c == '\\') {
            advance();
            if (peek() == '\\') {
                tok.text += advance();  // doubled backslash is one character
            } else {
                closed = true;
                break;
            }
        } else if (isGraphic(c) || c == '\t') {
            // Horizontal tab is not a graphic character (15.2), but real
            // sources contain it and accepting it cannot reject valid VHDL
            tok.text += advance();
        } else {
            flp->v3error("Non-graphic character in extended identifier");
            advance();
        }
    }
    if (!closed) {
        flp->v3error("Unterminated extended identifier, expected a closing backslash");
    } else if (tok.text.empty()) {
        flp->v3error("Extended identifier may not be empty");
    }
    // 15.4.3: extended identifiers are case sensitive, so no lowercasing here
    return tok;
}

VhdlToken V3VhdlScan::scanStringLiteral(FileLine* flp, char quote) {
    VhdlToken tok;
    tok.flp = flp;
    tok.type = VhdlTokType::STRING_LIT;
    advance();  // opening quote
    bool closed = false;
    while (!atEnd()) {
        const unsigned char c = static_cast<unsigned char>(peek());
        if (isLineEnd(c)) break;  // 15.7 NOTE: must fit on one line
        if (c == static_cast<unsigned char>(quote)) {
            advance();
            if (peek() == quote) {
                tok.text += advance();  // doubled quote is one quote
            } else {
                closed = true;
                break;
            }
        } else if (c == '"' && quote == '%') {
            // A string written between percent signs stands for one between
            // quotation marks, and so holds none of them (VHDL-93 13.10)
            flp->v3error("A string literal delimited by '%' cannot contain a '\"'");
            tok.text += advance();
        } else if (isGraphic(c) || c == '\t') {
            tok.text += advance();  // See the note on tabs above
        } else {
            flp->v3error("Non-graphic character in string literal");
            advance();
        }
    }
    if (!closed) {
        flp->v3error("Unterminated string literal, expected closing '" << quote << "'");
    }
    return tok;
}

VhdlToken V3VhdlScan::scanCharacterLiteral(FileLine* flp) {
    VhdlToken tok;
    tok.flp = flp;
    tok.type = VhdlTokType::CHARACTER_LIT;
    advance();  // opening apostrophe
    const unsigned char c = static_cast<unsigned char>(peek());
    if (atEnd() || isLineEnd(c) || !isGraphic(c)) {
        flp->v3error("Malformed character literal");
        return tok;
    }
    tok.text += advance();
    if (peek() != '\'') {
        flp->v3error("Malformed character literal, expected a closing apostrophe");
        return tok;
    }
    advance();  // closing apostrophe
    return tok;
}

VhdlToken V3VhdlScan::scanToolDirective(FileLine* flp) {
    // 15.11: a tool directive runs from a grave accent to end of line
    VhdlToken tok;
    tok.flp = flp;
    tok.type = VhdlTokType::TOOL_DIRECTIVE;
    // VHDL-2008 brought tool directives; before it the grave accent is a
    // character only a comment or a string may hold (VHDL-93 13.1). nvc
    // 1.22.1 differs by directive, GHDL 7.0.0-dev and Questa 2025.2 refuse.
    if (m_lang < V3LangCode::L1076_2008) {
        flp->v3error("Before VHDL-2008 a grave accent is allowed only in a comment or a string;"
                     " tool directives are VHDL-2008's");
    }
    advance();  // grave accent
    while (!atEnd() && !isLineEnd(static_cast<unsigned char>(peek()))) tok.text += advance();
    return tok;
}

VhdlToken V3VhdlScan::scanAbstractLiteral(FileLine* flp) {
    VhdlToken tok;
    tok.flp = flp;

    // Leading integer: either the whole decimal literal, the base of a based
    // literal, or the length of a bit string literal
    std::string intPart;
    while (!atEnd()) {
        const unsigned char c = static_cast<unsigned char>(peek());
        if (isDigitChar(c)) {
            intPart += advance();
        } else if (c == '_' && !intPart.empty()
                   && isDigitChar(static_cast<unsigned char>(peek(1)))) {
            advance();  // underlines do not affect the value (15.5.2)
        } else {
            break;
        }
    }

    // 15.8: [ integer ] base_specifier " bit_value "
    if (isLetter(static_cast<unsigned char>(peek()))) {
        std::string spec;
        size_t look = 0;
        while (look < 2 && isLetter(static_cast<unsigned char>(peek(look)))) {
            spec += peek(look);
            ++look;
        }
        char base = '\0';
        bool isSigned = false;
        if ((peek(look) == '"' || (peek(look) == '%' && replacementChars()))
            && parseBaseSpecifier(spec, &base, &isSigned)) {
            for (size_t i = 0; i < look; ++i) advance();
            return scanBitStringLiteral(flp, spec, true,
                                        std::strtoll(intPart.c_str(), nullptr, 10), peek());
        }
    }

    // Radix of the mantissa: ten, unless a based literal says otherwise.
    // ':' is the VHDL-93 replacement character for '#'; it is only taken as
    // one when an extended digit follows, which no other use of ':' does.
    const char hash = (peek() == '#') ? '#'
                      : (replacementChars() && peek() == ':'
                         && extendedDigitValue(static_cast<unsigned char>(peek(1))) >= 0)
                          ? ':'
                          : '\0';
    int base = 10;
    const bool based = (hash != '\0');
    if (based) {
        advance();
        base = static_cast<int>(std::strtol(intPart.c_str(), nullptr, 10));
        if (base < 2 || base > 16) {
            flp->v3error("Base of a based literal must be between 2 and 16, got " << base);
            base = 16;  // Keep scanning so later errors still make sense
        }
    }

    // Only a based literal may use letters as extended digits (15.5.3). In a
    // decimal literal an 'e' is the exponent, not the digit fourteen.
    const auto isDigitHere = [based](unsigned char c) VL_PURE -> bool {
        return based ? extendedDigitValue(c) >= 0 : isDigitChar(c);
    };

    // Mantissa. For a decimal literal the integer part was already read.
    std::string mantissa = based ? "" : intPart;
    if (based) {
        while (!atEnd()) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (isDigitHere(c)) {
                mantissa += advance();
            } else if (c == '_' && !mantissa.empty()
                       && isDigitHere(static_cast<unsigned char>(peek(1)))) {
                advance();
            } else {
                break;
            }
        }
        if (mantissa.empty()) flp->v3error("Based literal has no digits");
    }

    // Optional fractional part. A point must be followed by a digit (15.5.2),
    // so "1." is an integer literal followed by a dot.
    std::string fraction;
    const bool isReal = (peek() == '.' && isDigitHere(static_cast<unsigned char>(peek(1))));
    if (isReal) {
        advance();  // '.'
        while (!atEnd()) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (isDigitHere(c)) {
                fraction += advance();
            } else if (c == '_' && !fraction.empty()
                       && isDigitHere(static_cast<unsigned char>(peek(1)))) {
                advance();
            } else {
                break;
            }
        }
    }

    if (based) {
        if (peek() != hash) {
            flp->v3error("Expected '" << hash << "' to close based literal");
        } else {
            advance();
        }
    }

    // Optional exponent (15.5.2). The exponent is always in decimal.
    int64_t exponent = 0;
    bool expNegative = false;
    std::string expText;
    if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+') {
            advance();
        } else if (peek() == '-') {
            expNegative = true;
            advance();
        }
        std::string expDigits;
        while (!atEnd()) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (isDigitChar(c)) {
                expDigits += advance();
            } else if (c == '_' && !expDigits.empty()
                       && isDigitChar(static_cast<unsigned char>(peek(1)))) {
                advance();
            } else {
                break;
            }
        }
        if (expDigits.empty()) {
            flp->v3error("Exponent has no digits");
        } else {
            exponent = std::strtoll(expDigits.c_str(), nullptr, 10);
            expText = "e" + std::string{expNegative ? "-" : "+"} + expDigits;
        }
        if (expNegative && !isReal) {
            // 15.5.2: "An exponent for an integer literal shall not have a minus sign"
            flp->v3error("Exponent of an integer literal may not be negative");
            expNegative = false;
        }
    }

    // Validate digits against the base, which strtol-style parsing would not
    for (const char c : mantissa + fraction) {
        if (extendedDigitValue(static_cast<unsigned char>(c)) >= base) {
            flp->v3error("Digit '" << c << "' is not valid in base " << base);
            break;
        }
    }

    tok.text = (based ? (intPart + hash + mantissa + (isReal ? "." + fraction : "") + hash)
                      : (mantissa + (isReal ? "." + fraction : "")))
               + expText;

    if (isReal) {
        tok.type = VhdlTokType::REAL_LIT;
        double value = 0.0;
        if (base == 10) {
            // Converted as one number rather than digit by digit: adding
            // 7*0.1 and then 5*0.01 gives 0.7500000000000001, not 0.75,
            // because neither tenth is a binary fraction
            std::string text = mantissa.empty() ? "0" : mantissa;
            text += "." + (fraction.empty() ? std::string{"0"} : fraction);
            if (exponent != 0) {
                text += std::string{"e"} + (expNegative ? "-" : "") + std::to_string(exponent);
            }
            value = std::strtod(text.c_str(), nullptr);
        } else {
            for (const char c : mantissa) {
                value = value * base + extendedDigitValue(static_cast<unsigned char>(c));
            }
            // From the right, so each step is one exact division in a base
            // that is a power of two
            double frac = 0.0;
            for (auto it = fraction.rbegin(); it != fraction.rend(); ++it) {
                frac = (frac + extendedDigitValue(static_cast<unsigned char>(*it))) / base;
            }
            value += frac;
            // pow rather than a loop: an exponent may be astronomically
            // large, as in 0e100000000000
            if (exponent != 0) {
                value *= std::pow(static_cast<double>(base),
                                  static_cast<double>(expNegative ? -exponent : exponent));
            }
        }
        tok.realVal = value;
    } else {
        tok.type = VhdlTokType::INTEGER_LIT;
        uint64_t value = 0;
        bool overflow = false;
        for (const char c : mantissa) {
            const uint64_t digit
                = static_cast<uint64_t>(extendedDigitValue(static_cast<unsigned char>(c)));
            if (value > (UINT64_MAX - digit) / static_cast<uint64_t>(base)) overflow = true;
            value = value * static_cast<uint64_t>(base) + digit;
        }
        // Stop as soon as the value overflows or is known to stay zero, so a
        // huge exponent such as 0e100000000000 cannot spin
        for (int64_t i = 0; i < exponent && value != 0 && !overflow; ++i) {
            if (value > UINT64_MAX / static_cast<uint64_t>(base)) {
                overflow = true;
                break;
            }
            value *= static_cast<uint64_t>(base);
        }
        if (overflow || value > static_cast<uint64_t>(INT64_MAX)) {
            flp->v3error("Integer literal is too large to represent: " << tok.text);
            value = 0;
        }
        tok.intVal = static_cast<int64_t>(value);
    }
    return tok;
}

VhdlToken V3VhdlScan::scanBitStringLiteral(FileLine* flp, const std::string& spec,
                                           bool lengthGiven, int64_t length, char quote) {
    VhdlToken tok;
    tok.flp = flp;
    tok.type = VhdlTokType::BITSTRING_LIT;
    tok.hasLength = lengthGiven;
    tok.intVal = length;
    char base = '\0';
    bool isSigned = false;
    parseBaseSpecifier(spec, &base, &isSigned);  // Already validated by the caller
    tok.baseSpec = base;
    tok.signedBitStr = isSigned;

    // Simplified bit value: the characters between the quotes, underlines removed
    advance();  // opening quote
    std::string simplified;
    bool closed = false;
    // An underline only ever separates two characters of the bit value
    // (15.8), so it cannot lead, trail, or follow another underline
    bool afterUnderline = false;
    bool badUnderline = false;
    while (!atEnd()) {
        const unsigned char c = static_cast<unsigned char>(peek());
        if (isLineEnd(c)) break;
        if (c == static_cast<unsigned char>(quote)) {
            advance();
            closed = true;
            break;
        }
        if (c == '_') {
            advance();  // underlines do not affect the value (15.8)
            if (!badUnderline && (simplified.empty() || afterUnderline)) {
                flp->v3error((simplified.empty()
                                  ? "A bit string literal cannot begin with an underline"
                                  : "A bit string literal cannot have two underlines in a row"));
                badUnderline = true;
            }
            afterUnderline = true;
        } else if (isGraphic(c) || c == '\t') {
            simplified += advance();  // See the note on tabs above
            afterUnderline = false;
        } else {
            flp->v3error("Non-graphic character in bit string literal");
            advance();
        }
    }
    if (!closed) {
        flp->v3error("Unterminated bit string literal, expected closing '" << quote << "'");
        return tok;
    }
    if (afterUnderline && !badUnderline) {
        flp->v3error("A bit string literal cannot end with an underline");
    }

    // Expanded bit value (15.8)
    std::string expanded;
    if (base == 'D') {
        for (const char c : simplified) {
            if (!isDigitChar(static_cast<unsigned char>(c))) {
                flp->v3error("Decimal bit string literal may only contain digits, got '" << c
                                                                                        << "'");
                return tok;
            }
        }
        expanded = simplified.empty() ? "" : decimalToBinary(simplified);
    } else {
        const int bitsPer = (base == 'B') ? 1 : (base == 'O') ? 3 : 4;
        for (const char c : simplified) {
            const int value = extendedDigitValue(static_cast<unsigned char>(c));
            const bool isExtDigit = value >= 0 && value < (1 << bitsPer);
            if (!isExtDigit && m_lang < V3LangCode::L1076_2008) {
                // Passing other characters through, so B"XXXX" is four 'X'
                // elements, is VHDL-2008's; before it a bit value held only
                // the digits of its base (VHDL-93 13.7)
                flp->v3error("'" << c << "' is not a digit of base " << base
                                 << "; before VHDL-2008 a bit string literal holds only"
                                    " digits of its base");
                return tok;
            }
            if (!isExtDigit) {
                // Not an extended digit: replicate it, so B"XXXX" and X"F-"
                // keep their metavalues (15.8)
                expanded.append(static_cast<size_t>(bitsPer), c);
            } else {
                for (int bit = bitsPer - 1; bit >= 0; --bit) {
                    expanded += static_cast<char>('0' + ((value >> bit) & 1));
                }
            }
        }
    }

    // Adjust to the given length (15.8)
    if (!lengthGiven) {
        tok.text = expanded;
        return tok;
    }
    // The length is an arbitrary integer in the source, so bound it before
    // allocating: 32222222222sb"1" would otherwise ask for 32 gigabits
    if (length < 0 || length > v3Global.opt.maxNumWidth()) {
        flp->v3error("Unsupported: Width of bit string literal exceeds implementation limit: "
                     << length);
        length = v3Global.opt.maxNumWidth();
        tok.intVal = length;
    }
    const size_t want = static_cast<size_t>(length);
    if (want == expanded.size()) {
        tok.text = expanded;
    } else if (want > expanded.size()) {
        const char pad = isSigned ? (expanded.empty() ? '0' : expanded[0]) : '0';
        if (isSigned && expanded.empty()) {
            flp->v3error("Signed bit string literal may not be empty when a length is given");
        }
        tok.text = std::string(want - expanded.size(), pad) + expanded;
    } else {
        const std::string dropped = expanded.substr(0, expanded.size() - want);
        tok.text = expanded.substr(expanded.size() - want);
        const char keep = isSigned ? (tok.text.empty() ? '0' : tok.text[0]) : '0';
        for (const char c : dropped) {
            if (c != keep) {
                flp->v3error("Bit string literal does not fit in "
                             << length << " bits; truncating would discard '" << c << "'");
                break;
            }
        }
    }
    return tok;
}

VhdlToken V3VhdlScan::scanDelimiter(FileLine* flp) {
    VhdlToken tok;
    tok.flp = flp;
    const char c0 = peek();
    const char c1 = peek(1);
    const char c2 = peek(2);

    // Compound delimiters are matched longest first (15.3)
    switch (c0) {
    case '?':
        if (c1 == '/' && c2 == '=') {
            advance(), advance(), advance();
            tok.type = VhdlTokType::MATCH_NE;
        } else if (c1 == '<' && c2 == '=') {
            advance(), advance(), advance();
            tok.type = VhdlTokType::MATCH_LE;
        } else if (c1 == '>' && c2 == '=') {
            advance(), advance(), advance();
            tok.type = VhdlTokType::MATCH_GE;
        } else if (c1 == '?') {
            advance(), advance();
            tok.type = VhdlTokType::COND_CONV;
        } else if (c1 == '=') {
            advance(), advance();
            tok.type = VhdlTokType::MATCH_EQ;
        } else if (c1 == '<') {
            advance(), advance();
            tok.type = VhdlTokType::MATCH_LT;
        } else if (c1 == '>') {
            advance(), advance();
            tok.type = VhdlTokType::MATCH_GT;
        } else {
            advance();
            tok.type = VhdlTokType::QUERY;
        }
        return tok;
    case '<':
        if (c1 == '=') {
            advance(), advance();
            tok.type = VhdlTokType::LE;
        } else if (c1 == '>') {
            advance(), advance();
            tok.type = VhdlTokType::BOX;
        } else if (c1 == '<') {
            advance(), advance();
            tok.type = VhdlTokType::LTLT;
        } else {
            advance();
            tok.type = VhdlTokType::LT;
        }
        return tok;
    case '>':
        if (c1 == '=') {
            advance(), advance();
            tok.type = VhdlTokType::GE;
        } else if (c1 == '>') {
            advance(), advance();
            tok.type = VhdlTokType::GTGT;
        } else {
            advance();
            tok.type = VhdlTokType::GT;
        }
        return tok;
    case '=':
        if (c1 == '>') {
            advance(), advance();
            tok.type = VhdlTokType::ARROW;
        } else {
            advance();
            tok.type = VhdlTokType::EQ;
        }
        return tok;
    case '*':
        if (c1 == '*') {
            advance(), advance();
            tok.type = VhdlTokType::DOUBLE_STAR;
        } else {
            advance();
            tok.type = VhdlTokType::STAR;
        }
        return tok;
    case ':':
        if (c1 == '=') {
            advance(), advance();
            tok.type = VhdlTokType::VAR_ASSIGN;
        } else {
            advance();
            tok.type = VhdlTokType::COLON;
        }
        return tok;
    case '/':
        if (c1 == '=') {
            advance(), advance();
            tok.type = VhdlTokType::NE;
        } else {
            advance();
            tok.type = VhdlTokType::SLASH;
        }
        return tok;
    default: break;
    }

    advance();
    switch (c0) {
    case '&': tok.type = VhdlTokType::AMPERSAND; return tok;
    case '(': tok.type = VhdlTokType::LPAREN; return tok;
    case ')': tok.type = VhdlTokType::RPAREN; return tok;
    case '+': tok.type = VhdlTokType::PLUS; return tok;
    case ',': tok.type = VhdlTokType::COMMA; return tok;
    case '-': tok.type = VhdlTokType::MINUS; return tok;
    case '.': tok.type = VhdlTokType::DOT; return tok;
    case ';': tok.type = VhdlTokType::SEMICOLON; return tok;
    case '|': tok.type = VhdlTokType::BAR; return tok;
    case '^': tok.type = VhdlTokType::CIRCUMFLEX; return tok;
    case '!':
        if (replacementChars()) {
            tok.type = VhdlTokType::BAR;
            return tok;
        }
        break;
    case '[': tok.type = VhdlTokType::LBRACKET; return tok;
    case ']': tok.type = VhdlTokType::RBRACKET; return tok;
    case '@': tok.type = VhdlTokType::AT; return tok;
    default: break;
    }

    flp->v3error("Unexpected character in VHDL source: '" << c0 << "' (0x" << std::hex
                                                          << static_cast<int>(
                                                                 static_cast<unsigned char>(c0))
                                                          << std::dec << ")");
    tok.type = VhdlTokType::SCAN_ERROR;  // Already consumed; next() will retry
    return tok;
}

//######################################################################
// Self test

void V3VhdlScan::selfTest() {
    // The reserved word table must cover exactly the KWD_ run of the enum,
    // or keywordLookup and vhdlTokName silently disagree with the parser
    const size_t kwdCount = sizeof(VHDL_KEYWORDS) / sizeof(VHDL_KEYWORDS[0]);
    const size_t enumCount = static_cast<size_t>(VHDLTOK_KWD_LAST)
                             - static_cast<size_t>(VHDLTOK_KWD_FIRST) + 1;
    if (kwdCount != enumCount) {
        v3fatalSrc("VHDL reserved word table has " << kwdCount << " entries but the token enum has "
                                                   << enumCount);
    }
    // Every reserved word must map to a distinct token, and back to itself
    for (const VhdlKeyword& kw : VHDL_KEYWORDS) {
        if (std::string{vhdlTokName(kw.tok)} != kw.wordp) {
            v3fatalSrc("VHDL reserved word '" << kw.wordp << "' does not round trip through "
                                              << "vhdlTokName, got '" << vhdlTokName(kw.tok)
                                              << "'");
        }
        if (keywordLookup(kw.wordp, V3LangCode::L1076_2019) != kw.tok) {
            v3fatalSrc("VHDL reserved word '" << kw.wordp << "' does not look up to itself");
        }
    }
    // Revision gating: 'context' is reserved from 2008 only
    if (keywordLookup("context", V3LangCode::L1076_1993) != VhdlTokType::IDENTIFIER) {
        v3fatalSrc("'context' must be an ordinary identifier in VHDL-1993");
    }
    if (keywordLookup("context", V3LangCode::L1076_2008) != VhdlTokType::KWD_CONTEXT) {
        v3fatalSrc("'context' must be reserved in VHDL-2008");
    }
    // 15.8 worked examples
    if (decimalToBinary("35") != "100011") v3fatalSrc("decimalToBinary(35) wrong");
    if (decimalToBinary("0") != "0") v3fatalSrc("decimalToBinary(0) wrong");
    if (decimalToBinary("18446744073709551616")  // 2**64, beyond uint64
        != "10000000000000000000000000000000000000000000000000000000000000000") {
        v3fatalSrc("decimalToBinary overflowed on a wide value");
    }
}
