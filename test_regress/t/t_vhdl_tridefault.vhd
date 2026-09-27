-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The default of an inout port of a resolved type is a driver, not an
-- initial value: a port nothing in the instance drives contributes it to
-- the net (14.7.3.2), and a process that drives the port through a
-- procedure has a driver that starts at it (14.7.2). UVVM's UART
-- transmitter VVC declares its line "inout std_logic :=
-- GC_UART_CONFIG.idle_state" and checks the line is idle before sending.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_tridefault_sub is
  generic (IDLE : std_logic := '1');
  port (
    p : inout std_logic := '1';
    q : inout std_logic := IDLE
  );
end entity;

architecture a of t_vhdl_tridefault_sub is
  procedure drive(signal s : inout std_logic; constant v : in std_logic) is
  begin
    s <= v;
  end procedure;
begin
  process
  begin
    wait for 5 ns;
    assert p = '1' report "p undriven reads " & std_logic'image(p) severity failure;
    assert q = '1' report "q before the call reads " & std_logic'image(q) severity failure;
    drive(q, '0');
    wait for 1 ns;
    assert q = '0' report "q after the call reads " & std_logic'image(q) severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_tridefault is
end entity;

architecture a of t_vhdl_tridefault is
  signal s, t : std_logic;
begin
  i_sub : entity work.t_vhdl_tridefault_sub
    generic map (IDLE => '1')
    port map (p => s, q => t);

  process
  begin
    wait for 1 ns;
    assert s = '1' report "s reads " & std_logic'image(s) severity failure;
    assert t = '1' report "t reads " & std_logic'image(t) severity failure;
    wait for 10 ns;
    assert s = '1' report "s later reads " & std_logic'image(s) severity failure;
    assert t = '0' report "t later reads " & std_logic'image(t) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
