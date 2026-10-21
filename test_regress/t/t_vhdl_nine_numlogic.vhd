-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- numeric_std's logical operators over UNSIGNED and SIGNED, of lengths a
-- generic gives: PoC's arith_counter_bcd, "c <= ('0' & p) xor (('0' & p) + 1)".
-- They are std_logic_1164's element by element (16.5), and were left to the
-- package's body, whose arguments and result carry their bounds; the C++
-- compiler refused what that made of the joinings.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_numlogic_sub is
  generic (DIGITS : positive := 4);
  port (p : in unsigned(DIGITS - 1 downto 0); q : in signed(DIGITS - 1 downto 0);
        c, a, o, n : out unsigned(DIGITS downto 0); x : out signed(DIGITS - 1 downto 0));
end;
architecture rtl of t_vhdl_nine_numlogic_sub is
begin
  c <= ('0' & p) xor (('0' & p) + 1);
  a <= ('1' & p) and ('0' & not p);
  o <= ('0' & p) nor ('1' & p);
  n <= ('0' & p) xnor ('1' & p);
  x <= q nand (q sll 1);
end;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_numlogic is end;
architecture t of t_vhdl_nine_numlogic is
  signal p : unsigned(3 downto 0);
  signal q : signed(3 downto 0);
  signal c, a, o, n : unsigned(4 downto 0);
  signal x : signed(3 downto 0);
begin
  u : entity work.t_vhdl_nine_numlogic_sub generic map (DIGITS => 4)
    port map (p => p, q => q, c => c, a => a, o => o, n => n, x => x);
  process begin
    p <= "0111"; q <= "0110";
    wait for 1 ns;
    report "c=" & to_string(c) & " a=" & to_string(a) & " o=" & to_string(o)
      & " n=" & to_string(n) & " x=" & to_string(x);
    assert c = "01111" and a = "00000" and o = "01000" and n = "01111" and x = "1011"
      severity failure;
    p <= "1X0Z"; q <= "1LH-";
    wait for 1 ns;
    report "c=" & to_string(c) & " a=" & to_string(a) & " o=" & to_string(o)
      & " n=" & to_string(n) & " x=" & to_string(x);
    assert to_string(c) = "XXXXX" and to_string(a) = "00X0X" and to_string(o) = "00X1X"
      and to_string(n) = "01X1X" and to_string(x) = "11X1" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
