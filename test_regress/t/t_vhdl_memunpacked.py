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

# Arrays of arrays and of records are memories, unpacked
test.top_filename = "t/t_vhdl_memunpacked.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_memunpacked"],
             make_top_shell=False,
             make_main=False)

# The large memory is one word per element, not one packed vector
test.file_grep(test.obj_dir + "/" + test.vm_prefix + "___024root.h",
               r'VlUnpacked<IData/\*31:0\*/, 16384>')

test.execute()

test.passes()
