-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- "+", "-" and "abs" over a value whose length only running decides, and one
-- element standing beside such a value in a concatenation.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t is
end entity;

architecture rtl of t is
  signal width : integer := 6;

  function img (v : signed) return string is
    variable r : string(1 to v'length);
    variable k : integer := 1;
  begin
    for i in v'range loop
      r(k) := '1' when v(i) = '1' else '0';
      k := k + 1;
    end loop;
    return r;
  end function;

  function imgu (v : unsigned) return string is
    variable r : string(1 to v'length);
    variable k : integer := 1;
  begin
    for i in v'range loop
      r(k) := '1' when v(i) = '1' else '0';
      k := k + 1;
    end loop;
    return r;
  end function;

begin
  process
    -- A length nothing here settles: the signal decides it
    impure function mk (n : integer) return signed is
      variable r : signed(width - 1 downto 0);
    begin
      r := to_signed(n, width);
      return r;
    end function;

    impure function mku return unsigned is
      variable r : unsigned(width - 1 downto 0);
    begin
      r := "010110";
      return r;
    end function;

    variable a : signed(5 downto 0);
    variable b : unsigned(6 downto 0);
  begin
    a := -mk(5);
    assert img(a) = "111011" report "neg got " & img(a) severity failure;
    a := abs mk(-5);
    assert img(a) = "000101" report "abs neg got " & img(a) severity failure;
    a := abs mk(5);
    assert img(a) = "000101" report "abs pos got " & img(a) severity failure;
    a := -mk(-5) - 1;
    assert img(a) = "000100" report "negsub got " & img(a) severity failure;

    -- One element beside a value of a length only running decides
    b := mku & '1';
    assert imgu(b) = "0101101" report "cat got " & imgu(b) severity failure;

    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
