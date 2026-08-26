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
// Units whose key was already taken. Reported by computeOrder rather than by
// addFile, so that every library diagnostic comes from one place and a
// duplicate does not stop the missing and circular checks from running.
std::vector<std::pair<AstNodeVhdlUnit*, AstNodeVhdlUnit*>> s_duplicates;

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
                          std::vector<std::string>& deps) {
    if (itemp->kind() == VhdlContextKind::LIBRARY) return;  // Makes a name visible only
    for (AstNode* nodep = itemp->namesp(); nodep; nodep = nodep->nextp()) {
        std::vector<std::string> parts;
        if (!flattenName(VN_AS(nodep, NodeVhdlExpr), parts)) continue;
        if (parts.size() < 2) continue;  // "use foo;" names nothing analyzable
        std::string lib = parts[0];
        if (lib == "work") lib = work;
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
    s_ordered.clear();
    s_reportedMissing.clear();
    s_duplicates.clear();
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
            }
        } else if (const AstVhdlPackageInst* const instp = VN_CAST(unitp, VhdlPackageInst)) {
            // A package instantiation needs the package it copies, which its
            // context clause need not mention (4.9)
            unit.key = primaryKey(work, unitp->name());
            std::vector<std::string> parts;
            if (flattenName(instp->uninstantiatedp(), parts) && !parts.empty()) {
                const std::string lib
                    = parts.size() >= 2 ? lower(parts[parts.size() - 2]) : work;
                unit.dependsOn.push_back(primaryKey(lib == "work" ? work : lib, parts.back()));
                // And its body, where it has one, since the copy takes that
                // too -- but an uninstantiated package need not have a body,
                // so this only says which order to take them in
                unit.softDependsOn.push_back(
                    primaryKey(lib == "work" ? work : lib, parts.back()) + "%body");
            }
        } else {
            unit.key = primaryKey(work, unitp->name());
        }

        for (AstNode* itemp = unitp->contextp(); itemp; itemp = itemp->nextp()) {
            addContextDependency(VN_AS(itemp, VhdlContextItem), work, unit.dependsOn);
        }
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

        const auto it = s_byKey.find(unit.key);
        if (it != s_byKey.end()) {
            s_duplicates.emplace_back(unitp, s_units[it->second].nodep);
            continue;
        }
        s_byKey.emplace(unit.key, s_units.size());
        s_units.push_back(unit);
    }
}

const std::vector<AstNodeVhdlUnit*>& V3VhdlLib::ordered() { return s_ordered; }

bool V3VhdlLib::computeOrder() {
    s_ordered.clear();
    bool dupOk = true;
    for (const auto& pair : s_duplicates) {
        pair.first->fileline()->v3error(
            "Design unit is declared more than once; the earlier one is at "
            << pair.second->fileline()->ascii());
        dupOk = false;
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
    return ok && dupOk;
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
