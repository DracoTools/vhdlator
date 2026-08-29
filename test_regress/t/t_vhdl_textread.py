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

# What textio's READ takes off a line depends on the type: one character
# per bit for a vector, the name of a literal for an enumeration, a number
# and then a unit for a physical value, and for a number as much as can be
# part of one.  A whitespace-delimited item is the wrong unit for all of
# them, and a read that fails leaves the line where it was (16.4).
test.top_filename = "t/t_vhdl_textread.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_textread"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
