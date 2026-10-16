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

test.top_filename = "t/t_vhdl_nine_mulgeneric.vhd"

# Under --vhdl-metavalues; nvc, GHDL and Questa pass it
test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_nine_mulgeneric"],
             make_top_shell=False,
             make_main=False)

# Natively, with no value of a length only running knows
test.file_grep_not(test.obj_dir + "/Vt_vhdl_nine_mulgeneric___024root.h", r'Vdyn')

test.execute(check_finished=True)

test.passes()
