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

# FOREIGN subprograms that analysis accepts but that have no C form: one with
# a body of its own, and ones whose result or parameter has no length until
# the value exists.
test.top_filename = "t/t_vhdl_foreign_lower_bad.vhd"

test.lint(fails=True, expect_filename=test.golden_filename)

test.passes()
