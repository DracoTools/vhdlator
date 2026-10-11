-- DESCRIPTION: Verilator: VHDL index out of its array's range, checked
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_boundcheck_bad is
end entity;

architecture a of t_vhdl_boundcheck_bad is
  type mem_t is array (0 to 15) of bit_vector(7 downto 0);
  signal mem : mem_t;
begin
  process
    variable v : bit_vector(7 downto 0);
    variable i : integer := 3;
  begin
    -- A loop over the range is not checked
    for k in mem'range loop
      mem(k) <= x"5a";
    end loop;
    wait for 1 ns;
    i := 15;
    v := mem(i);
    assert v = x"5a" severity failure;
    i := i + 1;
    report "before";
    v := mem(i);
    report "after" severity failure;
    wait;
  end process;
end architecture;
