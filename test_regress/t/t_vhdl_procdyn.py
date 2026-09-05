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

# A declaration of a subprogram is elaborated on every call. A declaration of
# a process is elaborated once, before the process first runs, so the setup
# that gives an object its bounds and its room belongs in a region of its own
# rather than loose among the module's statements, where nothing may stand.

test.top_filename = "t/t_vhdl_procdyn.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_procdyn"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
