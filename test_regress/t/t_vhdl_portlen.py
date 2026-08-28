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

# A constant whose length names a port's bounds: TO_SIGNED and TO_UNSIGNED
# say the length in the call, and here that length is an expression
# elaboration folds rather than a number (16.5).
test.top_filename = "t/t_vhdl_portlen.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_portlen"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
