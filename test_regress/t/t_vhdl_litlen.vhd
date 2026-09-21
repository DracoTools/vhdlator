-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An array type's own "=" is false between operands of two lengths whatever
-- their elements hold (9.2.3), which is worth answering before the
-- comparison is built. How long a string literal is, though, is the number
-- of characters it stands for (9.3.2) -- not the length of whatever subtype
-- the context gave it. Measured the other way, "11" compared with a
-- two-element slice of a six-element type came out six long, the two were
-- called different lengths, and every such comparison answered false.

use std.env.all;

entity t_vhdl_litlen is
end entity;

architecture tb of t_vhdl_litlen is
  type mem is array (5 downto 0) of bit;
  type memup is array (0 to 5) of bit;
  signal s : mem := "011010";
  signal u : memup := "011010";
begin

  main: process is
    variable v : mem := "011010";
  begin
    -- A slice narrower than the type it comes from
    assert s(4 downto 3) = "11" report "a two-element slice did not equal its own value"
      severity failure;
    assert s(1 downto 0) = "10" report "the low slice did not equal its own value"
      severity failure;
    assert v(4 downto 3) = "11" report "the same of a variable" severity failure;
    assert u(2 to 3) = "10" report "the same of an ascending type" severity failure;

    -- ... and one that is not equal is still not equal
    assert s(4 downto 3) /= "10" report "two different values compared equal"
      severity failure;

    -- Two operands of genuinely different lengths are never equal (9.2.3)
    assert s(4 downto 3) /= "011010" report "two lengths compared equal" severity failure;

    -- The whole of it, which always worked
    assert s = "011010" report "the whole object did not equal its own value"
      severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
