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

# Which declaration of a name a use denotes: an inner one hides an outer one
# of the same profile, only what a protected type declares is reached through
# an object of it, and an access value is dereferenced without saying so.
test.top_filename = "t/t_vhdl_visible.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_visible"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
