-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A READ and a WRITE a package declares over a LINE for a type of its own
-- are ordinary procedures with bodies. A procedure whose first argument is
-- a LINE was taken for one of std.textio's, and the call was refused as a
-- READ of a type textio has none for. OSVVM's RandomBasePkg declares these
-- for RandomSeedType, and its CoveragePkg calls them.
use std.textio.all;

package t_vhdl_userread_pkg is
  type seed_t is array (1 to 2) of integer;
  procedure read(variable l : inout line; a : out seed_t; good : out boolean);
  procedure read(variable l : inout line; a : out seed_t);
  procedure write(variable l : inout line; a : in seed_t);
end package;

package body t_vhdl_userread_pkg is
  procedure read(variable l : inout line; a : out seed_t; good : out boolean) is
    variable ok1, ok2 : boolean;
  begin
    read(l, a(1), ok1);
    read(l, a(2), ok2);
    good := ok1 and ok2;
  end procedure;

  procedure read(variable l : inout line; a : out seed_t) is
    variable good : boolean;
  begin
    read(l, a, good);
    assert good severity failure;
  end procedure;

  procedure write(variable l : inout line; a : in seed_t) is
  begin
    write(l, a(1));
    write(l, ' ');
    write(l, a(2));
  end procedure;
end package body;

use std.textio.all;
use work.t_vhdl_userread_pkg.all;

entity t_vhdl_userread is
end entity;

architecture a of t_vhdl_userread is
begin
  process
    variable l : line;
    variable s : seed_t;
    variable good : boolean;
  begin
    write(l, seed_t'(17, 42));
    assert l.all = "17 42" severity failure;
    read(l, s, good);
    assert good and s = (17, 42) severity failure;
    write(l, string'("5 6"));
    read(l, s);
    assert s = (5, 6) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
