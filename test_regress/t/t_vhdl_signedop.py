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

# Every operator of numeric_std that reads a SIGNED as a number has to
# be told it is one (9.3.6, 16.5): the comparisons, division, the
# remainder, the product and the arithmetic shift.
test.top_filename = "t/t_vhdl_signedop.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_signedop"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
