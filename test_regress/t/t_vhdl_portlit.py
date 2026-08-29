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

# An architecture continues its entity's declarative region (3.3, 12.1), so
# a port is directly visible in it and hides an enumeration literal of the
# same name that a use clause only made potentially visible (12.4).
test.top_filename = "t/t_vhdl_portlit.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_portlit"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
