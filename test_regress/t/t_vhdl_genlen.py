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

# A length that elaboration settles but that is written against a generic,
# or against how long a signal-class parameter's signal is, converting both
# ways between a fixed value and one carrying its own bounds.
test.top_filename = "t/t_vhdl_genlen.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--debug-check", "--top-module t_vhdl_genlen"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
