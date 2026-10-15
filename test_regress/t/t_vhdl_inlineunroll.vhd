-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A function with unconstrained parameters is emitted once per call site,
-- and once inlined each copy's loops have numbers for bounds and are
-- unrolled. Counted as the loops were written, a shift-and-add multiplier
-- was small enough to inline at every call, and unrolled it was written
-- out in full at each: std_logic_arith's "*" over 16 bits in nvc's
-- synopsys2 came to 190 MB of C++. Its copies are now costed as unrolled.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_inlineunroll is
end entity;

architecture a of t_vhdl_inlineunroll is
  function mul_f(a, b : std_ulogic_vector) return std_ulogic_vector is
    variable acc : std_ulogic_vector(a'length + b'length - 1 downto 0) := (others => '0');
    variable carry, s : std_ulogic;
  begin
    for j in 0 to b'length - 1 loop
      if b(b'low + j) = '1' then
        carry := '0';
        for i in 0 to a'length - 1 loop
          s := acc(i + j) xor a(a'low + i) xor carry;
          carry := (acc(i + j) and a(a'low + i)) or (carry and (acc(i + j) xor a(a'low + i)));
          acc(i + j) := s;
        end loop;
        acc(a'length + j) := carry;
      end if;
    end loop;
    return acc;
  end function;
  signal x : std_ulogic_vector(15 downto 0) := x"00FF";
  signal y : std_ulogic_vector(15 downto 0) := x"0101";
begin
  process
  begin
    wait for 1 ns;
    assert mul_f(x, y) = x"0000FFFF" report "1" severity failure;
    assert mul_f(y, y) = x"00010201" report "2" severity failure;
    assert mul_f(x, x) = x"0000FE01" report "3" severity failure;
    assert mul_f(x(7 downto 0), y(7 downto 0)) = x"00FF" report "4" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
