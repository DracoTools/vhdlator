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

# VHDL subprograms implemented in C, bound with the FOREIGN attribute. They
# lower to the same node an "import" DPI-C" declaration produces, so the C
# side is ordinary DPI and needs nothing VHDL-specific.
test.top_filename = "t/t_vhdl_foreign.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_foreign", test.pli_filename],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
