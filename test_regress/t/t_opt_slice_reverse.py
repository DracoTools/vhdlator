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

# Left whole, as one operation each: up, o2 and h2 assigned and compared,
# the up comparison changed, and the four rows of u2 and of b2, whose inner
# dimensions differ, assigned and compared; 11 assignments, 12 comparisons
test.file_grep(test.stats, r'Optimizations, Slice array skips due to size limit\s+(\d+)', 23)

test.execute()

test.passes()
