-- DESCRIPTION: Verilator: VHDL projected waveform, assignments inside procedures
--
-- A procedure the process declares, and one given the signal for a signal formal, schedule through the process's own driver: preemption between their transactions and the process's own.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_wavesub_pkg is
  procedure pulse(signal o : out std_logic_vector; v1, v2 : std_logic_vector);
end package;
package body t_vhdl_wavesub_pkg is
  procedure pulse(signal o : out std_logic_vector; v1, v2 : std_logic_vector) is
  begin
    -- The second, earlier, preempts the first
    o <= v1 after 10 ns;
    o <= v2 after 5 ns;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_wavesub_pkg.all;
entity t_vhdl_wavesub is end entity;
architecture a of t_vhdl_wavesub is
  signal s : std_logic_vector(3 downto 0) := "0000";
  signal t : std_logic_vector(3 downto 0) := "0000";
  signal u : std_logic := '0';
begin
  process
    procedure local(v : std_logic) is
    begin
      u <= v after 10 ns;
      u <= not v after 4 ns;
    end procedure;
  begin
    pulse(s, "1111", "0101");
    local('1');
    wait for 20 ns;
    assert s = "0101" report "1: " & to_string(s) severity failure;
    assert u = '0' report "2" severity failure;
    -- The procedure's transaction and the process's own are one driver's
    pulse(t, "0011", "0001");
    t <= "1000" after 2 ns;
    wait for 20 ns;
    assert t = "1000" report "3: " & to_string(t) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
