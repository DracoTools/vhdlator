-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_operuser is
end entity;

architecture a of t_vhdl_operuser is

  subtype byte_t is std_logic_vector(7 downto 0);

  -- An operator a design writes for itself over a logic type means what its
  -- body says, not what the operator of that name in the IEEE packages
  -- means. This one saturates where the predefined one wraps.
  function "+" (l, r : byte_t) return byte_t is
    variable sum : integer;
  begin
    sum := to_integer(unsigned(l)) + to_integer(unsigned(r));
    if sum > 255 then sum := 255; end if;
    return std_logic_vector(to_unsigned(sum, 8));
  end function;

  function "and" (l, r : byte_t) return byte_t is
  begin
    -- Deliberately not the bitwise "and", to show the body is what runs
    return l;
  end function;

begin

  main : process is
    variable a, b : byte_t;
  begin
    a := x"F0";
    b := x"20";
    assert to_integer(unsigned(a + b)) = 255
      report "sum is " & integer'image(to_integer(unsigned(a + b))) severity failure;

    a := x"01";
    b := x"02";
    assert to_integer(unsigned(a + b)) = 3 report "small sum" severity failure;

    a := x"FF";
    b := x"0F";
    assert (a and b) = x"FF" report "and is not the body" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
