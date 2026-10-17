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

# TO_INTEGER of a value INTEGER cannot hold stops the run; nvc and GHDL stop the run too
test.top_filename = "t/t_vhdl_toint_range_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_toint_range_bad"],
             make_top_shell=False,
             make_main=False)

test.execute(fails=True, expect_filename=test.golden_filename)

test.passes()
