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

# A generic type nothing has bound yet is a type of its own and matches
# nothing else, so two subprograms differing only in it are two subprograms
# (6.5.5). An instantiation may then give them the same profile; the copy is
# the template with the actuals written in, so every call in it still means
# what it meant there (4.9).
test.top_filename = "t/t_vhdl_genpkgov.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_genpkgov"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
