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
//
// V3Vhdl is the entry point into the VHDL frontend.
//
// Unlike Verilog, a VHDL file cannot be analyzed in isolation: a design unit
// may depend on packages declared elsewhere, and IEEE 1076-2008 clause 13.5
// requires those to have been analyzed first. Reading is therefore split:
//
//   parseFile()        Called once per VHDL source, from V3ParseImp::parseFile.
//                      Lexes and parses into the design unit database. Resolves
//                      nothing across files. The Verilog preprocessor is not
//                      run -- VHDL has no preprocessor.
//   analyzeAndLower()  Called once, from V3Global::readFiles, after every file
//                      has been read. Orders the design units by dependency,
//                      analyzes them, and lowers the result into Verilator AST.
//
// This file currently holds the M0 skeleton: the database records which files
// were seen, and any VHDL input is reported as unsupported. The lexer, parser,
// analysis and lowering land in subsequent milestones.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3Vhdl.h"

#include "V3File.h"
#include "V3Global.h"
#include "V3VhdlLib.h"
#include "V3VhdlLower.h"
#include "V3VhdlSem.h"
#include "V3VhdlParse.h"
#include "V3VhdlScan.h"

#include "V3Os.h"

#include <set>
#include <string>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Design unit database

namespace {
// VHDL sources read this run, in the order given
std::vector<std::string> s_filenames;
// Parse trees, one per source, awaiting analysis
std::vector<AstVhdlDesignFile*> s_designFiles;
}  // namespace

namespace {
// Print one token in a stable, greppable form, for --dump-vhdl-tokens
void dumpToken(const VhdlToken& token) {
    std::cout << token.flp->filename() << ":" << token.flp->lineno() << ":"
              << token.flp->firstColumn() << ": " << vhdlTokName(token.type);
    switch (token.type) {
    case VhdlTokType::IDENTIFIER:
        std::cout << " " << (token.extendedId ? "\\" + token.text + "\\" : token.text);
        break;
    case VhdlTokType::CHARACTER_LIT: std::cout << " '" << token.text << "'"; break;
    case VhdlTokType::STRING_LIT:
    case VhdlTokType::TOOL_DIRECTIVE: std::cout << " \"" << token.text << "\""; break;
    case VhdlTokType::BITSTRING_LIT:
        std::cout << " " << (token.signedBitStr ? "S" : "") << token.baseSpec << "\""
                  << token.text << "\"";
        break;
    case VhdlTokType::INTEGER_LIT: std::cout << " " << token.intVal; break;
    case VhdlTokType::REAL_LIT: {
        // %g rather than operator<<, so the golden does not depend on the
        // stream's default precision
        char buf[64];
        VL_SNPRINTF(buf, sizeof(buf), "%.15g", token.realVal);
        std::cout << " " << token.text << " = " << buf;
        break;
    }
    default: break;
    }
    std::cout << "\n";
}
}  // namespace

//######################################################################
// The libraries Verilator ships
//
// A design that says "library ieee; use ieee.numeric_std.all" expects those
// packages to be there, exactly as a SystemVerilog one expects "std::". The
// IEEE P1076 working group publishes the standard packages under Apache-2.0,
// so Verilator can ship the normative sources rather than reimplement them.
//
// They are read on demand: a design that uses nothing but STD.STANDARD parses
// nothing extra, and one that uses NUMERIC_STD reads it and the package it
// depends on. Loading everything would cost every VHDL compile about half a
// second for packages most designs never mention.

namespace {

struct VhdlBundledUnit final {
    const char* namep;  // Package name, lowercased
    const char* dependsp;  // Space separated packages it needs first
    const char* filesp;  // Space separated sources, declaration before body
};

// The IEEE library. Order within each entry matters: a package declaration
// must be analyzed before its body.
const VhdlBundledUnit VHDL_IEEE_UNITS[] = {
    {"std_logic_1164", "", "std_logic_1164.vhdl std_logic_1164-body.vhdl"},
    {"numeric_std", "std_logic_1164", "numeric_std.vhdl numeric_std-body.vhdl"},
    {"numeric_bit", "", "numeric_bit.vhdl numeric_bit-body.vhdl"},
    {"numeric_std_unsigned", "numeric_std",
     "numeric_std_unsigned.vhdl numeric_std_unsigned-body.vhdl"},
    {"numeric_bit_unsigned", "numeric_bit",
     "numeric_bit_unsigned.vhdl numeric_bit_unsigned-body.vhdl"},
    {"math_real", "", "math_real.vhdl math_real-body.vhdl"},
    {"math_complex", "math_real", "math_complex.vhdl math_complex-body.vhdl"},
    {"std_logic_textio", "std_logic_1164", "std_logic_textio.vhdl"},
    {"fixed_float_types", "", "fixed_float_types.vhdl"},
    {"fixed_generic_pkg", "fixed_float_types numeric_std math_real",
     "fixed_generic_pkg.vhdl fixed_generic_pkg-body.vhdl"},
    {"fixed_pkg", "fixed_generic_pkg", "fixed_pkg.vhdl"},
    {"float_generic_pkg", "fixed_float_types numeric_std math_real fixed_pkg",
     "float_generic_pkg.vhdl float_generic_pkg-body.vhdl"},
    {"float_pkg", "float_generic_pkg", "float_pkg.vhdl"},
};

std::vector<std::string> splitWords(const std::string& text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t end = text.find(' ', pos);
        if (end == std::string::npos) {
            out.push_back(text.substr(pos));
            break;
        }
        if (end > pos) out.push_back(text.substr(pos, end - pos));
        pos = end + 1;
    }
    return out;
}

}  // namespace

// Which packages of a shipped library the source names.
//
// Scanning the whole tree rather than just the context clauses is deliberate:
// a package may be named by a use clause inside a declarative part (12.4), by
// a context declaration (13.3), or by a selected name written out in full
// with no use clause at all, and all three have to bring the package in.
class VhdlLibraryScanner final : public VNVisitorConst {
    const std::string m_libname;
    std::set<std::string>& m_wanted;

    void visit(AstVhdlSelected* nodep) override {
        // Walk down to the leftmost name, which is the library
        const AstVhdlSelected* innerp = nodep;
        while (VN_IS(innerp->prefixp(), VhdlSelected)) {
            innerp = VN_AS(innerp->prefixp(), VhdlSelected);
        }
        const AstVhdlName* const libp = VN_CAST(innerp->prefixp(), VhdlName);
        const AstVhdlName* const pkgp = VN_CAST(innerp->suffixp(), VhdlName);
        if (libp && pkgp && libp->name() == m_libname) m_wanted.insert(pkgp->name());
        iterateChildrenConst(nodep);
    }
    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    VhdlLibraryScanner(AstNode* nodep, const std::string& libname, std::set<std::string>& wanted)
        : m_libname{libname}
        , m_wanted{wanted} {
        iterateAndNextConstNull(nodep);
    }
    ~VhdlLibraryScanner() override = default;
};

void V3Vhdl::gatherWanted(const AstNodeVhdlUnit* unitp, const std::string& libname,
                          std::set<std::string>& wanted) {
    VhdlLibraryScanner{const_cast<AstNodeVhdlUnit*>(unitp), libname, wanted};
}

void V3Vhdl::loadBundledLibraries(VInFilter* filterp) {
    if (!v3Global.opt.stdVhdl()) return;
    std::set<std::string> wanted;
    for (const AstVhdlDesignFile* const filep : s_designFiles) {
        for (AstNode* unitp = filep->unitsp(); unitp; unitp = unitp->nextp()) {
            gatherWanted(VN_AS(unitp, NodeVhdlUnit), "ieee", wanted);
        }
    }
    if (wanted.empty()) return;

    // Follow what each wants, so that using NUMERIC_STD brings STD_LOGIC_1164
    std::set<std::string> loaded;
    bool progress = true;
    while (progress) {
        progress = false;
        for (const VhdlBundledUnit& unit : VHDL_IEEE_UNITS) {
            if (!wanted.count(unit.namep) || loaded.count(unit.namep)) continue;
            bool ready = true;
            for (const std::string& dep : splitWords(unit.dependsp)) {
                if (!loaded.count(dep)) {
                    wanted.insert(dep);
                    ready = false;
                }
            }
            if (!ready) {
                progress = true;
                continue;
            }
            loaded.insert(unit.namep);
            progress = true;
            for (const std::string& file : splitWords(unit.filesp)) {
                const std::string path
                    = V3Os::filenameJoin(V3Options::getVhdlLibPath("ieee"), file);
                FileLine* const flp = new FileLine{FileLine::builtInFilename()};
                parseFile(flp, path, "ieee", filterp,
                          "Cannot find the IEEE VHDL library Verilator ships;"
                          " use --no-std-vhdl to compile without it: ");
            }
        }
    }
}

//######################################################################
// V3Vhdl methods

void V3Vhdl::parseFile(FileLine* fl, const string& filename, const string& libname,
                       VInFilter* filterp, const string& errmsg) {
    UINFO(2, __FUNCTION__ << ": " << filename << " [lib " << libname << "]");

    // Resolve through the same search path as Verilog sources, so -y and
    // relative filenames behave identically across the two languages.
    const string foundname = v3Global.opt.filePath(fl, filename, "", errmsg);
    if (foundname.empty()) return;  // filePath() already reported it, if asked to

    s_filenames.push_back(foundname);
    // What the model was built from, so that "--skip-identical" notices when
    // a source changes and "-MMD" writes it as a dependency. The Verilog
    // preprocessor does this for Verilog; VHDL does not go through it.
    V3File::addSrcDepend(foundname);

    // Slurp the file. VHDL sources are small, and a flat buffer keeps the
    // scanner's lookahead trivial.
    VInFilter::StrList lines;
    if (!filterp->readWholefile(foundname, lines)) {  // LCOV_EXCL_START
        // Defensive: filePath() above already found the file, so this only
        // fires if it becomes unreadable between the two calls
        fl->v3error("Cannot read VHDL file: " << foundname);
        return;
    }  // LCOV_EXCL_STOP
    std::string text;
    for (const std::string& line : lines) text += line;

    FileLine* const flp = new FileLine{foundname};
    flp->language(v3Global.opt.fileLanguage(foundname));
    flp->lineno(1);
    // An ascending range is how VHDL usually writes an index, so warning
    // about one would fire on nearly every design. Every fileline the scanner
    // makes copies this setting, so it covers the whole source.
    flp->warnOff(V3ErrorCode::ASCRANGE, true);

    if (v3Global.opt.dumpLevel("vhdl-tokens") != 0) {
        // Scanner-level debug: dump the token stream and go no further
        V3VhdlScan scanner{flp, text, flp->language()};
        while (true) {
            const VhdlToken token = scanner.next();
            if (token.type == VhdlTokType::END_OF_FILE) break;
            dumpToken(token);
        }
        return;
    }

    V3VhdlParse parser{flp, text, flp->language(), libname};
    AstVhdlDesignFile* const filep = parser.parse(foundname);
    UINFO(3, "Parsed " << foundname << ", " << parser.errCount() << " syntax error(s)");
    if (v3Global.opt.dumpLevel("vhdl-parse") != 0) filep->dumpTree(std::cout, "  ", 0);
    V3VhdlLib::addFile(filep);
    s_designFiles.push_back(filep);
}

bool V3Vhdl::anyRead() { return !s_filenames.empty(); }

void V3Vhdl::analyzeAndLower(AstNetlist* rootp, VInFilter* filterp) {
    if (!anyRead()) return;  // No VHDL in this design; nothing to do
    UINFO(2, __FUNCTION__ << ": " << s_filenames.size() << " VHDL file(s)");

    // Bring in the libraries Verilator ships, for whatever the design uses
    loadBundledLibraries(filterp);

    // Order the design units so each follows what it depends on (13.5)
    V3VhdlLib::computeOrder();
    const std::vector<AstNodeVhdlUnit*>& order = V3VhdlLib::ordered();
    if (v3Global.opt.dumpLevel("vhdl-units") != 0) {
        for (const AstNodeVhdlUnit* const unitp : order) {
            std::cout << unitp->fileline()->filename() << ":" << unitp->fileline()->lineno()
                      << ": " << unitp->typeName() << " " << unitp->libname() << "."
                      << unitp->name() << "\n";
        }
    }

    // Give the parse tree meaning: scopes, symbols and types
    const bool analyzed = V3VhdlSem::analyze(order);

    // Then rewrite it as the AST the rest of Verilator works on. Lowering a
    // tree that failed analysis would report the same problems a second time
    // in less useful terms, so it only runs on a clean one.
    if (analyzed) {
        // An entity with a generic type was copied once per type the design
        // gave it, and those copies are design units the back end has to
        // build. They did not exist when the order was computed, and each
        // follows the entity it was copied from, so they go on the end.
        std::vector<AstNodeVhdlUnit*> withCopies = order;
        for (AstNodeVhdlUnit* const unitp : V3VhdlSem::specializedUnits()) {
            withCopies.push_back(unitp);
        }
        V3VhdlLower::lower(withCopies, rootp);
    }

    // The VHDL tree has served its purpose; nothing downstream may see one
    V3VhdlSem::clear();
    V3VhdlLib::clear();
    for (AstVhdlDesignFile* const filep : s_designFiles) {
        VL_DO_DANGLING(filep->deleteTree(), filep);
    }
    s_designFiles.clear();
}
