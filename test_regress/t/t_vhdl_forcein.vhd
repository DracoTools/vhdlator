-- DESCRIPTION: Verilator: VHDL force and release of a signal parameter or a
-- port of mode in
--
-- A force of a target of mode in overrides its effective value, which is
-- what force mode in means and what such a target is given when no mode is
-- written (10.5.2.1). A procedure may so force the signal it was given as
-- a parameter of mode in, and an architecture may force its own input
-- (GHDL gna issue2867).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_forcein_sub is
  port (i : in std_ulogic; o : out std_ulogic);
end entity;

architecture a of t_vhdl_forcein_sub is
begin
  o <= i;
  process is
  begin
    wait for 200 ns;
    i <= force '0';
    wait for 10 ns;
    i <= release;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_forcein is
end entity;

architecture a of t_vhdl_forcein is
  signal data : std_ulogic_vector(7 downto 0) := (others => '0');
  signal i, o : std_ulogic;
begin
  data <= (others => '1');
  i <= '1';
  u : entity work.t_vhdl_forcein_sub port map (i, o);

  process is
    procedure force_to_zero(signal output : in std_ulogic_vector) is
    begin
      output <= force (output'range => '0');
      wait for 100 ns;
      output <= release;
    end procedure;
  begin
    wait for 1 ns;
    assert data = x"FF" severity failure;
    force_to_zero(data);
    wait for 0 ns;
    assert data = x"FF" severity failure;
    wait;
  end process;

  process is
  begin
    wait for 50 ns;
    assert data = x"00" severity failure;
    wait for 100 ns;
    assert data = x"FF" severity failure;
    assert o = '1' severity failure;
    wait for 55 ns;
    assert o = '0' severity failure;
    wait for 10 ns;
    assert o = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
