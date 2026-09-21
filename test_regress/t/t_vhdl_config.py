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

# A configuration declaration chooses which architecture an entity is built
# from, for the design elaborated through it. Named as the top, it picks the
# first architecture; the default binding would take the one analyzed last.
test.top_filename = "t/t_vhdl_config.vhd"

test.compile(make_top_shell=False, make_main=False, verilator_flags2=["--exe", test.pli_filename, "--top-module pick"])

test.execute()

test.passes()
