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

# RESIZE to a length only running decides is built from the operand's own
# elements (9.3.6), so a negative operand widens with its sign and one
# wider than a machine word keeps all of it.  OSVVM's RandSigned.
test.top_filename = "t/t_vhdl_dynres.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_dynres"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
