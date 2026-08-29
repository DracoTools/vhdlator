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

# A subprogram of a package body is not a completion of whatever else is
# visible by that name: TIME's units MIN and HR are declared in
# STD.STANDARD, and a function of either name is a new declaration (4.3).
# And a procedure of a name does not stop the functions of that name being
# read as an expression, which is how an argument's own overloads are
# settled by the parameter's type (12.5).
test.top_filename = "t/t_vhdl_bodyname.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_bodyname"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
