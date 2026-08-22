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

# Control characters are not graphic characters (IEEE 1076-2008 clause 15.2)
# and so may not appear inside a string literal, extended identifier or bit
# string literal. The source is generated rather than committed, to keep a raw
# control character out of the repository.
test.top_filename = test.obj_dir + "/t_vhdl_lex_nongraphic_bad.vhd"

test.mkdir_ok(test.obj_dir)
with open(test.top_filename, 'w', encoding='latin-1', newline='\n') as fh:
    fh.write("architecture bad of t is\n")
    fh.write("  constant s1 : string := \"a\x01b\";\n")
    fh.write("  signal \\ext\x01id\\ : bit;\n")
    fh.write("  constant b1 : string := X\"F\x01F\";\n")
    fh.write("begin\n")
    fh.write("end architecture;\n")

test.lint(fails=True, expect_filename=test.golden_filename)

test.passes()
