-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal a package declares, driven by processes of several modules
-- through procedures' signal parameters: UVVM's barrier ("global_barrier :
-- std_logic := 'X'", await_barrier in uvvm_util/src/methods_pkg.vhd) and
-- VUnit's events ("runner", notify in vunit/vhdl/data_types/src/
-- event_common_pkg.vhd). Every process that drives the signal is a driver
-- of it from the start, holding the signal's default, and the signal is
-- what the drivers resolve to. Here the drivers were one variable every
-- process wrote: the first party to reach the barrier released itself at
-- once, and a 'Z' written by one process wiped out the '1' another held.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_pkgsigdrv_pkg is
  signal barrier : std_logic := 'X';
  signal events : std_logic_vector(0 to 3) := "ZZZZ";
  alias ev_hi is events(2 to 3);
  procedure await_barrier(signal b : inout std_logic; constant timeout : in time;
                          variable released : out time);
  procedure hold(signal e : inout std_logic_vector; constant idx : in natural);
  procedure hold_outer(signal e : inout std_logic_vector; constant idx : in natural);
end package;

package body t_vhdl_pkgsigdrv_pkg is
  procedure await_barrier(signal b : inout std_logic; constant timeout : in time;
                          variable released : out time) is
  begin
    b <= '0';
    wait until b = '0' for timeout;
    released := now;
    b <= '1';
  end procedure;

  -- One element high for ten nanoseconds, then let go
  procedure hold(signal e : inout std_logic_vector; constant idx : in natural) is
  begin
    e(idx) <= '1';
    wait for 10 ns;
    e(idx) <= 'Z';
  end procedure;

  procedure hold_outer(signal e : inout std_logic_vector; constant idx : in natural) is
  begin
    hold(e, idx);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgsigdrv_pkg.all;

entity t_vhdl_pkgsigdrv_party is
  generic (arrive : time; idx : natural);
end entity;

architecture a of t_vhdl_pkgsigdrv_party is
begin
  process
    variable released : time;
  begin
    wait for arrive;
    await_barrier(barrier, 100 ns, released);
    -- Released together, when the last party arrives
    assert released = 50 ns report "released at " & time'image(released) severity failure;
    wait for 10 ns;
    hold_outer(events, idx);
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgsigdrv_pkg.all;

entity t_vhdl_pkgsigdrv is
end entity;

architecture a of t_vhdl_pkgsigdrv is
begin
  early : entity work.t_vhdl_pkgsigdrv_party generic map (arrive => 10 ns, idx => 0);
  late : entity work.t_vhdl_pkgsigdrv_party generic map (arrive => 50 ns, idx => 1);

  process
  begin
    wait for 30 ns;
    -- One party waits at the barrier, the other has not arrived
    assert barrier /= '0' severity failure;
    wait for 25 ns;
    assert barrier = '1' severity failure;
    -- Both parties hold their element from 60 ns to 70 ns; this process
    -- letting go of the first is not the other's driver letting go of it
    wait for 10 ns;
    events(0) <= 'Z';
    wait for 2 ns;
    assert events(0) = '1' and events(1) = '1' severity failure;
    wait for 5 ns;
    assert events(0) /= '1' and events(1) /= '1' severity failure;
    -- Through an alias of part of it
    ev_hi(3) <= '1';
    wait for 1 ns;
    assert events(3) = '1' and events(2) /= '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
