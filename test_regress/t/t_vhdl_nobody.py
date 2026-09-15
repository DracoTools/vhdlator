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

# A package declaration analyzed without its body: naming a subprogram it
# declares is legal, and elaborating the call is what needs the body.
# GHDL 7.0.0-dev, nvc 1.22.1 and Questa 2025.2 all analyze this.
test.top_filename = "t/t_vhdl_nobody.vhd"

test.lint()

test.passes()
