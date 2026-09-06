// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Inlining of modules
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************

#ifndef VERILATOR_V3TASK_H_
#define VERILATOR_V3TASK_H_

#include "config_build.h"
#include "verilatedos.h"

#include "V3Ast.h"
#include "V3Container.h"
#include "V3Error.h"

#include <functional>
#include <utility>
#include <vector>

//============================================================================

using V3TaskConnect = std::pair<AstVar*, AstArg*>;  // [port, pin-connects-to]
using V3TaskConnects = std::vector<V3TaskConnect>;  // [ [port, pin-connects-to] ... ]

class V3TaskConnectState final {
    VNDeleter m_deleter;  // Allow delayed deletion of nodes
    bool m_didWrap = false;  // Made a wrapper
    using WrapMap = std::map<std::pair<AstNodeFTask*, std::string>, AstNodeFTask*>;
    WrapMap m_wrapMap;  // Map of {old function, arguments} -> new function
public:
    V3TaskConnectState() = default;
    ~V3TaskConnectState() = default;
    void pushDeletep(AstNode* nodep) { m_deleter.pushDeletep(nodep); }
    bool didWrap() const { return m_didWrap; }
    void setDidWrap() { m_didWrap = true; }
    WrapMap& wrapMap() { return m_wrapMap; }
};

//============================================================================

class V3Task final {
    static const char* const s_dpiTemporaryVarSuffix;

public:
    static void taskAll(AstNetlist* nodep) VL_MT_DISABLED;
    /// Return vector of [port, pin-connects-to]  (SLOW)
    static V3TaskConnects taskConnects(AstNodeFTaskRef* nodep, AstNode* taskStmtsp,
                                       V3TaskConnectState* statep = nullptr,
                                       bool makeChanges = true) VL_MT_DISABLED;
    static void taskConnectWrap(AstNodeFTaskRef* nodep, const V3TaskConnects& tconnects,
                                V3TaskConnectState* statep,
                                const VInsertionSet<const AstVar*>& argWrap) VL_MT_DISABLED;
    static AstNodeFTask*
    taskConnectWrapNew(AstNodeFTask* taskp, const string& newname, const V3TaskConnects& tconnects,
                       const VInsertionSet<const AstVar*>& argWrap) VL_MT_DISABLED;
    static string assignInternalToDpi(AstVar* portp, bool isPtr, const string& frSuffix,
                                      const string& toSuffix,
                                      const string& frPrefix = "") VL_MT_DISABLED;
    static string assignDpiToInternal(const string& lhsName, AstVar* rhsp) VL_MT_DISABLED;
    static const char* dpiTemporaryVarSuffix() VL_MT_SAFE { return s_dpiTemporaryVarSuffix; }

    // Call 'f' on every AstVarRef under 'nodep', descending into the bodies of
    // the tasks this pass left out of line (AstCFunc::ordersWithCaller): a call
    // is a statement of whoever wrote it, so what the task reads and writes is
    // read and written there. Variables local to such a task are skipped --
    // there is one of each per task rather than per call site, so they say
    // nothing about how the callers depend on one another.
    template <typename T_Callable>
    static void foreachVarRefThroughCalls(AstNode* nodep, T_Callable&& f) {
        std::vector<const AstCFunc*> following;
        const std::function<void(AstNode*)> recurse = [&](AstNode* rootp) {
            const bool inCall = !following.empty();
            rootp->foreach([&](AstNode* np) {
                if (AstVarRef* const refp = VN_CAST(np, VarRef)) {
                    if (inCall && refp->varp()->isFuncLocal()) return;
                    f(refp);
                } else if (const AstCCall* const callp = VN_CAST(np, CCall)) {
                    AstCFunc* const funcp = callp->funcp();
                    if (!funcp->ordersWithCaller()) return;
                    // Recursion, which is never left out of line, but be safe
                    for (const AstCFunc* const onStackp : following) {
                        if (onStackp == funcp) return;
                    }
                    following.push_back(funcp);
                    recurse(funcp);
                    following.pop_back();
                }
            });
        };
        recurse(nodep);
    }
};

#endif  // Guard
