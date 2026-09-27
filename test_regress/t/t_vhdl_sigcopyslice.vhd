-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A procedure with signal parameters is copied for the signals a call
-- gives it, and in the copy a variable parameter whose length is open is
-- held with its bounds. UVVM's avalon_st VVC gives such a parameter a slice
-- of an element of a record, "avalon_st_receive(channel_value =>
-- v_result.channel_value(GC_CHANNEL_WIDTH - 1 downto 0), ...)"
-- (bitvis_vip_avalon_st/src/avalon_st_vvc.vhd). A slice was only filled
-- into the parameter, and copied back out of it, where it was taken from a
-- whole variable: taken from an element of a record it was handed over as
-- it stood, and the C++ did not compile.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sigcopyslice_pkg is
  type t_if is record
    channel : std_logic_vector;
    data : std_logic_vector;
  end record;
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_slv_array_ptr is access t_slv_array;
  type t_res is record
    channel_value : std_logic_vector(7 downto 0);
    data : std_logic_vector(31 downto 0);
    n : natural;
  end record;
  procedure recv(variable channel_value : out std_logic_vector;
                 variable data_array : out t_slv_array;
                 signal clk : in std_logic; signal ifc : in t_if);
  procedure expect(constant channel_exp : in std_logic_vector; signal clk : in std_logic;
                   signal ifc : in t_if);
end package;

package body t_vhdl_sigcopyslice_pkg is
  procedure recv(variable channel_value : out std_logic_vector;
                 variable data_array : out t_slv_array;
                 signal clk : in std_logic; signal ifc : in t_if) is
    variable v_norm : std_logic_vector(channel_value'length - 1 downto 0) := (others => '0');
  begin
    wait until rising_edge(clk);
    v_norm := ifc.channel(channel_value'length - 1 downto 0);
    channel_value := v_norm;
    for i in data_array'range loop
      data_array(i) := ifc.data(data_array(i)'length - 1 downto 0);
    end loop;
  end procedure;

  -- The same procedure given an object only running sizes: this is what
  -- makes the copy hold channel_value with its bounds
  procedure expect(constant channel_exp : in std_logic_vector; signal clk : in std_logic;
                   signal ifc : in t_if) is
    variable v_rx : std_logic_vector(ifc.channel'length - 1 downto 0);
    variable v_rx_data : t_slv_array(0 to 1)(15 downto 0);
  begin
    recv(v_rx, v_rx_data, clk, ifc);
    assert v_rx = channel_exp report "expect " & to_hstring(v_rx) severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigcopyslice_pkg.all;

entity t_vhdl_sigcopyslice_vvc is
  generic (GC_CHANNEL_WIDTH : natural := 1);
  port (clk : in std_logic; ifc : in t_if; done : out boolean := false);
end entity;

architecture a of t_vhdl_sigcopyslice_vvc is
begin
  process
    variable v_result : t_res;
    variable v_ptr : t_slv_array_ptr;
  begin
    v_ptr := new t_slv_array(0 to 2)(15 downto 0);
    v_result.channel_value := (others => '0');
    v_result.data := x"00000000";
    -- The whole element, and a part of the one after it
    recv(v_result.channel_value(GC_CHANNEL_WIDTH - 1 downto 0), v_ptr.all, clk, ifc);
    assert v_result.channel_value = x"5a" report to_hstring(v_result.channel_value)
      severity failure;
    assert v_ptr(2) = x"1234" severity failure;
    recv(v_result.data(11 downto 4), v_ptr.all, clk, ifc);
    assert v_result.data = x"000005a0" report to_hstring(v_result.data) severity failure;
    expect(x"5a", clk, ifc);
    done <= true;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigcopyslice_pkg.all;

entity t_vhdl_sigcopyslice is
end entity;

architecture a of t_vhdl_sigcopyslice is
  signal clk : std_logic := '0';
  signal ifc : t_if(channel(7 downto 0), data(15 downto 0));
  signal done : boolean;
begin
  clk <= not clk after 5 ns;
  ifc.channel <= x"5a";
  ifc.data <= x"1234";
  u : entity work.t_vhdl_sigcopyslice_vvc
    generic map (GC_CHANNEL_WIDTH => 8)
    port map (clk => clk, ifc => ifc, done => done);
  process
  begin
    wait until done for 100 ns;
    assert done severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
