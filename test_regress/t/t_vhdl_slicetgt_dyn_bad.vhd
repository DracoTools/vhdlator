-- DESCRIPTION: Verilator: VHDL run-time check: a signal slice target whose ends only running gives, outside the array (NVC bounds5)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_slicetgt_dyn_bad is
end entity;

architecture a of t_vhdl_slicetgt_dyn_bad is
  type int_vec is array (natural range <>) of integer;
  signal s : int_vec(8 downto 0);
begin
  process is
    variable k : integer;
  begin
    k := 8;
    s(k downto 1) <= (others => 1);
    wait for 1 ns;
    assert s(8) = 1 and s(0) = integer'left severity failure;
    k := 0;
    s(k downto 1) <= (others => 2);
    wait for 1 ns;
    report "in range";
    k := 9;
    s(k downto 1) <= (others => 3);
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
