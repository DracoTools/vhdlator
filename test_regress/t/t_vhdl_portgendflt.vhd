-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record port whose elements the entity's generics constrain, associated
-- element by element, by an instance whose generic map leaves one of those
-- generics to its default: UVVM's AXI-Lite slave model, given only
-- C_AXI_DATA_WIDTH (bitvis_vip_axilite/tb/maintenance_tb/axilite_th.vhd).
-- The port's subtype was built in the instantiating architecture with
-- stand-ins for the generics the map names, and the defaulted one named
-- nothing there.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_portgendflt_pkg is
  type t_in_if is record
    awaddr : std_logic_vector;
    awvalid : std_logic;
    wdata : std_logic_vector;
  end record;
  type t_out_if is record
    ready : std_logic;
    rdata : std_logic_vector;
  end record;
  constant C_DEFAULT_ADDR_WIDTH : natural := 32;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_portgendflt_pkg.all;

entity t_vhdl_portgendflt_slave is
  generic (
    C_AXI_ADDR_WIDTH : integer := C_DEFAULT_ADDR_WIDTH;
    C_AXI_DATA_WIDTH : integer := 32);
  port (
    clk : in std_logic;
    wr_in : in t_in_if(awaddr(C_AXI_ADDR_WIDTH - 1 downto 0),
                       wdata(C_AXI_DATA_WIDTH - 1 downto 0));
    rd_out : out t_out_if(rdata(C_AXI_DATA_WIDTH - 1 downto 0)));
end entity;

architecture a of t_vhdl_portgendflt_slave is
begin
  process (clk)
  begin
    if rising_edge(clk) then
      rd_out.ready <= wr_in.awvalid;
      rd_out.rdata <= wr_in.wdata xor wr_in.awaddr(C_AXI_DATA_WIDTH - 1 downto 0);
    end if;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portgendflt is
end entity;

architecture a of t_vhdl_portgendflt is
  signal clk : std_logic := '0';
  signal areset : std_logic := '1';
  signal addr : std_logic_vector(31 downto 0) := x"000000f0";
  signal wdata : std_logic_vector(15 downto 0) := x"1234";
  signal ready : std_logic;
  signal rdata : std_logic_vector(15 downto 0);
begin
  clk <= not clk after 5 ns;

  s : entity work.t_vhdl_portgendflt_slave
    generic map (C_AXI_DATA_WIDTH => 16)
    port map (
      clk => clk,
      wr_in.awaddr => addr,
      wr_in.awvalid => not areset,
      wr_in.wdata => wdata,
      rd_out.ready => ready,
      rd_out.rdata => rdata);

  process
  begin
    wait for 12 ns;
    areset <= '0';
    wait for 20 ns;
    assert ready = '1' and rdata = x"12c4"
      report "ready " & std_logic'image(ready) & " rdata " & to_hstring(rdata) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
