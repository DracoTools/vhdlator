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

# A configuration declaration says which entity each instance is built from,
# and says it of one block at a time: what is written for a block does not
# reach the instances of a generate inside it (3.4.2, 3.4.3). "use
# configuration" names a configuration, which chose the entity already.
test.top_filename = "t/t_vhdl_confblk.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_confblk_cfg"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
