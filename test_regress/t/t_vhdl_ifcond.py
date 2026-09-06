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

# A condition that is not a BOOLEAN is read through the condition operator
# (9.2.9), so what decides the branch is whether the value is '1' -- not
# whether its position in the type is other than zero.
test.top_filename = "t/t_vhdl_ifcond.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_ifcond"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
