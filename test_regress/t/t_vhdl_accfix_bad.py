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

# An access value's array of the wrong length for the variable it is
# assigned to is an error when it runs (10.6.2.1), reported the way every
# length this frontend checks while running is
test.top_filename = "t/t_vhdl_accfix_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_accfix_bad"],
             make_top_shell=False,
             make_main=False)

# The mismatch ends the run
test.execute(fails=True)

test.file_grep(test.run_log_filename, r'Array lengths do not match')

test.passes()
