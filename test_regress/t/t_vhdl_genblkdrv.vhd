-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A resolved port driven by a concurrent assignment in a block of one
-- alternative of an if generate, and directly in the other (11.8): only the
-- alternative elaborated drives it. open-logic's olo_axi_master_simple ties
-- M_Axi_WValid to '0' when there is no write side, and drives it from a
-- block of the write side otherwise.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_genblkdrv_m is
  generic (Impl_g : boolean := true);
  port (o : out std_logic_vector(3 downto 0); v : out std_logic; r : out std_logic);
end entity;
architecture a of t_vhdl_genblkdrv_m is
begin
  g_w : if Impl_g generate
    b : block is
      signal s : std_logic := '1';
    begin
      o <= "1010";
      v <= s;
    end block;
    r <= '1';
  end generate;
  g_nw : if not Impl_g generate
    o <= (others => '0');
    v <= '0';
    r <= '0';
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_genblkdrv_mid is
  generic (Impl : boolean);
  port (o : out std_logic_vector(3 downto 0); v : out std_logic; r : out std_logic);
end entity;
architecture a of t_vhdl_genblkdrv_mid is
begin
  u : entity work.t_vhdl_genblkdrv_m generic map (Impl_g => Impl) port map (o, v, r);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_genblkdrv is
end entity;
architecture a of t_vhdl_genblkdrv is
  signal o1, o2 : std_logic_vector(3 downto 0);
  signal v1, v2, r1, r2 : std_logic;
begin
  u1 : entity work.t_vhdl_genblkdrv_mid generic map (Impl => true) port map (o1, v1, r1);
  u2 : entity work.t_vhdl_genblkdrv_mid generic map (Impl => false) port map (o2, v2, r2);
  process
  begin
    wait for 1 ns;
    assert to_string(o1) = "1010" and v1 = '1' and r1 = '1'
      report "u1 " & to_string(o1) & std_logic'image(v1) & std_logic'image(r1) severity failure;
    assert to_string(o2) = "0000" and v2 = '0' and r2 = '0'
      report "u2 " & to_string(o2) & std_logic'image(v2) & std_logic'image(r2) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
