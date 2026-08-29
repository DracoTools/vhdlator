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

# S'TRANSACTION toggles in each cycle in which S becomes active (16.2.4),
# so a waveform of three elements toggles it three times, at the times each
# element is delivered -- not three times at once when they are scheduled.
test.top_filename = "t/t_vhdl_transact.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_transact"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
