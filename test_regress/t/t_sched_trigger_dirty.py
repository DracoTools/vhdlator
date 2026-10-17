#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('simulator')

test.compile(verilator_flags2=["--binary", "--timing", "t/t_sched_trigger_dirty.cpp"])

# The 'act' triggers are computed only where flagged
test.file_grep(test.obj_dir + "/" + test.vm_prefix + "___024root__0.cpp", r'__VactDirty\[')

test.execute()

test.passes()
