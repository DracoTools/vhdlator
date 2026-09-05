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

# A subprogram that calls itself with a longer argument is copied only where
# the recursion descends: a series of copies for arguments that keep growing
# has no last member. What ends this recursion is a value, and no copy settles
# a value, so the original does the work. A design that copied per length
# would spend every copy it is allowed here, and the constant below -- whose
# length only the copy made for its call says -- would never get one.
test.top_filename = "t/t_vhdl_recgrow.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_recgrow"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
