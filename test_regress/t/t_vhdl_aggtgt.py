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

# The target of a selected assignment may be an aggregate, and the type of
# an aggregate target is the type of the value assigned to it (10.5.2.1,
# 11.6) -- which is what tells a bare string literal in one alternative
# that it is a vector rather than a STRING.
test.top_filename = "t/t_vhdl_aggtgt.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_aggtgt"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
