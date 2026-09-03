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

# A concurrent signal assignment schedules an update rather than making one,
# so its target holds the old value through the delta in which its source
# changed. Checked against nvc 1.22.1, GHDL 7.0.0-dev and Questa 25.1std.
test.top_filename = "t/t_vhdl_concdelta.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--debug-check", "--top-module t_vhdl_concdelta"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
