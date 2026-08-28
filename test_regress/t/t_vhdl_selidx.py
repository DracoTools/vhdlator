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

# "r.data(0)" indexes the record's element, whatever else is called "data"
# where the expression is written. The last identifier of the prefix was
# being looked up in the enclosing region instead (8.3, 8.4).
test.top_filename = "t/t_vhdl_selidx.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_selidx"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
