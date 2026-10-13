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

# A record element given a value of another length stops the run. The report
# names the line of the first such conversion, not this one (bugs.md 388lx)
test.top_filename = "t/t_vhdl_recaggdyn_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing",
                               "--top-module t_vhdl_recaggdyn_bad"],
             make_top_shell=False,
             make_main=False)

test.execute(fails=True, expect_filename=test.golden_filename)

test.passes()
