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

# A constant declared inside a subprogram from what the call supplies has
# a value only running knows (4.3.1.1), so an object as long as it says is
# one whose length only running decides.  A constant of a package is not.
test.top_filename = "t/t_vhdl_lenlocal.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_lenlocal"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
