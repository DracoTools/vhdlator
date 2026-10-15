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

# TO_UNSIGNED of an integer worked out in reals, at a generic's size. The
# real product is typed as an integer by analysis, which leaves the back end
# to convert it and say so (bugs.md 388mk)
test.top_filename = "t/t_vhdl_genrealsize.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "-Wno-REALCVT", "--top-module t_vhdl_genrealsize"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
