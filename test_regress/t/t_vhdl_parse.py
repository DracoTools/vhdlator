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

# A cross-section of the VHDL-2008 grammar. The golden is the parse tree, so a
# change in how any construct is parsed shows up as a diff.
# --dump-tree-addrids keeps node identifiers stable across runs.
test.top_filename = "t/t_vhdl_parse.vhd"

test.lint(fails=True,
          verilator_flags2=["--dump-vhdl-parse", "--dump-tree-addrids"],
          expect_filename=test.golden_filename)

test.passes()
