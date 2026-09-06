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

# VHDL-2008 gives the logical operators a form with one array operand and
# one of the element type, and the scalar stands for every element of the
# array (9.2.2), repeated across it rather than widened into it.
test.top_filename = "t/t_vhdl_arrscal.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_arrscal"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
