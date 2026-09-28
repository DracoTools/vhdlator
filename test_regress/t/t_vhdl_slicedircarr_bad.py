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

# A slice against the direction of a parameter handed a value whose length
# only running decides is an error when it runs (8.5), reported once; the
# slice that matches and the null one are not reported.
test.top_filename = "t/t_vhdl_slicedircarr_bad.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_slicedircarr_bad"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.file_grep(test.run_log_filename, r'slices that match passed')
test.file_grep_count(test.run_log_filename, r"Slice runs 'downto' but std_ulogic_vector is indexed 'to'", 1)

test.passes()
