-- DESCRIPTION: Verilator: RESOLVED over the nine values, for every pair of drivers and for three
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_resolve is
end entity;

architecture a of t_vhdl_nine_resolve is
  constant NINE : string(1 to 9) := "UX01ZWLH-";
  function va return std_logic_vector is
    variable r : std_logic_vector(0 to 80);
  begin
    for i in 0 to 8 loop
      for j in 0 to 8 loop
        r(9 * i + j) := std_logic'value("'" & NINE(i + 1) & "'");
      end loop;
    end loop;
    return r;
  end function;
  function vb return std_logic_vector is
    variable r : std_logic_vector(0 to 80);
  begin
    for i in 0 to 8 loop
      for j in 0 to 8 loop
        r(9 * i + j) := std_logic'value("'" & NINE(j + 1) & "'");
      end loop;
    end loop;
    return r;
  end function;
  signal s : std_logic_vector(0 to 80);
  signal t : std_logic;
begin
  s <= va;
  s <= vb;
  t <= '1';
  t <= 'L';
  t <= 'H';

  process is
    variable two : std_logic_vector(0 to 1);
    variable want : std_logic_vector(0 to 80);
  begin
    wait for 1 ns;
    for k in 0 to 80 loop
      two := (va(k), vb(k));
      want(k) := resolved(std_ulogic_vector(two));
    end loop;
    report "s=" & to_string(s);
    assert s = want report "want=" & to_string(want) severity failure;
    report "t=" & std_logic'image(t);
    assert t = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
