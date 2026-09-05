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

# Which way a range runs is how it was written, not what its two ends happen
# to be. Where a bound is worked out by calling something, neither end is a
# number where the array is declared, and a value handed back through a type
# nothing constrains has to carry the direction it was declared with.

test.top_filename = "t/t_vhdl_rangedir.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_rangedir"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
