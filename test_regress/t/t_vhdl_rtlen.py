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

# An impure function reads what only the run has settled, so an array whose
# bound is a call to one -- or names a constant declared from one -- is as
# long as the run makes it, and has to carry its own bounds. A pure function
# is not one of those: elaboration works its result out, and the array stays
# a plain one of a length the emitted code knows.

test.top_filename = "t/t_vhdl_rtlen.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_rtlen"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
