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

# This runtime holds no metavalues: 'U', 'X', 'W' and '-' are all written as
# a definite zero, so a comparison against one is never true and a case
# choice naming one is never taken. A matching case is the exception, where
# a '-' is the "any value" of 10.9.
test.top_filename = "t/t_vhdl_metaval.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_metaval"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
