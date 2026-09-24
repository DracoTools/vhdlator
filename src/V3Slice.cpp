// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Parse module/signal name references
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
// Slice TRANSFORMATIONS:
//      Top-down traversal (SliceVisitor):
//        NODEASSIGN
//          ARRAYSEL
//            Compare the dimensions to the Var to check for implicit slices.
//            Using ->length() calculate the number of clones needed.
//          VARREF
//            Check the dimensions of the Var for an implicit slice.
//            Replace with ArraySel nodes if needed.
//          SEL, EXTEND
//            We might be assigning a 1-D packed array to a 2-D packed array,
//            this is unsupported.
//          SliceCloneVisitor (called if this node is a slice):
//            NODEASSIGN
//              Clone and iterate the clone:
//                ARRAYSEL
//                  Modify bitp() for the new value and set ->length(1)
//
// TODO: This code was written before SLICESEL was a type, it might be
// simplified to look primarily for SLICESELs.
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3Slice.h"

#include "V3Stats.h"

VL_DEFINE_DEBUG_FUNCTIONS;

//*************************************************************************

class SliceVisitor final : public VNVisitor {
    // NODE STATE
    // Cleared on netlist
    //  AstNodeAssign::user1()      -> bool.  True if find is complete
    //  AstNodeUniop::user1()       -> bool.  True if find is complete
    //  AstArraySel::user1p()       -> AstVarRef. The VarRef that the final ArraySel points to
    const VNUser1InUse m_inuser1;
    //  AstInitArray::user2()       -> int.  Previously accessed itemIdx
    //  AstInitItem::user2()        -> int.  Corresponding first elemIdx
    const VNUser2InUse m_inuser2;

    // STATE - across all visitors
    VDouble0 m_statAssigns;  // Statistic tracking
    VDouble0 m_statSliceElementSkips;  // Statistic tracking

    // STATE - for current visit position (use VL_RESTORER)
    AstNode* m_assignp = nullptr;  // Assignment we are under
    AstScope* m_scopep = nullptr;  // Scope we are under, for temporaries
    bool m_assignError = false;  // True if the current assign already has an error
    bool m_okInitArray = false;  // Allow InitArray children
    int m_tempNum = 0;  // Number for the next temporary

    // METHODS
    AstNodeExpr* cloneAndSel(AstNodeExpr* const nodep, int elements, int elemIdx,
                             const bool needPure) {
        // Insert an ArraySel, except for a few special cases
        const AstUnpackArrayDType* const arrayp
            = VN_CAST(nodep->dtypep()->skipRefp(), UnpackArrayDType);
        if (!arrayp) {  // V3Width should have complained, but...
            if (!m_assignError) {
                nodep->v3error(
                    nodep->prettyTypeName()
                    << " is not an unpacked array, but is in an unpacked array context");
            } else {
                V3Error::incErrors();  // Otherwise might infinite loop
            }
            m_assignError = true;
            // Likely will cause downstream errors
            return nodep->cloneTree(false, needPure);
        }
        if (arrayp->rangep()->elementsConst() != elements) {
            if (!m_assignError) {
                nodep->v3error(
                    "Slices of arrays in assignments have different unpacked dimensions, "
                    << elements << " versus " << arrayp->rangep()->elementsConst());
            }
            m_assignError = true;
            elements = 1;
            elemIdx = 0;
        }
        AstNodeExpr* newp;
        if (AstInitArray* const initp = VN_CAST(nodep, InitArray)) {
            UINFO(9, "  cloneInitArray(" << elements << "," << elemIdx << ") " << nodep);

            auto considerOrder = [](const auto* nodep, int idxFromLeft) -> int {
                return !nodep->rangep()->ascending()
                           ? nodep->rangep()->elementsConst() - 1 - idxFromLeft
                           : idxFromLeft;
            };
            newp = nullptr;
            int itemIdx = 0;
            int i = 0;
            const AstInitArray::KeyItemMap& itemMap = initp->map();
            if (const int prevItemIdx = initp->user2()) {
                const auto it = itemMap.find(considerOrder(arrayp, prevItemIdx));
                if (it != itemMap.end()) {
                    const AstInitItem* itemp = it->second;
                    if (itemp->user2() && itemp->user2() < elemIdx) {
                        // Let's resume traversal from the previous position
                        itemIdx = prevItemIdx;
                        i = itemp->user2();
                    }
                }
            }
            const AstNodeDType* const expectedItemDTypep = arrayp->subDTypep()->skipRefp();
            while (i <= elemIdx) {
                const auto itemIt = itemMap.find(considerOrder(arrayp, itemIdx));
                AstNodeExpr* const itemp
                    = itemIt != itemMap.end() ? itemIt->second->valuep() : initp->defaultp();
                const bool directItem = itemIt != itemMap.end();
                if (!itemp && !m_assignError) {
                    nodep->v3error("Array initialization has too few elements, need element "
                                   << elemIdx);
                    m_assignError = true;
                    break;
                }
                const AstNodeDType* itemRawDTypep = itemp->dtypep()->skipRefp();
                const VCastable castable
                    = AstNode::computeCastable(expectedItemDTypep, itemRawDTypep, itemp);
                if (castable == VCastable::SAMEISH || castable == VCastable::COMPATIBLE) {
                    if (i == elemIdx) {
                        newp = itemp->cloneTree(false, !directItem && needPure);
                        break;
                    } else {  // Check the next item
                        ++i;
                        ++itemIdx;
                    }
                } else {
                    const AstUnpackArrayDType* const itemDTypep
                        = VN_CAST(itemRawDTypep, UnpackArrayDType);
                    if (!itemDTypep
                        || !expectedItemDTypep->isSame(itemDTypep->subDTypep()->skipRefp())) {
                        if (!m_assignError) {
                            itemp->v3error("Item is incompatible with the array type.");
                        }
                        m_assignError = true;
                        break;
                    }
                    if (i + itemDTypep->elementsConst()
                        > elemIdx) {  // This item contains the element
                        int offset = considerOrder(itemDTypep, elemIdx - i);
                        if (AstSliceSel* const slicep = VN_CAST(itemp, SliceSel)) {
                            offset += slicep->declRange().lo();
                            newp = new AstArraySel{nodep->fileline(),
                                                   slicep->fromp()->cloneTreePure(false), offset};
                        } else {
                            newp = new AstArraySel{nodep->fileline(), itemp->cloneTreePure(false),
                                                   offset};
                        }

                        if (!m_assignError && elemIdx + 1 == elements
                            && i + itemDTypep->elementsConst() > elements) {
                            nodep->v3error("Array initialization has too many elements. "
                                           << elements << " elements are expected, but at least "
                                           << i + itemDTypep->elementsConst()
                                           << " elements exist.");
                            m_assignError = true;
                        }
                        break;
                    } else {  // Check the next item
                        i += itemDTypep->elementsConst();
                        ++itemIdx;
                    }
                }
            }
            if (elemIdx + 1 == elements && static_cast<size_t>(itemIdx) + 1 < initp->map().size()
                && !m_assignError) {
                nodep->v3error("Array initialization has too many elements. "
                               << elements << " elements are expected, but at least "
                               << i + initp->map().size() - itemIdx << " elements exist.");
                m_assignError = true;
            }
            if (newp) {
                const auto it = itemMap.find(considerOrder(arrayp, itemIdx));
                if (it != itemMap.end()) {  // Remember current position for the next invocation.
                    initp->user2(itemIdx);
                    it->second->user2(i);
                }
            }
            if (!newp) newp = new AstConst{nodep->fileline(), 0};
        } else if (AstCond* const snodep = VN_CAST(nodep, Cond)) {
            UINFO(9, "  cloneCond(" << elements << "," << elemIdx << ") " << nodep);
            return new AstCond{snodep->fileline(), snodep->condp()->cloneTree(false, needPure),
                               cloneAndSel(snodep->thenp(), elements, elemIdx, needPure),
                               cloneAndSel(snodep->elsep(), elements, elemIdx, needPure)};
        } else if (const AstSliceSel* const snodep = VN_CAST(nodep, SliceSel)) {
            UINFO(9, "  cloneSliceSel(" << elements << "," << elemIdx << ") " << nodep);
            const int leOffset = (snodep->declRange().lo()
                                  + (!snodep->declRange().ascending()
                                         ? snodep->declRange().elements() - 1 - elemIdx
                                         : elemIdx));
            newp = new AstArraySel{nodep->fileline(), snodep->fromp()->cloneTree(false, needPure),
                                   leOffset};
        } else if (const AstSampled* const snodep = VN_CAST(nodep, Sampled)) {
            UINFO(9, "  cloneSliceSel(" << elements << "," << elemIdx << ") " << nodep);
            AstNodeExpr* const exprp = VN_AS(snodep->exprp(), NodeExpr);
            AstNodeExpr* const selp = cloneAndSel(exprp, elements, elemIdx, needPure);
            return new AstSampled{nodep->fileline(), selp, nullptr, snodep->internal()};
        } else if (AstExprStmt* const snodep = VN_CAST(nodep, ExprStmt)) {
            UINFO(9, "  cloneExprStmt(" << elements << "," << elemIdx << ") " << nodep);
            AstNodeExpr* const resultSelp
                = cloneAndSel(snodep->resultp(), elements, elemIdx, needPure);
            if (snodep->stmtsp()) {
                return new AstExprStmt{nodep->fileline(), snodep->stmtsp()->unlinkFrBackWithNext(),
                                       resultSelp};
            } else {
                return resultSelp;
            }
        } else if (VN_IS(nodep, NodeVarRef) || VN_IS(nodep, NodeSel) || VN_IS(nodep, CMethodHard)
                   || VN_IS(nodep, MemberSel) || VN_IS(nodep, StructSel)) {
            UINFO(9, "  cloneSel(" << elements << "," << elemIdx << ") " << nodep);
            const int leOffset = !arrayp->rangep()->ascending()
                                     ? arrayp->rangep()->elementsConst() - 1 - elemIdx
                                     : elemIdx;
            newp = new AstArraySel{nodep->fileline(), nodep->cloneTree(false, needPure), leOffset};
        } else {
            if (!m_assignError) {
                nodep->v3error(nodep->prettyTypeName()
                               << " unexpected in assignment to unpacked array");
            }
            m_assignError = true;
            // Likely will cause downstream errors
            newp = nodep->cloneTree(false, needPure);
        }
        return newp;
    }

    // The variable a reference ultimately reaches, through selects
    static const AstVar* rootVarp(const AstNode* nodep) {
        while (nodep) {
            if (const AstNodeVarRef* const refp = VN_CAST(nodep, NodeVarRef)) return refp->varp();
            if (const AstArraySel* const selp = VN_CAST(nodep, ArraySel)) {
                nodep = selp->fromp();
            } else if (const AstSliceSel* const selp = VN_CAST(nodep, SliceSel)) {
                nodep = selp->fromp();
            } else if (const AstSel* const selp = VN_CAST(nodep, Sel)) {
                nodep = selp->fromp();
            } else if (const AstMemberSel* const selp = VN_CAST(nodep, MemberSel)) {
                nodep = selp->fromp();
            } else if (const AstStructSel* const selp = VN_CAST(nodep, StructSel)) {
                nodep = selp->fromp();
            } else if (const AstCMethodHard* const methp = VN_CAST(nodep, CMethodHard)) {
                // How an element of a dynamic array or a queue is selected
                nodep = methp->fromp();
            } else {
                return nullptr;
            }
        }
        return nullptr;
    }

    // Whether expanding this assignment element by element could read an
    // element the expansion has already written. An assignment is evaluated
    // in full before any of it is assigned (IEEE 1800-2023 10.7), which
    // element-at-a-time assignment only preserves when the two sides do not
    // touch the same variable.
    static bool selfOverlaps(AstNodeAssign* nodep) {
        if (!VN_IS(nodep, Assign)) return false;  // Only immediate writes can bite
        // A whole-array copy reads element i to write element i, whatever the
        // two sides name, so the order cannot matter
        if (VN_IS(nodep->rhsp(), NodeVarRef)) return false;
        const AstVar* const lhsVarp = rootVarp(nodep->lhsp());
        // Where the destination cannot be named -- an element of a dynamic
        // array, say -- leave the assignment as it was rather than rewriting
        // it into another the same question would be asked of
        if (!lhsVarp) return false;
        return nodep->rhsp()->exists(
            [lhsVarp](const AstNodeVarRef* refp) { return refp->varp() == lhsVarp; });
    }

    // Assign through a temporary, so that the whole right-hand side is
    // computed before any of the destination changes
    void insertTemp(AstNodeAssign* nodep) {
        FileLine* const flp = nodep->fileline();
        AstNodeDType* const dtypep = nodep->lhsp()->dtypep();
        const std::string name = "__Vslicetmp" + cvtToStr(m_tempNum++);
        AstVar* const varp = new AstVar{flp, VVarType::BLOCKTEMP, name, dtypep};
        varp->funcLocal(false);
        m_scopep->modp()->addStmtsp(varp);
        AstVarScope* const vscp = new AstVarScope{flp, m_scopep, varp};
        m_scopep->addVarsp(vscp);
        AstNodeExpr* const rhsp = nodep->rhsp()->unlinkFrBack();
        AstNodeExpr* const lhsp = nodep->lhsp()->unlinkFrBack();
        AstAssign* const fillp
            = new AstAssign{flp, new AstVarRef{flp, vscp, VAccess::WRITE}, rhsp};
        AstAssign* const usep
            = new AstAssign{flp, lhsp, new AstVarRef{flp, vscp, VAccess::READ}};
        fillp->addNext(usep);
        nodep->replaceWith(fillp);
        VL_DO_DANGLING(nodep->deleteTree(), nodep);
    }

    // Elements over every unpacked dimension, which is what an expansion
    // writes one term per; stops growing once past any sensible limit
    static int64_t totalElements(const AstUnpackArrayDType* dtypep) {
        int64_t total = 1;
        for (const AstNodeDType* dp = dtypep;
             const AstUnpackArrayDType* const adp = VN_CAST(dp, UnpackArrayDType);
             dp = adp->subDTypep()->skipRefp()) {
            total *= adp->rangep()->elementsConst();
            if (total > (1 << 30)) break;
        }
        return total;
    }

    // Whether the VlUnpacked holding each type pairs their elements position
    // by position the way the language does, which is from the left: the
    // same count and direction in every dimension. With leafToo, the elements
    // must also be of one type, as for comparing them in C++.
    static bool sameLayout(const AstNodeDType* ap, const AstNodeDType* bp, bool leafToo) {
        ap = ap->skipRefp();
        bp = bp->skipRefp();
        const AstUnpackArrayDType* const aarrp = VN_CAST(ap, UnpackArrayDType);
        const AstUnpackArrayDType* const barrp = VN_CAST(bp, UnpackArrayDType);
        if (!aarrp || !barrp) return !aarrp && !barrp && (!leafToo || ap->similarDType(bp));
        return aarrp->rangep()->elementsConst() == barrp->rangep()->elementsConst()
               && aarrp->rangep()->ascending() == barrp->rangep()->ascending()
               && sameLayout(aarrp->subDTypep(), barrp->subDTypep(), leafToo);
    }
    // Whether the types differ only in the direction of the outer dimension,
    // which a reversed copy of the whole array puts right
    static bool reversedLayout(const AstNodeDType* ap, const AstNodeDType* bp, bool leafToo) {
        const AstUnpackArrayDType* const aarrp = VN_CAST(ap->skipRefp(), UnpackArrayDType);
        const AstUnpackArrayDType* const barrp = VN_CAST(bp->skipRefp(), UnpackArrayDType);
        return aarrp && barrp
               && aarrp->rangep()->elementsConst() == barrp->rangep()->elementsConst()
               && aarrp->rangep()->ascending() != barrp->rangep()->ascending()
               && sameLayout(aarrp->subDTypep(), barrp->subDTypep(), leafToo);
    }
    // An operand that is an array in storage, as cloneAndSel selects from
    static bool wholeArray(const AstNode* nodep) {
        return VN_IS(nodep, NodeVarRef) || VN_IS(nodep, NodeSel) || VN_IS(nodep, CMethodHard)
               || VN_IS(nodep, MemberSel) || VN_IS(nodep, StructSel);
    }
    // The operand's array with the outer dimension reversed, laid out as dtypep
    static AstNodeExpr* reversedCopy(AstNodeExpr* nodep, AstNodeDType* dtypep) {
        FileLine* const flp = nodep->fileline();
        AstCExpr* const revp
            = nodep->isPure() ? new AstCExpr{flp, AstCExpr::Pure{}} : new AstCExpr{flp};
        revp->add("VL_PACK_REVERSED(");
        revp->add(nodep);
        revp->add(")");
        revp->dtypep(dtypep);
        return revp;
    }
    // Whether an assignment's value can be copied whole into an array of
    // dtypep, reversed first where that puts it right
    static bool copiesWhole(AstNodeExpr* rhsp, const AstNodeDType* dtypep, bool& reverse) {
        reverse = false;
        if (VN_IS(rhsp, InitArray)) return true;  // Made for the target, index by index
        if (const AstCond* const condp = VN_CAST(rhsp, Cond)) {
            bool thenReverse;
            bool elseReverse;
            return copiesWhole(condp->thenp(), dtypep, thenReverse)
                   && copiesWhole(condp->elsep(), dtypep, elseReverse) && !thenReverse
                   && !elseReverse;
        }
        if (sameLayout(dtypep, rhsp->dtypep(), false)) return true;
        reverse = wholeArray(rhsp) && reversedLayout(dtypep, rhsp->dtypep(), false);
        return reverse;
    }

    bool assignOptimize(AstNodeAssign* nodep) {
        // Return true if did optimization
        AstNodeDType* const dtp = nodep->lhsp()->dtypep()->skipRefp();
        AstNode* stp = nodep->rhsp();
        const AstUnpackArrayDType* const arrayp = VN_CAST(dtp, UnpackArrayDType);
        if (!arrayp) return false;
        if (VN_IS(stp, CvtPackedToArray)) return false;
        if (VN_IS(stp, CReset)) return false;

        // Any isSc variables must be expanded regardless of --fno-slice
        const bool hasSc
            = nodep->exists([&](const AstVarRef* refp) -> bool { return refp->varp()->isSc(); });
        if (!hasSc && !v3Global.opt.fSlice()) {
            m_okInitArray = true;  // VL_RESTORER in visit(AstNodeAssign)
            return false;
        }

        // Skip optimization if array is too large, counting the elements of
        // every dimension: expanded, each is an assignment, and a 128 by 64
        // array was 8192 of them. Left whole, VlUnpacked copies position by
        // position, which is the language's pairing from the left only when
        // both sides run the same way: past the limit 'up = dn' assigned
        // up[0] from dn[0] rather than from dn[299].
        const int elements = arrayp->rangep()->elementsConst();
        const int elementLimit = v3Global.opt.fSliceElementLimit();
        // SystemC arrays keep the outer count, having to be expanded otherwise
        const int64_t size = hasSc ? elements : totalElements(arrayp);
        bool reverse = false;
        if (elementLimit > 0 && size > elementLimit
            && copiesWhole(nodep->rhsp(), arrayp, reverse)) {
            if (reverse) {
                AstNodeExpr* const rhsp = nodep->rhsp()->unlinkFrBack();
                nodep->rhsp(reversedCopy(rhsp, nodep->lhsp()->dtypep()));
            }
            ++m_statSliceElementSkips;
            m_okInitArray = true;  // VL_RESTORER in visit(AstNodeAssign)
            return false;
        }

        // Skip if this is a simple a = b assignment of identical arrays
        if (AstVarRef* const lhsp = VN_CAST(nodep->lhsp(), VarRef)) {
            if (AstVarRef* const rhsp = VN_CAST(nodep->rhsp(), VarRef)) {
                if (!hasSc && lhsp->dtypep()->skipRefp()->sameTree(rhsp->dtypep()->skipRefp())) {
                    m_okInitArray = true;  // VL_RESTORER in visit(AstNodeAssign)
                    return false;
                }
            }
        }

        UINFO(4, "Slice optimizing " << nodep);
        ++m_statAssigns;

        // Left and right could have different ascending/descending range,
        // but #elements is common and all variables are realigned to start at zero
        // Assign of an ascending range slice to a descending range one must reverse
        // the elements
        AstNodeAssign* newlistp = nullptr;
        for (int elemIdx = 0; elemIdx < elements; ++elemIdx) {
            // Original node is replaced, so it is safe to copy it one time even if it is impure.
            AstNodeAssign* const newp
                = nodep->cloneType(cloneAndSel(nodep->lhsp(), elements, elemIdx, elemIdx != 0),
                                   cloneAndSel(nodep->rhsp(), elements, elemIdx, elemIdx != 0));
            UINFOTREE(9, newp, "", "new");
            newlistp = AstNode::addNext(newlistp, newp);
        }
        UINFOTREE(9, nodep, "", "Deslice-Dn");
        nodep->replaceWith(newlistp);
        VL_DO_DANGLING(nodep->deleteTree(), nodep);
        // Normal edit iterator will now iterate on all of the expansion assignments
        // This will potentially call this function again to resolve next level of slicing
        return true;
    }

    void visit(AstNodeAssign* nodep) override {
        // Called recursively on newly created assignments
        if (nodep->user1SetOnce()) return;  // Process once
        UINFOTREE(9, nodep, "", "Deslice-In");
        VL_RESTORER(m_assignError);
        VL_RESTORER(m_assignp);
        VL_RESTORER(m_okInitArray);  // Set in assignOptimize
        m_assignError = false;
        m_assignp = nodep;
        if (m_scopep && VN_IS(nodep->lhsp()->dtypep()->skipRefp(), UnpackArrayDType)
            && selfOverlaps(nodep)) {
            // The two new assignments are iterated in turn, and neither of
            // them overlaps itself, so this happens at most once per assign
            insertTemp(nodep);
            return;
        }
        if (assignOptimize(nodep)) return;
        iterateChildren(nodep);
    }

    void visit(AstConsPackUOrStruct* nodep) override {
        VL_RESTORER(m_okInitArray);
        m_okInitArray = true;
        iterateChildren(nodep);
    }
    void visit(AstConsDynArray* nodep) override {
        VL_RESTORER(m_okInitArray);
        m_okInitArray = true;
        iterateChildren(nodep);
    }
    void visit(AstConsQueue* nodep) override {
        VL_RESTORER(m_okInitArray);
        m_okInitArray = true;
        iterateChildren(nodep);
    }
    void visit(AstInitArray* nodep) override {
        UASSERT_OBJ(!m_assignp || m_okInitArray, nodep,
                    "Array initialization should have been removed earlier");
    }

    template <typename T_NodeBiop>
    void expandBiOp(T_NodeBiop* biopp) {
        AstNodeBiop* nodep = biopp;
        if (nodep->user1SetOnce()) return;  // Process once
        UINFO(9, "  Bi-Eq/Neq expansion " << nodep);

        // Only expand if lhs is an unpacked array (we assume type checks already passed)
        const AstNodeDType* const fromDtp = nodep->lhsp()->dtypep()->skipRefp();
        if (const AstUnpackArrayDType* const adtypep = VN_CAST(fromDtp, UnpackArrayDType)) {
            // Expanded, a comparison is one term per element, of every
            // dimension: two int [1024][64] arrays compared once gave 14 MB
            // of C++. Past the limit an assignment is left whole, and so is a
            // comparison of two stored arrays laid out alike, or alike once
            // one is reversed, which VlUnpacked makes element by element in
            // a loop.
            const int elementLimit = v3Global.opt.fSliceElementLimit();
            const AstNodeDType* const rhsDtp = nodep->rhsp()->dtypep();
            if (elementLimit > 0 && totalElements(adtypep) > elementLimit
                && wholeArray(nodep->lhsp()) && wholeArray(nodep->rhsp())
                && (sameLayout(adtypep, rhsDtp, true) || reversedLayout(adtypep, rhsDtp, true))) {
                ++m_statSliceElementSkips;
                FileLine* const flp = nodep->fileline();
                const bool equal = VN_IS(nodep, Eq) || VN_IS(nodep, EqCase);
                const bool reverse = !sameLayout(adtypep, rhsDtp, true);
                AstNodeExpr* const lhsp = nodep->lhsp()->unlinkFrBack();
                AstNodeExpr* rhsp = nodep->rhsp()->unlinkFrBack();
                if (reverse) rhsp = reversedCopy(rhsp, lhsp->dtypep());
                AstCExpr* const cmpp = lhsp->isPure() && rhsp->isPure()
                                           ? new AstCExpr{flp, AstCExpr::Pure{}, "("}
                                           : new AstCExpr{flp, "("};
                cmpp->add(lhsp);
                cmpp->add(equal ? " == " : " != ");
                cmpp->add(rhsp);
                cmpp->add(")");
                cmpp->dtypeSetBit();
                nodep->replaceWith(cmpp);
                VL_DO_DANGLING(pushDeletep(nodep), nodep);
                iterateChildren(cmpp);
                return;
            }
            AstNodeBiop* logp = nullptr;

            const int elements = adtypep->rangep()->elementsConst();
            for (int elemIdx = 0; elemIdx < elements; ++elemIdx) {
                // EQ(a,b) -> LOGAND(EQ(ARRAYSEL(a,0), ARRAYSEL(b,0)), ...[1])
                // Original node is replaced, so it is safe to copy it one time even if it is
                // impure.
                T_NodeBiop* const clonep = new T_NodeBiop{
                    nodep->fileline(), cloneAndSel(nodep->lhsp(), elements, elemIdx, elemIdx != 0),
                    cloneAndSel(nodep->rhsp(), elements, elemIdx, elemIdx != 0)};
                if (!logp) {
                    logp = clonep;
                } else {
                    switch (nodep->type()) {
                    case VNType::Eq:  // FALLTHRU
                    case VNType::EqCase:
                        logp = new AstLogAnd{nodep->fileline(), logp, clonep};
                        break;
                    case VNType::Neq:  // FALLTHRU
                    case VNType::NeqCase:
                        logp = new AstLogOr{nodep->fileline(), logp, clonep};
                        break;
                    default: nodep->v3fatalSrc("Unknown node type processing array slice"); break;
                    }
                }
            }
            UASSERT_OBJ(logp, nodep, "Unpacked array with empty indices range");
            nodep->replaceWith(logp);
            VL_DO_DANGLING(pushDeletep(nodep), nodep);
            nodep = logp;
        }

        iterateChildren(nodep);
    }

    void visit(AstEq* nodep) override { expandBiOp(nodep); }
    void visit(AstNeq* nodep) override { expandBiOp(nodep); }
    void visit(AstEqCase* nodep) override { expandBiOp(nodep); }
    void visit(AstNeqCase* nodep) override { expandBiOp(nodep); }

    void visit(AstScope* nodep) override {
        VL_RESTORER(m_scopep);
        m_scopep = nodep;
        iterateChildren(nodep);
    }
    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // CONSTRUCTORS
    explicit SliceVisitor(AstNetlist* nodep) { iterate(nodep); }
    ~SliceVisitor() override {
        V3Stats::addStat("Optimizations, Slice array assignments", m_statAssigns);
        V3Stats::addStat("Optimizations, Slice array skips due to size limit",
                         m_statSliceElementSkips);
    }
};

//######################################################################
// Link class functions

void V3Slice::sliceAll(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ":");
    { SliceVisitor{nodep}; }  // Destruct before checking
    V3Global::dumpCheckGlobalTree("slice", 0, dumpTreeEitherLevel() >= 3);
}
