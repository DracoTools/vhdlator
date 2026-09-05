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

# RESIZE fills what the result is short with the operand's leftmost
# element -- its sign -- and writes the low elements over that (9.3.6),
# so a SIGNED keeps its sign leftmost however the lengths compare.
test.top_filename = "t/t_vhdl_ressign.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_ressign"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
