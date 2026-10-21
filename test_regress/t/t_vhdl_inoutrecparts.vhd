-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An INOUT port of a record of records of resolved elements, which the
-- architecture drives only through an instance's output associated element
-- by element: "o.awready => ax.wa.awready". The port is two in lowering,
-- what it reads and what it drives, and each association drives the driving
-- one; it was written into the port's input ("Assigning to input/const
-- variable"). Shaped after UVVM's axilite_th, "wr_port_out.awready =>
-- axilite_if_2.write_address_channel.awready". The elements the harness
-- drives nowhere are not checked: by 14.7.3.2 their default is a source of
-- the actual (nvc), where Questa and GHDL leave it out.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_inoutrecparts_pkg is
  type wa_t is record
    awaddr : std_logic_vector;
    awvalid : std_logic;
    awready : std_logic;
  end record;
  type wr_t is record
    bready : std_logic;
    bresp : std_logic_vector(1 downto 0);
    bvalid : std_logic;
  end record;
  type if_t is record
    wa : wa_t;
    wr : wr_t;
  end record;
  type out_if is record
    awready : std_logic;
    bresp : std_logic_vector(1 downto 0);
    bvalid : std_logic;
  end record;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutrecparts_pkg.all;
entity t_vhdl_inoutrecparts_slave is
  port (o : out out_if);
end entity;
architecture a of t_vhdl_inoutrecparts_slave is
begin
  process
  begin
    o.awready <= '1';
    o.bresp <= "10";
    o.bvalid <= '1';
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutrecparts_pkg.all;
entity t_vhdl_inoutrecparts_th is
  generic (AW : natural := 8);
  port (signal ax : inout if_t(wa(awaddr(AW - 1 downto 0))));
end entity;
architecture a of t_vhdl_inoutrecparts_th is
begin
  u : entity work.t_vhdl_inoutrecparts_slave
    port map (o.awready => ax.wa.awready, o.bresp => ax.wr.bresp, o.bvalid => ax.wr.bvalid);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutrecparts_pkg.all;
entity t_vhdl_inoutrecparts is
end entity;
architecture a of t_vhdl_inoutrecparts is
  signal ax : if_t(wa(awaddr(7 downto 0)));
begin
  ax.wa.awaddr <= (others => '0');
  ax.wa.awvalid <= '0';
  ax.wr.bready <= '1';
  u : entity work.t_vhdl_inoutrecparts_th port map (ax => ax);
  process
  begin
    wait for 1 ns;
    assert ax.wa.awready = '1' and ax.wr.bresp = "10" and ax.wr.bvalid = '1'
      report "slave " & std_logic'image(ax.wa.awready) & to_string(ax.wr.bresp) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
