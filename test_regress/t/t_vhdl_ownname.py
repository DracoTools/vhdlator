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

# Which subprograms the frontend carries out itself is a question about the
# declaration, not about the name: a package may declare a RESIZE or a
# MINIMUM of its own and mean something else by it.
test.top_filename = "t/t_vhdl_ownname.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_ownname"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
