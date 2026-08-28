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

# A function whose result subtype is unconstrained gets it from what its
# body returns, and what it returns may itself be a call whose own length
# only the call site settles (4.2.1). The inner call has to get its copy
# first, or the outer one falls back to the length of its argument.
test.top_filename = "t/t_vhdl_reslen.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_reslen"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
