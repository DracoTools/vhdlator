// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Language code class
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

#ifndef VERILATOR_V3LANGCODE_H_
#define VERILATOR_V3LANGCODE_H_

#include "config_build.h"
#include "verilatedos.h"

#include <map>
#include <set>
#include <vector>

//######################################################################
//! Class for the different languages supported.
//! A separate file, since used both in V3Options (globally) and FileLine 9per
//! file).
class V3LangCode final {
public:
    enum en : uint8_t {
        L_ERROR,  // Must be first.
        L1364_1995,
        L1364_2001,
        L1364_2005,
        L1800_2005,
        L1800_2009,
        L1800_2012,
        L1800_2017,
        L1800_2023,
        // VHDL (IEEE 1076). Not `begin_keywords values; see ascii() below
        L1076_1993,
        L1076_2000,
        L1076_2002,
        L1076_2008,
        L1076_2019,
        // ***Add new elements below also***
        _ENUM_END
    };
    const char* ascii() const {
        const char* const names[]
            = {// The Verilog entries must match the `begin_keywords values.
               // The VHDL entries are IEEE 1076 revisions, accepted by --language
               // and by --vhdl-std, but never by `begin_keywords.
               " ERROR",    "1364-1995", "1364-2001", "1364-2005", "1800-2005",
               "1800-2009", "1800-2012", "1800-2017", "1800-2023", "1076-1993",
               "1076-2000", "1076-2002", "1076-2008", "1076-2019"};
        return names[m_e];
    }
    static V3LangCode mostRecent() VL_MT_SAFE { return V3LangCode{L1800_2023}; }
    // Most recent VHDL revision Verilator defaults to. Deliberately 2008, not 2019:
    // 2008 is what the IEEE released the standard packages under and what real code targets.
    static V3LangCode mostRecentVhdl() VL_MT_SAFE { return V3LangCode{L1076_2008}; }
    bool systemVerilog() const {
        return m_e == L1800_2005 || m_e == L1800_2009 || m_e == L1800_2012 || m_e == L1800_2017
               || m_e == L1800_2023;
    }
    bool vhdl() const VL_MT_SAFE {
        return m_e == L1076_1993 || m_e == L1076_2000 || m_e == L1076_2002 || m_e == L1076_2008
               || m_e == L1076_2019;
    }
    bool legal() const { return m_e != L_ERROR; }
    //
    enum en m_e;
    V3LangCode()
        : m_e{L_ERROR} {}
    // cppcheck-suppress noExplicitConstructor
    constexpr V3LangCode(en _e)
        : m_e{_e} {}
    explicit V3LangCode(const char* textp);
    explicit V3LangCode(int _e)
        : m_e(static_cast<en>(_e)) {}  // Need () or GCC 4.8 false warning
    constexpr operator en() const { return m_e; }
};

//######################################################################

#endif  // guard
