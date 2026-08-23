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

# std.textio, whose LINE is a string here rather than a runtime object, so
# building one is concatenation and writing it a formatted write.
test.top_filename = "t/t_vhdl_textio.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_textio"],
             make_top_shell=False,
             make_main=False)

test.execute()

# The three lines are byte for byte what GHDL prints for the same source
test.file_grep(test.run_log_filename, r'count=42 bits=10110001')
test.file_grep(test.run_log_filename, r'hex=B1 oct=12')
test.file_grep(test.run_log_filename, r'real=2\.5')

test.passes()
