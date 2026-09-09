// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Hash AST trees to find duplicates
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2005-2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
//
//  Datastructure for finding duplicate AstNode trees via hashing
//
//*************************************************************************

#ifndef VERILATOR_V3DUPFINDER_H_
#define VERILATOR_V3DUPFINDER_H_
#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"
#include "V3Error.h"
#include "V3Hasher.h"

#include <map>
#include <memory>

//============================================================================

struct V3DupFinderUserSame VL_NOT_FINAL {
    // Functor for V3DupFinder::findDuplicate
    virtual bool isSame(AstNode*, AstNode*) = 0;
    V3DupFinderUserSame() = default;
    virtual ~V3DupFinderUserSame() = default;
};

// This really is just a multimap from 'node hash' to 'node pointer', with some minor extensions.
class V3DupFinder final : private std::multimap<V3Hash, AstNode*> {
    using Super = std::multimap<V3Hash, AstNode*>;

    // MEMBERS
    const V3Hasher* const m_hasherOwnedp = nullptr;  // Pointer to owned hasher
    const V3Hasher& m_hasher;  // Reference to hasher

public:
    // CONSTRUCTORS
    V3DupFinder()
        : m_hasherOwnedp{new V3Hasher}
        , m_hasher{*m_hasherOwnedp} {}
    explicit V3DupFinder(const V3Hasher& hasher)
        : m_hasher{hasher} {}
    ~V3DupFinder() {
        if (m_hasherOwnedp) delete m_hasherOwnedp;
    }

    // METHODS

    // Expose minimal set of superclass interface
    using Super::begin;
    using Super::cbegin;
    using Super::cend;
    using Super::clear;
    using Super::const_iterator;
    using Super::empty;
    using Super::end;
    using Super::erase;
    using Super::iterator;
    using Super::size_type;

    // Insert node into data structure
    iterator insert(AstNode* nodep) { return emplace(m_hasher(nodep), nodep); }

    // Insert node into data structure, under an additional key the caller also requires.
    // Use when the caller's isSame() demands more than the node's own tree: hashing that
    // requirement here keeps candidates it would reject out of the bucket, rather than
    // paying for a comparison per pair. 'extra' must agree with isSame(), that is, two
    // nodes it accepts must have been given the same 'extra'. A node inserted this way
    // cannot be found by erase(), which knows only the node's own hash.
    iterator insert(AstNode* nodep, const V3Hash& extra) {
        return emplace(m_hasher(nodep) + extra.value(), nodep);
    }

    // Erase node from data structure
    size_type erase(AstNode* nodep) VL_MT_DISABLED;

    // Return duplicate, if one was inserted, with optional user check for sameness
    iterator findDuplicate(AstNode* nodep, V3DupFinderUserSame* checkp = nullptr) VL_MT_DISABLED;

    // Return duplicate inserted under the same additional key, with user check for sameness
    iterator findDuplicate(AstNode* nodep, V3DupFinderUserSame* checkp,
                           const V3Hash& extra) VL_MT_DISABLED;

private:
    iterator findDuplicateAt(V3Hash key, AstNode* nodep,
                             V3DupFinderUserSame* checkp) VL_MT_DISABLED;

public:

    // Dump for debug
    void dumpFile(const string& filename, bool tree) VL_MT_DISABLED;
    void dumpFilePrefixed(const string& nameComment, bool tree = false) VL_MT_DISABLED;
};

#endif  // Guard
