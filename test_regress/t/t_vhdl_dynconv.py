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

# TO_UNSIGNED and its kin take their width from what the value is given to,
# which works wherever that has a width to give. A result that carries its own
# bounds has nothing of the sort, so the bits are built with the length the
# call names instead -- the branch that does so was unreachable.

test.top_filename = "t/t_vhdl_dynconv.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_dynconv"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
