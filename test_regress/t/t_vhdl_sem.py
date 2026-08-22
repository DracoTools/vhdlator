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

# Declarations that analysis must turn into symbols and types. The golden is
# the symbol table, so a change in what any declaration means shows as a diff.
test.top_filename = "t/t_vhdl_sem.vhd"

test.lint(fails=True,
          verilator_flags2=["--dump-vhdl-syms"],
          expect_filename=test.golden_filename)

test.passes()
