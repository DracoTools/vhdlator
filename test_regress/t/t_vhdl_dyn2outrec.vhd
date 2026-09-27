-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A procedure writing an array of vectors through a parameter of mode out
-- that leaves both lengths open, given a variable whose element is as long
-- as a signal formal's record element and whose count only running
-- settles: UVVM's axi_check hands axi_read "variable v_rdata_value :
-- t_slv_array(0 to n)(axi_if.read_data_channel.rdata'length - 1 downto 0)"
-- (bitvis_vip_axi/src/axi_bfm_pkg.vhd). The variable was made a value held
-- two levels deep for the call, and nothing copied it back: every word
-- read came back 0.
library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_dyn2outrec_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_if is record
    rdata : std_logic_vector;
    valid : std_logic;
  end record;
  procedure rd(signal c : in std_logic; signal ifc : in t_if; variable r : out t_slv_array);
  procedure chk(n : natural; signal c : in std_logic; signal ifc : in t_if);
end package;
package body t_vhdl_dyn2outrec_pkg is
  procedure rd(signal c : in std_logic; signal ifc : in t_if; variable r : out t_slv_array) is
  begin
    for i in r'range loop
      wait until rising_edge(c);
      r(i) := ifc.rdata;
    end loop;
  end procedure;
  procedure chk(n : natural; signal c : in std_logic; signal ifc : in t_if) is
    variable v : t_slv_array(0 to n)(ifc.rdata'length - 1 downto 0);
  begin
    rd(c, ifc, r => v);
    report "v0 " & to_hstring(v(0)) & " v1 " & to_hstring(v(1));
    assert v(0) = x"12345678" and v(1) = x"12345678" severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_dyn2outrec_pkg.all;
entity t_vhdl_dyn2outrec is
end entity;
architecture a of t_vhdl_dyn2outrec is
  signal clk : std_logic := '0';
  signal ifc : t_if(rdata(31 downto 0)) := (rdata => x"12345678", valid => '0');
begin
  clk <= not clk after 5 ns;
  process
    variable k : natural := 1;
    variable big : t_slv_array(0 to 3)(31 downto 0);
  begin
    rd(clk, ifc, r => big);
    assert big(0) = x"12345678" and big(3) = x"12345678" severity failure;
    chk(k, clk, ifc);
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
