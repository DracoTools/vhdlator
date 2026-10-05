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

# A delayed concurrent assignment of a generic to a port several instances drive
test.top_filename = "t/t_vhdl_wavegeneric.vhd"

# The generic's 'Z' reaches the scheduled transaction as a constant, which
# --x-assign unique (the driver's default) turns into a random value: a
# known limit, recorded with this test; the default --x-assign keeps it
test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--x-assign fast", "--top-module t_vhdl_wavegeneric"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
