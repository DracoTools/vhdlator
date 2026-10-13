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

# Assertion and report lines, labelled as GHDL labels them
test.top_filename = "t/t_vhdl_assertmsg.vhd"

test.compile(verilator_flags=["-Mdir " + test.obj_dir, "--exe", "--build", "--main"],
             verilator_flags2=["--timing",
                               "--top-module t_vhdl_assertmsg"],
             make_top_shell=False,
             make_main=False)

test.execute()

test.file_grep(test.run_log_filename, r':@0:\(report note\): a report$')
test.file_grep(test.run_log_filename, r':@0:\(report warning\): a warning$')
test.file_grep(test.run_log_filename, r':18:5:@0:\(assertion note\): Assertion violation\.$')
test.file_grep(test.run_log_filename, r':@0:\(assertion warning\): with a message$')
test.file_grep(test.run_log_filename, r':20:5:@0:\(assertion error\): Assertion violation\.$')

test.passes()
