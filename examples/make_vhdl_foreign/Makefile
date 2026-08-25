######################################################################
#
# DESCRIPTION: Verilator Example: VHDL calling C, and C calling VHDL
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

# A subprogram given the FOREIGN attribute (LRM 16.2) is implemented in C.
# Verilator links statically, so there is no shared library to find at run
# time and the C++ compiler can inline across the boundary.

ifeq ($(VERILATOR_ROOT),)
  VERILATOR = verilator
else
  export VERILATOR_ROOT
  VERILATOR = $(VERILATOR_ROOT)/bin/verilator
endif

default:
	@echo "-- Verilator VHDL foreign-function example"
	@echo "-- VERILATE & BUILD --------"
	$(VERILATOR) --binary --timing -j 0 top.vhd sim_main.cpp
	@echo "-- RUN ---------------------"
	obj_dir/Vtop
	@echo "-- DONE --------------------"
	@echo "Note: See docs/guide/connecting-vhdl.rst for the full story."
	@echo "Note: See also https://verilator.org/guide/latest/examples.html"

######################################################################

maintainer-copy::
clean mostlyclean distclean maintainer-clean::
	-rm -rf obj_dir *.log *.dmp *.vpd core
