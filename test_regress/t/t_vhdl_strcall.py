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

# A STRING is carried as text rather than in a bounds carrier, so a variable
# of one has to start out as long as its constraint says or writing to an
# element writes nothing. Where that length is a call, the fill is a statement
# of the region and its length an object of its own: asked for inside the
# fill, a call is a value the back end tries to settle before the design runs.

test.top_filename = "t/t_vhdl_strcall.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_strcall"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
