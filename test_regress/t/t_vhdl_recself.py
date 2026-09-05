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

# A subprogram that calls itself with the arguments it was given needs one
# copy, not one per level: the copy is claimed before its own body is looked
# at, so the call it makes to itself finds the copy it is already in. A design
# that made one per level would spend every copy it is allowed on the
# recursion, and the constant below -- whose length only the copy made for its
# call says -- would never be given a length at all.
test.top_filename = "t/t_vhdl_recself.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_recself"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
