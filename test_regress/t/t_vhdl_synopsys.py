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

# The Synopsys packages -- std_logic_arith and the unsigned and signed ones
# beside it -- are what legacy VHDL is written against, and every other
# simulator ships them. They need an operator the design declared to be
# copied per use like any other subprogram (4.5.2), and an array indexed by
# a value of a logic type to be indexed by that value's position.
test.top_filename = "t/t_vhdl_synopsys.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_synopsys"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
