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

# A '-' stands for a place that matches whatever it is compared with
# (9.2.3, 16.8.3).  Nothing carries a don't-care once the metavalues have
# collapsed, so a pattern settled before the design runs is read early.
test.top_filename = "t/t_vhdl_dontcare.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_dontcare"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
