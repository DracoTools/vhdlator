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

# A conversion to an unconstrained array type of a value whose bounds the
# target's index subtype cannot hold is an error when it runs (9.3.6)
test.top_filename = "t/t_vhdl_convbounds_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_convbounds_bad"],
             make_top_shell=False,
             make_main=False)

# The failure ends the run, after the conversions that are fine
test.execute(fails=True)

test.file_grep(test.run_log_filename, r'within range')
test.file_grep(test.run_log_filename, r'Bound check failure: the converted value is indexed 31 downto -32')
test.file_grep_count(test.run_log_filename, r'not reached', 0)

test.passes()
