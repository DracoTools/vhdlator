#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('linter')

# GHDL's default VHDL-93 mode, "93c", takes "others" against an unconstrained
# target (GHDL gna issue2422 needs it); the same source strict VHDL-93 refuses
test.top_filename = "t/t_vhdl_others_open93_bad.vhd"

test.lint(verilator_flags2=["--vhdl-std 93c"])

test.passes()
