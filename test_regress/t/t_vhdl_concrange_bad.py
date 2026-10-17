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

# A concurrent assignment of a value outside its target subtype stops the run,
# as nvc, GHDL and Questa stop it
test.top_filename = "t/t_vhdl_concrange_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_concrange_bad"],
             make_top_shell=False,
             make_main=False)

test.execute(fails=True)

test.file_grep(test.run_log_filename, r'Value 101 is outside of the range 0 to 100 of subtype my_int')

test.passes()
