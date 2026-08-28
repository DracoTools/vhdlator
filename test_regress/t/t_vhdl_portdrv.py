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

# A port of mode "out" is a driver of the signal associated with it, exactly
# as an assignment to that signal would be, so a signal an instance and a
# process both drive is resolved between them (6.5.6.3, 14.7.2). A block is
# transparent: a statement inside it is a driver of its own (11.2).
test.top_filename = "t/t_vhdl_portdrv.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_portdrv"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
