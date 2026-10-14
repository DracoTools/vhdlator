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

# STD_ULOGIC objects over VPI under --vhdl-metavalues: one element per bit,
# the nine characters as a binary string, as nvc and GHDL give cocotb
test.top_filename = "t/t_vhdl_nine_vpi.vhd"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=[
                 "--exe", "--vpi", "--public-flat-rw", "--vhdl-metavalues", test.pli_filename,
                 "--top-module t_vhdl_nine_vpi"
             ])

test.execute()

test.passes()
