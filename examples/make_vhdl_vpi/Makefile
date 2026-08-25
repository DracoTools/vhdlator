######################################################################
#
# DESCRIPTION: Verilator Example: VHDL objects over VPI
#
# This file ONLY is placed under the Creative Commons Public Domain
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: CC0-1.0
#
######################################################################
# Check for sanity to avoid later confusion

ifneq ($(words $(CURDIR)),1)
  $(error Unsupported: GNU Make cannot build in directories containing spaces, build elsewhere: '$(CURDIR)')
endif

######################################################################

# A public VHDL object is reachable by the standard VPI calls: it lands in
# the same tables a Verilog one does. This is the path cocotb takes to a
# VHDL design.

ifeq ($(VERILATOR_ROOT),)
  VERILATOR = verilator
else
  export VERILATOR_ROOT
  VERILATOR = $(VERILATOR_ROOT)/bin/verilator
endif

default:
	@echo "-- Verilator VHDL over VPI example"
	@echo "-- VERILATE & BUILD --------"
	$(VERILATOR) --cc --exe --build -j 0 --vpi --public-flat-rw --top-module top top.vhd sim_main.cpp
	@echo "-- RUN ---------------------"
	obj_dir/Vtop
	@echo "-- DONE --------------------"
	@echo "Note: See docs/guide/connecting-vhdl.rst for VPI over VHDL and for cocotb."
	@echo "Note: See also https://verilator.org/guide/latest/examples.html"

######################################################################

maintainer-copy::
clean mostlyclean distclean maintainer-clean::
	-rm -rf obj_dir *.log *.dmp *.vpd core
