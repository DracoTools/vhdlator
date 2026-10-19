-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Ports named by extended identifiers, as a netlist writes them (15.4.3;
-- VESTS ap_a_fg_a_11)

entity t_vhdl_extport_sub is
  port (\a<0>\, \a<1>\ : in bit; \o\ : out bit);
end entity;

architecture a of t_vhdl_extport_sub is
begin
  \o\ <= \a<0>\ and \a<1>\;
end architecture;

entity t_vhdl_extport is
end entity;

architecture a of t_vhdl_extport is
  signal x, y, z : bit;
begin
  u : entity work.t_vhdl_extport_sub port map (\a<0>\ => x, \a<1>\ => y, \o\ => z);
  process is
  begin
    x <= '1';
    y <= '1';
    wait for 1 ns;
    assert z = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
