-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The length of an element of a record port whose range the actual gives,
-- where that range is null: UVVM's AXI VVC with no ID bits,
-- awid(GC_ID_WIDTH - 1 downto 0) with GC_ID_WIDTH 0, tests "if
-- awid'length > 0" before normalizing the ID (bitvis_vip_axi/src/
-- axi_channel_handler_pkg.vhd). The range comes with the instance and so
-- does its direction, which the instance binds; the direction was read off
-- the bounds instead, and "-1 downto 0" read as "-1 to 0", two long.
library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nullportlen_pkg is
  type t_ch is record
    awid : std_logic_vector;
    awaddr : std_logic_vector;
  end record;
  procedure show(constant awid_value : in std_logic_vector; signal awid : inout std_logic_vector;
                 signal awaddr : inout std_logic_vector);
end package;
package body t_vhdl_nullportlen_pkg is
  procedure show(constant awid_value : in std_logic_vector; signal awid : inout std_logic_vector;
                 signal awaddr : inout std_logic_vector) is
  begin
    assert awid'length = awid_value'length and awaddr'length = 32
      report "awid'length=" & integer'image(awid'length) severity failure;
    assert (awid'length > 0) = (awid_value'length > 0) severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nullportlen_pkg.all;
entity t_vhdl_nullportlen_vvc is
  generic (GC_ID_WIDTH : natural := 1);
  port (vif : inout t_ch);
end entity;
architecture a of t_vhdl_nullportlen_vvc is
begin
  process
    variable v_awid : std_logic_vector(GC_ID_WIDTH - 1 downto 0) := (others => '0');
  begin
    show(v_awid, vif.awid, vif.awaddr);
    assert vif.awid'length = GC_ID_WIDTH and vif.awid'ascending = false
      report "direct " & integer'image(vif.awid'length) severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nullportlen_pkg.all;
entity t_vhdl_nullportlen is
  generic (C_W1 : natural := 8; C_W2 : natural := 0);
end entity;
architecture a of t_vhdl_nullportlen is
  signal ch1 : t_ch(awid(C_W1 - 1 downto 0), awaddr(31 downto 0));
  signal ch2 : t_ch(awid(C_W2 - 1 downto 0), awaddr(31 downto 0));
begin
  v1 : entity work.t_vhdl_nullportlen_vvc generic map (GC_ID_WIDTH => C_W1) port map (vif => ch1);
  v2 : entity work.t_vhdl_nullportlen_vvc generic map (GC_ID_WIDTH => C_W2) port map (vif => ch2);
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
