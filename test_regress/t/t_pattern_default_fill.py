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

test.compile()

# Each array the default fills is one value passed to a fill, not an
# element list: the struct member, the member assigned alone, the element
# of an array of arrays
test.file_grep_count(test.obj_dir + "/" + test.vm_prefix + "___024root__0__Slow.cpp",
                     r'::filled\(', 6)

test.execute()

test.passes()
