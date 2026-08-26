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

# A context declaration in one library, referenced from another. -work applies
# to the files that follow it, so the two libraries are given in order.
test.top_filename = "t/t_vhdl_context.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--top-module t_vhdl_context",
                 "-work mylib", "t/t_vhdl_context_b.vhd", "-work work"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
