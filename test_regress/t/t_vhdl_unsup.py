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

# Constructs the frontend understands but cannot lower yet. Each must say so
# clearly rather than be mis-compiled. Also covers --vhdl-std selecting the
# revision.
test.top_filename = "t/t_vhdl_unsup.vhd"

test.lint(fails=True,
          verilator_flags2=["--vhdl-std 2008"],
          expect_filename=test.golden_filename)

test.passes()
