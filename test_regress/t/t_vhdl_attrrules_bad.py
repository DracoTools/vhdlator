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

# Where an attribute specification may appear and what it names, and what a user attribute or SIMPLE_NAME may be asked of
test.top_filename = "t/t_vhdl_attrrules_bad.vhd"

test.lint(fails=True, expect_filename=test.golden_filename)

test.passes()
