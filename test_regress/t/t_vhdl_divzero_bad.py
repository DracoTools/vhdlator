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

# An integer division by a zero the analyzer can see is an error when it runs:
# a warning as the design is built, and the run stops where it is reached
test.top_filename = "t/t_vhdl_divzero_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "-Wno-VHDLBOUNDS",
                               "--top-module t_vhdl_divzero_bad"],
             make_top_shell=False,
             make_main=False)

test.execute(fails=True)

test.file_grep(test.run_log_filename, r'before the division')
test.file_grep(test.run_log_filename, r'Division by zero')
test.file_grep_count(test.run_log_filename, r'not reached', 0)

test.passes()
