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

# A function that may be called with no arguments is called by writing its
# name and nothing else (4.2.2.1), so "return put_msg" is a call however
# little it looks like one, and its result is not a parameter's length.
test.top_filename = "t/t_vhdl_barecall.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_barecall"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
