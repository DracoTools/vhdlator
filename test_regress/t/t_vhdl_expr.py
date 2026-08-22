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

# Every expression here has exactly one interpretation once the context is
# taken into account, so analysis must report nothing beyond the placeholder
# that says lowering is still to come.
test.top_filename = "t/t_vhdl_expr.vhd"

test.lint(fails=True, expect_filename=test.golden_filename)

test.passes()
