-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_specrange is
end entity;

architecture a of t_vhdl_specrange is

  subtype small_t is integer range 0 to 7;

  -- Copied per call site for the length of v, which makes n fold there too.
  -- The arm the guard rules out then folds to a value outside small_t, and
  -- it must not be reported: it is a value that never comes about (4.2.2.1).
  function shifted (v : std_logic_vector; n : integer) return integer is
    variable r : std_logic_vector(v'length - 1 downto 0) := v;
    variable s : small_t;
  begin
    if n >= 4 then
      s := n - 4;
    else
      s := n;
    end if;
    return s + r'length;
  end function;

begin

  main : process is
    variable x : std_logic_vector(7 downto 0) := x"00";
  begin
    assert shifted(x, 1) = 9 report "low arm" severity failure;
    assert shifted(x, 6) = 10 report "high arm" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
