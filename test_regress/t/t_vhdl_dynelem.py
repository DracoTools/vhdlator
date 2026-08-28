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

# An access value that carries its own bounds may be reached through an
# index and a selection -- "reg(i).tally(j)" -- and not only by a plain
# name. Reading the bounds and reading the elements are two reads of the
# same object, which an index expression that cannot change allows.
test.top_filename = "t/t_vhdl_dynelem.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_dynelem"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
