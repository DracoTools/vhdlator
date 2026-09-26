-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A bus interface record that a bus-functional procedure drives whole from
-- a function giving 'Z' for the elements the other side drives: UVVM's
-- "axistream_if <= init_axistream_if_signals(...)" (bitvis_vip_axistream/
-- src/axistream_bfm_pkg.vhd). The value a call computes carries no 'Z' in a
-- two-state runtime, so the procedure drove '0' over the ready the other
-- side drives, and a transfer through UVVM's test harness never completed.
-- The elements the call leaves high impedance are read from the function
-- with the call's own arguments, and driven 'Z'.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_recinitz_pkg is
  type t_if is record
    tdata : std_logic_vector;
    tvalid : std_logic;
    tready : std_logic;
  end record;
  function init_if(is_master : boolean; data_width : natural) return t_if;
  -- The same, written as an aggregate it gives back
  function init_if_agg(is_master : boolean) return t_if;
  procedure transmit(constant data : std_logic_vector; signal clk : in std_logic;
                     signal ifc : inout t_if);
end package;

package body t_vhdl_recinitz_pkg is
  function init_if(is_master : boolean; data_width : natural) return t_if is
    variable v : t_if(tdata(data_width - 1 downto 0));
  begin
    if is_master then
      v.tready := 'Z';
      v.tvalid := '0';
      v.tdata := (v.tdata'range => '0');
    else
      v.tready := '0';
      v.tvalid := 'Z';
      v.tdata := (v.tdata'range => 'Z');
    end if;
    return v;
  end function;

  function init_if_agg(is_master : boolean) return t_if is
  begin
    if is_master then
      return (tdata => "0000", tvalid => '0', tready => 'Z');
    end if;
    return (tdata => "ZZZZ", tvalid => 'Z', tready => '0');
  end function;

  procedure transmit(constant data : std_logic_vector; signal clk : in std_logic;
                     signal ifc : inout t_if) is
  begin
    ifc <= init_if(true, ifc.tdata'length);
    wait until rising_edge(clk);
    ifc.tdata <= data;
    ifc.tvalid <= '1';
    -- The other side says when it has taken it
    wait until rising_edge(clk) and ifc.tready = '1';
    ifc <= init_if(true, ifc.tdata'length);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;

-- Takes one word when it sees it valid, and says so on ready
entity t_vhdl_recinitz_sink is
  port (clk : in std_logic;
        valid : in std_logic;
        data : in std_logic_vector;
        ready : out std_logic;
        got : out std_logic_vector);
end entity;

architecture a of t_vhdl_recinitz_sink is
begin
  process (clk)
  begin
    if rising_edge(clk) then
      ready <= '0';
      if valid = '1' then
        got <= data;
        ready <= '1';
      end if;
    end if;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_recinitz_pkg.all;

entity t_vhdl_recinitz is
  generic (GC_DATA_WIDTH : natural := 8);
end entity;

architecture a of t_vhdl_recinitz is
  signal clk : std_logic := '0';
  signal ifc : t_if(tdata(GC_DATA_WIDTH - 1 downto 0));
  signal got : std_logic_vector(GC_DATA_WIDTH - 1 downto 0) := (others => '0');
  signal small : t_if(tdata(3 downto 0));
begin
  clk <= not clk after 5 ns;
  sink : entity work.t_vhdl_recinitz_sink
    port map (clk => clk, valid => ifc.tvalid, data => ifc.tdata, ready => ifc.tready,
              got => got);
  small.tready <= '1';

  process
  begin
    transmit(x"a5", clk, ifc);
    assert got = x"a5" report "sink got " & to_hstring(got) severity failure;
    transmit(x"3c", clk, ifc);
    assert got = x"3c" report "sink got " & to_hstring(got) severity failure;
    -- The aggregate form, with the other side's element driven elsewhere
    small <= init_if_agg(true);
    wait for 1 ns;
    assert small.tready = '1' and small.tvalid = '0' severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
