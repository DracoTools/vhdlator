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

# VPI reaching VHDL objects. A VHDL signal, port and generic land in the same
# runtime tables a Verilog one does, so this needs nothing VHDL-specific on
# the C side.
test.top_filename = "t/t_vhdl_vpi.vhd"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=[
                 "--exe", "--vpi", "--public-flat-rw", test.pli_filename,
                 "--top-module t_vhdl_vpi"
             ])

test.execute()

test.passes()
