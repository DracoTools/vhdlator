-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, an inout port of the nine values driven element
-- by element through an alias that renumbers it, by the outputs of
-- instances a generate makes: driving through the alias is driving the
-- port (6.6.2), each instance a driver of its element (VESTS ch_14_fg_14_05)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_aliaspartdrv_cell is
  port (d : in std_logic; q : out std_logic);
end entity;

architecture a of t_vhdl_nine_aliaspartdrv_cell is
begin
  q <= not d;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_aliaspartdrv_reg is
  generic (n : positive);
  port (din : in std_logic; pd : inout std_logic_vector(n - 1 downto 0));
end entity;

architecture a of t_vhdl_nine_aliaspartdrv_reg is
  alias npd : std_logic_vector(0 to n - 1) is pd;
begin
  g : for index in 0 to n - 1 generate
    first : if index = 0 generate
      c : entity work.t_vhdl_nine_aliaspartdrv_cell port map (d => din, q => npd(index));
    end generate;
    other : if index /= 0 generate
      c : entity work.t_vhdl_nine_aliaspartdrv_cell port map (d => npd(index - 1), q => npd(index));
    end generate;
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_aliaspartdrv is
end entity;

architecture a of t_vhdl_nine_aliaspartdrv is
  signal din : std_logic := '0';
  signal pd : std_logic_vector(3 downto 0);
begin
  r : entity work.t_vhdl_nine_aliaspartdrv_reg generic map (4) port map (din, pd);

  process
  begin
    wait for 1 ns;
    report to_string(pd);
    assert pd = "1010" severity failure;
    din <= '1';
    wait for 1 ns;
    report to_string(pd);
    assert pd = "0101" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
