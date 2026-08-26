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

# VHDL has no $dumpfile, so the tool asks for the waveform on the design's
# behalf and writes it beside the model.
test.top_filename = "t/t_vhdl_trace.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--trace-vcd", "--top-module t_vhdl_trace"],
             make_top_shell=False,
             make_main=False)

test.execute()

# The names and the ranges are what a waveform shows, and are checked here
# rather than by comparing the whole file, which needs a wavediff tool
vcd = test.obj_dir + "/Vt_vhdl_trace.vcd"
test.file_grep(vcd, r'\$scope module t_vhdl_trace \$end')
test.file_grep(vcd, r'\$var wire 1 \S+ clk \$end')
test.file_grep(vcd, r'\$var wire 32 \S+ cnt \[31:0\] \$end')
test.file_grep(vcd, r'\$var wire 8 \S+ down \[7:0\] \$end')
# A range that runs upward is shown the way it was declared
test.file_grep(vcd, r'\$var wire 4 \S+ up \[0:3\] \$end')
test.file_grep(vcd, r'\$var wire 2 \S+ st \[1:0\] \$end')
test.file_grep(vcd, r'\$var real 64 \S+ re \$end')
# And the values change as the design runs
test.file_grep(vcd, r'#45000000')
test.file_grep(vcd, r'b00001111 ')

test.passes()
