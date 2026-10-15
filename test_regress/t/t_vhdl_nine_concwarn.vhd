-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- numeric_std's comparisons and TO_INTEGER warn when an operand holds a
-- metavalue, and a concurrent statement that uses them is a process (11.6)
-- that warns each time it runs: once at the start, again when what it
-- reads changes to another metavalue, not when it changes to a number.
-- Emitted as a continuous assignment it had nowhere to say so; emitted as
-- a block that starts and then reacts, it said so twice at the start. A
-- statement whose value calls a function that warns is one as well (GHDL
-- gna perf02-long's table lookups).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_concwarn is
end entity;
architecture a of t_vhdl_nine_concwarn is
  signal free : unsigned(3 downto 0) := "00X1";
  signal less, eq : std_ulogic;
  signal idx : integer;
  signal sum : unsigned(3 downto 0);
  signal idx2 : integer;
  function to_int(v : std_ulogic_vector) return integer is
  begin
    return to_integer(unsigned(v));
  end function;
begin
  less <= '1' when free < 3 else '0';
  eq <= '1' when free = "0001" else '0';
  idx <= to_integer(free);
  sum <= free + 1;
  idx2 <= to_int(std_ulogic_vector(free));
  process
  begin
    wait for 1 ns;
    report "less=" & std_ulogic'image(less) & " eq=" & std_ulogic'image(eq) & " idx=" & integer'image(idx);
    free <= "0011";
    wait for 1 ns;
    free <= "0X11";
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
