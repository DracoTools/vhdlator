-- DESCRIPTION: Verilator: VHDL 'POS and 'VAL of a logic type while running
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A value of a logic type is held as the bit it stands for, not as its
-- position, so 'POS and 'VAL translate: '1' is at position 3 and '0' at 2.
-- VUnit prints a std_logic through a table indexed by 'POS, which printed
-- 'X' for '1' and 'U' for '0'.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_logicpos is
end entity;
architecture a of t_vhdl_logicpos is
  signal one : std_logic := '1';
  constant tbl : string := "UX01ZWLH-";
  function via(s : std_ulogic) return character is
  begin
    return tbl(std_ulogic'pos(s) + 1);
  end;
begin
  process
    variable z : std_ulogic := '0';
    variable n : integer;
  begin
    wait for 1 ns;
    report "pos " & integer'image(std_logic'pos(one)) & " " & integer'image(std_ulogic'pos(z)) & " via " & via(one) & via(z);
    for i in 2 to 3 loop
      n := i;
      report "val " & std_ulogic'image(std_ulogic'val(n)) & " rt " & integer'image(std_ulogic'pos(std_ulogic'val(n)));
    end loop;
    n := 7;
    report "valH " & boolean'image(std_ulogic'val(n) = '1' or std_ulogic'val(n) = 'H');
    assert std_logic'pos(one) = 3 and std_ulogic'pos(z) = 2 report "pos" severity failure;
    assert via(one) = '1' and via(z) = '0' report "table" severity failure;
    n := 3;
    assert std_ulogic'val(n) = '1' report "val 3" severity failure;
    n := 2;
    assert std_ulogic'val(n) = '0' report "val 2" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
