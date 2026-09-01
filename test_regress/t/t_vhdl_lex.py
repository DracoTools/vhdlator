#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('linter')

# Lexical elements, IEEE 1076-2008 clause 15. The golden is the token stream,
# so a change in how any construct is scanned shows up as a diff.
test.top_filename = "t/t_vhdl_lex.vhd"

test.lint(fails=False,
          verilator_flags2=["--dump-vhdl-tokens"],
          expect_filename=test.golden_filename)

test.passes()
