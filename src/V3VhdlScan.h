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

#ifndef VERILATOR_V3VHDLSCAN_H_
#define VERILATOR_V3VHDLSCAN_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Error.h"
#include "V3LangCode.h"

#include <cstdint>
#include <string>

//============================================================================
// Token kinds, per IEEE 1076-2008 clause 15.
//
// Ordering is meaningful only in that the reserved words form one contiguous
// run, which lets isKeyword() be a range check.

enum class VhdlTokType : uint16_t {
    END_OF_FILE,
    SCAN_ERROR,  // Internal: a character was rejected and skipped; next() retries
    // Literals and names (15.4 - 15.8)
    IDENTIFIER,  // basic or extended identifier
    CHARACTER_LIT,  // 'x'
    STRING_LIT,  // "..."  (also an operator symbol, e.g. "and")
    BITSTRING_LIT,  // [n] B|O|X|D|UB|UO|UX|SB|SO|SX "..."
    INTEGER_LIT,  // abstract literal with no point
    REAL_LIT,  // abstract literal with a point
    TOOL_DIRECTIVE,  // ` identifier ... end-of-line  (15.11)

    // Single-character delimiters (15.3)
    AMPERSAND,  // &
    TICK,  // '   (attribute, or qualified expression)
    LPAREN,  // (
    RPAREN,  // )
    STAR,  // *
    PLUS,  // +
    COMMA,  // ,
    MINUS,  // -
    DOT,  // .
    SLASH,  // /
    COLON,  // :
    SEMICOLON,  // ;
    LT,  // <
    EQ,  // =
    GT,  // >
    BAR,  // |   (also written ! )
    LBRACKET,  // [
    RBRACKET,  // ]
    QUERY,  // ?
    AT,  // @
    CIRCUMFLEX,  // ^   (relative external pathname, 8.7)

    // Compound delimiters (15.3)
    ARROW,  // =>
    DOUBLE_STAR,  // **
    VAR_ASSIGN,  // :=
    NE,  // /=
    GE,  // >=
    LE,  // <=   (also signal assignment)
    BOX,  // <>
    COND_CONV,  // ??
    MATCH_EQ,  // ?=
    MATCH_NE,  // ?/=
    MATCH_LT,  // ?<
    MATCH_LE,  // ?<=
    MATCH_GT,  // ?>
    MATCH_GE,  // ?>=
    LTLT,  // <<
    GTGT,  // >>

    // Reserved words (15.10). KWD_FIRST/KWD_LAST bracket this run.
    KWD_ABS,
    KWD_ACCESS,
    KWD_AFTER,
    KWD_ALIAS,
    KWD_ALL,
    KWD_AND,
    KWD_ARCHITECTURE,
    KWD_ARRAY,
    KWD_ASSERT,
    KWD_ASSUME,
    KWD_ASSUME_GUARANTEE,
    KWD_ATTRIBUTE,
    KWD_BEGIN,
    KWD_BLOCK,
    KWD_BODY,
    KWD_BUFFER,
    KWD_BUS,
    KWD_CASE,
    KWD_COMPONENT,
    KWD_CONFIGURATION,
    KWD_CONSTANT,
    KWD_CONTEXT,
    KWD_COVER,
    KWD_DEFAULT,
    KWD_DISCONNECT,
    KWD_DOWNTO,
    KWD_ELSE,
    KWD_ELSIF,
    KWD_END,
    KWD_ENTITY,
    KWD_EXIT,
    KWD_FAIRNESS,
    KWD_FILE,
    KWD_FOR,
    KWD_FORCE,
    KWD_FUNCTION,
    KWD_GENERATE,
    KWD_GENERIC,
    KWD_GROUP,
    KWD_GUARDED,
    KWD_IF,
    KWD_IMPURE,
    KWD_IN,
    KWD_INERTIAL,
    KWD_INOUT,
    KWD_IS,
    KWD_LABEL,
    KWD_LIBRARY,
    KWD_LINKAGE,
    KWD_LITERAL,
    KWD_LOOP,
    KWD_MAP,
    KWD_MOD,
    KWD_NAND,
    KWD_NEW,
    KWD_NEXT,
    KWD_NOR,
    KWD_NOT,
    KWD_NULL,
    KWD_OF,
    KWD_ON,
    KWD_OPEN,
    KWD_OR,
    KWD_OTHERS,
    KWD_OUT,
    KWD_PACKAGE,
    KWD_PARAMETER,
    KWD_PORT,
    KWD_POSTPONED,
    KWD_PRIVATE,  // VHDL-2019
    KWD_PROCEDURE,
    KWD_PROCESS,
    KWD_PROPERTY,
    KWD_PROTECTED,
    KWD_PURE,
    KWD_RANGE,
    KWD_RECORD,
    KWD_REGISTER,
    KWD_REJECT,
    KWD_RELEASE,
    KWD_REM,
    KWD_REPORT,
    KWD_RESTRICT,
    KWD_RESTRICT_GUARANTEE,
    KWD_RETURN,
    KWD_ROL,
    KWD_ROR,
    KWD_SELECT,
    KWD_SEQUENCE,
    KWD_SEVERITY,
    KWD_SHARED,
    KWD_SIGNAL,
    KWD_SLA,
    KWD_SLL,
    KWD_SRA,
    KWD_SRL,
    KWD_STRONG,
    KWD_SUBTYPE,
    KWD_THEN,
    KWD_TO,
    KWD_TRANSPORT,
    KWD_TYPE,
    KWD_UNAFFECTED,
    KWD_UNITS,
    KWD_UNTIL,
    KWD_USE,
    KWD_VARIABLE,
    KWD_VIEW,  // VHDL-2019
    KWD_VMODE,
    KWD_VPROP,
    KWD_VUNIT,
    KWD_WAIT,
    KWD_WHEN,
    KWD_WHILE,
    KWD_WITH,
    KWD_XNOR,
    KWD_XOR,
};

constexpr VhdlTokType VHDLTOK_KWD_FIRST = VhdlTokType::KWD_ABS;
constexpr VhdlTokType VHDLTOK_KWD_LAST = VhdlTokType::KWD_XOR;

inline bool vhdlTokIsKeyword(VhdlTokType tok) VL_MT_SAFE {
    return tok >= VHDLTOK_KWD_FIRST && tok <= VHDLTOK_KWD_LAST;
}

// Human-readable spelling, for error messages
const char* vhdlTokName(VhdlTokType tok) VL_MT_DISABLED;

//============================================================================
// One scanned token.
//
// The payloads are deliberately plain: the scanner does no interpretation
// beyond what clause 15 requires, and in particular does not build V3Numbers.
// Widths and types are the parser's and analyzer's business.

struct VhdlToken final {
    VhdlTokType type = VhdlTokType::END_OF_FILE;
    FileLine* flp = nullptr;
    // IDENTIFIER: name, lowercased for basic identifiers, verbatim for
    //   extended ones (15.4.2/15.4.3 -- basic identifiers are case
    //   insensitive, extended ones are not).
    // STRING_LIT: decoded contents, doubled quotes collapsed.
    // CHARACTER_LIT: the single character.
    // BITSTRING_LIT: expanded bit value, before length adjustment.
    // INTEGER_LIT / REAL_LIT: source text with underlines removed.
    // TOOL_DIRECTIVE: text after the grave accent.
    std::string text;
    bool extendedId = false;  // IDENTIFIER came from \...\ form
    int64_t intVal = 0;  // INTEGER_LIT, and BITSTRING_LIT explicit length
    double realVal = 0.0;  // REAL_LIT
    char baseSpec = '\0';  // BITSTRING_LIT: 'B','O','X','D'
    bool signedBitStr = false;  // BITSTRING_LIT: base specifier was S*
    bool hasLength = false;  // BITSTRING_LIT: an explicit length was given
};

//============================================================================
// Scanner over one source file.
//
// Holds the whole file in memory: VHDL sources are small by C++ standards, and
// a flat buffer keeps lookahead trivial. Construct once per file, then call
// next() until END_OF_FILE.

class V3VhdlScan final {
    // MEMBERS
    const std::string m_text;  // Entire source
    FileLine* const m_fileline;  // Template fileline for this file
    const V3LangCode m_lang;  // Which revision's reserved words apply
    size_t m_pos = 0;  // Read cursor into m_text
    int m_lineno = 1;  // 1-based line of m_pos
    int m_column = 1;  // 1-based column of m_pos
    VhdlTokType m_prevType = VhdlTokType::END_OF_FILE;  // For the ' ambiguity
    VhdlTokType m_beforePrevType = VhdlTokType::END_OF_FILE;  // Ditto, one further back

    VL_UNCOPYABLE(V3VhdlScan);

public:
    // CONSTRUCTORS
    V3VhdlScan(FileLine* fileline, const std::string& text, V3LangCode lang) VL_MT_DISABLED;
    ~V3VhdlScan() = default;

    // METHODS
    // Scan and return the next token. Returns END_OF_FILE forever at the end.
    VhdlToken next() VL_MT_DISABLED;

    // True if 'word' (already lowercased) is reserved in this revision
    static VhdlTokType keywordLookup(const std::string& word, V3LangCode lang) VL_MT_DISABLED;

    static void selfTest() VL_MT_DISABLED;

private:
    // One scan step, which may report a bad character and return SCAN_ERROR
    VhdlToken nextOrError() VL_MT_DISABLED;

    // Character access
    char peek(size_t ahead = 0) const {
        return (m_pos + ahead < m_text.size()) ? m_text[m_pos + ahead] : '\0';
    }
    bool atEnd() const { return m_pos >= m_text.size(); }
    char advance() VL_MT_DISABLED;
    FileLine* newFileline() const VL_MT_DISABLED;

    // Sub-scanners, each entered with m_pos on the construct's first character
    void skipSeparatorsAndComments() VL_MT_DISABLED;
    VhdlToken scanIdentifierOrKeyword(FileLine* flp) VL_MT_DISABLED;
    VhdlToken scanExtendedIdentifier(FileLine* flp) VL_MT_DISABLED;
    VhdlToken scanAbstractLiteral(FileLine* flp) VL_MT_DISABLED;
    VhdlToken scanCharacterLiteral(FileLine* flp) VL_MT_DISABLED;
    // quote is '"' normally, or '%' when the VHDL-93 replacement character is used
    VhdlToken scanStringLiteral(FileLine* flp, char quote) VL_MT_DISABLED;
    VhdlToken scanBitStringLiteral(FileLine* flp, const std::string& spec, bool lengthGiven,
                                   int64_t length, char quote) VL_MT_DISABLED;
    VhdlToken scanToolDirective(FileLine* flp) VL_MT_DISABLED;
    VhdlToken scanDelimiter(FileLine* flp) VL_MT_DISABLED;

    // True if a "'" here begins an attribute tick rather than a character
    // literal, decided from the previous token (15.6 is ambiguous alone)
    bool tickIsAttribute() const VL_MT_DISABLED;

    // True if this revision still admits the VHDL-93 replacement characters
    bool replacementChars() const VL_MT_DISABLED;
};

#endif  // Guard
