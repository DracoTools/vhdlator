#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios("simulator")
test.top_filename = "t/t_task_inline_mult.v"

# The same design at the other end of the knob: a budget of one operation
# leaves every task that is called more than once out of line.
test.compile(verilator_flags2=["--stats", "--inline-task-mult 1"])

test.execute()

if test.vlt_all:
    test.file_grep(test.stats, r'Functions not inlined by cost\s+(\d+)', 6)

test.passes()
