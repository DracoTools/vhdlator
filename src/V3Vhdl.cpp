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
#include "V3VhdlParse.h"
#include "V3VhdlScan.h"

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
// V3Vhdl methods

void V3Vhdl::parseFile(FileLine* fl, const string& filename, const string& libname,
                       VInFilter* filterp, const string& errmsg) {
    UINFO(2, __FUNCTION__ << ": " << filename << " [lib " << libname << "]");

    // Resolve through the same search path as Verilog sources, so -y and
    // relative filenames behave identically across the two languages.
    const string foundname = v3Global.opt.filePath(fl, filename, "", errmsg);
    if (foundname.empty()) return;  // filePath() already reported it, if asked to

    s_filenames.push_back(foundname);

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

void V3Vhdl::analyzeAndLower(AstNetlist* rootp) {
    if (!anyRead()) return;  // No VHDL in this design; nothing to do
    UINFO(2, __FUNCTION__ << ": " << s_filenames.size() << " VHDL file(s)");

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

    // M3+ : resolve names, check types, and lower the result into rootp. Until
    // then the parse tree has served its purpose.
    const size_t units = order.size();
    V3VhdlLib::clear();
    for (AstVhdlDesignFile* const filep : s_designFiles) {
        VL_DO_DANGLING(filep->deleteTree(), filep);
    }
    s_designFiles.clear();

    v3Global.rootp()->fileline()->v3error(
        "VHDL analysis is not yet implemented; ordered "
        << units << " design unit(s) from " << s_filenames.size()
        << " file(s)\n... The VHDL frontend is under development;"
           " Verilog and SystemVerilog are unaffected.");
}
