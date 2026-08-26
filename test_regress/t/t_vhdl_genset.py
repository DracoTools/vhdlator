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

# -G gives a top-level generic its value. A VHDL name is written without
# regard to case, so each of these spellings has to find its generic.
test.top_filename = "t/t_vhdl_genset.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--top-module t_vhdl_genset", "-GWIDTH=12",
                 "-Gname='\"hello\"'", "-GScAlE=2.5", "-GENABLED=1"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
