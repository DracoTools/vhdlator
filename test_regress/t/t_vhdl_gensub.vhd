-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

-- A generic package, instantiated as a design unit of its own
package holder is
  generic (type t);
  type box is record
    v : t;
  end record;
  function unwrap(b : box) return t;
end package;

package body holder is
  function unwrap(b : box) return t is
  begin
    return b.v;
  end function;
end package body;

package int_holder is new work.holder generic map (t => integer);

-- An entity whose generics are a subprogram and a package, not values
library ieee;
use ieee.std_logic_1164.all;

entity edged is
  generic (function edge(signal s : std_ulogic) return boolean;
           package hold is new work.holder generic map (<>));
  port (clock : in std_ulogic; d : in hold.box; q : out hold.box);
end entity;

architecture arch of edged is
begin
  reg : process(clock)
    variable b : hold.box;
  begin
    if edge(clock) then
      b.v := hold.unwrap(d);
      q <= b;
    end if;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gensub is
end entity;

architecture a of t_vhdl_gensub is
  signal clock : std_ulogic := '0';
  signal d, qf, qr : work.int_holder.box := (v => 0);
begin

  -- The subprogram is named without arguments, and the package by its
  -- library: neither is an expression (6.5.5)
  ufall : entity work.edged
    generic map (edge => falling_edge, hold => work.int_holder)
    port map (clock => clock, d => d, q => qf);

  urise : entity work.edged
    generic map (edge => rising_edge, hold => work.int_holder)
    port map (clock => clock, d => d, q => qr);

  main : process is
  begin
    d <= (v => 7);
    wait for 1 ns;
    clock <= '1';
    wait for 1 ns;
    assert qr.v = 7 report "rising took " & integer'image(qr.v) severity failure;

    d <= (v => 9);
    wait for 1 ns;
    clock <= '0';
    wait for 1 ns;
    assert qf.v = 9 report "falling took " & integer'image(qf.v) severity failure;
    assert qr.v = 7 report "rising moved" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;

end architecture;
