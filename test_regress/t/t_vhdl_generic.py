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

# One subprogram written against unconstrained operands, called with several
# lengths. Each call gets its own fixed-width copy, so what is emitted carries
# no runtime array descriptor.
test.top_filename = "t/t_vhdl_generic.vhd"

test.compile(make_top_shell=False, make_main=False, verilator_flags2=["--exe", test.pli_filename])

test.execute()

test.passes()
