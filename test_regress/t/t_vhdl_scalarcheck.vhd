-- DESCRIPTION: Verilator: VHDL run-time check: values inside the subtypes the checks look at do not stop
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_scalarcheck is
end entity;

architecture a of t_vhdl_scalarcheck is
  type t is range 0 to 1;
  constant c : t := t(1);
  signal i_s : integer := 5;
  signal v_s : unsigned(7 downto 0);
  signal w_s : signed(3 downto 0);
begin
  v_s <= to_unsigned(i_s, 8);
  w_s <= to_signed(natural'(i_s), 4);
  assert c = t(1) severity failure;
  process is
    variable n : integer;
  begin
    wait for 1 ns;
    assert v_s = 5 and w_s = 5 severity failure;
    n := natural'(i_s) + integer(t(c)) + natural(0.4);
    assert n = 6 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
