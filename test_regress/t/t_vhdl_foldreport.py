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

# A call that reports, in a statement that runs, is made when it runs
test.top_filename = "t/t_vhdl_foldreport.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_foldreport"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.file_grep_count(test.run_log_filename, r'f reads 5', 1)
test.file_grep_count(test.run_log_filename, r'g given too much', 1)

test.passes()
