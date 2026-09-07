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

# TO_01 and the conversions of std_logic_1164 give back a value as long as
# their argument, which is what lets that value be passed on to a subprogram
# written against an unconstrained array.
test.top_filename = "t/t_vhdl_convbnd.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_convbnd"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
