-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A generic whose subtype is written with an earlier generic of the same
-- entity, given "(others => 'Z')" by the instance: UVVM's
-- "GC_DEFAULT_LINE_VALUE : std_logic_vector(GC_DATA_WIDTH - 1 downto 0)"
-- (bitvis_vip_gpio/src/gpio_vvc.vhd). The aggregate is as long as the
-- generic, and that length names GC_DATA_WIDTH, which means nothing in the
-- instantiating architecture unless the generic map says what it is there.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gendepagg_sub is
  generic (
    GC_DATA_WIDTH : natural range 1 to 1024;
    GC_DEFAULT : std_logic_vector(GC_DATA_WIDTH - 1 downto 0));
  port (o : out std_logic_vector(GC_DATA_WIDTH - 1 downto 0));
end entity;

architecture a of t_vhdl_gendepagg_sub is
begin
  o <= GC_DEFAULT;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_gendepagg is
end entity;

architecture a of t_vhdl_gendepagg is
  signal o1 : std_logic_vector(0 downto 0);
  signal o8 : std_logic_vector(7 downto 0);
  signal o4 : std_logic_vector(3 downto 0);
begin
  u1 : entity work.t_vhdl_gendepagg_sub
    generic map (GC_DATA_WIDTH => 1, GC_DEFAULT => (others => '1'))
    port map (o => o1);
  u8 : entity work.t_vhdl_gendepagg_sub
    generic map (GC_DATA_WIDTH => 8, GC_DEFAULT => (others => '0'))
    port map (o => o8);
  -- Positionally
  u4 : entity work.t_vhdl_gendepagg_sub
    generic map (4, (others => '1'))
    port map (o4);

  process
  begin
    wait for 1 ns;
    assert o1 = "1" and o8 = x"00" and o4 = x"f"
      report to_string(o1) & " " & to_hstring(o8) & " " & to_hstring(o4) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
