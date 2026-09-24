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

test.compile(verilator_flags2=['--stats'])

# Left whole: the 13 assignments of arrays past the limit whose sides run
# the same way or differ only in the outer direction -- up, and each row of
# u2, o2 and b2
test.file_grep(test.stats, r'Optimizations, Slice array skips due to size limit\s+(\d+)', 13)

test.execute()

test.passes()
