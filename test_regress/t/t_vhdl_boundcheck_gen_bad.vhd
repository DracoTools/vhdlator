-- DESCRIPTION: Verilator: VHDL index written outside an array a generic sizes
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_boundcheck_gen_sub is
  generic (depth : positive);
end entity;

architecture a of t_vhdl_boundcheck_gen_sub is
  type mem_t is array (0 to depth - 1) of bit_vector(7 downto 0);
  signal mem : mem_t;
begin
  process
    variable i : integer := 0;
  begin
    for k in 0 to depth loop
      i := k;
      if k = depth then
        report "before";
      end if;
      mem(i) <= x"a5";
      wait for 1 ns;
    end loop;
    report "after" severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_boundcheck_gen_bad is
end entity;

architecture a of t_vhdl_boundcheck_gen_bad is
begin
  u : entity work.t_vhdl_boundcheck_gen_sub generic map (depth => 6);
end architecture;
