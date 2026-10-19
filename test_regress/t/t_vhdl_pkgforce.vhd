-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A procedure of a package forces a signal of a package and releases it
-- (10.5.2.1). Released, the signal takes what its drivers give again -- here
-- none, so its initial value -- rather than keeping the forced one (GHDL gna
-- issue2471).

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_pkgforce_sig is
  signal glob : std_logic := '1';
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_pkgforce_sig.all;

package t_vhdl_pkgforce_proc is
  procedure force_and_release;
end package;

package body t_vhdl_pkgforce_proc is
  procedure force_and_release is
  begin
    glob <= force '0';
    wait for 0 ns;
    assert glob = '0' report "not forced" severity failure;
    glob <= release;
    wait for 0 ns;
    assert glob = '1' report "not released" severity failure;
  end procedure;
end package body;

use work.t_vhdl_pkgforce_proc.all;

entity t_vhdl_pkgforce is
end entity;

architecture behav of t_vhdl_pkgforce is
begin
  process begin
    force_and_release;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
