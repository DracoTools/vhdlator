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

# A value the analyzer can see is out of range, in a statement never reached.
# The language makes that an error of execution, so it is a warning here and
# the design still builds.
test.top_filename = "t/t_vhdl_boundswarn.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "-Wno-VHDLBOUNDS",
                               "--top-module t_vhdl_boundswarn"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
