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

# 'PATH_NAME and 'INSTANCE_NAME name every region from the design unit down
# (16.2.7). The body of a generate is not one of them: the generate
# statement's own label is, and an unlabelled body contributed an empty
# component and a stray colon.
test.top_filename = "t/t_vhdl_pathname.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_pathname"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
