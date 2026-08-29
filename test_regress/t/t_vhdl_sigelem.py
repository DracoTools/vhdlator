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

# A signal-class formal must be the caller's own signal (4.2.2.3), and the
# actual may be one element of an array signal at an index only the call
# knows. The copy made for the call site takes that index as a parameter of
# its own and indexes the array with it.
test.top_filename = "t/t_vhdl_sigelem.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_sigelem"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
