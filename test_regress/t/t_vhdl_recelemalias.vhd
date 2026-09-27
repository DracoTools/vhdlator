-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An inout record port whose elements are associated one by one with
-- signals of the instantiating architecture (6.5.7.1): each element and its
-- actual are one signal, driven from inside the instance and from outside
-- it at once. UVVM's test harnesses connect the SBI VVC this way
-- (bitvis_vip_uart/tb/maintenance_tb/uart_monitor_th.vhd): the VVC drives
-- cs and addr through a procedure's signal parameter, the DUT answers on
-- rdata, and the VVC reads it back.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_recelemalias_pkg is
  type t_bus is record
    cs    : std_logic;
    addr  : unsigned;
    rdata : std_logic_vector;
  end record;
  function init_bus(aw, dw : natural) return t_bus;
  procedure bus_read(signal clk : in std_logic; signal b : inout t_bus;
                     constant a : in natural; variable v : out std_logic_vector);
end package;

package body t_vhdl_recelemalias_pkg is
  function init_bus(aw, dw : natural) return t_bus is
    variable r : t_bus(addr(aw - 1 downto 0), rdata(dw - 1 downto 0));
  begin
    r.cs := '0';
    r.addr := (r.addr'range => '0');
    r.rdata := (r.rdata'range => 'Z');
    return r;
  end function;

  procedure bus_read(signal clk : in std_logic; signal b : inout t_bus;
                     constant a : in natural; variable v : out std_logic_vector) is
  begin
    wait until falling_edge(clk);
    b.cs <= '1';
    b.addr <= to_unsigned(a, b.addr'length);
    wait until rising_edge(clk);
    v := b.rdata;
    wait until falling_edge(clk);
    b.cs <= '0';
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recelemalias_pkg.all;

entity t_vhdl_recelemalias_master is
  generic (AW : natural := 3; DW : natural := 8);
  port (
    clk  : in std_logic;
    done : out boolean := false;
    m    : inout t_bus(addr(AW - 1 downto 0), rdata(DW - 1 downto 0)) := init_bus(AW, DW)
  );
end entity;

architecture a of t_vhdl_recelemalias_master is
begin
  process
    variable v : std_logic_vector(DW - 1 downto 0);
  begin
    wait for 20 ns;
    bus_read(clk, m, 3, v);
    assert v = x"05" report "read of address 3 gave " & to_hstring(v) severity failure;
    -- Assigned here directly as well as through the procedure
    wait until falling_edge(clk);
    m.cs <= '1';
    m.addr <= "110";
    wait until rising_edge(clk);
    assert m.rdata = x"a6" report "read of address 6 gave " & to_hstring(m.rdata)
      severity failure;
    wait until falling_edge(clk);
    m.cs <= '0';
    done <= true;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_recelemalias is
end entity;

architecture a of t_vhdl_recelemalias is
  signal clk   : std_logic := '0';
  signal cs    : std_logic;
  signal addr  : unsigned(2 downto 0);
  signal rdata : std_logic_vector(7 downto 0);
  signal done  : boolean;
  signal edges : natural := 0;
begin
  clk <= not clk after 5 ns when not done;

  -- The responder sees what the instance drives
  rdata <= std_logic_vector(x"a0" + addr) when cs = '1' and addr = 6
           else x"05" when cs = '1' and addr = 3
           else x"00";

  process (cs)
  begin
    if rising_edge(cs) then
      edges <= edges + 1;
    end if;
  end process;

  i_master : entity work.t_vhdl_recelemalias_master
    generic map (AW => 3, DW => 8)
    port map (
      clk     => clk,
      done    => done,
      m.cs    => cs,
      m.addr  => addr,
      m.rdata => rdata
    );

  process
  begin
    wait until done;
    assert edges = 2 report "cs rose " & integer'image(edges) & " times" severity failure;
    assert cs = '0' report "cs left high" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
