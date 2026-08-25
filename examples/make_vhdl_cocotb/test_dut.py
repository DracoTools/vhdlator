# DESCRIPTION: Verilator Example: a cocotb testbench for a VHDL design
#
# This file ONLY is placed under the Creative Commons Public Domain
# SPDX-FileCopyrightText: 2026 Wilson Snyder
# SPDX-License-Identifier: CC0-1.0
#
# Nothing here is VHDL-specific. cocotb reaches Verilator over VPI, which is
# the same path it uses for GHDL, so a testbench written for one runs against
# the other unchanged. VHDL folds a basic identifier to lower case, and that
# is the spelling cocotb sees.

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, Timer


@cocotb.test()
async def counts(dut):
    """Reset the counter, let it run, and check what it reached."""
    cocotb.start_soon(Clock(dut.clk, 10, unit="ns").start())

    dut.rst.value = 1
    await RisingEdge(dut.clk)
    await RisingEdge(dut.clk)
    dut.rst.value = 0

    for _ in range(10):
        await RisingEdge(dut.clk)
    await Timer(1, unit="ns")

    dut._log.info("q = %s", dut.q.value)
    assert int(dut.q.value) == 10, f"expected 10, got {int(dut.q.value)}"
