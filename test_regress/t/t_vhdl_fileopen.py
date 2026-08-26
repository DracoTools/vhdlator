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

# A file declaration may name the file to open, and then it is opened when the
# declaration is elaborated: once for an architecture or a process, on every
# call for a subprogram.
test.top_filename = "t/t_vhdl_fileopen.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=[
                 "--timing", "--top-module t_vhdl_fileopen",
                 # The scratch file belongs beside the model, not in the source
                 # tree. VHDL has no preprocessor, so the directory arrives as
                 # a generic.
                 "-GOUTDIR=" + "'\"" + test.obj_dir + "\"'"
             ],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
