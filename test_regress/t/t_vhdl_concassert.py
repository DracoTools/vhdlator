#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('vlt')

# A concurrent assertion runs when what it reads changes, and at no other time
test.top_filename = "t/t_vhdl_concassert.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_concassert"],
             make_top_shell=False,
             make_main=False)

test.execute()

# At the start and when a changes; b changing twice is no reason
test.file_grep_count(test.run_log_filename, r'check of a', 2)
# At the start and when s changes, which only the function's argument shows
test.file_grep_count(test.run_log_filename, r'check of s', 2)
# At the start only: once w changes the condition holds
test.file_grep_count(test.run_log_filename, r'check of w', 1)

test.passes()
