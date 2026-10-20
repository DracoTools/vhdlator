-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- MATH_REAL's LOG2 of a power of two is no more than that power, as C's
-- log2 gives it to GHDL and nvc (nvc's is exact), so its ceiling is the
-- power: OSVVM's Wishbone subordinate works out "BYTE_ADDR_WIDTH :=
-- integer(ceil(log2(real(DATA_NUM_BYTES))))" from a port's length, which
-- elaboration settles but analysis does not fold. The body IEEE publishes,
-- MATH_LOG2_OF_E*LOG(X), gave a little over 2.0 for 4.0, whose ceiling is
-- 3, and every word address was a bit too high.

library ieee;
use ieee.math_real.all;

entity t_vhdl_log2exact is
end;

architecture t of t_vhdl_log2exact is
  signal n : integer := 4;
begin
  process
    variable p : real;
  begin
    p := 1.0;
    for k in 0 to 40 loop
      assert integer(ceil(log2(p))) = k report "log2 of 2**" & integer'image(k) severity failure;
      p := p * 2.0;
    end loop;
    assert integer(ceil(log2(real(n)))) = 2 severity failure;
    assert integer(ceil(log2(real(n + 1)))) = 3 severity failure;
    assert abs (log2(10.0) - 3.321928094887362) < 1.0e-12 severity failure;
    assert abs (log2(0.3) - (-1.736965594166206)) < 1.0e-12 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
