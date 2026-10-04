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

# Operators a design declares over unconstrained arrays
test.top_filename = "t/t_vhdl_useropres.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             # The concurrent "(c, z) <= ..." is reported as driven twice under
             # the driver's --x-assign unique, and only for elements wider than
             # a bit; a default build is clean (open in bugs.md, 388hz)
             verilator_flags2=["--timing", "--top-module t_vhdl_useropres", "-Wno-MULTIDRIVEN"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
