-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A formal named by a slice of itself. The whole of the formal is
-- associated, and where the formal has no length of its own the slice is
-- what gives it one, so the bounds are the slice's and not the actual's.

use std.env.all;

entity t_vhdl_sliceformal is
end entity;

architecture tb of t_vhdl_sliceformal is

  -- An unconstrained formal: the slice decides its bounds
  procedure show(s : string; l : natural; r : natural) is
  begin
    assert s'left = l
      report "left " & integer'image(s'left) & " wanted " & integer'image(l)
      severity failure;
    assert s'right = r
      report "right " & integer'image(s'right) & " wanted " & integer'image(r)
      severity failure;
    assert s = "Hello world!" report "text" severity failure;
  end procedure;

  function total(a : bit_vector) return natural is
    variable n : natural := 0;
  begin
    for i in a'range loop
      if a(i) = '1' then n := n + 1; end if;
    end loop;
    return n;
  end function;

  signal src : bit_vector(3 downto 0) := "1011";

begin

  main : process is
  begin
    -- The slice runs 4 to 15, so that is what the formal runs, even though
    -- the actual string literal would otherwise be 1 to 12
    show(s(4 to 15) => "Hello world!", l => 4, r => 15);

    -- The same formal written plainly takes the actual's own bounds
    show("Hello world!", 1, 12);

    -- A slice of an unconstrained formal in a function call
    assert total(a(3 downto 0) => src) = 3
      report "total" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
