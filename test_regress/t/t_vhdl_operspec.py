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

# An operator a design declares is a subprogram whose name happens to be an
# operator symbol (4.5.2), so it is copied per use like any other, and its
# result may be as long as the use makes it.  std_logic_unsigned's "*" is
# written that way, and legacy VHDL is written against it.
test.top_filename = "t/t_vhdl_operspec.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_operspec"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
