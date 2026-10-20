-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal resolved by a function of the design's own, driven by two
-- processes through a procedure's signal formal: each process that calls it
-- has a driver of its own, which starts at the signal's initial value
-- (14.7.2). OSVVM's barriers count the drivers that are not zero, and
-- WaitForBarrier drives the barrier through its formal: both drivers counted
-- as one released the first process at once.

package t_vhdl_ownrescall_pkg is
  function rb(s : integer_vector) return integer;
  subtype bt is rb integer range integer'low + 1 to integer'high;
  procedure wfb(signal sig : inout bt);
end package;

package body t_vhdl_ownrescall_pkg is
  function rb(s : integer_vector) return integer is
    variable r : integer := 0;
  begin
    for i in s'range loop
      if s(i) /= 0 then
        r := r + 1;
      end if;
    end loop;
    return r;
  end function;
  procedure wfb(signal sig : inout bt) is
  begin
    sig <= 0;
    wait until sig = 0;
    sig <= 1;
    wait for 0 ns;
  end procedure;
end package body;

use work.t_vhdl_ownrescall_pkg.all;

entity t_vhdl_ownrescall is
end entity;

architecture a of t_vhdl_ownrescall is
  signal done : bt := 1;
  signal t1, t2 : time := 0 ns;
begin
  p1 : process
  begin
    wait for 10 ns;
    assert done = 2 report "two drivers at 1" severity failure;
    wfb(done);
    t1 <= now;
    wait;
  end process;
  p2 : process
  begin
    wait for 100 ns;
    wfb(done);
    t2 <= now;
    wait for 1 ns;
    assert t1 = 100 ns report "p1 released at " & time'image(t1) severity failure;
    assert t2 = 100 ns report "p2 released at " & time'image(t2) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
