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
VERILOG_SOURCES = $(CURDIR)/dut.vhd
TOPLEVEL = dut
MODULE = test_dut

# A VHDL process that waits, or an "after" delay, needs --timing
EXTRA_ARGS += --timing

# cocotb runs the "verilator" command, so a Verilator built but not installed
# has to be findable: VERILATOR_ROOT says where it is
ifneq ($(VERILATOR_ROOT),)
export PATH := $(VERILATOR_ROOT)/bin:$(PATH)
endif

# cocotb is a separate project, installed with pip. Without it there is
# nothing to include, so say so rather than failing with a missing file.
COCOTB_MAKEFILES := $(shell cocotb-config --makefiles 2>/dev/null)
# cocotb runs the command, so it has to be there to run at all
VERILATOR_CMD := $(shell command -v verilator 2>/dev/null)

ifeq ($(COCOTB_MAKEFILES)$(VERILATOR_CMD),)
default:
	@echo "-- SKIPPED ------------------"
	@echo "This example needs cocotb and a verilator on PATH."
	@echo "Install cocotb with:  pip install cocotb"
else
ifeq ($(COCOTB_MAKEFILES),)
default:
	@echo "-- SKIPPED ------------------"
	@echo "This example needs cocotb, which is not installed."
	@echo "Install it with:  pip install cocotb"
else
ifeq ($(VERILATOR_CMD),)
default:
	@echo "-- SKIPPED ------------------"
	@echo "This example needs a 'verilator' on PATH, which cocotb runs."
else
include $(COCOTB_MAKEFILES)/Makefile.sim
endif
endif
endif
