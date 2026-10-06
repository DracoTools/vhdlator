-- DESCRIPTION: Verilator: VHDL entity's generic package given no package
--
-- A component that does not declare the entity's interface package leaves
-- it with no actual, and an interface package has no default (6.5.5): Questa
-- 2025.2 refuses the instance, nvc 1.22.1 crashes, GHDL 7.0.0-dev accepts
-- (GHDL gna issue2749).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_genpkgnone_pkg is
  generic (N : integer);
  subtype slv_t is bit_vector(N-1 downto 0);
end package;

entity t_vhdl_genpkgnone_ent is
  generic (WIDTH : integer;
           package slv_pkg is new work.t_vhdl_genpkgnone_pkg generic map (N => WIDTH));
  port (o : out slv_pkg.slv_t);
end entity;

architecture beh of t_vhdl_genpkgnone_ent is
begin
  o <= (others => '1');
end architecture;

entity t_vhdl_genpkgnone_bad is
end entity;

architecture test of t_vhdl_genpkgnone_bad is
  component t_vhdl_genpkgnone_ent
    generic (WIDTH : integer);
    port (o : out bit_vector);
  end component;
  signal s : bit_vector(2 downto 0);
begin
  e : t_vhdl_genpkgnone_ent
    generic map (WIDTH => 3)
    port map (o => s);
end architecture;
