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

# What a nested subprogram reaches out of its own region for is passed to it
# as extra parameters. They go by name: a parameter the call leaves out
# because it has a default is still a parameter, and an argument put after it
# in order is read as that one's.

test.top_filename = "t/t_vhdl_capname.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_capname"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
