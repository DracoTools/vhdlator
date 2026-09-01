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

# The same package name in two libraries, each with a shared variable of its
# own. -work applies to the files that follow it, so the libraries are given
# in order. Checked against GHDL 7.0.0-dev.
test.top_filename = "t/t_vhdl_libshared.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--top-module t_vhdl_libshared", "-work liba",
                 "t/t_vhdl_libshared_a.vhd", "-work libb", "t/t_vhdl_libshared_b.vhd",
                 "-work work"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
