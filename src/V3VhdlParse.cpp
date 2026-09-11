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
//
// Recursive descent parser for VHDL, following IEEE 1076-2008 Annex C.
//
// The parser resolves no names and checks no types. Where the grammar is
// ambiguous without that information -- notably "name ( ... )", which may be a
// function call, an indexed name, a slice or a type conversion (8.1) -- it
// records the source form verbatim and leaves the choice to later phases.
//
// On a syntax error it reports once, then resynchronizes on a statement or
// declaration boundary so that one mistake does not bury the rest of the file.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlParse.h"

#include "V3Global.h"

#include <functional>
#include <set>
#include <sstream>

VL_DEFINE_DEBUG_FUNCTIONS;

// An operator symbol names one of the reserved words, and a reserved word is
// not case sensitive (15.4.1, 15.10): IEEE's MATH_REAL declares "MOD" while
// the language writes "mod", and they are the same operator.
static std::string lowerOperator(const std::string& text) {
    std::string out = text;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Whether a string names one of the operators of 9.2, which are the only
// strings an operator symbol may be (4.5.2): not "foo", and not " + "
static bool isOperatorSymbol(const std::string& lowered) {
    static const std::set<std::string> ops{
        "and", "or",  "nand", "nor", "xor", "xnor", "=",   "/=",  "<",   "<=",  ">",   ">=",
        "?=",  "?/=", "?<",   "?<=", "?>",  "?>=",  "sll", "srl", "sla", "sra", "rol", "ror",
        "+",   "-",   "&",    "*",   "/",   "mod",  "rem", "**",  "abs", "not", "??"};
    return ops.count(lowered) != 0;
}

//######################################################################
// Construction and token access

V3VhdlParse::V3VhdlParse(FileLine* fileline, const std::string& text, V3LangCode lang,
                         const std::string& libname)
    : m_scanner{fileline, text, lang}
    , m_fileline{fileline}
    , m_lang{lang}
    , m_libname{libname} {}

const VhdlToken& V3VhdlParse::peek(size_t ahead) {
    while (m_ahead.size() <= ahead) {
        if (m_atEnd) break;
        VhdlToken tok = m_scanner.next();
        // A tool directive is not part of the grammar (15.11), so it never
        // reaches the parser
        if (tok.type == VhdlTokType::TOOL_DIRECTIVE) continue;
        if (tok.type == VhdlTokType::END_OF_FILE) m_atEnd = true;
        m_ahead.push_back(tok);
    }
    if (m_ahead.size() <= ahead) return m_ahead.back();  // The END_OF_FILE token
    return m_ahead[ahead];
}

VhdlTokType V3VhdlParse::peekType(size_t ahead) { return peek(ahead).type; }

// A lexical element, spelled so that two of them are the same string exactly
// when 4.10 counts them the same: case does not matter in a basic identifier,
// and a numeric literal may be written differently if its value is the same
static std::string conformanceSpelling(const VhdlToken& tok) {
    switch (tok.type) {
    case VhdlTokType::IDENTIFIER: return tok.extendedId ? "\\" + tok.text + "\\" : tok.text;
    case VhdlTokType::INTEGER_LIT: return "#" + std::to_string(tok.intVal);
    case VhdlTokType::REAL_LIT: {
        std::ostringstream os;
        os.precision(17);
        os << "#" << tok.realVal;
        return os.str();
    }
    case VhdlTokType::STRING_LIT: return "\"" + tok.text + "\"";
    case VhdlTokType::CHARACTER_LIT: return "'" + tok.text + "'";
    case VhdlTokType::BITSTRING_LIT: return std::string(1, tok.baseSpec) + "\"" + tok.text + "\"";
    default: return "%" + std::to_string(static_cast<int>(tok.type));
    }
}

VhdlToken V3VhdlParse::consume() {
    const VhdlToken tok = peek();
    if (tok.type != VhdlTokType::END_OF_FILE) m_ahead.pop_front();
    if (m_specp) {
        if (tok.type == VhdlTokType::DOT && m_specp->size() > 1) {
            // A simple name may be written as an expanded name that ends in
            // it (4.10), so what comes before a dot is not compared: drop
            // the prefix already recorded
            const size_t end = m_specp->size() - 1;  // Its trailing space
            const size_t start = m_specp->rfind(' ', end - 1);
            m_specp->erase(start == std::string::npos ? 0 : start + 1);
        } else {
            *m_specp += conformanceSpelling(tok);
            *m_specp += ' ';
        }
    }
    // A semicolon ends a statement or declaration, which is far enough from
    // the last mistake to trust diagnostics again
    if (tok.type == VhdlTokType::SEMICOLON) m_panic = false;
    return tok;
}

bool V3VhdlParse::check(VhdlTokType type) {
    if (peekType() != type) return false;
    consume();
    return true;
}

FileLine* V3VhdlParse::here() { return peek().flp; }

bool V3VhdlParse::expect(VhdlTokType type, const char* what) {
    if (check(type)) return true;
    syntaxError(what);
    return false;
}

//######################################################################
// Diagnostics and recovery

void V3VhdlParse::syntaxError(const std::string& expected) {
    ++m_errCount;
    // A failed expectation does not consume the offending token, so without
    // this every later check would fail at the same place. Report the first
    // problem and stay quiet until a statement boundary resynchronizes us.
    if (m_panic) return;
    m_panic = true;
    if (m_printed >= MAX_ERRORS) return;  // One bad file should not flood the log
    ++m_printed;
    const VhdlToken& tok = peek();
    std::string found = vhdlTokName(tok.type);
    if (tok.type == VhdlTokType::IDENTIFIER) found += " '" + tok.text + "'";
    tok.flp->v3error("Syntax error: expected " << expected << ", found " << found);
    if (m_printed == MAX_ERRORS) {
        tok.flp->v3error("Too many syntax errors; not reporting further errors in this file");
    }
}

void V3VhdlParse::recoverTo(VhdlTokType a, VhdlTokType b, VhdlTokType c) {
    // Skip forward to a token that can plausibly start something new, so one
    // error does not cascade through the rest of the file
    while (peekType() != VhdlTokType::END_OF_FILE) {
        const VhdlTokType t = peekType();
        if (t == a || (b != VhdlTokType::END_OF_FILE && t == b)
            || (c != VhdlTokType::END_OF_FILE && t == c)) {
            return;
        }
        consume();
    }
}

void V3VhdlParse::skipToSemicolon() {
    recoverTo(VhdlTokType::SEMICOLON);
    check(VhdlTokType::SEMICOLON);
}

void V3VhdlParse::recoverToDesignUnit() {
    m_panic = false;
    // Always make progress, then stop at whatever could begin the next unit
    if (peekType() != VhdlTokType::END_OF_FILE) consume();
    while (peekType() != VhdlTokType::END_OF_FILE) {
        switch (peekType()) {
        case VhdlTokType::KWD_ENTITY:
        case VhdlTokType::KWD_ARCHITECTURE:
        case VhdlTokType::KWD_PACKAGE:
        case VhdlTokType::KWD_CONFIGURATION:
        case VhdlTokType::KWD_CONTEXT:
        case VhdlTokType::KWD_LIBRARY: return;
        default: break;
        }
        consume();
    }
}

//######################################################################
// Lexical helpers

std::string V3VhdlParse::identifier(const char* what) {
    if (peekType() == VhdlTokType::IDENTIFIER) return consume().text;
    syntaxError(what);
    return "";
}

bool V3VhdlParse::checkEndLabel(const std::string& name) {
    // The identifier after "end" must repeat the construct's label (3.2 etc.)
    if (peekType() != VhdlTokType::IDENTIFIER) return true;
    const VhdlToken tok = consume();
    if (!name.empty() && tok.text != name) {
        tok.flp->v3error("Label at 'end' is '" << tok.text << "' but the construct is named '"
                                               << name << "'");
        return false;
    }
    return true;
}

//######################################################################
// Design file and design units (clauses 3, 13)

AstVhdlDesignFile* V3VhdlParse::parse(const std::string& filename) {
    AstVhdlDesignFile* const filep = new AstVhdlDesignFile{m_fileline, filename};
    while (peekType() != VhdlTokType::END_OF_FILE) {
        const int errBefore = m_errCount;
        const size_t before = m_ahead.size();
        AstNodeVhdlUnit* const unitp = parseDesignUnit();
        if (unitp) {
            unitp->libname(m_libname);
            filep->addUnitsp(unitp);
        }
        if (m_errCount > errBefore) {
            // This unit did not parse; resynchronize rather than reporting the
            // same confusion again for every token that follows
            recoverToDesignUnit();
        } else if (!unitp && m_ahead.size() == before
                   && peekType() != VhdlTokType::END_OF_FILE) {
            consume();  // Guarantee progress even when nothing matched
        }
    }
    return filep;
}

AstNodeVhdlUnit* V3VhdlParse::parseDesignUnit() {
    // design_unit ::= context_clause library_unit  (13.1)
    AstVhdlContextItem* contextp = nullptr;
    while (peekType() == VhdlTokType::KWD_LIBRARY || peekType() == VhdlTokType::KWD_USE
           || (peekType() == VhdlTokType::KWD_CONTEXT
               && peekType(2) != VhdlTokType::KWD_IS)) {
        // "context id is" is a context declaration, not a context reference
        AstVhdlContextItem* const itemp = parseContextItem();
        contextp = AstNode::addNextNull(contextp, itemp);
    }

    AstNodeVhdlUnit* unitp = nullptr;
    switch (peekType()) {
    case VhdlTokType::KWD_ENTITY: unitp = parseEntity(); break;
    case VhdlTokType::KWD_ARCHITECTURE: unitp = parseArchitecture(); break;
    case VhdlTokType::KWD_PACKAGE:
        unitp = (peekType(1) == VhdlTokType::KWD_BODY) ? parsePackageBody() : parsePackage();
        break;
    case VhdlTokType::KWD_CONFIGURATION: unitp = parseConfiguration(); break;
    case VhdlTokType::KWD_CONTEXT: unitp = parseContextDecl(); break;
    case VhdlTokType::END_OF_FILE: break;
    default:
        syntaxError("a design unit (entity, architecture, package, configuration or context)");
        break;
    }

    if (unitp) {
        unitp->addContextp(contextp);
    } else if (contextp) {
        VL_DO_DANGLING(contextp->deleteTree(), contextp);
    }
    return unitp;
}

AstVhdlContextItem* V3VhdlParse::parseContextItem() {
    // library_clause | use_clause | context_reference  (13.2, 13.4)
    FileLine* const flp = here();
    VhdlContextKind kind;
    if (check(VhdlTokType::KWD_LIBRARY)) {
        kind = VhdlContextKind::LIBRARY;
    } else if (check(VhdlTokType::KWD_USE)) {
        kind = VhdlContextKind::USE;
    } else {
        consume();  // "context"
        kind = VhdlContextKind::CONTEXT_REF;
    }
    AstVhdlContextItem* const itemp = new AstVhdlContextItem{flp, kind};
    do {
        if (kind == VhdlContextKind::LIBRARY) {
            const std::string name = identifier("a library name");
            if (name.empty()) break;
            itemp->addNamesp(new AstVhdlName{flp, name});
        } else {
            AstNodeVhdlExpr* const namep = parseName();
            if (!namep) break;
            itemp->addNamesp(namep);
        }
    } while (check(VhdlTokType::COMMA));
    expect(VhdlTokType::SEMICOLON, "';' after a context item");
    return itemp;
}

AstNodeVhdlUnit* V3VhdlParse::parseEntity() {
    // entity_declaration ::= ENTITY id IS entity_header entity_declarative_part
    //                        [ BEGIN concurrent_statements ] END [ ENTITY ] [ id ] ;   (3.2)
    FileLine* const flp = here();
    consume();  // "entity"
    const std::string name = identifier("an entity name");
    expect(VhdlTokType::KWD_IS, "'is' after the entity name");
    AstVhdlEntity* const nodep = new AstVhdlEntity{flp, name};

    if (check(VhdlTokType::KWD_GENERIC)) {
        expect(VhdlTokType::LPAREN, "'(' after 'generic'");
        nodep->addGenericsp(parseInterfaceList(VhdlIfaceKind::CONSTANT));
        expect(VhdlTokType::RPAREN, "')' after the generic clause");
        expect(VhdlTokType::SEMICOLON, "';' after the generic clause");
    }
    if (check(VhdlTokType::KWD_PORT)) {
        expect(VhdlTokType::LPAREN, "'(' after 'port'");
        nodep->addPortsp(parseInterfaceList(VhdlIfaceKind::SIGNAL));
        expect(VhdlTokType::RPAREN, "')' after the port clause");
        expect(VhdlTokType::SEMICOLON, "';' after the port clause");
    }
    nodep->addDeclsp(parseDeclarativeItems());
    if (check(VhdlTokType::KWD_BEGIN)) {
        // Passive concurrent statements are legal here, and are kept with the
        // declarations so that lowering sees them in one place
        nodep->addDeclsp(parseConcurrentStmts());
    }
    expect(VhdlTokType::KWD_END, "'end' to close the entity");
    check(VhdlTokType::KWD_ENTITY);
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstNodeVhdlUnit* V3VhdlParse::parseArchitecture() {
    // architecture_body ::= ARCHITECTURE id OF entity_name IS decls
    //                       BEGIN statements END [ ARCHITECTURE ] [ id ] ;   (3.3)
    FileLine* const flp = here();
    consume();  // "architecture"
    const std::string name = identifier("an architecture name");
    expect(VhdlTokType::KWD_OF, "'of' after the architecture name");
    AstNodeVhdlExpr* entityNamep = parseName();
    if (!entityNamep) entityNamep = new AstVhdlName{flp, ""};
    expect(VhdlTokType::KWD_IS, "'is' after the entity name");

    AstVhdlArchitecture* const nodep = new AstVhdlArchitecture{flp, name, entityNamep};
    nodep->addDeclsp(parseDeclarativeItems());
    expect(VhdlTokType::KWD_BEGIN, "'begin' to start the architecture body");
    nodep->addStmtsp(parseConcurrentStmts());
    expect(VhdlTokType::KWD_END, "'end' to close the architecture");
    check(VhdlTokType::KWD_ARCHITECTURE);
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstNodeVhdlUnit* V3VhdlParse::parsePackage() {
    // package_declaration ::= PACKAGE id IS [ generic_clause [ generic_map ; ] ]
    //                         decls END [ PACKAGE ] [ id ] ;   (4.7)
    // Also package_instantiation_declaration: PACKAGE id IS NEW name [ generic_map ] ;  (4.9)
    FileLine* const flp = here();
    consume();  // "package"
    const std::string name = identifier("a package name");
    expect(VhdlTokType::KWD_IS, "'is' after the package name");

    if (check(VhdlTokType::KWD_NEW)) {
        AstNodeVhdlExpr* uninstp = parseName();
        if (!uninstp) uninstp = new AstVhdlName{flp, ""};
        AstVhdlPackageInst* const instp = new AstVhdlPackageInst{flp, name, uninstp};
        if (check(VhdlTokType::KWD_GENERIC)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
            expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
            instp->addGenericMapp(parseAssociationList());
            expect(VhdlTokType::RPAREN, "')' after the generic map");
        }
        expect(VhdlTokType::SEMICOLON, "';' after the package instantiation");
        return instp;
    }

    AstVhdlPackage* const nodep = new AstVhdlPackage{flp, name};
    if (check(VhdlTokType::KWD_GENERIC)) {
        expect(VhdlTokType::LPAREN, "'(' after 'generic'");
        nodep->addGenericsp(parseInterfaceList(VhdlIfaceKind::CONSTANT));
        expect(VhdlTokType::RPAREN, "')' after the generic clause");
        expect(VhdlTokType::SEMICOLON, "';' after the generic clause");
        if (check(VhdlTokType::KWD_GENERIC)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
            expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
            nodep->addGenericMapp(parseAssociationList());
            expect(VhdlTokType::RPAREN, "')' after the generic map");
            expect(VhdlTokType::SEMICOLON, "';' after the generic map");
        }
    }
    nodep->addDeclsp(parseDeclarativeItems());
    expect(VhdlTokType::KWD_END, "'end' to close the package");
    check(VhdlTokType::KWD_PACKAGE);
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstNodeVhdlUnit* V3VhdlParse::parsePackageBody() {
    // package_body ::= PACKAGE BODY id IS decls END [ PACKAGE BODY ] [ id ] ;   (4.8)
    FileLine* const flp = here();
    consume();  // "package"
    consume();  // "body"
    const std::string name = identifier("a package body name");
    expect(VhdlTokType::KWD_IS, "'is' after the package body name");
    AstVhdlPackageBody* const nodep = new AstVhdlPackageBody{flp, name};
    nodep->addDeclsp(parseDeclarativeItems());
    expect(VhdlTokType::KWD_END, "'end' to close the package body");
    if (check(VhdlTokType::KWD_PACKAGE)) check(VhdlTokType::KWD_BODY);
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstNodeVhdlUnit* V3VhdlParse::parseConfiguration() {
    // configuration_declaration ::= CONFIGURATION id OF entity_name IS decls
    //                               block_configuration END [ CONFIGURATION ] [ id ] ;  (3.4)
    FileLine* const flp = here();
    consume();  // "configuration"
    const std::string name = identifier("a configuration name");
    expect(VhdlTokType::KWD_OF, "'of' after the configuration name");
    AstNodeVhdlExpr* entityNamep = parseName();
    if (!entityNamep) entityNamep = new AstVhdlName{flp, ""};
    expect(VhdlTokType::KWD_IS, "'is' after the entity name");
    AstVhdlConfiguration* const nodep = new AstVhdlConfiguration{flp, name, entityNamep};
    nodep->addDeclsp(parseDeclarativeItems(false));
    if (peekType() == VhdlTokType::KWD_FOR) nodep->blockConfigp(parseBlockConfig());
    expect(VhdlTokType::KWD_END, "'end' to close the configuration");
    check(VhdlTokType::KWD_CONFIGURATION);
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstNodeVhdlUnit* V3VhdlParse::parseContextDecl() {
    // context_declaration ::= CONTEXT id IS context_clause END [ CONTEXT ] [ id ] ;  (13.3)
    FileLine* const flp = here();
    consume();  // "context"
    const std::string name = identifier("a context name");
    expect(VhdlTokType::KWD_IS, "'is' after the context name");
    AstVhdlContextDecl* const nodep = new AstVhdlContextDecl{flp, name};
    while (peekType() == VhdlTokType::KWD_LIBRARY || peekType() == VhdlTokType::KWD_USE
           || peekType() == VhdlTokType::KWD_CONTEXT) {
        nodep->addContextp(parseContextItem());
    }
    expect(VhdlTokType::KWD_END, "'end' to close the context declaration");
    check(VhdlTokType::KWD_CONTEXT);
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstVhdlBlockConfig* V3VhdlParse::parseBlockConfig() {
    // block_configuration ::= FOR block_spec { use_clause } { config_item } END FOR ;  (3.4.2)
    // component_configuration ::= FOR component_spec [ binding ; ] [ block_config ]
    //                             END FOR ;   (3.4.3)
    FileLine* const flp = here();
    consume();  // "for"
    AstVhdlBlockConfig* const nodep = new AstVhdlBlockConfig{flp};
    // The specification is either a block name (possibly indexed) or a
    // component specification "labels : component_name"
    if (peekType() != VhdlTokType::KWD_END) {
        AstVhdlEntitySpec* const specp = parseEntitySpec(true);
        nodep->specp(specp);
    }
    // "use entity ..." and "use configuration ..." are binding indications, not
    // use clauses (7.3.2), so only take the latter here
    while (peekType() == VhdlTokType::KWD_USE && peekType(1) != VhdlTokType::KWD_ENTITY
           && peekType(1) != VhdlTokType::KWD_CONFIGURATION
           && peekType(1) != VhdlTokType::KWD_OPEN) {
        nodep->addUsesp(parseContextItem());
    }
    if (peekType() == VhdlTokType::KWD_USE || peekType() == VhdlTokType::KWD_ENTITY
        || peekType() == VhdlTokType::KWD_CONFIGURATION || peekType() == VhdlTokType::KWD_OPEN
        || peekType() == VhdlTokType::KWD_GENERIC || peekType() == VhdlTokType::KWD_PORT) {
        nodep->bindingp(parseBinding());
        expect(VhdlTokType::SEMICOLON, "';' after the binding indication");
    }
    while (peekType() == VhdlTokType::KWD_FOR) nodep->addItemsp(parseBlockConfig());
    expect(VhdlTokType::KWD_END, "'end' to close the configuration item");
    expect(VhdlTokType::KWD_FOR, "'for' after 'end'");
    expect(VhdlTokType::SEMICOLON, "';' after 'end for'");
    return nodep;
}

AstVhdlBinding* V3VhdlParse::parseBinding() {
    // binding_indication ::= [ USE entity_aspect ] [ generic_map ] [ port_map ]  (7.3.2)
    FileLine* const flp = here();
    AstVhdlBinding* const nodep = new AstVhdlBinding{flp};
    check(VhdlTokType::KWD_USE);
    if (check(VhdlTokType::KWD_ENTITY)) {
        nodep->kind("entity");
        nodep->unitp(parseName());
        if (check(VhdlTokType::LPAREN)) {
            nodep->archName(identifier("an architecture name"));
            expect(VhdlTokType::RPAREN, "')' after the architecture name");
        }
    } else if (check(VhdlTokType::KWD_CONFIGURATION)) {
        nodep->kind("configuration");
        nodep->unitp(parseName());
    } else if (check(VhdlTokType::KWD_OPEN)) {
        nodep->kind("open");
    }
    if (check(VhdlTokType::KWD_GENERIC)) {
        expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
        expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
        nodep->addGenericMapp(parseAssociationList());
        expect(VhdlTokType::RPAREN, "')' after the generic map");
    }
    if (check(VhdlTokType::KWD_PORT)) {
        expect(VhdlTokType::KWD_MAP, "'map' after 'port'");
        expect(VhdlTokType::LPAREN, "'(' after 'port map'");
        nodep->addPortMapp(parseAssociationList());
        expect(VhdlTokType::RPAREN, "')' after the port map");
    }
    return nodep;
}

//######################################################################
// Declarations (clause 6)

AstNodeVhdl* V3VhdlParse::parseDeclarativeItems(bool allowConfigSpec) {
    // Declarative items run until "begin" or "end", or until something that
    // cannot start a declaration
    AstNodeVhdl* listp = nullptr;
    while (true) {
        switch (peekType()) {
        case VhdlTokType::KWD_BEGIN:
        case VhdlTokType::KWD_END:
        case VhdlTokType::END_OF_FILE: return listp;
        default: break;
        }
        if (!allowConfigSpec && peekType() == VhdlTokType::KWD_FOR) return listp;
        const size_t before = m_ahead.size();
        FileLine* const startp = here();
        AstNodeVhdl* const itemp = parseDeclarativeItem();
        if (itemp) {
            listp = AstNode::addNextNull(listp, itemp);
        } else if (m_ahead.size() == before && here() == startp) {
            // Nothing consumed and nothing produced: this is not a declaration
            return listp;
        }
    }
}

AstNodeVhdl* V3VhdlParse::parseDeclarativeItem() {
    switch (peekType()) {
    case VhdlTokType::KWD_TYPE: return parseTypeDecl();
    case VhdlTokType::KWD_SUBTYPE: return parseSubtypeDecl();
    case VhdlTokType::KWD_CONSTANT:
    case VhdlTokType::KWD_SIGNAL:
    case VhdlTokType::KWD_VARIABLE:
    case VhdlTokType::KWD_SHARED:
    case VhdlTokType::KWD_FILE: return parseObjectDecl();
    case VhdlTokType::KWD_FUNCTION:
    case VhdlTokType::KWD_PROCEDURE:
    case VhdlTokType::KWD_PURE:
    case VhdlTokType::KWD_IMPURE: return parseSubprogram();
    case VhdlTokType::KWD_COMPONENT: return parseComponentDecl();
    case VhdlTokType::KWD_ALIAS: return parseAliasDecl();
    case VhdlTokType::KWD_ATTRIBUTE: return parseAttribute();
    case VhdlTokType::KWD_FOR: return parseConfigSpec();
    case VhdlTokType::KWD_DISCONNECT: return parseDisconnectSpec();
    case VhdlTokType::KWD_GROUP: return parseGroup();
    case VhdlTokType::KWD_USE: return parseContextItem();
    case VhdlTokType::KWD_PACKAGE:
        return (peekType(1) == VhdlTokType::KWD_BODY) ? parsePackageBody() : parsePackage();
    case VhdlTokType::KWD_CONTEXT: return parseContextItem();
    default: return nullptr;
    }
}

AstVhdlIfaceDecl* V3VhdlParse::parseInterfaceList(VhdlIfaceKind dflt) {
    // interface_list ::= interface_element { ; interface_element }   (6.5.6)
    AstVhdlIfaceDecl* listp = nullptr;
    // An interface list has at least one element: "port ( )" declares no
    // ports by leaving the clause out, not by leaving it empty
    if (peekType() == VhdlTokType::RPAREN) {
        here()->v3error("An interface list must declare at least one element");
    }
    do {
        if (peekType() == VhdlTokType::RPAREN) break;  // Tolerate a trailing ';'
        AstVhdlIfaceDecl* const elemp = parseInterfaceElement(dflt);
        if (!elemp) {
            recoverTo(VhdlTokType::SEMICOLON, VhdlTokType::RPAREN);
            if (peekType() == VhdlTokType::RPAREN) break;
        }
        listp = AstNode::addNextNull(listp, elemp);
    } while (check(VhdlTokType::SEMICOLON));
    return listp;
}

AstVhdlIfaceDecl* V3VhdlParse::parseInterfaceElement(VhdlIfaceKind dflt) {
    // interface_declaration ::= [ class ] id_list : [ mode ] subtype [ BUS ] [ := expr ]
    // Also the VHDL-2008 type, subprogram and package forms (6.5.5)
    FileLine* const flp = here();

    if (check(VhdlTokType::KWD_TYPE)) {
        const std::string name = identifier("a generic type name");
        AstVhdlIfaceDecl* const nodep = new AstVhdlIfaceDecl{flp, name, VhdlIfaceKind::TYPE};
        if (check(VhdlTokType::KWD_IS)) {  // VHDL-2019 interface type definition
            if (!check(VhdlTokType::BOX)) parseSubtypeIndication();
        }
        return nodep;
    }
    if (peekType() == VhdlTokType::KWD_FUNCTION || peekType() == VhdlTokType::KWD_PROCEDURE
        || peekType() == VhdlTokType::KWD_PURE || peekType() == VhdlTokType::KWD_IMPURE) {
        AstNodeVhdl* const subp = parseSubprogram(true);
        // An interface declaration carrying the subprogram's own declaration,
        // since the body that uses it needs the profile to resolve calls
        const std::string name = subp ? subp->name() : "";
        AstVhdlIfaceDecl* const nodep
            = new AstVhdlIfaceDecl{flp, name, VhdlIfaceKind::SUBPROGRAM};
        if (AstVhdlSubprogDecl* const declp = VN_CAST(subp, VhdlSubprogDecl)) {
            nodep->profilep(declp);
        } else if (subp) {
            VL_DO_DANGLING(subp->deleteTree(), subp);
        }
        return nodep;
    }
    if (check(VhdlTokType::KWD_PACKAGE)) {
        const std::string name = identifier("a generic package name");
        AstVhdlIfaceDecl* const nodep = new AstVhdlIfaceDecl{flp, name, VhdlIfaceKind::PACKAGE};
        expect(VhdlTokType::KWD_IS, "'is' in a generic package interface");
        expect(VhdlTokType::KWD_NEW, "'new' in a generic package interface");
        // Which package the formal stands for is what makes its declarations
        // nameable inside the unit that declares it (6.5.5)
        if (AstNodeVhdlExpr* const fromp = parseName()) {
            std::string dotted;
            const std::function<void(const AstNodeVhdlExpr*)> flatten
                = [&](const AstNodeVhdlExpr* np) {
                      if (const AstVhdlSelected* const selp = VN_CAST(np, VhdlSelected)) {
                          flatten(selp->prefixp());
                          flatten(selp->suffixp());
                          return;
                      }
                      if (const AstVhdlName* const namep = VN_CAST(np, VhdlName)) {
                          if (!dotted.empty()) dotted += ".";
                          dotted += namep->name();
                      }
                  };
            flatten(fromp);
            nodep->newFrom(dotted);
            VL_DO_DANGLING(fromp->deleteTree(), fromp);
        }
        if (check(VhdlTokType::KWD_GENERIC)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
            expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
            if (check(VhdlTokType::BOX)) {
                nodep->boxMap(true);
            } else {
                parseAssociationList();
            }
            expect(VhdlTokType::RPAREN, "')' after the generic map");
        }
        return nodep;
    }

    VhdlIfaceKind kind = dflt;
    bool classGiven = false;
    if (check(VhdlTokType::KWD_CONSTANT)) {
        kind = VhdlIfaceKind::CONSTANT;
        classGiven = true;
    } else if (check(VhdlTokType::KWD_SIGNAL)) {
        kind = VhdlIfaceKind::SIGNAL;
        classGiven = true;
    } else if (check(VhdlTokType::KWD_VARIABLE)) {
        kind = VhdlIfaceKind::VARIABLE;
        classGiven = true;
    } else if (check(VhdlTokType::KWD_FILE)) {
        kind = VhdlIfaceKind::FILE;
        classGiven = true;
    }
    // A port clause is parsed with SIGNAL as its class, and a port is a
    // signal whatever is written (6.5.6.3)
    if (classGiven && dflt == VhdlIfaceKind::SIGNAL && kind != VhdlIfaceKind::SIGNAL) {
        flp->v3error("A port is a signal, so it cannot be declared "
                     << (kind == VhdlIfaceKind::CONSTANT   ? "a constant"
                         : kind == VhdlIfaceKind::VARIABLE ? "a variable"
                                                           : "a file"));
    }

    // identifier_list
    std::vector<std::pair<std::string, FileLine*>> names;
    do {
        FileLine* const nflp = here();
        const std::string name = identifier("an interface object name");
        if (name.empty()) return nullptr;
        names.emplace_back(name, nflp);
    } while (check(VhdlTokType::COMMA));
    if (!expect(VhdlTokType::COLON, "':' after the interface object names")) return nullptr;

    // mode
    VhdlMode mode = VhdlMode::IN;
    bool modeGiven = false;
    if (check(VhdlTokType::KWD_IN)) {
        mode = VhdlMode::IN;
        modeGiven = true;
    } else if (check(VhdlTokType::KWD_OUT)) {
        mode = VhdlMode::OUT;
        modeGiven = true;
    } else if (check(VhdlTokType::KWD_INOUT)) {
        mode = VhdlMode::INOUT;
        modeGiven = true;
    } else if (check(VhdlTokType::KWD_BUFFER)) {
        mode = VhdlMode::BUFFER;
        modeGiven = true;
    } else if (check(VhdlTokType::KWD_LINKAGE)) {
        mode = VhdlMode::LINKAGE;
        modeGiven = true;
    }

    AstVhdlSubtypeInd* const subtypep = parseSubtypeIndication();
    VhdlSignalKind sigKind = VhdlSignalKind::NONE;
    if (check(VhdlTokType::KWD_BUS)) sigKind = VhdlSignalKind::BUS;
    AstNodeVhdlExpr* defaultp = nullptr;
    if (check(VhdlTokType::VAR_ASSIGN)) defaultp = parseExpression();

    // One node per name, each with its own copy of the shared parts
    AstVhdlIfaceDecl* listp = nullptr;
    for (size_t i = 0; i < names.size(); ++i) {
        AstVhdlIfaceDecl* const nodep = new AstVhdlIfaceDecl{names[i].second, names[i].first, kind};
        if (modeGiven) nodep->mode(mode);
        nodep->signalKind(sigKind);
        const bool last = (i + 1 == names.size());
        if (subtypep) nodep->subtypep(last ? subtypep : subtypep->cloneTree(false));
        if (defaultp) nodep->defaultp(last ? defaultp : defaultp->cloneTree(false));
        listp = AstNode::addNextNull(listp, nodep);
    }
    return listp;
}

AstNodeVhdl* V3VhdlParse::parseObjectDecl() {
    // constant/signal/variable/file declaration (6.4.2)
    VhdlObjKind kind = VhdlObjKind::CONSTANT;
    if (check(VhdlTokType::KWD_CONSTANT)) {
        kind = VhdlObjKind::CONSTANT;
    } else if (check(VhdlTokType::KWD_SIGNAL)) {
        kind = VhdlObjKind::SIGNAL;
    } else if (check(VhdlTokType::KWD_VARIABLE)) {
        kind = VhdlObjKind::VARIABLE;
    } else if (check(VhdlTokType::KWD_SHARED)) {
        expect(VhdlTokType::KWD_VARIABLE, "'variable' after 'shared'");
        kind = VhdlObjKind::SHARED_VARIABLE;
    } else {
        consume();  // "file"
        kind = VhdlObjKind::FILE;
    }

    std::vector<std::pair<std::string, FileLine*>> names;
    do {
        FileLine* const nflp = here();
        const std::string name = identifier("an object name");
        if (name.empty()) {
            skipToSemicolon();
            return nullptr;
        }
        names.emplace_back(name, nflp);
    } while (check(VhdlTokType::COMMA));
    if (!expect(VhdlTokType::COLON, "':' after the object names")) {
        skipToSemicolon();
        return nullptr;
    }

    AstVhdlSubtypeInd* const subtypep = parseSubtypeIndication();
    VhdlSignalKind sigKind = VhdlSignalKind::NONE;
    if (check(VhdlTokType::KWD_REGISTER)) {
        sigKind = VhdlSignalKind::REGISTER;
    } else if (check(VhdlTokType::KWD_BUS)) {
        sigKind = VhdlSignalKind::BUS;
    }

    AstNodeVhdlExpr* defaultp = nullptr;
    AstNodeVhdlExpr* fileOpenp = nullptr;
    AstNodeVhdlExpr* fileNamep = nullptr;
    if (kind == VhdlObjKind::FILE) {
        // file_open_information ::= [ OPEN expr ] IS file_logical_name   (6.4.2.5)
        if (check(VhdlTokType::KWD_OPEN)) fileOpenp = parseExpression();
        if (check(VhdlTokType::KWD_IS)) fileNamep = parseExpression();
    } else if (check(VhdlTokType::VAR_ASSIGN)) {
        defaultp = parseExpression();
    }
    expect(VhdlTokType::SEMICOLON, "';' after the declaration");

    AstNodeVhdl* listp = nullptr;
    for (size_t i = 0; i < names.size(); ++i) {
        AstVhdlObjectDecl* const nodep
            = new AstVhdlObjectDecl{names[i].second, names[i].first, kind};
        nodep->signalKind(sigKind);
        const bool last = (i + 1 == names.size());
        if (subtypep) nodep->subtypep(last ? subtypep : subtypep->cloneTree(false));
        if (defaultp) nodep->defaultp(last ? defaultp : defaultp->cloneTree(false));
        if (fileOpenp) nodep->fileOpenp(last ? fileOpenp : fileOpenp->cloneTree(false));
        if (fileNamep) nodep->fileNamep(last ? fileNamep : fileNamep->cloneTree(false));
        listp = AstNode::addNextNull(listp, nodep);
    }
    return listp;
}

AstNodeVhdl* V3VhdlParse::parseTypeDecl() {
    // full_type_declaration | incomplete_type_declaration (6.2, 5.4.2)
    FileLine* const flp = here();
    consume();  // "type"
    const std::string name = identifier("a type name");
    if (check(VhdlTokType::SEMICOLON)) {
        return new AstVhdlTypeDecl{flp, name, nullptr};  // Incomplete
    }
    if (!expect(VhdlTokType::KWD_IS, "'is' after the type name")) {
        skipToSemicolon();
        return nullptr;
    }
    AstNodeVhdl* const defp = parseTypeDefinition(name);
    expect(VhdlTokType::SEMICOLON, "';' after the type definition");
    return new AstVhdlTypeDecl{flp, name, defp};
}

AstNodeVhdl* V3VhdlParse::parseTypeDefinition(const std::string& typeName) {
    FileLine* const flp = here();
    switch (peekType()) {
    case VhdlTokType::LPAREN: {
        // enumeration_type_definition (5.2.2)
        consume();
        AstVhdlEnumType* const nodep = new AstVhdlEnumType{flp};
        do {
            FileLine* const lflp = here();
            if (peekType() == VhdlTokType::IDENTIFIER) {
                nodep->addLiteralsp(new AstVhdlEnumLiteral{lflp, consume().text, false});
            } else if (peekType() == VhdlTokType::CHARACTER_LIT) {
                nodep->addLiteralsp(new AstVhdlEnumLiteral{lflp, consume().text, true});
            } else {
                syntaxError("an enumeration literal");
                break;
            }
        } while (check(VhdlTokType::COMMA));
        expect(VhdlTokType::RPAREN, "')' after the enumeration literals");
        return nodep;
    }
    case VhdlTokType::KWD_RANGE: {
        // integer, floating or physical type definition (5.2.3, 5.2.4, 5.2.5)
        consume();
        AstVhdlRange* const rangep = parseRange();
        if (check(VhdlTokType::KWD_UNITS)) {
            AstVhdlPhysicalType* const nodep = new AstVhdlPhysicalType{flp, rangep};
            // The primary unit has no value; secondary units are "name = expr"
            while (peekType() == VhdlTokType::IDENTIFIER) {
                FileLine* const uflp = here();
                const std::string uname = consume().text;
                AstNodeVhdlExpr* valuep = nullptr;
                if (check(VhdlTokType::EQ)) valuep = parseExpression();
                nodep->addUnitsp(new AstVhdlUnitDecl{uflp, uname, valuep});
                expect(VhdlTokType::SEMICOLON, "';' after a unit declaration");
            }
            expect(VhdlTokType::KWD_END, "'end' to close the units");
            expect(VhdlTokType::KWD_UNITS, "'units' after 'end'");
            checkEndLabel(typeName);
            return nodep;
        }
        return new AstVhdlNumericType{flp, rangep};
    }
    case VhdlTokType::KWD_ARRAY: {
        // array_type_definition (5.3.2)
        consume();
        expect(VhdlTokType::LPAREN, "'(' after 'array'");
        AstVhdlIndexConstraint* const indexp = new AstVhdlIndexConstraint{flp};
        // An array type is either unbounded in every index or constrained in
        // every index (5.3.2.1): its object is either given all its bounds
        // or already has them all
        bool sawBox = false;
        bool sawRange = false;
        bool mixed = false;
        do {
            FileLine* const iflp = here();
            // Either "type_mark range <>" (unbounded) or a discrete range
            const bool box = peekType() == VhdlTokType::IDENTIFIER
                             && peekType(1) == VhdlTokType::KWD_RANGE
                             && peekType(2) == VhdlTokType::BOX;
            if (!mixed && (box ? sawRange : sawBox)) {
                iflp->v3error("An array type cannot have both an index left open with"
                              " 'range <>' and one with bounds; either every index is open"
                              " or none is");
                mixed = true;
            }
            (box ? sawBox : sawRange) = true;
            if (box) {
                AstNodeVhdlExpr* const markp = parseName();
                consume();  // "range"
                consume();  // "<>"
                AstVhdlSubtypeInd* const sip = new AstVhdlSubtypeInd{iflp, markp};
                sip->constraintp(new AstVhdlBox{iflp});
                indexp->addRangesp(sip);
            } else {
                indexp->addRangesp(parseDiscreteRange());
            }
        } while (check(VhdlTokType::COMMA));
        expect(VhdlTokType::RPAREN, "')' after the array index");
        expect(VhdlTokType::KWD_OF, "'of' after the array index");
        AstVhdlSubtypeInd* elemp = parseSubtypeIndication();
        if (!elemp) elemp = new AstVhdlSubtypeInd{flp, nullptr};
        return new AstVhdlArrayType{flp, indexp, elemp};
    }
    case VhdlTokType::KWD_RECORD: {
        // record_type_definition (5.3.3)
        consume();
        AstVhdlRecordType* const nodep = new AstVhdlRecordType{flp};
        // A record has at least one element
        if (peekType() == VhdlTokType::KWD_END) {
            flp->v3error("A record type must declare at least one element");
        }
        while (peekType() == VhdlTokType::IDENTIFIER) {
            std::vector<std::pair<std::string, FileLine*>> names;
            do {
                FileLine* const nflp = here();
                const std::string nm = identifier("a record element name");
                if (nm.empty()) break;
                names.emplace_back(nm, nflp);
            } while (check(VhdlTokType::COMMA));
            expect(VhdlTokType::COLON, "':' after the record element names");
            AstVhdlSubtypeInd* const sip = parseSubtypeIndication();
            for (size_t i = 0; i < names.size(); ++i) {
                const bool last = (i + 1 == names.size());
                AstVhdlSubtypeInd* const usep
                    = sip ? (last ? sip : sip->cloneTree(false)) : nullptr;
                nodep->addElemsp(new AstVhdlElementDecl{names[i].second, names[i].first, usep});
            }
            expect(VhdlTokType::SEMICOLON, "';' after a record element");
        }
        expect(VhdlTokType::KWD_END, "'end' to close the record");
        expect(VhdlTokType::KWD_RECORD, "'record' after 'end'");
        checkEndLabel(typeName);
        return nodep;
    }
    case VhdlTokType::KWD_ACCESS: {
        consume();
        AstVhdlSubtypeInd* sip = parseSubtypeIndication();
        if (!sip) sip = new AstVhdlSubtypeInd{flp, nullptr};
        return new AstVhdlAccessType{flp, sip};
    }
    case VhdlTokType::KWD_FILE: {
        consume();
        expect(VhdlTokType::KWD_OF, "'of' after 'file'");
        AstNodeVhdlExpr* markp = parseName();
        if (!markp) markp = new AstVhdlName{flp, ""};
        return new AstVhdlFileType{flp, markp};
    }
    case VhdlTokType::KWD_PROTECTED: {
        // protected_type_declaration or protected_type_body (5.6)
        consume();
        const bool isBody = check(VhdlTokType::KWD_BODY);
        AstVhdlProtectedType* const nodep = new AstVhdlProtectedType{flp, isBody};
        nodep->addDeclsp(parseDeclarativeItems());
        expect(VhdlTokType::KWD_END, "'end' to close the protected type");
        expect(VhdlTokType::KWD_PROTECTED, "'protected' after 'end'");
        if (isBody) expect(VhdlTokType::KWD_BODY, "'body' after 'end protected'");
        checkEndLabel(typeName);
        return nodep;
    }
    default:
        syntaxError("a type definition");
        recoverTo(VhdlTokType::SEMICOLON);
        return nullptr;
    }
}

AstNodeVhdl* V3VhdlParse::parseSubtypeDecl() {
    // subtype_declaration ::= SUBTYPE id IS subtype_indication ;   (6.3)
    FileLine* const flp = here();
    consume();  // "subtype"
    const std::string name = identifier("a subtype name");
    expect(VhdlTokType::KWD_IS, "'is' after the subtype name");
    AstVhdlSubtypeInd* sip = parseSubtypeIndication();
    if (!sip) sip = new AstVhdlSubtypeInd{flp, nullptr};
    expect(VhdlTokType::SEMICOLON, "';' after the subtype declaration");
    return new AstVhdlSubtypeDecl{flp, name, sip};
}

AstVhdlSubtypeInd* V3VhdlParse::parseSubtypeIndication() {
    // subtype_indication ::= [ resolution_indication ] type_mark [ constraint ]   (6.3)
    FileLine* const flp = here();
    AstNodeVhdlExpr* resolutionp = nullptr;
    AstNodeVhdlExpr* typeMarkp = nullptr;

    if (peekType() == VhdlTokType::LPAREN) {
        // A parenthesised resolution indication, VHDL-2008 (6.3)
        resolutionp = parseParenOrAggregate();
        typeMarkp = parseName();
    } else {
        typeMarkp = parseName();
        // "resolution type_mark", where the first name resolved the second
        if (peekType() == VhdlTokType::IDENTIFIER) {
            resolutionp = typeMarkp;
            typeMarkp = parseName();
        }
    }
    if (!typeMarkp) {
        if (resolutionp) VL_DO_DANGLING(resolutionp->deleteTree(), resolutionp);
        return nullptr;
    }
    AstVhdlSubtypeInd* const nodep = new AstVhdlSubtypeInd{flp, typeMarkp};
    if (resolutionp) nodep->resolutionp(resolutionp);
    AstNodeVhdl* const constraintp = parseConstraint();
    if (constraintp) nodep->constraintp(constraintp);
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseConstraint() {
    // constraint ::= range_constraint | array_constraint | record_constraint  (6.3)
    FileLine* const flp = here();
    if (check(VhdlTokType::KWD_RANGE)) return parseRange();
    if (peekType() != VhdlTokType::LPAREN) return nullptr;

    // Distinguish an index constraint from a record constraint: a record
    // constraint names its elements, as in "(re(7 downto 0), im(7 downto 0))"
    if (peekType(1) == VhdlTokType::IDENTIFIER && peekType(2) == VhdlTokType::LPAREN) {
        // Could still be an index constraint over a function call, so only
        // take the record form when a comma or ')' follows the inner group.
        // Erring towards the index constraint keeps the common case right.
    }
    consume();  // "("
    AstVhdlIndexConstraint* const nodep = new AstVhdlIndexConstraint{flp};
    if (check(VhdlTokType::BOX)) {
        nodep->addRangesp(new AstVhdlBox{flp});
    } else {
        do {
            AstNodeVhdl* const rangep = parseDiscreteRange();
            if (!rangep) break;
            nodep->addRangesp(rangep);
        } while (check(VhdlTokType::COMMA));
    }
    expect(VhdlTokType::RPAREN, "')' after the constraint");
    // A second constraint may follow for the element, e.g. "t(3 downto 0)(7 downto 0)"
    AstNodeVhdl* const nestedp = parseConstraint();
    if (nestedp) nodep->addRangesp(nestedp);
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseSubprogram(bool inInterface) {
    // subprogram_declaration, subprogram_body or subprogram_instantiation
    // (4.2, 4.3, 4.4)
    FileLine* const flp = here();
    // A subprogram of an interface list is part of the enclosing
    // specification's spelling, so only the outermost one records its own
    std::string spec;
    VL_RESTORER(m_specp);
    const bool recording = !m_specp;
    if (recording) m_specp = &spec;
    bool isPure = true;
    bool pureGiven = false;
    if (check(VhdlTokType::KWD_PURE)) {
        pureGiven = true;
    } else if (check(VhdlTokType::KWD_IMPURE)) {
        isPure = false;
        pureGiven = true;
    }
    (void)pureGiven;
    const bool isFunction = (peekType() == VhdlTokType::KWD_FUNCTION);
    consume();  // "function" or "procedure"

    // designator ::= identifier | operator_symbol
    std::string name;
    bool isOperator = false;
    if (peekType() == VhdlTokType::STRING_LIT) {
        const VhdlToken tok = consume();
        name = lowerOperator(tok.text);
        isOperator = true;
        // Only a function overloads an operator (4.2.1, 4.5.2)
        if (!isFunction) {
            tok.flp->v3error("A procedure is named by an identifier, not by an operator symbol:"
                             " only a function can overload an operator");
        } else if (!isOperatorSymbol(name)) {
            tok.flp->v3error("\"" << tok.text
                                  << "\" is not an operator of the language, so a function"
                                     " cannot be named by it");
        }
    } else {
        name = identifier("a subprogram name");
    }

    AstVhdlSubprogDecl* const nodep = new AstVhdlSubprogDecl{flp, name, isFunction};
    nodep->vhdlPure(isPure);
    nodep->isOperator(isOperator);

    // VHDL-2008 generic subprogram
    if (check(VhdlTokType::KWD_GENERIC)) {
        expect(VhdlTokType::LPAREN, "'(' after 'generic'");
        // A subprogram's generics are interface declarations like its
        // parameters, so they are kept in the same list with a count of how
        // many of them come first
        nodep->addParamsp(parseInterfaceList(VhdlIfaceKind::CONSTANT));
        int generics = 0;
        for (AstNode* p = nodep->paramsp(); p; p = p->nextp()) ++generics;
        nodep->genericCount(generics);
        expect(VhdlTokType::RPAREN, "')' after the generic clause");
        if (check(VhdlTokType::KWD_GENERIC)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
            expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
            parseAssociationList();
            expect(VhdlTokType::RPAREN, "')' after the generic map");
        }
    }

    // Instantiation form: "is new name"
    if (peekType() == VhdlTokType::KWD_IS && peekType(1) == VhdlTokType::KWD_NEW) {
        consume();  // "is"
        consume();  // "new"
        AstNodeVhdlExpr* uninstp = parseName();
        if (!uninstp) uninstp = new AstVhdlName{flp, ""};
        AstVhdlSubprogInst* const instp
            = new AstVhdlSubprogInst{flp, name, isFunction, uninstp};
        if (peekType() == VhdlTokType::LBRACKET) instp->signaturep(parseSignature());
        if (check(VhdlTokType::KWD_GENERIC)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
            expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
            instp->addGenericMapp(parseAssociationList());
            expect(VhdlTokType::RPAREN, "')' after the generic map");
        }
        expect(VhdlTokType::SEMICOLON, "';' after the subprogram instantiation");
        VL_DO_DANGLING(nodep->deleteTree(), nodep);
        return instp;
    }

    check(VhdlTokType::KWD_PARAMETER);
    if (check(VhdlTokType::LPAREN)) {
        nodep->addParamsp(parseInterfaceList(isFunction ? VhdlIfaceKind::CONSTANT
                                                        : VhdlIfaceKind::VARIABLE));
        expect(VhdlTokType::RPAREN, "')' after the parameter list");
    }
    if (isFunction) {
        expect(VhdlTokType::KWD_RETURN, "'return' in a function declaration");
        // VHDL-2019 allows a full subtype indication as the return
        nodep->returnTypep(parseName());
    }
    if (recording) {
        m_specp = nullptr;
        nodep->specText(spec);
    }

    // "is <>" marks a generic subprogram that defaults to the visible one (6.5.5)
    if (peekType() == VhdlTokType::KWD_IS && peekType(1) == VhdlTokType::BOX) {
        consume();
        consume();
        nodep->defaultsToVisible(true);
        return nodep;
    }
    // In an interface list the specification ends at ';' or ')', both of which
    // belong to the enclosing list
    if (inInterface) return nodep;

    if (check(VhdlTokType::SEMICOLON)) return nodep;  // Declaration only

    if (!expect(VhdlTokType::KWD_IS, "'is' or ';' after the subprogram specification")) {
        skipToSemicolon();
        return nodep;
    }
    nodep->hasBody(true);
    nodep->addDeclsp(parseDeclarativeItems());
    expect(VhdlTokType::KWD_BEGIN, "'begin' to start the subprogram body");
    nodep->addStmtsp(parseSequentialStmts());
    expect(VhdlTokType::KWD_END, "'end' to close the subprogram");
    check(VhdlTokType::KWD_FUNCTION);
    check(VhdlTokType::KWD_PROCEDURE);
    if (peekType() == VhdlTokType::STRING_LIT) {
        consume();  // Repeated operator symbol
    } else {
        checkEndLabel(name);
    }
    expect(VhdlTokType::SEMICOLON, "';' after 'end'");
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseComponentDecl() {
    // component_declaration ::= COMPONENT id [ IS ] [ generic ] [ port ]
    //                           END COMPONENT [ id ] ;   (6.8)
    FileLine* const flp = here();
    consume();  // "component"
    const std::string name = identifier("a component name");
    check(VhdlTokType::KWD_IS);
    AstVhdlComponentDecl* const nodep = new AstVhdlComponentDecl{flp, name};
    if (check(VhdlTokType::KWD_GENERIC)) {
        expect(VhdlTokType::LPAREN, "'(' after 'generic'");
        nodep->addGenericsp(parseInterfaceList(VhdlIfaceKind::CONSTANT));
        expect(VhdlTokType::RPAREN, "')' after the generic clause");
        expect(VhdlTokType::SEMICOLON, "';' after the generic clause");
    }
    if (check(VhdlTokType::KWD_PORT)) {
        expect(VhdlTokType::LPAREN, "'(' after 'port'");
        nodep->addPortsp(parseInterfaceList(VhdlIfaceKind::SIGNAL));
        expect(VhdlTokType::RPAREN, "')' after the port clause");
        expect(VhdlTokType::SEMICOLON, "';' after the port clause");
    }
    expect(VhdlTokType::KWD_END, "'end' to close the component");
    expect(VhdlTokType::KWD_COMPONENT, "'component' after 'end'");
    checkEndLabel(name);
    expect(VhdlTokType::SEMICOLON, "';' after 'end component'");
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseAliasDecl() {
    // alias_declaration ::= ALIAS designator [ : subtype ] IS name [ signature ] ;  (6.6)
    FileLine* const flp = here();
    consume();  // "alias"
    std::string name;
    if (peekType() == VhdlTokType::STRING_LIT) {
        name = lowerOperator(consume().text);
    } else if (peekType() == VhdlTokType::CHARACTER_LIT) {
        name = consume().text;
    } else {
        name = identifier("an alias designator");
    }
    AstVhdlSubtypeInd* subtypep = nullptr;
    if (check(VhdlTokType::COLON)) subtypep = parseSubtypeIndication();
    expect(VhdlTokType::KWD_IS, "'is' in an alias declaration");
    AstNodeVhdlExpr* targetp = parseName();
    if (!targetp) targetp = new AstVhdlName{flp, ""};
    AstVhdlAliasDecl* const nodep = new AstVhdlAliasDecl{flp, name, targetp};
    if (subtypep) nodep->subtypep(subtypep);
    if (peekType() == VhdlTokType::LBRACKET) nodep->signaturep(parseSignature());
    expect(VhdlTokType::SEMICOLON, "';' after the alias declaration");
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseAttribute() {
    // attribute_declaration (6.7) or attribute_specification (7.2)
    FileLine* const flp = here();
    consume();  // "attribute"
    const std::string name = identifier("an attribute name");
    if (check(VhdlTokType::COLON)) {
        AstNodeVhdlExpr* markp = parseName();
        if (!markp) markp = new AstVhdlName{flp, ""};
        expect(VhdlTokType::SEMICOLON, "';' after the attribute declaration");
        return new AstVhdlAttrDecl{flp, name, markp};
    }
    expect(VhdlTokType::KWD_OF, "'of' or ':' after the attribute name");
    AstVhdlEntitySpec* const specp = parseEntitySpec(false);
    expect(VhdlTokType::KWD_IS, "'is' in an attribute specification");
    AstNodeVhdlExpr* valuep = parseExpression();
    if (!valuep) valuep = new AstVhdlName{flp, ""};
    expect(VhdlTokType::SEMICOLON, "';' after the attribute specification");
    return new AstVhdlAttrSpec{flp, name, specp, valuep};
}

AstVhdlEntitySpec* V3VhdlParse::parseEntitySpec(bool componentForm) {
    // entity_specification ::= entity_name_list : entity_class   (7.2)
    // component_specification ::= instantiation_list : component_name   (7.3.1)
    FileLine* const flp = here();
    AstVhdlEntitySpec* const nodep = new AstVhdlEntitySpec{flp};
    if (check(VhdlTokType::KWD_OTHERS)) {
        nodep->others(true);
    } else if (check(VhdlTokType::KWD_ALL)) {
        nodep->all(true);
    } else {
        do {
            AstNodeVhdlExpr* namep = nullptr;
            if (peekType() == VhdlTokType::STRING_LIT
                || peekType() == VhdlTokType::CHARACTER_LIT) {
                namep = new AstVhdlName{here(), consume().text};
            } else {
                namep = parseName();
            }
            if (!namep) break;
            if (peekType() == VhdlTokType::LBRACKET) {
                // An overloaded designator is disambiguated by a signature
                AstVhdlSignature* const sigp = parseSignature();
                VL_DO_DANGLING(sigp->deleteTree(), sigp);
            }
            nodep->addNamesp(namep);
        } while (check(VhdlTokType::COMMA));
    }
    if (check(VhdlTokType::COLON)) {
        if (componentForm) {
            AstNodeVhdlExpr* const compp = parseName();
            if (compp) nodep->addNamesp(compp);
            nodep->entityClass("component");
        } else {
            // entity_class is a reserved word such as signal, variable, type
            const VhdlToken tok = consume();
            nodep->entityClass(vhdlTokIsKeyword(tok.type) ? vhdlTokName(tok.type) : tok.text);
        }
    }
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseConfigSpec() {
    // configuration_specification ::= FOR component_specification binding ;   (7.3)
    FileLine* const flp = here();
    consume();  // "for"
    AstVhdlEntitySpec* const specp = parseEntitySpec(true);
    AstVhdlConfigSpec* const nodep = new AstVhdlConfigSpec{flp, specp};
    nodep->bindingp(parseBinding());
    expect(VhdlTokType::SEMICOLON, "';' after the configuration specification");
    // VHDL-2008 allows an explicit "end for;"
    if (check(VhdlTokType::KWD_END)) {
        expect(VhdlTokType::KWD_FOR, "'for' after 'end'");
        expect(VhdlTokType::SEMICOLON, "';' after 'end for'");
    }
    return nodep;
}

AstNodeVhdl* V3VhdlParse::parseDisconnectSpec() {
    // disconnection_specification ::= DISCONNECT signal_spec AFTER expression ;  (7.4)
    FileLine* const flp = here();
    consume();  // "disconnect"
    AstVhdlEntitySpec* const specp = parseEntitySpec(true);
    expect(VhdlTokType::KWD_AFTER, "'after' in a disconnection specification");
    AstNodeVhdlExpr* afterp = parseExpression();
    if (!afterp) afterp = new AstVhdlName{flp, ""};
    expect(VhdlTokType::SEMICOLON, "';' after the disconnection specification");
    return new AstVhdlDisconnectSpec{flp, specp, afterp};
}

AstNodeVhdl* V3VhdlParse::parseGroup() {
    // group_template_declaration (6.9) or group_declaration (6.10)
    FileLine* const flp = here();
    consume();  // "group"
    const std::string name = identifier("a group name");
    if (check(VhdlTokType::KWD_IS)) {
        expect(VhdlTokType::LPAREN, "'(' in a group template declaration");
        while (peekType() != VhdlTokType::RPAREN && peekType() != VhdlTokType::END_OF_FILE) {
            consume();  // entity class entries are not otherwise used
        }
        expect(VhdlTokType::RPAREN, "')' after the entity class entries");
        expect(VhdlTokType::SEMICOLON, "';' after the group template declaration");
        return new AstVhdlGroupTemplate{flp, name};
    }
    expect(VhdlTokType::COLON, "':' in a group declaration");
    AstNodeVhdlExpr* templatep = parseName();
    if (!templatep) templatep = new AstVhdlName{flp, ""};
    AstVhdlGroupDecl* const nodep = new AstVhdlGroupDecl{flp, name, templatep};
    if (check(VhdlTokType::LPAREN)) {
        do {
            AstNodeVhdlExpr* const memberp = parseName();
            if (!memberp) break;
            nodep->addMembersp(memberp);
        } while (check(VhdlTokType::COMMA));
        expect(VhdlTokType::RPAREN, "')' after the group constituents");
    }
    expect(VhdlTokType::SEMICOLON, "';' after the group declaration");
    return nodep;
}

AstVhdlSignature* V3VhdlParse::parseSignature() {
    // signature ::= [ [ type_mark { , type_mark } ] [ RETURN type_mark ] ]   (4.5.3)
    FileLine* const flp = here();
    consume();  // "["
    AstVhdlSignature* const nodep = new AstVhdlSignature{flp};
    if (peekType() != VhdlTokType::RBRACKET && peekType() != VhdlTokType::KWD_RETURN) {
        do {
            AstNodeVhdlExpr* const markp = parseName();
            if (!markp) break;
            nodep->addParamTypesp(markp);
        } while (check(VhdlTokType::COMMA));
    }
    if (check(VhdlTokType::KWD_RETURN)) nodep->returnTypep(parseName());
    expect(VhdlTokType::RBRACKET, "']' to close the signature");
    return nodep;
}

//######################################################################
// Concurrent statements (clause 11)

AstNodeVhdlStmt* V3VhdlParse::parseConcurrentStmts() {
    AstNodeVhdlStmt* listp = nullptr;
    while (true) {
        switch (peekType()) {
        case VhdlTokType::KWD_END:
        case VhdlTokType::KWD_ELSIF:
        case VhdlTokType::KWD_ELSE:
        case VhdlTokType::KWD_WHEN:
        case VhdlTokType::END_OF_FILE: return listp;
        default: break;
        }
        FileLine* const startp = here();
        AstNodeVhdlStmt* const stmtp = parseConcurrentStmt();
        if (stmtp) {
            listp = AstNode::addNextNull(listp, stmtp);
        } else if (here() == startp) {
            syntaxError("a concurrent statement");
            skipToSemicolon();
            if (peekType() == VhdlTokType::END_OF_FILE) return listp;
        }
    }
}

AstNodeVhdlStmt* V3VhdlParse::parseConcurrentStmt() {
    // An optional label precedes every concurrent statement (11.1)
    std::string label;
    if (peekType() == VhdlTokType::IDENTIFIER && peekType(1) == VhdlTokType::COLON) {
        label = consume().text;
        consume();  // ":"
    }
    bool postponed = check(VhdlTokType::KWD_POSTPONED);

    switch (peekType()) {
    case VhdlTokType::KWD_PROCESS: return parseProcess(label, postponed);
    case VhdlTokType::KWD_BLOCK: return parseBlock(label);
    case VhdlTokType::KWD_ASSERT: return parseAssert(label, postponed);
    case VhdlTokType::KWD_WITH: return parseConcurrentSimple(label, postponed);
    case VhdlTokType::KWD_COMPONENT:
    case VhdlTokType::KWD_ENTITY:
    case VhdlTokType::KWD_CONFIGURATION: return parseInstance(label);
    case VhdlTokType::KWD_FOR:
    case VhdlTokType::KWD_IF:
    case VhdlTokType::KWD_CASE: return parseGenerate(label);
    default: break;
    }
    return parseConcurrentSimple(label, postponed);
}

AstNodeVhdlStmt* V3VhdlParse::parseConcurrentSimple(const std::string& label, bool postponed) {
    // A concurrent signal assignment, a procedure call, or an instantiation
    // that named its component implicitly (11.4, 11.6, 11.7)
    FileLine* const flp = here();

    if (peekType() == VhdlTokType::KWD_WITH) {
        // selected_signal_assignment ::= WITH expr SELECT [?] target <= ...  (11.6)
        consume();
        AstNodeVhdlExpr* const selectorp = parseExpression();
        bool matching = check(VhdlTokType::QUERY);
        expect(VhdlTokType::KWD_SELECT, "'select' after the selector expression");
        // 11.6 and 10.5.4 write the question mark after "select"
        if (check(VhdlTokType::QUERY)) matching = true;
        // The target may be an aggregate, which assigns each element of the
        // value to a target of its own (10.5.2.1, 11.6)
        AstNodeVhdlExpr* targetp = (peekType() == VhdlTokType::LPAREN)
                                       ? parseParenOrAggregate()
                                       : parseName();
        if (!targetp) targetp = new AstVhdlName{flp, ""};
        expect(VhdlTokType::LE, "'<=' in a selected signal assignment");
        AstVhdlSigAssign* const nodep
            = new AstVhdlSigAssign{flp, VhdlAssignKind::SELECTED, targetp};
        nodep->name(label);
        nodep->postponed(postponed);
        nodep->matching(matching);
        nodep->selectorp(selectorp);
        nodep->guarded(check(VhdlTokType::KWD_GUARDED));
        parseDelayMechanism(nodep);
        do {
            AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
            altp->addWavesp(parseWaveform());
            expect(VhdlTokType::KWD_WHEN, "'when' in a selected signal assignment");
            altp->addChoicesp(parseChoices());
            nodep->addAltsp(altp);
        } while (check(VhdlTokType::COMMA));
        expect(VhdlTokType::SEMICOLON, "';' after the selected signal assignment");
        return nodep;
    }

    // A target may be an aggregate, e.g. "(a, b) <= x" (10.5.2.1)
    AstNodeVhdlExpr* const namep = (peekType() == VhdlTokType::LPAREN)
                                       ? parseParenOrAggregate()
                                       : parseName();
    if (!namep) return nullptr;

    if (check(VhdlTokType::LE)) {
        AstVhdlSigAssign* const nodep
            = new AstVhdlSigAssign{flp, VhdlAssignKind::SIMPLE, namep};
        nodep->name(label);
        nodep->postponed(postponed);
        nodep->guarded(check(VhdlTokType::KWD_GUARDED));
        // VHDL-2008 force and release
        if (check(VhdlTokType::KWD_FORCE)) {
            nodep->force(true);
            if (check(VhdlTokType::KWD_IN)) {
                nodep->forceMode("in");
            } else if (check(VhdlTokType::KWD_OUT)) {
                nodep->forceMode("out");
            }
        } else if (check(VhdlTokType::KWD_RELEASE)) {
            nodep->release(true);
            if (check(VhdlTokType::KWD_IN)) {
                nodep->forceMode("in");
            } else if (check(VhdlTokType::KWD_OUT)) {
                nodep->forceMode("out");
            }
            expect(VhdlTokType::SEMICOLON, "';' after 'release'");
            return nodep;
        } else {
            parseDelayMechanism(nodep);
        }
        AstVhdlWaveform* const wavep = parseWaveform();
        if (peekType() == VhdlTokType::KWD_WHEN) {
            // conditional_signal_assignment (11.6)
            AstVhdlSigAssign* const condp
                = new AstVhdlSigAssign{flp, VhdlAssignKind::CONDITIONAL,
                                       nodep->targetp()->unlinkFrBack()};
            condp->name(label);
            condp->postponed(postponed);
            condp->guarded(nodep->guarded());
            condp->delay(nodep->delay());
            AstVhdlAlternative* firstp = new AstVhdlAlternative{flp};
            firstp->addWavesp(wavep);
            consume();  // "when"
            firstp->condp(parseExpression());
            condp->addAltsp(firstp);
            while (check(VhdlTokType::KWD_ELSE)) {
                AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
                altp->addWavesp(parseWaveform());
                if (check(VhdlTokType::KWD_WHEN)) altp->condp(parseExpression());
                condp->addAltsp(altp);
            }
            expect(VhdlTokType::SEMICOLON, "';' after the conditional signal assignment");
            VL_DO_DANGLING(nodep->deleteTree(), nodep);
            return condp;
        }
        nodep->addWavesp(wavep);
        expect(VhdlTokType::SEMICOLON, "';' after the signal assignment");
        return nodep;
    }

    // An instantiated unit, or a concurrent procedure call
    if (peekType() == VhdlTokType::KWD_GENERIC || peekType() == VhdlTokType::KWD_PORT) {
        AstVhdlInstance* const nodep = new AstVhdlInstance{flp, namep};
        nodep->name(label);
        if (check(VhdlTokType::KWD_GENERIC)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
            expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
            nodep->addGenericMapp(parseAssociationList());
            expect(VhdlTokType::RPAREN, "')' after the generic map");
        }
        if (check(VhdlTokType::KWD_PORT)) {
            expect(VhdlTokType::KWD_MAP, "'map' after 'port'");
            expect(VhdlTokType::LPAREN, "'(' after 'port map'");
            nodep->addPortMapp(parseAssociationList());
            expect(VhdlTokType::RPAREN, "')' after the port map");
        }
        expect(VhdlTokType::SEMICOLON, "';' after the instantiation");
        return nodep;
    }

    // "label : name ;" with no maps is a component instantiation; anything
    // with arguments is a procedure call. Both are legal here and the forms
    // are identical, so this follows the far more common reading (11.7).
    if (!label.empty() && VN_IS(namep, VhdlName)) {
        AstVhdlInstance* const nodep = new AstVhdlInstance{flp, namep};
        nodep->name(label);
        expect(VhdlTokType::SEMICOLON, "';' after the instantiation");
        return nodep;
    }
    AstVhdlProcCall* const nodep = new AstVhdlProcCall{flp, namep};
    nodep->name(label);
    nodep->postponed(postponed);
    expect(VhdlTokType::SEMICOLON, "';' after the procedure call");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseProcess(const std::string& label, bool postponed) {
    // process_statement (11.3)
    FileLine* const flp = here();
    consume();  // "process"
    AstVhdlProcess* const nodep = new AstVhdlProcess{flp};
    nodep->name(label);
    nodep->postponed(postponed);
    if (check(VhdlTokType::LPAREN)) {
        nodep->hasSensList(true);
        if (check(VhdlTokType::KWD_ALL)) {
            nodep->sensAll(true);  // VHDL-2008
        } else {
            do {
                AstNodeVhdlExpr* const sigp = parseName();
                if (!sigp) break;
                nodep->addSensp(sigp);
            } while (check(VhdlTokType::COMMA));
        }
        expect(VhdlTokType::RPAREN, "')' after the sensitivity list");
    }
    check(VhdlTokType::KWD_IS);
    nodep->addDeclsp(parseDeclarativeItems());
    expect(VhdlTokType::KWD_BEGIN, "'begin' to start the process body");
    nodep->addStmtsp(parseSequentialStmts());
    expect(VhdlTokType::KWD_END, "'end' to close the process");
    check(VhdlTokType::KWD_POSTPONED);
    expect(VhdlTokType::KWD_PROCESS, "'process' after 'end'");
    checkEndLabel(label);
    expect(VhdlTokType::SEMICOLON, "';' after 'end process'");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseBlock(const std::string& label) {
    // block_statement (11.2)
    FileLine* const flp = here();
    consume();  // "block"
    AstVhdlBlock* const nodep = new AstVhdlBlock{flp};
    nodep->name(label);
    if (check(VhdlTokType::LPAREN)) {
        nodep->guardp(parseExpression());
        expect(VhdlTokType::RPAREN, "')' after the guard expression");
    }
    check(VhdlTokType::KWD_IS);

    if (peekType() == VhdlTokType::KWD_GENERIC || peekType() == VhdlTokType::KWD_PORT) {
        AstVhdlBlockHeader* const headerp = new AstVhdlBlockHeader{here()};
        if (check(VhdlTokType::KWD_GENERIC)) {
            if (check(VhdlTokType::KWD_MAP)) {
                expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
                headerp->addGenericMapp(parseAssociationList());
                expect(VhdlTokType::RPAREN, "')' after the generic map");
            } else {
                expect(VhdlTokType::LPAREN, "'(' after 'generic'");
                headerp->addGenericsp(parseInterfaceList(VhdlIfaceKind::CONSTANT));
                expect(VhdlTokType::RPAREN, "')' after the generic clause");
            }
            expect(VhdlTokType::SEMICOLON, "';' after the generic clause");
            if (check(VhdlTokType::KWD_GENERIC)) {
                expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
                expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
                headerp->addGenericMapp(parseAssociationList());
                expect(VhdlTokType::RPAREN, "')' after the generic map");
                expect(VhdlTokType::SEMICOLON, "';' after the generic map");
            }
        }
        if (check(VhdlTokType::KWD_PORT)) {
            if (check(VhdlTokType::KWD_MAP)) {
                expect(VhdlTokType::LPAREN, "'(' after 'port map'");
                headerp->addPortMapp(parseAssociationList());
                expect(VhdlTokType::RPAREN, "')' after the port map");
            } else {
                expect(VhdlTokType::LPAREN, "'(' after 'port'");
                headerp->addPortsp(parseInterfaceList(VhdlIfaceKind::SIGNAL));
                expect(VhdlTokType::RPAREN, "')' after the port clause");
            }
            expect(VhdlTokType::SEMICOLON, "';' after the port clause");
            if (check(VhdlTokType::KWD_PORT)) {
                expect(VhdlTokType::KWD_MAP, "'map' after 'port'");
                expect(VhdlTokType::LPAREN, "'(' after 'port map'");
                headerp->addPortMapp(parseAssociationList());
                expect(VhdlTokType::RPAREN, "')' after the port map");
                expect(VhdlTokType::SEMICOLON, "';' after the port map");
            }
        }
        nodep->headerp(headerp);
    }

    nodep->addDeclsp(parseDeclarativeItems());
    expect(VhdlTokType::KWD_BEGIN, "'begin' to start the block body");
    nodep->addStmtsp(parseConcurrentStmts());
    expect(VhdlTokType::KWD_END, "'end' to close the block");
    expect(VhdlTokType::KWD_BLOCK, "'block' after 'end'");
    checkEndLabel(label);
    expect(VhdlTokType::SEMICOLON, "';' after 'end block'");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseInstance(const std::string& label) {
    // component_instantiation_statement with an explicit unit kind (11.7)
    FileLine* const flp = here();
    std::string kind;
    std::string archName;
    if (check(VhdlTokType::KWD_COMPONENT)) {
        kind = "component";
    } else if (check(VhdlTokType::KWD_ENTITY)) {
        kind = "entity";
    } else {
        consume();  // "configuration"
        kind = "configuration";
    }
    AstNodeVhdlExpr* unitp = parseName();
    if (!unitp) unitp = new AstVhdlName{flp, ""};
    if (kind == "entity" && check(VhdlTokType::LPAREN)) {
        archName = identifier("an architecture name");
        expect(VhdlTokType::RPAREN, "')' after the architecture name");
    }
    AstVhdlInstance* const nodep = new AstVhdlInstance{flp, unitp};
    nodep->name(label);
    nodep->kind(kind);
    nodep->archName(archName);
    if (check(VhdlTokType::KWD_GENERIC)) {
        expect(VhdlTokType::KWD_MAP, "'map' after 'generic'");
        expect(VhdlTokType::LPAREN, "'(' after 'generic map'");
        nodep->addGenericMapp(parseAssociationList());
        expect(VhdlTokType::RPAREN, "')' after the generic map");
    }
    if (check(VhdlTokType::KWD_PORT)) {
        expect(VhdlTokType::KWD_MAP, "'map' after 'port'");
        expect(VhdlTokType::LPAREN, "'(' after 'port map'");
        nodep->addPortMapp(parseAssociationList());
        expect(VhdlTokType::RPAREN, "')' after the port map");
    }
    expect(VhdlTokType::SEMICOLON, "';' after the instantiation");
    return nodep;
}

AstVhdlGenerateBody* V3VhdlParse::parseGenerateBody() {
    // generate_statement_body ::= [ decls BEGIN ] statements [ END [ label ] ; ]  (11.8)
    AstVhdlGenerateBody* const nodep = new AstVhdlGenerateBody{here()};
    // Declarations only appear when followed by "begin"
    AstNodeVhdl* const declsp = parseDeclarativeItems();
    if (declsp) nodep->addDeclsp(declsp);
    check(VhdlTokType::KWD_BEGIN);
    nodep->addStmtsp(parseConcurrentStmts());
    // An inner "end [label] ;" closes the body but not the generate
    if (peekType() == VhdlTokType::KWD_END && peekType(1) != VhdlTokType::KWD_GENERATE) {
        consume();  // "end"
        if (peekType() == VhdlTokType::IDENTIFIER) consume();
        expect(VhdlTokType::SEMICOLON, "';' after the generate body's 'end'");
    }
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseGenerate(const std::string& label) {
    // for/if/case generate_statement (11.8)
    FileLine* const flp = here();
    AstVhdlGenerate* nodep = nullptr;
    if (check(VhdlTokType::KWD_FOR)) {
        nodep = new AstVhdlGenerate{flp, VhdlLoopKind::FOR};
        nodep->paramName(identifier("a generate parameter name"));
        expect(VhdlTokType::KWD_IN, "'in' in a for generate");
        nodep->rangep(parseDiscreteRange());
        expect(VhdlTokType::KWD_GENERATE, "'generate' after the range");
        nodep->addAltsp(parseGenerateBody());
    } else if (check(VhdlTokType::KWD_IF)) {
        nodep = new AstVhdlGenerate{flp, VhdlLoopKind::LOOP};
        while (true) {
            AstVhdlGenerateBody* bodyp = nullptr;
            std::string altLabel;
            // VHDL-2008 allows "alternative_label : condition"
            if (peekType() == VhdlTokType::IDENTIFIER && peekType(1) == VhdlTokType::COLON) {
                altLabel = consume().text;
                consume();
            }
            AstNodeVhdlExpr* const condp = parseExpression();
            expect(VhdlTokType::KWD_GENERATE, "'generate' after the condition");
            bodyp = parseGenerateBody();
            bodyp->condp(condp);
            bodyp->name(altLabel);
            nodep->addAltsp(bodyp);
            if (check(VhdlTokType::KWD_ELSIF)) continue;
            if (check(VhdlTokType::KWD_ELSE)) {
                std::string elseLabel;
                if (peekType() == VhdlTokType::IDENTIFIER && peekType(1) == VhdlTokType::COLON) {
                    elseLabel = consume().text;
                    consume();
                }
                expect(VhdlTokType::KWD_GENERATE, "'generate' after 'else'");
                AstVhdlGenerateBody* const elsep = parseGenerateBody();
                elsep->name(elseLabel);
                nodep->addAltsp(elsep);
            }
            break;
        }
    } else {
        consume();  // "case"
        nodep = new AstVhdlGenerate{flp, VhdlLoopKind::LOOP};
        nodep->exprp(parseExpression());
        expect(VhdlTokType::KWD_GENERATE, "'generate' after the case expression");
        while (check(VhdlTokType::KWD_WHEN)) {
            std::string altLabel;
            if (peekType() == VhdlTokType::IDENTIFIER && peekType(1) == VhdlTokType::COLON) {
                altLabel = consume().text;
                consume();
            }
            AstVhdlChoice* const choicesp = parseChoices();
            expect(VhdlTokType::ARROW, "'=>' after the choices");
            AstVhdlGenerateBody* const bodyp = parseGenerateBody();
            bodyp->addChoicesp(choicesp);
            bodyp->name(altLabel);
            nodep->addAltsp(bodyp);
        }
    }
    nodep->name(label);
    expect(VhdlTokType::KWD_END, "'end' to close the generate");
    expect(VhdlTokType::KWD_GENERATE, "'generate' after 'end'");
    checkEndLabel(label);
    expect(VhdlTokType::SEMICOLON, "';' after 'end generate'");
    return nodep;
}

//######################################################################
// Sequential statements (clause 10)

AstNodeVhdlStmt* V3VhdlParse::parseSequentialStmts() {
    AstNodeVhdlStmt* listp = nullptr;
    while (true) {
        switch (peekType()) {
        case VhdlTokType::KWD_END:
        case VhdlTokType::KWD_ELSIF:
        case VhdlTokType::KWD_ELSE:
        case VhdlTokType::KWD_WHEN:
        case VhdlTokType::END_OF_FILE: return listp;
        default: break;
        }
        FileLine* const startp = here();
        AstNodeVhdlStmt* const stmtp = parseSequentialStmt();
        if (stmtp) {
            listp = AstNode::addNextNull(listp, stmtp);
        } else if (here() == startp) {
            syntaxError("a sequential statement");
            skipToSemicolon();
            if (peekType() == VhdlTokType::END_OF_FILE) return listp;
        }
    }
}

AstNodeVhdlStmt* V3VhdlParse::parseSequentialStmt() {
    std::string label;
    if (peekType() == VhdlTokType::IDENTIFIER && peekType(1) == VhdlTokType::COLON) {
        label = consume().text;
        consume();  // ":"
    }
    const bool postponed = check(VhdlTokType::KWD_POSTPONED);
    FileLine* const flp = here();

    switch (peekType()) {
    case VhdlTokType::KWD_WAIT: return parseWait(label);
    case VhdlTokType::KWD_ASSERT: return parseAssert(label, postponed);
    case VhdlTokType::KWD_REPORT: return parseReport(label, postponed);
    case VhdlTokType::KWD_IF: return parseIf(label);
    case VhdlTokType::KWD_CASE: return parseCase(label);
    case VhdlTokType::KWD_LOOP:
    case VhdlTokType::KWD_WHILE:
    case VhdlTokType::KWD_FOR: return parseLoop(label);
    case VhdlTokType::KWD_NEXT:
    case VhdlTokType::KWD_EXIT: {
        const bool isExit = (peekType() == VhdlTokType::KWD_EXIT);
        consume();
        AstVhdlNext* const nodep = new AstVhdlNext{flp, isExit};
        nodep->name(label);
        if (peekType() == VhdlTokType::IDENTIFIER) nodep->loopLabel(consume().text);
        if (check(VhdlTokType::KWD_WHEN)) nodep->condp(parseExpression());
        expect(VhdlTokType::SEMICOLON, "';' after the statement");
        return nodep;
    }
    case VhdlTokType::KWD_RETURN: {
        consume();
        AstVhdlReturn* const nodep = new AstVhdlReturn{flp};
        nodep->name(label);
        if (peekType() != VhdlTokType::SEMICOLON) nodep->valuep(parseExpression());
        expect(VhdlTokType::SEMICOLON, "';' after 'return'");
        return nodep;
    }
    case VhdlTokType::KWD_NULL: {
        consume();
        AstVhdlNull* const nodep = new AstVhdlNull{flp};
        nodep->name(label);
        expect(VhdlTokType::SEMICOLON, "';' after 'null'");
        return nodep;
    }
    default: return parseAssignOrCall(label);
    }
}

AstNodeVhdlStmt* V3VhdlParse::parseWait(const std::string& label) {
    // wait_statement ::= WAIT [ ON signals ] [ UNTIL cond ] [ FOR time ] ;   (10.2)
    FileLine* const flp = here();
    consume();  // "wait"
    AstVhdlWait* const nodep = new AstVhdlWait{flp};
    nodep->name(label);
    if (check(VhdlTokType::KWD_ON)) {
        do {
            AstNodeVhdlExpr* const sigp = parseName();
            if (!sigp) break;
            nodep->addSensp(sigp);
        } while (check(VhdlTokType::COMMA));
    }
    if (check(VhdlTokType::KWD_UNTIL)) nodep->condp(parseExpression());
    if (check(VhdlTokType::KWD_FOR)) nodep->timeoutp(parseExpression());
    expect(VhdlTokType::SEMICOLON, "';' after 'wait'");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseAssert(const std::string& label, bool postponed) {
    // assertion_statement ::= ASSERT cond [ REPORT expr ] [ SEVERITY expr ] ;   (10.3)
    FileLine* const flp = here();
    consume();  // "assert"
    AstVhdlAssert* const nodep = new AstVhdlAssert{flp};
    nodep->name(label);
    nodep->postponed(postponed);
    nodep->condp(parseExpression());
    if (check(VhdlTokType::KWD_REPORT)) nodep->reportp(parseExpression());
    if (check(VhdlTokType::KWD_SEVERITY)) nodep->severityp(parseExpression());
    expect(VhdlTokType::SEMICOLON, "';' after the assertion");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseReport(const std::string& label, bool postponed) {
    // report_statement ::= REPORT expr [ SEVERITY expr ] ;   (10.4)
    FileLine* const flp = here();
    consume();  // "report"
    AstVhdlAssert* const nodep = new AstVhdlAssert{flp};
    nodep->name(label);
    nodep->postponed(postponed);
    nodep->reportp(parseExpression());
    if (check(VhdlTokType::KWD_SEVERITY)) nodep->severityp(parseExpression());
    expect(VhdlTokType::SEMICOLON, "';' after the report");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseIf(const std::string& label) {
    // if_statement (10.8)
    FileLine* const flp = here();
    consume();  // "if"
    AstVhdlIf* const nodep = new AstVhdlIf{flp};
    nodep->name(label);
    while (true) {
        AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
        altp->condp(parseExpression());
        expect(VhdlTokType::KWD_THEN, "'then' after the condition");
        altp->addStmtsp(parseSequentialStmts());
        nodep->addAltsp(altp);
        if (check(VhdlTokType::KWD_ELSIF)) continue;
        if (check(VhdlTokType::KWD_ELSE)) {
            AstVhdlAlternative* const elsep = new AstVhdlAlternative{here()};
            elsep->addStmtsp(parseSequentialStmts());
            nodep->addAltsp(elsep);
        }
        break;
    }
    expect(VhdlTokType::KWD_END, "'end' to close the if statement");
    expect(VhdlTokType::KWD_IF, "'if' after 'end'");
    checkEndLabel(label);
    expect(VhdlTokType::SEMICOLON, "';' after 'end if'");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseCase(const std::string& label) {
    // case_statement (10.9), including VHDL-2008's matching form "case?"
    FileLine* const flp = here();
    consume();  // "case"
    const bool matching = check(VhdlTokType::QUERY);
    AstNodeVhdlExpr* exprp = parseExpression();
    if (!exprp) exprp = new AstVhdlName{flp, ""};
    AstVhdlCase* const nodep = new AstVhdlCase{flp, exprp};
    nodep->name(label);
    nodep->matching(matching);
    expect(VhdlTokType::KWD_IS, "'is' after the case expression");
    // A case statement has at least one alternative (10.9)
    if (peekType() != VhdlTokType::KWD_WHEN) syntaxError("'when' to start an alternative");
    while (check(VhdlTokType::KWD_WHEN)) {
        AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
        altp->addChoicesp(parseChoices());
        expect(VhdlTokType::ARROW, "'=>' after the choices");
        altp->addStmtsp(parseSequentialStmts());
        nodep->addAltsp(altp);
    }
    expect(VhdlTokType::KWD_END, "'end' to close the case statement");
    expect(VhdlTokType::KWD_CASE, "'case' after 'end'");
    check(VhdlTokType::QUERY);
    checkEndLabel(label);
    expect(VhdlTokType::SEMICOLON, "';' after 'end case'");
    return nodep;
}

AstNodeVhdlStmt* V3VhdlParse::parseLoop(const std::string& label) {
    // loop_statement (10.10), in its bare, while and for forms
    FileLine* const flp = here();
    AstVhdlLoop* nodep = nullptr;
    if (check(VhdlTokType::KWD_WHILE)) {
        nodep = new AstVhdlLoop{flp, VhdlLoopKind::WHILE};
        nodep->condp(parseExpression());
    } else if (check(VhdlTokType::KWD_FOR)) {
        nodep = new AstVhdlLoop{flp, VhdlLoopKind::FOR};
        nodep->paramName(identifier("a loop parameter name"));
        expect(VhdlTokType::KWD_IN, "'in' in a for loop");
        // Either "1 to 5" or a subtype indication such as "integer range 1 to 5"
        nodep->rangep(parseDiscreteRange());
    } else {
        nodep = new AstVhdlLoop{flp, VhdlLoopKind::LOOP};
    }
    nodep->name(label);
    expect(VhdlTokType::KWD_LOOP, "'loop' to start the loop body");
    nodep->addStmtsp(parseSequentialStmts());
    expect(VhdlTokType::KWD_END, "'end' to close the loop");
    expect(VhdlTokType::KWD_LOOP, "'loop' after 'end'");
    checkEndLabel(label);
    expect(VhdlTokType::SEMICOLON, "';' after 'end loop'");
    return nodep;
}

void V3VhdlParse::parseDelayMechanism(AstVhdlSigAssign* nodep) {
    // delay_mechanism ::= TRANSPORT | [ REJECT time ] INERTIAL   (10.5.2.1)
    if (check(VhdlTokType::KWD_TRANSPORT)) {
        nodep->delay(VhdlDelayKind::TRANSPORT);
    } else if (check(VhdlTokType::KWD_REJECT)) {
        nodep->delay(VhdlDelayKind::REJECT_INERTIAL);
        parseExpression();  // Rejection limit; kept only as a flag for now
        expect(VhdlTokType::KWD_INERTIAL, "'inertial' after the rejection limit");
    } else if (check(VhdlTokType::KWD_INERTIAL)) {
        nodep->delay(VhdlDelayKind::INERTIAL);
    }
}

AstVhdlWaveform* V3VhdlParse::parseWaveform() {
    // waveform ::= waveform_element { , waveform_element } | UNAFFECTED   (10.5.2.1)
    FileLine* const flp = here();
    if (check(VhdlTokType::KWD_UNAFFECTED)) return new AstVhdlWaveform{flp, nullptr, nullptr};
    AstVhdlWaveform* listp = nullptr;
    do {
        FileLine* const wflp = here();
        AstNodeVhdlExpr* valuep = nullptr;
        if (!check(VhdlTokType::KWD_NULL)) valuep = parseExpression();
        AstNodeVhdlExpr* afterp = nullptr;
        if (check(VhdlTokType::KWD_AFTER)) afterp = parseExpression();
        listp = AstNode::addNextNull(listp, new AstVhdlWaveform{wflp, valuep, afterp});
    } while (check(VhdlTokType::COMMA));
    return listp;
}

AstNodeVhdlStmt* V3VhdlParse::parseAssignOrCall(const std::string& label) {
    // A sequential signal or variable assignment, or a procedure call (10.5 - 10.7)
    FileLine* const flp = here();

    if (check(VhdlTokType::KWD_WITH)) {
        // VHDL-2008 selected signal or variable assignment (10.5.4, 10.6.4)
        AstNodeVhdlExpr* const selectorp = parseExpression();
        bool matching = check(VhdlTokType::QUERY);
        expect(VhdlTokType::KWD_SELECT, "'select' after the selector expression");
        // 11.6 and 10.5.4 write the question mark after "select"
        if (check(VhdlTokType::QUERY)) matching = true;
        // The target may be an aggregate, which assigns each element of the
        // value to a target of its own (10.5.2.1, 11.6)
        AstNodeVhdlExpr* targetp = (peekType() == VhdlTokType::LPAREN)
                                       ? parseParenOrAggregate()
                                       : parseName();
        if (!targetp) targetp = new AstVhdlName{flp, ""};
        if (check(VhdlTokType::VAR_ASSIGN)) {
            AstVhdlVarAssign* const nodep
                = new AstVhdlVarAssign{flp, VhdlAssignKind::SELECTED, targetp};
            nodep->name(label);
            nodep->matching(matching);
            nodep->selectorp(selectorp);
            do {
                AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
                AstVhdlWaveform* const wavep
                    = new AstVhdlWaveform{here(), parseExpression(), nullptr};
                altp->addWavesp(wavep);
                expect(VhdlTokType::KWD_WHEN, "'when' in a selected assignment");
                altp->addChoicesp(parseChoices());
                nodep->addAltsp(altp);
            } while (check(VhdlTokType::COMMA));
            expect(VhdlTokType::SEMICOLON, "';' after the selected assignment");
            return nodep;
        }
        expect(VhdlTokType::LE, "'<=' or ':=' in a selected assignment");
        AstVhdlSigAssign* const nodep
            = new AstVhdlSigAssign{flp, VhdlAssignKind::SELECTED, targetp};
        nodep->name(label);
        nodep->matching(matching);
        nodep->selectorp(selectorp);
        parseDelayMechanism(nodep);
        do {
            AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
            altp->addWavesp(parseWaveform());
            expect(VhdlTokType::KWD_WHEN, "'when' in a selected signal assignment");
            altp->addChoicesp(parseChoices());
            nodep->addAltsp(altp);
        } while (check(VhdlTokType::COMMA));
        expect(VhdlTokType::SEMICOLON, "';' after the selected signal assignment");
        return nodep;
    }

    AstNodeVhdlExpr* const namep = (peekType() == VhdlTokType::LPAREN)
                                       ? parseParenOrAggregate()
                                       : parseName();
    if (!namep) return nullptr;

    if (check(VhdlTokType::VAR_ASSIGN)) {
        AstNodeVhdlExpr* const valuep = parseExpression();
        if (peekType() == VhdlTokType::KWD_WHEN) {
            AstVhdlVarAssign* const nodep
                = new AstVhdlVarAssign{flp, VhdlAssignKind::CONDITIONAL, namep};
            nodep->name(label);
            AstVhdlAlternative* const firstp = new AstVhdlAlternative{flp};
            firstp->addWavesp(new AstVhdlWaveform{flp, valuep, nullptr});
            consume();  // "when"
            firstp->condp(parseExpression());
            nodep->addAltsp(firstp);
            while (check(VhdlTokType::KWD_ELSE)) {
                AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
                altp->addWavesp(new AstVhdlWaveform{here(), parseExpression(), nullptr});
                if (check(VhdlTokType::KWD_WHEN)) altp->condp(parseExpression());
                nodep->addAltsp(altp);
            }
            expect(VhdlTokType::SEMICOLON, "';' after the conditional assignment");
            return nodep;
        }
        AstVhdlVarAssign* const nodep
            = new AstVhdlVarAssign{flp, VhdlAssignKind::SIMPLE, namep};
        nodep->name(label);
        nodep->valuep(valuep);
        expect(VhdlTokType::SEMICOLON, "';' after the variable assignment");
        return nodep;
    }

    if (check(VhdlTokType::LE)) {
        AstVhdlSigAssign* const nodep
            = new AstVhdlSigAssign{flp, VhdlAssignKind::SIMPLE, namep};
        nodep->name(label);
        if (check(VhdlTokType::KWD_FORCE)) {
            nodep->force(true);
            if (check(VhdlTokType::KWD_IN)) {
                nodep->forceMode("in");
            } else if (check(VhdlTokType::KWD_OUT)) {
                nodep->forceMode("out");
            }
        } else if (check(VhdlTokType::KWD_RELEASE)) {
            nodep->release(true);
            if (check(VhdlTokType::KWD_IN)) {
                nodep->forceMode("in");
            } else if (check(VhdlTokType::KWD_OUT)) {
                nodep->forceMode("out");
            }
            expect(VhdlTokType::SEMICOLON, "';' after 'release'");
            return nodep;
        } else {
            parseDelayMechanism(nodep);
        }
        AstVhdlWaveform* const wavep = parseWaveform();
        if (peekType() == VhdlTokType::KWD_WHEN) {
            AstVhdlSigAssign* const condp
                = new AstVhdlSigAssign{flp, VhdlAssignKind::CONDITIONAL,
                                       nodep->targetp()->unlinkFrBack()};
            condp->name(label);
            condp->delay(nodep->delay());
            AstVhdlAlternative* const firstp = new AstVhdlAlternative{flp};
            firstp->addWavesp(wavep);
            consume();  // "when"
            firstp->condp(parseExpression());
            condp->addAltsp(firstp);
            while (check(VhdlTokType::KWD_ELSE)) {
                AstVhdlAlternative* const altp = new AstVhdlAlternative{here()};
                altp->addWavesp(parseWaveform());
                if (check(VhdlTokType::KWD_WHEN)) altp->condp(parseExpression());
                condp->addAltsp(altp);
            }
            expect(VhdlTokType::SEMICOLON, "';' after the conditional signal assignment");
            VL_DO_DANGLING(nodep->deleteTree(), nodep);
            return condp;
        }
        nodep->addWavesp(wavep);
        expect(VhdlTokType::SEMICOLON, "';' after the signal assignment");
        return nodep;
    }

    AstVhdlProcCall* const nodep = new AstVhdlProcCall{flp, namep};
    nodep->name(label);
    expect(VhdlTokType::SEMICOLON, "';' after the procedure call");
    return nodep;
}

//######################################################################
// Expressions and names (clauses 8, 9)

bool V3VhdlParse::looksLikeRange() {
    // True if the tokens from here contain a "to", "downto" or "range" at the
    // current parenthesis depth, before the group closes. Used to tell a slice
    // or discrete range from an ordinary expression.
    size_t i = 0;
    int depth = 0;
    for (; i < 400; ++i) {
        const VhdlTokType t = peekType(i);
        if (t == VhdlTokType::END_OF_FILE) return false;
        if (t == VhdlTokType::LPAREN) {
            ++depth;
        } else if (t == VhdlTokType::RPAREN) {
            if (depth == 0) return false;
            --depth;
        } else if (depth == 0) {
            switch (t) {
            case VhdlTokType::KWD_TO:
            case VhdlTokType::KWD_DOWNTO:
            case VhdlTokType::KWD_RANGE: return true;
            case VhdlTokType::IDENTIFIER:
                // "v'reverse_range" names a range just as "v'range" does, but
                // its name is an ordinary identifier where "range" is a
                // keyword, so it has to be recognised by what it says
                if (i > 0 && peekType(i - 1) == VhdlTokType::TICK
                    && peek(i).text == "reverse_range") {
                    return true;
                }
                break;
            case VhdlTokType::COMMA:
            case VhdlTokType::SEMICOLON:
            case VhdlTokType::ARROW:
            case VhdlTokType::BAR:
            case VhdlTokType::KWD_WHEN:
            case VhdlTokType::KWD_ELSE:
            case VhdlTokType::KWD_LOOP:
            case VhdlTokType::KWD_GENERATE: return false;
            default: break;
            }
        }
    }
    return false;
}

AstVhdlRange* V3VhdlParse::parseRange() {
    // range ::= range_attribute_name | simple_expression direction simple_expression  (5.2.1)
    FileLine* const flp = here();
    AstNodeVhdlExpr* const lhsp = parseSimpleExpr();
    if (peekType() == VhdlTokType::KWD_TO || peekType() == VhdlTokType::KWD_DOWNTO) {
        const VhdlDir dir
            = (consume().type == VhdlTokType::KWD_TO) ? VhdlDir::TO : VhdlDir::DOWNTO;
        return new AstVhdlRange{flp, lhsp, dir, parseSimpleExpr()};
    }
    return new AstVhdlRange{flp, lhsp};  // A range attribute name
}

AstNodeVhdl* V3VhdlParse::parseDiscreteRange() {
    // discrete_range ::= discrete_subtype_indication | range   (5.3.2.1)
    FileLine* const flp = here();
    AstNodeVhdlExpr* const lhsp = parseSimpleExpr();
    if (peekType() == VhdlTokType::KWD_TO || peekType() == VhdlTokType::KWD_DOWNTO) {
        const VhdlDir dir
            = (consume().type == VhdlTokType::KWD_TO) ? VhdlDir::TO : VhdlDir::DOWNTO;
        return new AstVhdlRange{flp, lhsp, dir, parseSimpleExpr()};
    }
    if (check(VhdlTokType::KWD_RANGE)) {
        // A subtype indication constrained by a range, e.g. "integer range 1 to 5"
        AstVhdlSubtypeInd* const sip = new AstVhdlSubtypeInd{flp, lhsp};
        if (check(VhdlTokType::BOX)) {
            sip->constraintp(new AstVhdlBox{flp});
        } else {
            sip->constraintp(parseRange());
        }
        return sip;
    }
    // A plain type mark, or a range attribute name
    return new AstVhdlRange{flp, lhsp};
}

AstVhdlChoice* V3VhdlParse::parseChoices() {
    // choices ::= choice { | choice }   (9.3.3.1)
    AstVhdlChoice* listp = nullptr;
    do {
        FileLine* const flp = here();
        if (check(VhdlTokType::KWD_OTHERS)) {
            listp = AstNode::addNextNull(listp, new AstVhdlChoice{flp, nullptr, true});
            continue;
        }
        if (looksLikeRange()) {
            listp = AstNode::addNextNull(listp,
                                         new AstVhdlChoice{flp, parseDiscreteRange()});
            continue;
        }
        AstNodeVhdlExpr* const exprp = parseExpression();
        if (!exprp) break;
        listp = AstNode::addNextNull(listp, new AstVhdlChoice{flp, exprp});
    } while (check(VhdlTokType::BAR));
    return listp;
}

AstVhdlAssoc* V3VhdlParse::parseAssociationList() {
    // association_list ::= association_element { , association_element }   (6.5.7)
    AstVhdlAssoc* listp = nullptr;
    if (peekType() == VhdlTokType::RPAREN) return nullptr;
    do {
        FileLine* const flp = here();
        bool inertial = check(VhdlTokType::KWD_INERTIAL);
        AstNodeVhdlExpr* firstp = nullptr;
        if (check(VhdlTokType::KWD_OPEN)) {
            firstp = new AstVhdlOpen{flp};
        } else if (check(VhdlTokType::KWD_DEFAULT)) {
            firstp = new AstVhdlDefault{flp};
        } else if (peekType() == VhdlTokType::BOX) {
            consume();
            firstp = new AstVhdlDefault{flp};
        } else if (looksLikeRange()) {
            // A subtype indication used as an actual, e.g. in a generic map
            AstNodeVhdl* const rangep = parseDiscreteRange();
            if (rangep) VL_DO_DANGLING(rangep->deleteTree(), rangep);
            firstp = new AstVhdlOpen{flp};
        } else {
            firstp = parseExpression();
        }
        AstVhdlAssoc* assocp = nullptr;
        if (check(VhdlTokType::ARROW)) {
            const bool actInertial = check(VhdlTokType::KWD_INERTIAL);
            AstNodeVhdlExpr* actualp = nullptr;
            if (check(VhdlTokType::KWD_OPEN)) {
                actualp = new AstVhdlOpen{here()};
            } else if (check(VhdlTokType::KWD_DEFAULT)) {
                actualp = new AstVhdlDefault{here()};
            } else {
                actualp = parseExpression();
            }
            assocp = new AstVhdlAssoc{flp, firstp, actualp};
            inertial = inertial || actInertial;
        } else {
            assocp = new AstVhdlAssoc{flp, nullptr, firstp};
        }
        assocp->inertial(inertial);
        listp = AstNode::addNextNull(listp, assocp);
    } while (check(VhdlTokType::COMMA));
    return listp;
}

AstNodeVhdlExpr* V3VhdlParse::parseExpression() {
    // expression ::= condition_operator primary | logical_expression   (9.1)
    AstNodeVhdlExpr* lhsp = nullptr;
    if (peekType() == VhdlTokType::COND_CONV) {
        FileLine* const flp = here();
        consume();
        lhsp = new AstVhdlUnary{flp, VhdlUnaryOp::COND_CONV, parsePrimary()};
    } else {
        lhsp = parseRelation();
    }
    if (!lhsp) return nullptr;
    while (true) {
        VhdlBinaryOp op;
        switch (peekType()) {
        case VhdlTokType::KWD_AND: op = VhdlBinaryOp::AND; break;
        case VhdlTokType::KWD_OR: op = VhdlBinaryOp::OR; break;
        case VhdlTokType::KWD_XOR: op = VhdlBinaryOp::XOR; break;
        case VhdlTokType::KWD_NAND: op = VhdlBinaryOp::NAND; break;
        case VhdlTokType::KWD_NOR: op = VhdlBinaryOp::NOR; break;
        case VhdlTokType::KWD_XNOR: op = VhdlBinaryOp::XNOR; break;
        default: return lhsp;
        }
        FileLine* const flp = here();
        consume();
        AstNodeVhdlExpr* const rhsp = parseRelation();
        if (!rhsp) return lhsp;
        lhsp = new AstVhdlBinary{flp, op, lhsp, rhsp};
    }
}

AstNodeVhdlExpr* V3VhdlParse::parseRelation() {
    // relation ::= shift_expression [ relational_operator shift_expression ]   (9.1)
    AstNodeVhdlExpr* const lhsp = parseShiftExpr();
    if (!lhsp) return nullptr;
    VhdlBinaryOp op;
    switch (peekType()) {
    case VhdlTokType::EQ: op = VhdlBinaryOp::EQ; break;
    case VhdlTokType::NE: op = VhdlBinaryOp::NE; break;
    case VhdlTokType::LT: op = VhdlBinaryOp::LT; break;
    case VhdlTokType::LE: op = VhdlBinaryOp::LE; break;
    case VhdlTokType::GT: op = VhdlBinaryOp::GT; break;
    case VhdlTokType::GE: op = VhdlBinaryOp::GE; break;
    case VhdlTokType::MATCH_EQ: op = VhdlBinaryOp::MATCH_EQ; break;
    case VhdlTokType::MATCH_NE: op = VhdlBinaryOp::MATCH_NE; break;
    case VhdlTokType::MATCH_LT: op = VhdlBinaryOp::MATCH_LT; break;
    case VhdlTokType::MATCH_LE: op = VhdlBinaryOp::MATCH_LE; break;
    case VhdlTokType::MATCH_GT: op = VhdlBinaryOp::MATCH_GT; break;
    case VhdlTokType::MATCH_GE: op = VhdlBinaryOp::MATCH_GE; break;
    default: return lhsp;
    }
    FileLine* const flp = here();
    consume();
    AstNodeVhdlExpr* const rhsp = parseShiftExpr();
    if (!rhsp) return lhsp;
    return new AstVhdlBinary{flp, op, lhsp, rhsp};
}

AstNodeVhdlExpr* V3VhdlParse::parseShiftExpr() {
    // shift_expression ::= simple_expression [ shift_operator simple_expression ]   (9.1)
    AstNodeVhdlExpr* const lhsp = parseSimpleExpr();
    if (!lhsp) return nullptr;
    VhdlBinaryOp op;
    switch (peekType()) {
    case VhdlTokType::KWD_SLL: op = VhdlBinaryOp::SLL; break;
    case VhdlTokType::KWD_SRL: op = VhdlBinaryOp::SRL; break;
    case VhdlTokType::KWD_SLA: op = VhdlBinaryOp::SLA; break;
    case VhdlTokType::KWD_SRA: op = VhdlBinaryOp::SRA; break;
    case VhdlTokType::KWD_ROL: op = VhdlBinaryOp::ROL; break;
    case VhdlTokType::KWD_ROR: op = VhdlBinaryOp::ROR; break;
    default: return lhsp;
    }
    FileLine* const flp = here();
    consume();
    AstNodeVhdlExpr* const rhsp = parseSimpleExpr();
    if (!rhsp) return lhsp;
    return new AstVhdlBinary{flp, op, lhsp, rhsp};
}

AstNodeVhdlExpr* V3VhdlParse::parseSimpleExpr() {
    // simple_expression ::= [ sign ] term { adding_operator term }   (9.1)
    FileLine* const signFlp = here();
    VhdlUnaryOp signOp = VhdlUnaryOp::IDENTITY;
    bool hasSign = false;
    if (check(VhdlTokType::PLUS)) {
        hasSign = true;
    } else if (check(VhdlTokType::MINUS)) {
        signOp = VhdlUnaryOp::NEGATE;
        hasSign = true;
    }
    AstNodeVhdlExpr* lhsp = parseTerm();
    if (!lhsp) return nullptr;
    if (hasSign) lhsp = new AstVhdlUnary{signFlp, signOp, lhsp};
    while (true) {
        VhdlBinaryOp op;
        switch (peekType()) {
        case VhdlTokType::PLUS: op = VhdlBinaryOp::ADD; break;
        case VhdlTokType::MINUS: op = VhdlBinaryOp::SUB; break;
        case VhdlTokType::AMPERSAND: op = VhdlBinaryOp::CONCAT; break;
        default: return lhsp;
        }
        FileLine* const flp = here();
        consume();
        AstNodeVhdlExpr* const rhsp = parseTerm();
        if (!rhsp) return lhsp;
        lhsp = new AstVhdlBinary{flp, op, lhsp, rhsp};
    }
}

AstNodeVhdlExpr* V3VhdlParse::parseTerm() {
    // term ::= factor { multiplying_operator factor }   (9.1)
    AstNodeVhdlExpr* lhsp = parseFactor();
    if (!lhsp) return nullptr;
    while (true) {
        VhdlBinaryOp op;
        switch (peekType()) {
        case VhdlTokType::STAR: op = VhdlBinaryOp::MUL; break;
        case VhdlTokType::SLASH: op = VhdlBinaryOp::DIV; break;
        case VhdlTokType::KWD_MOD: op = VhdlBinaryOp::MOD; break;
        case VhdlTokType::KWD_REM: op = VhdlBinaryOp::REM; break;
        default: return lhsp;
        }
        FileLine* const flp = here();
        consume();
        AstNodeVhdlExpr* const rhsp = parseFactor();
        if (!rhsp) return lhsp;
        lhsp = new AstVhdlBinary{flp, op, lhsp, rhsp};
    }
}

AstNodeVhdlExpr* V3VhdlParse::parseFactor() {
    // factor ::= primary [ ** primary ] | ABS primary | NOT primary
    //          | logical_operator primary        (9.1, the last is VHDL-2008)
    FileLine* const flp = here();
    VhdlUnaryOp op;
    switch (peekType()) {
    case VhdlTokType::KWD_ABS: op = VhdlUnaryOp::ABS; break;
    case VhdlTokType::KWD_NOT: op = VhdlUnaryOp::NOT; break;
    case VhdlTokType::KWD_AND: op = VhdlUnaryOp::AND; break;
    case VhdlTokType::KWD_NAND: op = VhdlUnaryOp::NAND; break;
    case VhdlTokType::KWD_OR: op = VhdlUnaryOp::OR; break;
    case VhdlTokType::KWD_NOR: op = VhdlUnaryOp::NOR; break;
    case VhdlTokType::KWD_XOR: op = VhdlUnaryOp::XOR; break;
    case VhdlTokType::KWD_XNOR: op = VhdlUnaryOp::XNOR; break;
    default: {
        AstNodeVhdlExpr* const lhsp = parsePrimary();
        if (!lhsp) return nullptr;
        if (peekType() == VhdlTokType::DOUBLE_STAR) {
            FileLine* const pflp = here();
            consume();
            AstNodeVhdlExpr* const rhsp = parsePrimary();
            if (!rhsp) return lhsp;
            return new AstVhdlBinary{pflp, VhdlBinaryOp::POW, lhsp, rhsp};
        }
        return lhsp;
    }
    }
    consume();
    AstNodeVhdlExpr* const operandp = parsePrimary();
    if (!operandp) return nullptr;
    return new AstVhdlUnary{flp, op, operandp};
}

AstNodeVhdlExpr* V3VhdlParse::parseLiteral() {
    // Abstract, string, bit string, character or null literal, plus the
    // physical literal form "abstract_literal unit_name" (9.3.2, 15.5 - 15.8)
    FileLine* const flp = here();
    const VhdlToken tok = consume();
    AstVhdlLiteral* nodep = nullptr;
    switch (tok.type) {
    case VhdlTokType::INTEGER_LIT:
        nodep = new AstVhdlLiteral{flp, VhdlLitKind::INTEGER, tok.text};
        nodep->intVal(tok.intVal);
        break;
    case VhdlTokType::REAL_LIT:
        nodep = new AstVhdlLiteral{flp, VhdlLitKind::REAL, tok.text};
        nodep->realVal(tok.realVal);
        break;
    case VhdlTokType::STRING_LIT:
        nodep = new AstVhdlLiteral{flp, VhdlLitKind::STRING, tok.text};
        break;
    case VhdlTokType::BITSTRING_LIT:
        nodep = new AstVhdlLiteral{flp, VhdlLitKind::BITSTRING, tok.text};
        nodep->baseSpec(tok.baseSpec);
        nodep->signedBitStr(tok.signedBitStr);
        break;
    case VhdlTokType::CHARACTER_LIT:
        nodep = new AstVhdlLiteral{flp, VhdlLitKind::CHARACTER, tok.text};
        break;
    default:
        nodep = new AstVhdlLiteral{flp, VhdlLitKind::NULLLIT, ""};
        break;
    }
    // An identifier directly after an abstract literal names a physical unit
    if ((tok.type == VhdlTokType::INTEGER_LIT || tok.type == VhdlTokType::REAL_LIT)
        && peekType() == VhdlTokType::IDENTIFIER) {
        AstVhdlLiteral* const physp = new AstVhdlLiteral{flp, VhdlLitKind::PHYSICAL, tok.text};
        physp->intVal(tok.intVal);
        physp->realVal(tok.realVal);
        physp->unitp(parseName());
        VL_DO_DANGLING(nodep->deleteTree(), nodep);
        return physp;
    }
    return nodep;
}

AstNodeVhdlExpr* V3VhdlParse::parseParenOrAggregate() {
    // A parenthesised expression and an aggregate share the same syntax
    // (9.3.3.1); only a single element with no choice can be the former.
    FileLine* const flp = here();
    consume();  // "("
    AstVhdlAggregate* const aggp = new AstVhdlAggregate{flp};
    bool anyChoices = false;
    int elems = 0;
    if (peekType() != VhdlTokType::RPAREN) {
        do {
            FileLine* const eflp = here();
            AstVhdlChoice* choicesp = nullptr;
            AstNodeVhdlExpr* valuep = nullptr;
            if (peekType() == VhdlTokType::KWD_OTHERS) {
                choicesp = parseChoices();
                anyChoices = true;
                expect(VhdlTokType::ARROW, "'=>' after 'others'");
                valuep = parseExpression();
            } else if (looksLikeRange()) {
                choicesp = parseChoices();
                anyChoices = true;
                expect(VhdlTokType::ARROW, "'=>' after the choices");
                valuep = parseExpression();
            } else {
                AstNodeVhdlExpr* const firstp = parseExpression();
                if (peekType() == VhdlTokType::BAR || peekType() == VhdlTokType::ARROW) {
                    // That was a choice, not a value
                    choicesp = new AstVhdlChoice{eflp, firstp};
                    while (check(VhdlTokType::BAR)) {
                        choicesp = AstNode::addNextNull(choicesp, parseChoices());
                    }
                    anyChoices = true;
                    expect(VhdlTokType::ARROW, "'=>' after the choices");
                    valuep = parseExpression();
                } else {
                    valuep = firstp;
                }
            }
            if (!valuep) break;
            aggp->addElemsp(new AstVhdlElemAssoc{eflp, choicesp, valuep});
            ++elems;
        } while (check(VhdlTokType::COMMA));
    }
    expect(VhdlTokType::RPAREN, "')' to close the expression");

    if (elems == 1 && !anyChoices) {
        // A parenthesised expression: unwrap it
        AstVhdlElemAssoc* const onlyp = aggp->elemsp();
        AstNodeVhdlExpr* const valuep = onlyp->valuep()->unlinkFrBack();
        VL_DO_DANGLING(aggp->deleteTree(), aggp);
        return valuep;
    }
    return aggp;
}

AstNodeVhdlExpr* V3VhdlParse::parseAllocator() {
    // allocator ::= NEW subtype_indication | NEW qualified_expression   (9.3.7)
    FileLine* const flp = here();
    consume();  // "new"
    AstVhdlAllocator* const nodep = new AstVhdlAllocator{flp};
    AstNodeVhdlExpr* const namep = parseName();
    if (!namep) return nodep;
    // A name may already have taken the qualification: "new integer'(42)"
    if (VN_IS(namep, VhdlQualified)) {
        nodep->valuep(namep);
        return nodep;
    }
    if (peekType() == VhdlTokType::TICK) {
        consume();
        AstNodeVhdlExpr* const valuep = parseParenOrAggregate();
        nodep->valuep(new AstVhdlQualified{flp, namep, valuep});
        return nodep;
    }
    AstVhdlSubtypeInd* const sip = new AstVhdlSubtypeInd{flp, namep};
    AstNodeVhdl* const constraintp = parseConstraint();
    if (constraintp) sip->constraintp(constraintp);
    nodep->subtypep(sip);
    return nodep;
}

AstNodeVhdlExpr* V3VhdlParse::parseExternalName() {
    // external_name ::= << object_class pathname : subtype >>   (8.7)
    FileLine* const flp = here();
    consume();  // "<<"
    std::string objClass = "signal";
    if (check(VhdlTokType::KWD_SIGNAL)) {
        objClass = "signal";
    } else if (check(VhdlTokType::KWD_VARIABLE)) {
        objClass = "variable";
    } else if (check(VhdlTokType::KWD_CONSTANT)) {
        objClass = "constant";
    }
    int upLevels = 0;
    bool absolute = false;
    bool package = false;
    if (check(VhdlTokType::AT)) {
        package = true;
    } else if (check(VhdlTokType::DOT)) {
        absolute = true;
    } else {
        while (peekType() == VhdlTokType::CIRCUMFLEX && peekType(1) == VhdlTokType::DOT) {
            consume();
            consume();
            ++upLevels;
        }
    }
    // The pathname is a sequence of identifiers separated by dots
    std::vector<AstNodeVhdlExpr*> path;
    do {
        if (peekType() != VhdlTokType::IDENTIFIER) break;
        AstNodeVhdlExpr* elemp = new AstVhdlName{here(), consume().text};
        if (check(VhdlTokType::LPAREN)) {  // An indexed element of the path
            AstVhdlCall* const callp = new AstVhdlCall{elemp->fileline(), elemp};
            do {
                AstNodeVhdlExpr* const argp = parseExpression();
                if (!argp) break;
                callp->addArgsp(new AstVhdlAssoc{argp->fileline(), nullptr, argp});
            } while (check(VhdlTokType::COMMA));
            expect(VhdlTokType::RPAREN, "')' in an external pathname");
            elemp = callp;
        }
        path.push_back(elemp);
    } while (check(VhdlTokType::DOT));

    expect(VhdlTokType::COLON, "':' before the external name's subtype");
    AstVhdlSubtypeInd* sip = parseSubtypeIndication();
    if (!sip) sip = new AstVhdlSubtypeInd{flp, nullptr};
    expect(VhdlTokType::GTGT, "'>>' to close the external name");

    AstVhdlExternalName* const nodep = new AstVhdlExternalName{flp, objClass, sip};
    nodep->upLevels(upLevels);
    nodep->absolute(absolute);
    nodep->package(package);
    for (AstNodeVhdlExpr* const elemp : path) nodep->addPathp(elemp);
    return nodep;
}

AstNodeVhdlExpr* V3VhdlParse::parseNamePrefix() {
    FileLine* const flp = here();
    switch (peekType()) {
    case VhdlTokType::IDENTIFIER: {
        const VhdlToken tok = consume();
        return new AstVhdlName{flp, tok.text, tok.extendedId};
    }
    case VhdlTokType::STRING_LIT: {
        // An operator symbol used as a name, e.g. "and"(a, b)
        const VhdlToken tok = consume();
        // An operator symbol names a reserved word, which is not case
        // sensitive (15.4.1), and the declaration side already folds it:
        // IEEE's MATH_REAL declares "MOD" and the language writes "mod"
        AstVhdlName* const nodep = new AstVhdlName{flp, lowerOperator(tok.text)};
        nodep->isOperator(true);
        return nodep;
    }
    case VhdlTokType::CHARACTER_LIT: {
        const VhdlToken tok = consume();
        AstVhdlName* const nodep = new AstVhdlName{flp, tok.text};
        nodep->isChar(true);
        return nodep;
    }
    case VhdlTokType::LTLT: return parseExternalName();
    default: return nullptr;
    }
}

AstNodeVhdlExpr* V3VhdlParse::parseNameSuffixes(AstNodeVhdlExpr* prefixp) {
    while (prefixp) {
        FileLine* const flp = here();
        if (check(VhdlTokType::DOT)) {
            if (check(VhdlTokType::KWD_ALL)) {
                prefixp = new AstVhdlSelected{flp, prefixp, nullptr};
                continue;
            }
            AstNodeVhdlExpr* const suffixp = parseNamePrefix();
            if (!suffixp) {
                syntaxError("a name after '.'");
                return prefixp;
            }
            prefixp = new AstVhdlSelected{flp, prefixp, suffixp};
            continue;
        }
        if (peekType() == VhdlTokType::LPAREN) {
            // A function call, an indexed name, a slice or a type conversion;
            // which one cannot be known until names are resolved (8.1)
            consume();
            AstVhdlCall* const callp = new AstVhdlCall{flp, prefixp};
            if (peekType() != VhdlTokType::RPAREN) {
                do {
                    if (looksLikeRange()) {
                        AstNodeVhdl* const rangep = parseDiscreteRange();
                        if (rangep) callp->addArgsp(rangep);
                        continue;
                    }
                    FileLine* const aflp = here();
                    AstNodeVhdlExpr* const firstp = parseExpression();
                    if (!firstp) break;
                    if (check(VhdlTokType::ARROW)) {
                        AstNodeVhdlExpr* actualp = nullptr;
                        if (check(VhdlTokType::KWD_OPEN)) {
                            actualp = new AstVhdlOpen{here()};
                        } else {
                            actualp = parseExpression();
                        }
                        callp->addArgsp(new AstVhdlAssoc{aflp, firstp, actualp});
                    } else {
                        callp->addArgsp(new AstVhdlAssoc{aflp, nullptr, firstp});
                    }
                } while (check(VhdlTokType::COMMA));
            }
            expect(VhdlTokType::RPAREN, "')' to close the name");
            prefixp = callp;
            continue;
        }
        if (peekType() == VhdlTokType::LBRACKET && signaturePrecedesTick()) {
            // A signature before an attribute, e.g. f[return bit]'simple_name.
            // A signature not followed by a tick belongs to the enclosing alias
            // or specification, so it is left for the caller.
            AstVhdlSignature* const sigp = parseSignature();
            if (!expect(VhdlTokType::TICK, "''' after a signature")) {
                VL_DO_DANGLING(sigp->deleteTree(), sigp);
                return prefixp;
            }
            const std::string attr = (peekType() == VhdlTokType::IDENTIFIER)
                                         ? consume().text
                                         : std::string{vhdlTokName(consume().type)};
            AstVhdlAttrName* const attrp = new AstVhdlAttrName{flp, prefixp, attr};
            attrp->signaturep(sigp);
            prefixp = attrp;
            continue;
        }
        if (check(VhdlTokType::TICK)) {
            if (peekType() == VhdlTokType::LPAREN) {
                // qualified_expression ::= type_mark ' ( expression )   (9.3.5)
                AstNodeVhdlExpr* const valuep = parseParenOrAggregate();
                prefixp = new AstVhdlQualified{flp, prefixp, valuep};
                continue;
            }
            // attribute_name ::= prefix ' attribute_designator [ ( expr ) ]   (8.6)
            std::string attr;
            if (peekType() == VhdlTokType::IDENTIFIER) {
                attr = consume().text;
            } else if (vhdlTokIsKeyword(peekType())) {
                attr = vhdlTokName(consume().type);  // range, subtype, ...
            } else {
                syntaxError("an attribute designator");
                return prefixp;
            }
            AstVhdlAttrName* const attrp = new AstVhdlAttrName{flp, prefixp, attr};
            if (check(VhdlTokType::LPAREN)) {
                attrp->argp(parseExpression());
                expect(VhdlTokType::RPAREN, "')' after the attribute argument");
            }
            prefixp = attrp;
            continue;
        }
        break;
    }
    return prefixp;
}

bool V3VhdlParse::signaturePrecedesTick() {
    // Look past the matching ']' to see whether a tick follows
    size_t i = 0;
    int depth = 0;
    for (; i < 200; ++i) {
        const VhdlTokType t = peekType(i);
        if (t == VhdlTokType::END_OF_FILE) return false;
        if (t == VhdlTokType::LBRACKET) {
            ++depth;
        } else if (t == VhdlTokType::RBRACKET) {
            if (--depth == 0) return peekType(i + 1) == VhdlTokType::TICK;
        }
    }
    return false;
}

AstNodeVhdlExpr* V3VhdlParse::parseName() {
    AstNodeVhdlExpr* const prefixp = parseNamePrefix();
    if (!prefixp) return nullptr;
    return parseNameSuffixes(prefixp);
}

AstNodeVhdlExpr* V3VhdlParse::parsePrimary() {
    // primary ::= name | literal | aggregate | function_call | qualified_expression
    //           | type_conversion | allocator | ( expression )   (9.1)
    switch (peekType()) {
    case VhdlTokType::INTEGER_LIT:
    case VhdlTokType::REAL_LIT:
    case VhdlTokType::BITSTRING_LIT: return parseLiteral();
    case VhdlTokType::KWD_NULL: {
        FileLine* const flp = here();
        consume();
        return new AstVhdlLiteral{flp, VhdlLitKind::NULLLIT, ""};
    }
    case VhdlTokType::KWD_NEW: return parseAllocator();
    case VhdlTokType::LPAREN: return parseParenOrAggregate();
    case VhdlTokType::KWD_OPEN: {
        FileLine* const flp = here();
        consume();
        return new AstVhdlOpen{flp};
    }
    case VhdlTokType::CHARACTER_LIT:
    case VhdlTokType::STRING_LIT: {
        // Either a literal or a name; a following '.', '(' or ''' makes it a
        // name, so parse it as one and let the suffix loop decide
        if (peekType(1) == VhdlTokType::DOT || peekType(1) == VhdlTokType::LPAREN
            || peekType(1) == VhdlTokType::TICK) {
            return parseName();
        }
        return parseLiteral();
    }
    case VhdlTokType::IDENTIFIER:
    case VhdlTokType::LTLT: return parseName();
    default:
        syntaxError("an expression");
        return nullptr;
    }
}
