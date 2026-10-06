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

# A helper naming an object of a unit that is analyzed but not elaborated
test.top_filename = "t/t_vhdl_constfuncdead.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--top-module t_vhdl_constfuncdead", "t/t_vhdl_constfuncread.vhd"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.file_grep_count(test.run_log_filename, r'constfuncdead done', 1)

test.passes()
