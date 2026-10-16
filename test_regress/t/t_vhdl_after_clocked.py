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

# A clocked process that also assigns with an after delay
test.top_filename = "t/t_vhdl_after_clocked.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_after_clocked"],
             make_top_shell=False,
             make_main=False)

# Only the process that starts a delivery has to stay in the 'act' region
test.file_grep(test.obj_dir + "/Vt_vhdl_after_clocked_t_vhdl_after_clocked__0.cpp",
               r'_nba_sequent__')

test.execute()

test.passes()
