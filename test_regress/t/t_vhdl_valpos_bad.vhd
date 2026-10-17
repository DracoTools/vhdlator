-- DESCRIPTION: Verilator: VHDL run-time check: T'VAL of a position no value of T is at (NVC bounds6)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_valpos_bad is
end entity;

architecture a of t_vhdl_valpos_bad is
  type color is (red, green, blue);
  subtype warm is color range red to green;
begin
  process is
    variable i : integer := 32;
    variable c : character;
    variable k : color;
  begin
    c := character'val(i);
    assert c = ' ' severity failure;
    i := 2;
    k := color'val(i);
    assert k = blue severity failure;
    report "in range";
    k := warm'val(i);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
