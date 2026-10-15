-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, a logical operator whose operand joins thirty-two
-- elements one after another, as generated code writes a bit spread over a
-- word (GHDL gna perf02-long): the joining is as long as its elements
-- together, however deep it nests, and the operator is emitted natively

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_longcat is
end entity;

architecture a of t_vhdl_nine_longcat is
  signal x : std_logic_vector(31 downto 0) := x"5A5A5A5A";
  signal n : std_logic := '1';
  signal y : std_logic_vector(31 downto 0);
begin
  y <=
    x and
    n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n & n;

  process
  begin
    wait for 1 ns;
    assert y = x"5A5A5A5A" report to_hstring(y) severity failure;
    n <= 'X';
    wait for 1 ns;
    assert to_string(y) = "0X0XX0X0" & "0X0XX0X0" & "0X0XX0X0" & "0X0XX0X0"
      report to_string(y) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
