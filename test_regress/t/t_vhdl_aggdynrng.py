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

# An aggregate of one range choice whose bounds only elaboration or running
# settle fills every element where its length is the object's, and is an
# error while running where it is not.
test.top_filename = "t/t_vhdl_aggdynrng.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_aggdynrng"],
             make_top_shell=False,
             make_main=False)

# The last assignment gives three bits a thousand-element aggregate, which
# nvc and GHDL both stop at, and so does this: after the checks have passed
test.execute(fails=True)

test.file_grep(test.run_log_filename, r'\*-\* All Finished \*-\*')
test.file_grep(test.run_log_filename, r'Array lengths do not match')

test.passes()
