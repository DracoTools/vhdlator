-- DESCRIPTION: Verilator: VHDL slice outside its array's range, checked
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_boundcheck_slice_bad is
end entity;

architecture a of t_vhdl_boundcheck_slice_bad is
begin
  process
    variable v : bit_vector(7 downto 0) := x"5a";
    variable w : bit_vector(3 downto 0);
    variable n : integer := 3;
  begin
    -- In range, and a null slice, which may have any ends
    w := v(n downto 0);
    assert w = x"a" severity failure;
    assert v(n - 4 downto n) = "" severity failure;
    n := 9;
    report "before";
    w := v(n downto n - 3);
    report "after" severity failure;
    wait;
  end process;
end architecture;
