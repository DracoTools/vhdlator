-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A product of vectors is as long as its operands together (numeric_std
-- 16.5), and a product of a vector and an integer twice the vector: in a
-- place that gives it no width of its own -- the operand of RESIZE, a
-- comparison, a concatenation -- it must not be taken at the longer
-- operand's length and lose its top.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_mulwidth is
end entity;

architecture a of t_vhdl_mulwidth is
  signal a, b : signed(7 downto 0) := to_signed(-37, 8);
  signal u : unsigned(3 downto 0) := x"B";
  signal v : unsigned(7 downto 0) := x"F0";
  signal p, q : signed(15 downto 0);
  signal r : unsigned(15 downto 0);
begin
  p <= resize(signed('0' & std_ulogic_vector(a(3 downto 0))) * b, 16);
  q <= resize(signed('0' & a(3 downto 0)) * b, 16);
  r <= resize(u * v, 16);
  process
  begin
    wait for 1 ns;
    assert p = -407 report "p " & integer'image(to_integer(p)) severity failure;
    assert q = -407 report "q " & integer'image(to_integer(q)) severity failure;
    assert r = 2640 report "r " & integer'image(to_integer(r)) severity failure;
    assert to_integer(u * v) = 2640 report "u*v" severity failure;
    assert u * v = 2640 report "u*v cmp" severity failure;
    assert to_integer(v * 3) = 720 report "v*3 " & integer'image(to_integer(v * 3)) severity failure;
    assert to_integer(5 * u) = 55 report "5*u " & integer'image(to_integer(5 * u)) severity failure;
    assert to_integer(a * (-3)) = 111 report "a*-3 " & integer'image(to_integer(a * (-3))) severity failure;
    assert to_integer(a * b) = 1369 report "a*b " & integer'image(to_integer(a * b)) severity failure;
    assert std_ulogic_vector(u * u) & "1" = "011110011" report "concat" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
