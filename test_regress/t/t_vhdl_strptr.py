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

# An access to STRING is emitted as a string of its own rather than as an
# object to dereference, so an index into one is an index into the text it
# points at. Asking the pointer's own type where text starts answers nothing,
# and an index left uncorrected reads and writes the wrong character.

test.top_filename = "t/t_vhdl_strptr.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing", "--top-module t_vhdl_strptr"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.passes()
