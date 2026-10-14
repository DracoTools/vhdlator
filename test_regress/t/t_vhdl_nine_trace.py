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

# STD_ULOGIC objects in a waveform under --vhdl-metavalues: one bit per
# element, '1' and 'H' as 1 and every other value as 0, with the range the
# vector was declared with
test.top_filename = "t/t_vhdl_nine_trace.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--trace-vcd", "--vhdl-metavalues", "--top-module t_vhdl_nine_trace"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

vcd = test.obj_dir + "/Vt_vhdl_nine_trace.vcd"
test.file_grep(vcd, r'\$var wire 9 \S+ up \[0:8\] \$end')
test.file_grep(vcd, r'\$var wire 1 \S+ s \$end')
test.vcd_identical(vcd, test.golden_filename)

test.passes()
