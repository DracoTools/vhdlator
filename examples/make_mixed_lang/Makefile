######################################################################
#
# DESCRIPTION: Verilator Example: VHDL and Verilog in one model
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

# One model, one symbol table, one eval(). A VHDL entity instantiated from
# Verilog, and a Verilog module instantiated from VHDL, in the same design.


ifeq ($(VERILATOR_ROOT),)
  VERILATOR = verilator
else
  export VERILATOR_ROOT
  VERILATOR = $(VERILATOR_ROOT)/bin/verilator
endif

default:
	@echo "-- Verilator mixed-language example"
	@echo "-- VERILATE & BUILD --------"
	$(VERILATOR) --binary --timing -j 0 --top-module top top.sv counter.vhd
	@echo "-- RUN ---------------------"
	obj_dir/Vtop
	@echo "-- DONE --------------------"
	@echo "Note: See docs/guide/mixed-language.rst for the port type mapping."
	@echo "Note: See also https://verilator.org/guide/latest/examples.html"

######################################################################

maintainer-copy::
clean mostlyclean distclean maintainer-clean::
	-rm -rf obj_dir *.log *.dmp *.vpd core
