-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A STRING whose bounds only running decides keeps the direction and the left
-- bound its range was written with: "string(n + 3 downto 1)" with n worked out
-- from a parameter was indexed, sliced and asked its bounds as if it ran from 1
-- upwards. OSVVM's format of a time fills such a string from its right end,
-- and every alert's time came out blank.

entity t_vhdl_dynstrdir is
end entity;

architecture a of t_vhdl_dynstrdir is
  function pick(c : boolean; x, y : integer) return integer is
  begin
    if c then
      return x;
    end if;
    return y;
  end function;
  function down(u : time) return string is
    constant n : integer := pick(u = 1 sec, 4, 3);
    variable s : string(n + 3 downto 1) := (others => '.');
  begin
    s(1) := 'a';
    s(2) := 'b';
    return s & "|" & s(2 downto 1) & "|" & integer'image(s'left) & integer'image(s'right)
           & "|" & integer'image(s'high) & integer'image(s'low) & "|"
           & boolean'image(s'ascending);
  end function;
  function up(u : time) return string is
    constant n : integer := pick(u = 1 sec, 4, 3);
    variable s : string(n to n + 3) := (others => '.');
  begin
    s(n) := 'a';
    s(n + 1) := 'b';
    return s & "|" & s(n to n + 1) & "|" & integer'image(s'left) & integer'image(s'right);
  end function;
begin
  process
    variable tu : time := 1 ns;
  begin
    assert down(1 ns) = "....ba|ba|61|61|false" report "static: " & down(1 ns) severity failure;
    assert down(tu) = "....ba|ba|61|61|false" report "dynamic: " & down(tu) severity failure;
    assert up(tu) = "ab..|ab|36" report "dynamic up: " & up(tu) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
