-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal two processes drive, each through a procedure that passes its
-- signal parameter on to another. The drive the inner one makes is still
-- the calling process's (4.2.2.3, 14.7.2), as VUnit's notify is when
-- entry_gate calls it (vunit/vhdl/run/src/run.vhd). The inner copy named
-- the signal itself, so the second process's 'Z' replaced the first's '1'.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_nesteddrv_pkg is
  procedure put(signal s : inout std_logic_vector; v : std_logic);
  procedure outer(signal s : inout std_logic_vector; v : std_logic);
end package;

package body t_vhdl_nesteddrv_pkg is
  procedure put(signal s : inout std_logic_vector; v : std_logic) is
  begin
    s(s'left) <= v;
  end procedure;

  procedure outer(signal s : inout std_logic_vector; v : std_logic) is
  begin
    put(s, v);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nesteddrv_pkg.all;

entity t_vhdl_nesteddrv is
end entity;

architecture a of t_vhdl_nesteddrv is
  signal s : std_logic_vector(0 to 1) := "ZZ";
begin
  p1 : process
  begin
    outer(s, '1');
    wait;
  end process;

  p2 : process
  begin
    wait for 2 ns;
    outer(s, 'Z');
    wait for 1 ns;
    assert s(0) = '1' report "s = " & to_string(s) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
