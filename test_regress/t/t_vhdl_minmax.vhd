-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- MAXIMUM and MINIMUM give one of their two operands back, so the result
-- is as long as an operand and no longer -- however unconstrained the
-- subtype numeric_std writes it with (5.3.2.4).  Which of the two it is
-- comes from reading a SIGNED as the number it stands for.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_minmax is
end entity;

architecture a of t_vhdl_minmax is
  -- Read through a signal so nothing is settled before it runs
  signal three : integer := 3;

  -- A length only running decides, to keep the carried path covered
  function grow(v : signed; n : natural) return signed is
  begin
    return resize(v, v'length + n);
  end function;

begin
  process
    variable a  : signed(7 downto 0);
    variable b  : signed(7 downto 0);
    variable ua : unsigned(7 downto 0);
    variable ub : unsigned(7 downto 0);
    variable m  : signed(7 downto 0);
    variable iv : integer_vector(0 to 2) := (4, 9, 2);
    variable n  : natural := 4;
  begin
    wait for 1 ns;
    a  := to_signed(-10, 8);
    b  := to_signed(three, 8);
    ua := to_unsigned(246, 8);
    ub := to_unsigned(three, 8);

    -- Below zero is the smaller, however the elements read
    assert maximum(a, b) = to_signed(3, 8) report "signed maximum" severity failure;
    assert minimum(a, b) = to_signed(-10, 8) report "signed minimum" severity failure;

    -- The same elements, read without a sign
    assert maximum(ua, ub) = to_unsigned(246, 8) report "unsigned maximum" severity failure;
    assert minimum(ua, ub) = to_unsigned(3, 8) report "unsigned minimum" severity failure;

    -- Two fixed operands give a result an object of that length can take
    m := maximum(a, b);
    assert m = to_signed(3, 8) report "result did not fit its length" severity failure;

    -- An operand as long as running says carries the result with it
    assert maximum(grow(a, n), grow(b, n)) = to_signed(3, 12)
      report "carried maximum" severity failure;
    assert minimum(grow(a, n), grow(b, n)) = to_signed(-10, 12)
      report "carried minimum" severity failure;

    -- Over one array, the answer is an element and not an array at all
    assert maximum(iv) = 9 report "array maximum" severity failure;
    assert minimum(iv) = 2 report "array minimum" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
