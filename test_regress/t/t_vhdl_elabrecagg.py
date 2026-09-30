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

# A record aggregate of another width in a branch not run for the width -G gives.
# The widths are compared only when the design is elaborated, so the branch
# that does not run is still warned about
test.top_filename = "t/t_vhdl_elabrecagg.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_elabrecagg", "-Gg_len=16",
                              "-Wno-WIDTHEXPAND", "-Wno-WIDTHTRUNC"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
