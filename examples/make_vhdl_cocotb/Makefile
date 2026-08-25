######################################################################
#
# DESCRIPTION: Verilator Example: a VHDL design driven by cocotb
#
# This file ONLY is placed under the Creative Commons Public Domain
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: CC0-1.0
#
######################################################################

# cocotb reaches Verilator over VPI, which is also how it reaches GHDL, so a
# VHDL design needs no cocotb-side change. TOPLEVEL_LANG stays "verilog":
# it selects cocotb's VPI interface, not the language of the source.

SIM ?= verilator
TOPLEVEL_LANG ?= verilog
VERILOG_SOURCES = $(PWD)/dut.vhd
TOPLEVEL = dut
MODULE = test_dut

# A VHDL process that waits, or an "after" delay, needs --timing
EXTRA_ARGS += --timing

include $(shell cocotb-config --makefiles)/Makefile.sim
