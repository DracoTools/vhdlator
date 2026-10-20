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

# Under --vhdl-metavalues (under two states a record element "(f) std_ulogic_vector" is
# resolved as a whole, a limitation of its own): an inout port of an open array of records
test.top_filename = "t/t_vhdl_nine_recarrayport.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--vhdl-metavalues", "--top-module t_vhdl_nine_recarrayport"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
