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

# A conversion to REAL is written as a cast rather than as the conversion
# itself: the back end settles what an operand is worth before it decides
# which conversion to make, and one made in the frontend would be handed an
# operand nothing had settled yet -- 'POS being exactly such an operand.

test.top_filename = "t/t_vhdl_realpos.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_realpos"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
