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

# Mixed language, in both directions at once: a Verilog testbench holds a VHDL
# entity, which holds a Verilog module. Every name is spelled in a different
# case on each side of the boundary, since VHDL identifiers are case
# insensitive and Verilog's are not.
test.top_filename = "t/t_vhdl_mixed.v"
test.v_other_filenames = ["t/t_vhdl_mixed.vhd"]

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=["--exe", test.pli_filename, "--top-module t"])

test.execute()

test.passes()
