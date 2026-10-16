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

# A VHDL design's model and runtime are built at -O2 rather than verilated.mk's
# -Os: scheduling one is comparing triggers, which -Os leaves calling memcmp
test.top_filename = "t/t_vhdl_nullpin.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_nullpin"],
             make_top_shell=False,
             make_main=False)

test.file_grep(test.obj_dir + "/" + test.vm_prefix + ".mk", r'^OPT_FAST = -O2')
test.file_grep(test.obj_dir + "/" + test.vm_prefix + ".mk", r'^OPT_GLOBAL = -O2')

test.execute(check_finished=True)

test.passes()
