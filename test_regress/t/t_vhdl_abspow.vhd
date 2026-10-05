-- DESCRIPTION: Verilator: VHDL "abs" of a primary raised to a power
--
-- "abs (x * 5.0) ** 2": the grammar gives ABS a primary and no exponent
-- (9.1), but nvc and GHDL take one, and |x| ** n is |x ** n| either way
-- (nvc parse issue805).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_abspow is
end entity;

architecture beh of t_vhdl_abspow is
begin
  process is
    variable x : real := -2.0;
    variable i : integer := -3;
    variable y : real;
    variable t : time;
  begin
    y := abs (x * 5.0) ** 2;
    assert y = 100.0 severity failure;
    assert abs i ** 3 = 27 severity failure;
    t := abs (x * 5.0) ** 2 * 1.0 ns;
    assert t = 100 ns severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
