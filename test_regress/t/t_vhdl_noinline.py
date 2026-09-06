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

# A VHDL subprogram emitted as a function of its own rather than written out
# at every call site, which is what a design with a deep call graph needs.
test.top_filename = "t/t_vhdl_noinline.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--debug-check", "--inline-task-mult 1",
                 "--top-module t_vhdl_noinline"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
