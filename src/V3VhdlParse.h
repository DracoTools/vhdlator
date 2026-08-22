// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL parser
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

#ifndef VERILATOR_V3VHDLPARSE_H_
#define VERILATOR_V3VHDLPARSE_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"
#include "V3Error.h"
#include "V3LangCode.h"
#include "V3VhdlScan.h"

#include <deque>
#include <string>

//============================================================================
// Recursive descent parser for VHDL, following the grammar of IEEE 1076-2008
// Annex C.
//
// Recursive descent rather than a generated parser because VHDL's grammar is
// not LALR-friendly: "name ( expr )" is a function call, an array index, a
// slice or a type conversion, and which one cannot be known until names are
// resolved (8.1). The parser therefore records the ambiguous forms verbatim
// -- as AstVhdlCall -- and leaves the choice to name resolution.
//
// Produces the VHDL parse tree of V3AstNodeVhdl.h. It resolves no names,
// checks no types, and evaluates nothing.

class V3VhdlParse final {
    // MEMBERS
    V3VhdlScan m_scanner;  // Token source
    std::deque<VhdlToken> m_ahead;  // Lookahead buffer, filled on demand
    FileLine* const m_fileline;  // Template fileline for this file
    const V3LangCode m_lang;  // Revision being parsed
    const std::string m_libname;  // Library the units are analyzed into
    int m_errCount = 0;  // Syntax errors seen for this file
    int m_printed = 0;  // Of those, how many were reported
    bool m_panic = false;  // Suppressing a cascade from an earlier error
    bool m_atEnd = false;  // Scanner is exhausted

    static constexpr int MAX_ERRORS = 20;  // Stop reporting after this many

    VL_UNCOPYABLE(V3VhdlParse);

public:
    // CONSTRUCTORS
    V3VhdlParse(FileLine* fileline, const std::string& text, V3LangCode lang,
                const std::string& libname) VL_MT_DISABLED;
    ~V3VhdlParse() = default;

    // METHODS
    // Parse the whole file. Returns the design file node, which is never null,
    // though it may hold no units if the source was empty or badly broken.
    AstVhdlDesignFile* parse(const std::string& filename) VL_MT_DISABLED;

    int errCount() const { return m_errCount; }

private:
    // Token access
    const VhdlToken& peek(size_t ahead = 0) VL_MT_DISABLED;
    VhdlTokType peekType(size_t ahead = 0) VL_MT_DISABLED;
    VhdlToken consume() VL_MT_DISABLED;
    bool check(VhdlTokType type) VL_MT_DISABLED;  // Consume if it matches
    bool expect(VhdlTokType type, const char* what) VL_MT_DISABLED;
    FileLine* here() VL_MT_DISABLED;

    // Diagnostics and recovery
    void syntaxError(const std::string& expected) VL_MT_DISABLED;
    void recoverTo(VhdlTokType a, VhdlTokType b = VhdlTokType::END_OF_FILE,
                   VhdlTokType c = VhdlTokType::END_OF_FILE) VL_MT_DISABLED;
    void skipToSemicolon() VL_MT_DISABLED;
    // Skip to the next plausible design unit, so that one broken unit does
    // not turn into a cascade of errors through the rest of the file
    void recoverToDesignUnit() VL_MT_DISABLED;

    // Lexical helpers
    std::string identifier(const char* what) VL_MT_DISABLED;
    bool checkEndLabel(const std::string& name) VL_MT_DISABLED;

    // Design units (clauses 3, 13)
    AstNodeVhdlUnit* parseDesignUnit() VL_MT_DISABLED;
    void parseContextClause(AstNodeVhdlUnit* unitp) VL_MT_DISABLED;
    AstVhdlContextItem* parseContextItem() VL_MT_DISABLED;
    AstNodeVhdlUnit* parseEntity() VL_MT_DISABLED;
    AstNodeVhdlUnit* parseArchitecture() VL_MT_DISABLED;
    AstNodeVhdlUnit* parsePackage() VL_MT_DISABLED;
    AstNodeVhdlUnit* parsePackageBody() VL_MT_DISABLED;
    AstNodeVhdlUnit* parseConfiguration() VL_MT_DISABLED;
    AstNodeVhdlUnit* parseContextDecl() VL_MT_DISABLED;
    AstVhdlBlockConfig* parseBlockConfig() VL_MT_DISABLED;
    AstVhdlBinding* parseBinding() VL_MT_DISABLED;

    // Declarations (clause 6)
    AstNodeVhdl* parseDeclarativeItem() VL_MT_DISABLED;
    // allowConfigSpec is false inside a configuration declaration, where a
    // leading 'for' begins a block configuration, not a configuration spec
    AstNodeVhdl* parseDeclarativeItems(bool allowConfigSpec = true) VL_MT_DISABLED;
    AstVhdlIfaceDecl* parseInterfaceList(VhdlIfaceKind dflt) VL_MT_DISABLED;
    AstVhdlIfaceDecl* parseInterfaceElement(VhdlIfaceKind dflt) VL_MT_DISABLED;
    AstNodeVhdl* parseObjectDecl() VL_MT_DISABLED;
    AstNodeVhdl* parseTypeDecl() VL_MT_DISABLED;
    AstNodeVhdl* parseTypeDefinition(const std::string& typeName) VL_MT_DISABLED;
    AstNodeVhdl* parseSubtypeDecl() VL_MT_DISABLED;
    AstVhdlSubtypeInd* parseSubtypeIndication() VL_MT_DISABLED;
    AstNodeVhdl* parseConstraint() VL_MT_DISABLED;
    AstNodeVhdl* parseSubprogram(bool inInterface = false) VL_MT_DISABLED;
    AstNodeVhdl* parseComponentDecl() VL_MT_DISABLED;
    AstNodeVhdl* parseAliasDecl() VL_MT_DISABLED;
    AstNodeVhdl* parseAttribute() VL_MT_DISABLED;
    AstNodeVhdl* parseConfigSpec() VL_MT_DISABLED;
    AstNodeVhdl* parseDisconnectSpec() VL_MT_DISABLED;
    AstNodeVhdl* parseGroup() VL_MT_DISABLED;
    AstVhdlSignature* parseSignature() VL_MT_DISABLED;
    AstVhdlEntitySpec* parseEntitySpec(bool componentForm) VL_MT_DISABLED;

    // Concurrent statements (clause 11)
    AstNodeVhdlStmt* parseConcurrentStmt() VL_MT_DISABLED;
    AstNodeVhdlStmt* parseConcurrentStmts() VL_MT_DISABLED;  // Until end/elsif/when
    AstNodeVhdlStmt* parseProcess(const std::string& label, bool postponed) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseBlock(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseGenerate(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseInstance(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseConcurrentSimple(const std::string& label, bool postponed)
        VL_MT_DISABLED;
    AstVhdlGenerateBody* parseGenerateBody() VL_MT_DISABLED;

    // Sequential statements (clause 10)
    AstNodeVhdlStmt* parseSequentialStmt() VL_MT_DISABLED;
    AstNodeVhdlStmt* parseSequentialStmts() VL_MT_DISABLED;  // Until end/else/elsif/when
    AstNodeVhdlStmt* parseIf(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseCase(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseLoop(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseWait(const std::string& label) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseAssert(const std::string& label, bool postponed) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseReport(const std::string& label, bool postponed) VL_MT_DISABLED;
    AstNodeVhdlStmt* parseAssignOrCall(const std::string& label) VL_MT_DISABLED;
    AstVhdlWaveform* parseWaveform() VL_MT_DISABLED;
    void parseDelayMechanism(AstVhdlSigAssign* nodep) VL_MT_DISABLED;

    // Expressions and names (clauses 8, 9)
    AstNodeVhdlExpr* parseExpression() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseRelation() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseShiftExpr() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseSimpleExpr() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseTerm() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseFactor() VL_MT_DISABLED;
    AstNodeVhdlExpr* parsePrimary() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseName() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseNamePrefix() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseNameSuffixes(AstNodeVhdlExpr* prefixp) VL_MT_DISABLED;
    // True if a '[' here opens a signature that is followed by a tick
    bool signaturePrecedesTick() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseParenOrAggregate() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseAllocator() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseExternalName() VL_MT_DISABLED;
    AstVhdlAssoc* parseAssociationList() VL_MT_DISABLED;
    AstVhdlChoice* parseChoices() VL_MT_DISABLED;
    AstVhdlRange* parseRange() VL_MT_DISABLED;
    AstNodeVhdl* parseDiscreteRange() VL_MT_DISABLED;
    AstNodeVhdlExpr* parseLiteral() VL_MT_DISABLED;

    // True if the tokens starting here look like a range rather than a plain
    // expression, i.e. contain a top level "to" or "downto"
    bool looksLikeRange() VL_MT_DISABLED;
};

#endif  // Guard
