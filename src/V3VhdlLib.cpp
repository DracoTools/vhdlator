// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: VHDL design libraries and analysis order
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
// Collects the design units of every parsed file into libraries, works out
// what each depends on, and sorts them into an order in which they may be
// analyzed (IEEE 1076-2008 clause 13.5).
//
// A unit depends on:
//   * its primary unit, for an architecture, package body or configuration;
//   * every library unit named in its context clause.
//
// Units in libraries Verilator has not been given sources for -- ieee, std --
// are reported differently from units genuinely missing from the user's own
// library, because the former only means the bundled sources are not in yet.
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3VhdlLib.h"

#include "V3Global.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// State

namespace {

std::vector<V3VhdlLib::Unit> s_units;  // Every unit, in the order read
std::map<std::string, size_t> s_byKey;  // key -> index into s_units
std::vector<AstNodeVhdlUnit*> s_ordered;  // Result of computeOrder()
std::set<std::string> s_reportedMissing;  // Keys already complained about
// Where each key was first written: a later unit of the same name replaces
// it, but a unit written in between was analyzed against the first (13.5)
std::map<std::string, const FileLine*> s_firstWritten;
// Where a unit was written again, replacing the one before (13.5)
std::map<std::string, const FileLine*> s_replacedAt;
// Units whose key was already taken. Reported by computeOrder rather than by
// addFile, so that every library diagnostic comes from one place and a
// duplicate does not stop the missing and circular checks from running.

// Lowercase, since VHDL library and unit names are case insensitive
std::string lower(const std::string& in) {
    std::string out = in;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string primaryKey(const std::string& lib, const std::string& name) {
    return lower(lib) + "." + lower(name);
}

// Flatten a selected name into its identifier components. Returns false if the
// name is not a plain sequence of identifiers, e.g. because it is indexed.
bool flattenName(const AstNodeVhdlExpr* nodep, std::vector<std::string>& out) {
    if (const AstVhdlName* const namep = VN_CAST(nodep, VhdlName)) {
        out.push_back(lower(namep->name()));
        return true;
    }
    if (const AstVhdlSelected* const selp = VN_CAST(nodep, VhdlSelected)) {
        if (!flattenName(selp->prefixp(), out)) return false;
        if (!selp->suffixp()) {
            out.push_back("all");
            return true;
        }
        return flattenName(selp->suffixp(), out);
    }
    return false;
}

// The library a unit's "work" refers to
std::string workLib(const AstNodeVhdlUnit* unitp) {
    return unitp->libname().empty() ? "work" : lower(unitp->libname());
}

// Add the dependency a context item implies, if any
void addContextDependency(const AstVhdlContextItem* itemp, const std::string& work,
                          std::vector<std::string>& deps, std::vector<std::string>& libs) {
    if (itemp->kind() == VhdlContextKind::LIBRARY) return;  // Makes a name visible only
    for (AstNode* nodep = itemp->namesp(); nodep; nodep = nodep->nextp()) {
        std::vector<std::string> parts;
        if (!flattenName(VN_AS(nodep, NodeVhdlExpr), parts)) continue;
        if (parts.size() < 2) continue;  // "use foo;" names nothing analyzable
        std::string lib = parts[0];
        if (lib == "work") lib = work;
        // "use work.all" makes every primary unit of the library directly
        // visible (12.4), so it names no one unit to follow
        if (parts.size() == 2 && parts[1] == "all") {
            libs.push_back(lib);
            continue;
        }
        deps.push_back(lib + "." + parts[1]);
    }
}

}  // namespace

//######################################################################
// V3VhdlLib methods

bool V3VhdlLib::isPredefinedLib(const std::string& libname) {
    const std::string lib = lower(libname);
    // Libraries the LRM defines or that Verilator will bundle (clause 16)
    return lib == "std" || lib == "ieee";
}

void V3VhdlLib::clear() {
    s_units.clear();
    s_byKey.clear();
    s_firstWritten.clear();
    s_replacedAt.clear();
    s_ordered.clear();
    s_reportedMissing.clear();
}

void V3VhdlLib::addFile(AstVhdlDesignFile* filep) {
    for (AstNode* nodep = filep->unitsp(); nodep; nodep = nodep->nextp()) {
        AstNodeVhdlUnit* const unitp = VN_AS(nodep, NodeVhdlUnit);
        const std::string work = workLib(unitp);

        Unit unit;
        unit.nodep = unitp;

        // The key, and the primary unit a secondary unit must follow
        if (const AstVhdlArchitecture* const archp = VN_CAST(unitp, VhdlArchitecture)) {
            std::vector<std::string> parts;
            std::string entName;
            if (flattenName(archp->entityNamep(), parts) && !parts.empty()) {
                entName = parts.back();
            }
            unit.key = primaryKey(work, entName) + "(" + lower(unitp->name()) + ")";
            unit.dependsOn.push_back(primaryKey(work, entName));
        } else if (VN_IS(unitp, VhdlPackageBody)) {
            unit.key = primaryKey(work, unitp->name()) + "%body";
            unit.dependsOn.push_back(primaryKey(work, unitp->name()));
        } else if (const AstVhdlConfiguration* const cfgp = VN_CAST(unitp, VhdlConfiguration)) {
            unit.key = primaryKey(work, unitp->name());
            std::vector<std::string> parts;
            if (flattenName(cfgp->entityNamep(), parts) && !parts.empty()) {
                unit.dependsOn.push_back(primaryKey(work, parts.back()));
                // And the architecture it configures, whose statements its
                // block configurations name (3.4)
                const AstVhdlBlockConfig* const bcp = cfgp->blockConfigp();
                const AstVhdlEntitySpec* const specp
                    = bcp ? VN_CAST(bcp->specp(), VhdlEntitySpec) : nullptr;
                const AstVhdlName* const archp
                    = specp ? VN_CAST(specp->namesp(), VhdlName) : nullptr;
                if (archp) {
                    unit.dependsOn.push_back(primaryKey(work, parts.back()) + "("
                                             + lower(archp->name()) + ")");
                }
            }
            // And whatever its binding indications name: "use entity
            // work.dut(single)" is a unit this one has to follow, and no
            // context clause says so (3.4.2, 7.3.2). Soft, because a binding
            // may name a unit analyzed in an earlier run, or none at all.
            cfgp->foreach([&](const AstVhdlBinding* bindp) {
                // "entity work.dut(single)" parses as a call: a name followed
                // by a parenthesised name is not known to be an architecture
                // until the binding says so, so the architecture is the
                // call's argument and the entity is its prefix (7.3.2.1)
                const AstNodeVhdlExpr* namep = bindp->unitp();
                std::string arch = lower(bindp->archName());
                if (const AstVhdlCall* const callp = VN_CAST(namep, VhdlCall)) {
                    namep = callp->prefixp();
                    const AstVhdlAssoc* const asp = VN_CAST(callp->argsp(), VhdlAssoc);
                    const AstVhdlName* const argp
                        = asp ? VN_CAST(asp->actualp(), VhdlName) : nullptr;
                    if (arch.empty() && argp) arch = lower(argp->name());
                }
                std::vector<std::string> bparts;
                if (!namep || !flattenName(namep, bparts) || bparts.empty()) return;
                const std::string lib = bparts.size() >= 2
                                            ? (lower(bparts[bparts.size() - 2]) == "work"
                                                   ? work
                                                   : lower(bparts[bparts.size() - 2]))
                                            : work;
                const std::string key = primaryKey(lib, bparts.back());
                unit.softDependsOn.push_back(key);
                if (!arch.empty()) unit.softDependsOn.push_back(key + "(" + arch + ")");
            });
        } else if (const AstVhdlPackageInst* const instp = VN_CAST(unitp, VhdlPackageInst)) {
            // A package instantiation needs the package it copies, which its
            // context clause need not mention (4.9)
            unit.key = primaryKey(work, unitp->name());
            std::vector<std::string> parts;
            if (flattenName(instp->uninstantiatedp(), parts) && !parts.empty()) {
                // The uninstantiated package may be nested inside another --
                // "work.axi4s.make" -- and then it is the outer one that is
                // the design unit to analyze first (4.9)
                const std::string lib
                    = parts.size() >= 2 ? lower(parts[parts.size() - 2]) : work;
                const std::string key
                    = parts.size() >= 3
                          ? primaryKey(parts[0] == "work" ? work : parts[0], parts[1])
                          : primaryKey(lib == "work" ? work : lib, parts.back());
                unit.dependsOn.push_back(key);
                // And its body, where it has one, since the copy takes that
                // too -- but an uninstantiated package need not have a body,
                // so this only says which order to take them in
                unit.softDependsOn.push_back(key + "%body");
            }
            // A generic package is given another package by name --
            // "fixed_pkg => IEEE.fixed_pkg" -- and that one has to be
            // analyzed first, though no context clause says so (4.9, 6.5.5)
            instp->foreach([&](const AstVhdlSelected* selp) {
                if (VN_IS(selp->backp(), VhdlSelected)) return;  // Only the outermost
                std::vector<std::string> aparts;
                if (!flattenName(const_cast<AstVhdlSelected*>(selp), aparts)) return;
                if (aparts.size() < 2) return;
                const std::string alib = lower(aparts[aparts.size() - 2]);
                unit.softDependsOn.push_back(
                    primaryKey(alib == "work" ? work : alib, aparts.back()));
            });
        } else {
            unit.key = primaryKey(work, unitp->name());
        }

        std::set<const AstNode*> contextItems;
        for (AstNode* itemp = unitp->contextp(); itemp; itemp = itemp->nextp()) {
            contextItems.insert(itemp);
            addContextDependency(VN_AS(itemp, VhdlContextItem), work, unit.dependsOn,
                                 unit.followsLibs);
        }
        // A use clause in a declarative part needs its package as much as one
        // in the context clause does (12.4). Only "lib.unit.item" is a design
        // unit for certain: "use inst.all" may name a package instantiated
        // in the region itself.
        unitp->foreach([&](const AstVhdlContextItem* itemp) {
            if (contextItems.count(itemp) || itemp->kind() != VhdlContextKind::USE) return;
            for (AstNode* nodep = itemp->namesp(); nodep; nodep = nodep->nextp()) {
                std::vector<std::string> parts;
                if (!flattenName(VN_AS(nodep, NodeVhdlExpr), parts) || parts.size() < 2) continue;
                // "use work.unit" names a unit too, where the prefix is
                // certainly a library
                if (parts.size() == 2 && (parts[0] != "work" || parts[1] == "all")) continue;
                const std::string lib = parts[0] == "work" ? work : parts[0];
                unit.dependsOn.push_back(lib + "." + parts[1]);
            }
        });
        // "package p is new work.poly generic map (<>)" needs work.poly
        // analyzed first, and says so nowhere in the context clause (6.5.5)
        unitp->foreach([&](const AstVhdlIfaceDecl* ifp) {
            if (ifp->kind() != VhdlIfaceKind::PACKAGE || ifp->newFrom().empty()) return;
            const std::string& from = ifp->newFrom();
            const size_t dot = from.rfind('.');
            const std::string lib = dot == std::string::npos ? work : lower(from.substr(0, dot));
            const std::string name = dot == std::string::npos ? from : from.substr(dot + 1);
            unit.dependsOn.push_back(primaryKey(lib == "work" ? work : lib, name));
        });

        // "ieee.numeric_bit.to_unsigned(x, 8)" names a package no context
        // clause need mention (12.3), and it has to be analyzed first
        unitp->foreach([&](const AstVhdlSelected* selp) {
            const AstVhdlSelected* innerp = selp;
            while (VN_IS(innerp->prefixp(), VhdlSelected)) {
                innerp = VN_AS(innerp->prefixp(), VhdlSelected);
            }
            if (innerp != selp) return;  // Only the innermost
            const AstVhdlName* const libp = VN_CAST(selp->prefixp(), VhdlName);
            const AstVhdlName* const pkgp = VN_CAST(selp->suffixp(), VhdlName);
            if (!libp || !pkgp || !isPredefinedLib(libp->name())) return;
            unit.softDependsOn.push_back(primaryKey(lower(libp->name()), pkgp->name()));
        });

        s_firstWritten.emplace(unit.key, unitp->fileline());
        const auto it = s_byKey.find(unit.key);
        if (it != s_byKey.end()) {
            // Analyzing a design unit deletes any earlier one of the same
            // simple name in the same library (13.5), so the last one written
            // is the one the design is built from
            s_units[it->second] = unit;
            s_replacedAt[unit.key] = unitp->fileline();
            continue;
        }
        s_byKey.emplace(unit.key, s_units.size());
        s_units.push_back(unit);
    }
}

const std::vector<AstNodeVhdlUnit*>& V3VhdlLib::ordered() { return s_ordered; }

bool V3VhdlLib::computeOrder() {
    s_ordered.clear();
    // "use work.all" is not a dependency on one unit but on all of them, and
    // which those are is only known once every file has been read (12.4)
    for (Unit& unit : s_units) {
        for (const std::string& lib : unit.followsLibs) {
            const std::string prefix = lib + ".";
            for (const Unit& otherp : s_units) {
                if (&otherp == &unit) continue;
                if (otherp.key.compare(0, prefix.size(), prefix) != 0) continue;
                // Only a primary unit can be named: an architecture or a
                // package body is not something a use clause reaches, and
                // following one would put a unit after its own architecture
                if (otherp.key.find('(') != std::string::npos) continue;
                if (otherp.key.find('%') != std::string::npos) continue;
                unit.softDependsOn.push_back(otherp.key);
            }
        }
    }
    // 0 = not visited, 1 = on the stack, 2 = emitted
    std::vector<uint8_t> state(s_units.size(), 0);
    std::vector<size_t> stack;  // For reporting a cycle in source order
    bool ok = true;

    // Iterative depth-first search, so a deeply nested design cannot overflow
    // the C++ stack
    struct Frame final {
        size_t index;
        size_t depIdx;
    };

    for (size_t root = 0; root < s_units.size(); ++root) {
        if (state[root] != 0) continue;
        std::vector<Frame> frames{{root, 0}};
        state[root] = 1;
        stack.push_back(root);
        while (!frames.empty()) {
            Frame& frame = frames.back();
            const Unit& unit = s_units[frame.index];
            const size_t nHard = unit.dependsOn.size();
            if (frame.depIdx < nHard + unit.softDependsOn.size()) {
                const bool soft = frame.depIdx >= nHard;
                const std::string& dep = soft ? unit.softDependsOn[frame.depIdx - nHard]
                                              : unit.dependsOn[frame.depIdx];
                ++frame.depIdx;
                const auto it = s_byKey.find(dep);
                if (it == s_byKey.end()) {
                    // A soft dependency is only there to say which order to
                    // take two units in, so its absence is not a finding
                    if (soft) continue;
                    // Not in any library we were given
                    const std::string lib = dep.substr(0, dep.find('.'));
                    if (!isPredefinedLib(lib) && s_reportedMissing.insert(dep).second) {
                        unit.nodep->fileline()->v3error(
                            "Design unit '" << dep << "' not found, needed by '" << unit.key
                                            << "'");
                        ok = false;
                    }
                    continue;
                }
                const size_t next = it->second;
                // A design file is analyzed in order (13.1), so a unit cannot
                // need one written after it in the same file: that one is not
                // yet in the library when this one is analyzed
                const FileLine* const ufl = unit.nodep->fileline();
                const FileLine* const dfl = s_firstWritten[s_units[next].key];
                if (!soft && ufl->filename() == dfl->filename() && dfl->lineno() > ufl->lineno()) {
                    unit.nodep->fileline()->v3error(
                        "Design unit '" << unit.key << "' needs '" << s_units[next].key
                                        << "', which is written after it in the same file;"
                                           " a design file is analyzed in order");
                    ok = false;
                }
                // A unit analyzed against a version of another that was then
                // replaced is obsolete (13.5), and one written after the
                // replacement cannot use it until it is analyzed again
                if (!soft && ufl->filename() == s_units[next].nodep->fileline()->filename()) {
                    const FileLine* const nfl = s_units[next].nodep->fileline();
                    for (const std::string& ddep : s_units[next].dependsOn) {
                        const auto rit = s_replacedAt.find(ddep);
                        if (rit == s_replacedAt.end()) continue;
                        const FileLine* const rfl = rit->second;
                        if (rfl->filename() != ufl->filename()) continue;
                        if (nfl->lineno() < rfl->lineno() && ufl->lineno() > rfl->lineno()) {
                            unit.nodep->fileline()->v3error(
                                "Design unit '" << unit.key << "' needs '" << s_units[next].key
                                                << "', which was analyzed against the '" << ddep
                                                << "' written again at line " << rfl->lineno()
                                                << ", and is obsolete until analyzed again");
                            ok = false;
                            break;
                        }
                    }
                }
                // A soft dependency only says which order to take two units
                // in, so where it would close a cycle it is simply dropped --
                // "use work.all" names the whole library, itself included
                if (soft && state[next] == 1) continue;
                if (state[next] == 1) {
                    s_units[next].nodep->fileline()->v3error(
                        "Circular dependency between design units: '"
                        << s_units[next].key << "' and '" << unit.key << "'");
                    ok = false;
                    continue;
                }
                if (state[next] == 0) {
                    state[next] = 1;
                    stack.push_back(next);
                    frames.push_back({next, 0});
                }
                continue;
            }
            state[frame.index] = 2;
            s_ordered.push_back(unit.nodep);
            if (!stack.empty()) stack.pop_back();
            frames.pop_back();
        }
    }
    UINFO(3, "VHDL analysis order: " << s_ordered.size() << " unit(s)");
    return ok;
}

void V3VhdlLib::selfTest() {
    // flattenName must reduce a selected name to its components
    FileLine* const flp = new FileLine{FileLine::builtInFilename()};
    AstVhdlName* const libp = new AstVhdlName{flp, "ieee"};
    AstVhdlName* const pkgp = new AstVhdlName{flp, "numeric_std"};
    AstVhdlSelected* const selp = new AstVhdlSelected{flp, libp, pkgp};
    AstVhdlSelected* const allp = new AstVhdlSelected{flp, selp, nullptr};
    std::vector<std::string> parts;
    if (!flattenName(allp, parts)) v3fatalSrc("flattenName failed on a selected name");
    if (parts.size() != 3 || parts[0] != "ieee" || parts[1] != "numeric_std"
        || parts[2] != "all") {
        v3fatalSrc("flattenName produced the wrong components");
    }
    VL_DO_DANGLING(allp->deleteTree(), allp);
}
