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

# Under --vhdl-metavalues a signal of an array of resolved vectors that
# several instances drive through inout ports is refused rather than left to
# whichever writes last: its words are a memory, not lanes one after another
test.top_filename = "t/t_vhdl_inoutzelem.vhd"

test.lint(verilator_flags2=["--vhdl-metavalues", "--top-module t_vhdl_inoutzelem"],
          fails=True,
          expect_filename=test.golden_filename)

test.passes()
