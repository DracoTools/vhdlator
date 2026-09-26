-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A Verilog module instantiated from VHDL and given character literals:
-- "generic map (INV => '1') port map (x => '0', ...)", as UVVM's I2C test
-- harness writes for the core it wraps (bitvis_vip_i2c/tb/maintenance_tb/
-- i2c_master_dut.vhd). No VHDL declaration says what the formals are, so
-- '1' was ambiguous between BIT, CHARACTER and STD_ULOGIC and refused. A
-- Verilog port or parameter is logic, and the literal is a STD_ULOGIC one.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_litverilog is
end entity;

architecture a of t_vhdl_litverilog is
  signal y1, y2 : std_logic;
begin
  u1 : entity work.t_vhdl_litverilog_sub
    generic map (INV => '1')
    port map (x => '0', en => '1', y => y1);
  u2 : entity work.t_vhdl_litverilog_sub
    port map (x => '1', en => '1', y => y2);

  process
  begin
    wait for 1 ns;
    assert y1 = '1' and y2 = '1' report std_logic'image(y1) & std_logic'image(y2)
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
