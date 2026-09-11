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

# What a constant, signal, generic or port may be of, where a constant may
# be deferred, and the type of an interface default
test.top_filename = "t/t_vhdl_holdvalue_bad.vhd"

test.lint(fails=True, expect_filename=test.golden_filename)

test.passes()
