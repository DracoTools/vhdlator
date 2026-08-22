#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('linter')

# The files are given in reverse dependency order. IEEE 1076-2008 clause 13.5
# requires a unit to be analyzed after everything it depends on, so the golden
# order must be package, package body's package, entity, architecture -- not
# the order the files were named in.
test.top_filename = "t/t_vhdl_lib_order.vhd"
test.v_other_filenames = ["t/t_vhdl_lib_order_b.vhd", "t/t_vhdl_lib_order_c.vhd"]

test.lint(fails=True,
          verilator_flags2=["--dump-vhdl-units"],
          expect_filename=test.golden_filename)

test.passes()
