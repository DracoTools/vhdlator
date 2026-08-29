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

# A value of an unconstrained array type that no call site can measure is
# carried with its own bounds: the elements in a dynamic array, and the
# left and right the value was made with beside them.  A copy per call site
# -- which is what every length that can be known takes, and so what all
# synthesizable code takes -- cannot serve, because the length is not
# decided until the value is built.
test.top_filename = "t/t_vhdl_dynlen.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_dynlen"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
