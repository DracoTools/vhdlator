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

# A transaction is not an event: assigning the value a signal already holds
# makes it active without changing it, and before anything has happened at
# all the "how long ago" attributes answer TIME'HIGH. Checked against
# GHDL 7.0.0-dev and Questa 25.1std.0.1129, which agree.
test.top_filename = "t/t_vhdl_sigattr.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_sigattr"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
