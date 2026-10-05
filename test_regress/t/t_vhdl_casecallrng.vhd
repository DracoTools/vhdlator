-- DESCRIPTION: Verilator: VHDL range choice whose bounds are calls of
-- NUMERIC_STD
--
-- "when to_integer(unsigned'(\"0000\")) to to_integer(unsigned'(\"0101\"))"
-- is a range choice whose bounds call an operation of NUMERIC_STD with
-- locally static actuals, which 9.4.2 counts as locally static; nvc and
-- Questa take it in a case statement and in a selected assignment, though
-- GHDL refuses it (GHDL gna issue3072). The bounds are compared as the
-- statement runs, in the direction the range is written.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_casecallrng is
end entity;

architecture a of t_vhdl_casecallrng is
  signal sel : natural := 0;
  signal y : character;
  function kind(n : natural) return character is
  begin
    case n is
      when to_integer(unsigned'("0000")) to to_integer(unsigned'("0101")) =>
        return 'a';
      when to_integer(unsigned'("1001")) downto to_integer(unsigned'("0110")) =>
        return 'b';
      when to_integer(unsigned(std_logic_vector'("1010"))) =>
        return 'c';
      when others =>
        return 'z';
    end case;
  end function;
begin
  with sel select y <=
    'p' when to_integer(unsigned'("0000")) to to_integer(unsigned'("0011")),
    'q' when to_integer(unsigned'("0111")) downto to_integer(unsigned'("0100")),
    'r' when others;

  process is
  begin
    assert kind(0) = 'a' severity failure;
    assert kind(5) = 'a' severity failure;
    assert kind(6) = 'b' severity failure;
    assert kind(9) = 'b' severity failure;
    assert kind(10) = 'c' severity failure;
    assert kind(11) = 'z' severity failure;
    wait for 1 ns;
    assert y = 'p' severity failure;
    sel <= 3;
    wait for 1 ns;
    assert y = 'p' severity failure;
    sel <= 4;
    wait for 1 ns;
    assert y = 'q' severity failure;
    sel <= 7;
    wait for 1 ns;
    assert y = 'q' severity failure;
    sel <= 8;
    wait for 1 ns;
    assert y = 'r' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
