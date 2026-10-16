-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- numeric_std's "+" and "-" of a vector and an integer make the integer a
-- vector of the other's length, TO_UNSIGNED(R, L'LENGTH) or TO_SIGNED, which
-- warns "vector truncated" where it does not fit: a constant, a variable,
-- a length the instance gives, and a concurrent statement. nvc 1.22.1, GHDL
-- 7.0.0-dev and Questa 2025.2 print the same six warnings.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_inttrunc is
  generic (W : natural := 4);
end entity;
architecture a of t_vhdl_nine_inttrunc is
  signal u : unsigned(3 downto 0) := "0001";
  signal s : signed(3 downto 0) := "0001";
  signal g : unsigned(W - 1 downto 0) := (others => '0');
  signal r1, r2, r3, r4, r5 : unsigned(3 downto 0);
  signal q1, q2 : signed(3 downto 0);
  signal c1 : unsigned(3 downto 0);
begin
  -- A concurrent statement is a process, and warns as one
  c1 <= u + 31;
  process
    variable n : natural := 20;
    variable i : integer := -9;
  begin
    wait for 1 ns;
    r1 <= u + 15;        -- fits
    r2 <= u + 16;        -- truncated
    r3 <= u + n;         -- truncated, at run time
    r4 <= u - n;
    r5 <= g + 17;        -- a length the instance gives
    q1 <= s + 7;         -- fits
    q2 <= s + i;         -- truncated
    wait for 1 ns;
    report to_string(r1) & " " & to_string(r2) & " " & to_string(r3) & " " & to_string(r4) & " " & to_string(r5) & " " & to_string(q1) & " " & to_string(q2) & " " & to_string(c1);
    assert r1 = "0000" and r2 = "0001" and r3 = "0101" and r4 = "1101" and r5 = "0001"
      and q1 = "1000" and q2 = "1000" and c1 = "0000" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
