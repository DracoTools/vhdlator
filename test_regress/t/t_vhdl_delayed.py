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

# A simulation cycle delivers every transaction due at the current time
# before it resumes the processes whose timeouts expired there, and 'DELAYED
# is the same signal moved along in time. Checked against NVC 1.22.1 and
# Questa 25.1std.0.1129, which agree; GHDL 7.0.0-dev reads 'DELAYED(T) one
# delta late and disagrees with both.
test.top_filename = "t/t_vhdl_delayed.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_delayed"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
