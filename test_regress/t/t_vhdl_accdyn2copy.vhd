-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- UVVM's Avalon-ST BFM (bitvis_vip_avalon_st/src/avalon_st_bfm_pkg.vhd):
-- an array of vectors whose element length a constant of the procedure
-- gives, "v_normalized_data : t_slv_array(0 to data_array'length -
-- 1)(C_DATA_WORD_SIZE - 1 downto 0)", copied whole into what an access to
-- an array of std_logic_vector points at, "v_symbol_array.all :=
-- v_normalized_data". The object pointed at holds a carrier per element,
-- and the value was copied as a carrier of its own subtype, whose element
-- length read the procedure's constant from a structure the whole design
-- shares. The procedure is called by one of the testbench's own, with
-- arrays of words of eight bits and of a generic's width
-- (tb/maintenance_tb/avalon_st_bfm_tb.vhd), and the second was refused.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_accdyn2copy_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_slv_array_ptr is access t_slv_array;
  procedure tx(constant data_array : in t_slv_array; constant msg : in string := "";
               signal clk : in std_logic; signal o : inout std_logic_vector);
end package;

package body t_vhdl_accdyn2copy_pkg is
  procedure tx(constant data_array : in t_slv_array; constant msg : in string := "";
               signal clk : in std_logic; signal o : inout std_logic_vector) is
    constant C_DATA_WORD_SIZE : natural := data_array(data_array'low)'length;
    variable v_normalized_data : t_slv_array(0 to data_array'length - 1)(C_DATA_WORD_SIZE - 1 downto 0)
      := data_array;
    variable v_symbol_array : t_slv_array_ptr;
  begin
    v_symbol_array := new t_slv_array(0 to v_normalized_data'length - 1)(C_DATA_WORD_SIZE - 1 downto 0);
    v_symbol_array.all := v_normalized_data;
    assert v_symbol_array'length = data_array'length severity failure;
    for i in v_symbol_array'range loop
      wait until rising_edge(clk);
      o <= (o'range => '0');
      o(C_DATA_WORD_SIZE - 1 downto 0) <= v_symbol_array(i);
    end loop;
    wait until rising_edge(clk);
    deallocate(v_symbol_array);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_accdyn2copy_pkg.all;

entity t_vhdl_accdyn2copy is
  generic (GC_DATA_WIDTH : natural := 16);
end entity;

architecture a of t_vhdl_accdyn2copy is
  signal clk : std_logic := '0';
  signal o : std_logic_vector(15 downto 0) := (others => '0');
begin
  clk <= not clk after 5 ns;

  process
    variable v_data_packet : t_slv_array(0 to 3)(7 downto 0) := (x"01", x"02", x"03", x"04");
    variable v_data_stream : t_slv_array(0 to 1)(GC_DATA_WIDTH - 1 downto 0) := (x"1234", x"5678");
    procedure transmit(data_array : in t_slv_array) is
    begin
      tx(data_array, "t", clk, o);
    end procedure;
  begin
    transmit(v_data_packet);
    assert o = x"0004" report to_hstring(o) severity failure;
    transmit(v_data_stream);
    assert o = x"5678" report to_hstring(o) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
