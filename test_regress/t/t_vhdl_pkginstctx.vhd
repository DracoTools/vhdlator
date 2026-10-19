-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A copy of a generic package sees what the package's own context clause
-- made visible, not the use clauses of the unit that instantiates it (4.9,
-- 12.4): to_string(x) in the generic package is STD.STANDARD's, though the
-- instantiating package uses one that takes to_string(integer, base) too, in
-- a function and in a protected type's method (GHDL gna issue317: OSVVM's
-- ScoreboardGenericPkg instantiated beside PoC's strings package). An actual
-- is still read where the instantiation is written: log2(N) is the
-- instantiating unit's log2, not MATH_REAL's, which the package uses.

library ieee;
use ieee.math_real.all;
package t_vhdl_pkginstctx_gp is
  generic (type T; W : positive := 1);
  constant WW : positive := W;
  function f(x : integer) return string;
  type pt is protected
    impure function g(x : integer) return string;
  end protected;
end package;

package body t_vhdl_pkginstctx_gp is
  function f(x : integer) return string is
  begin
    return to_string(x);
  end function;
  type pt is protected body
    impure function g(x : integer) return string is
    begin
      return to_string(x);
    end function;
  end protected body;
end package body;

package t_vhdl_pkginstctx_strs is
  function to_string(v : integer; base : positive := 10) return string;
  function log2(n : positive) return natural;
end package;

package body t_vhdl_pkginstctx_strs is
  function to_string(v : integer; base : positive := 10) return string is
  begin
    return "x";
  end function;
  function log2(n : positive) return natural is
    variable r : natural := 0;
    variable v : natural := 1;
  begin
    while v < n loop
      v := v * 2;
      r := r + 1;
    end loop;
    return r;
  end function;
end package body;

use work.t_vhdl_pkginstctx_strs.all;
package t_vhdl_pkginstctx_user is
  package inst is new work.t_vhdl_pkginstctx_gp generic map (T => bit);
  -- Here both are visible, and the one with the second parameter is named
  constant c : string := to_string(7, 2);
end package;

use work.t_vhdl_pkginstctx_strs.all;
use work.t_vhdl_pkginstctx_user.all;
entity t_vhdl_pkginstctx is
  generic (N : positive := 16);
end entity;

architecture a of t_vhdl_pkginstctx is
  package inst2 is new work.t_vhdl_pkginstctx_gp generic map (T => bit, W => log2(N));
begin
  process
    variable v : inst.pt;
  begin
    assert inst.f(5) = "5" report "inst.f(5) /= 5" severity failure;
    assert v.g(6) = "6" report "v.g(6) /= 6" severity failure;
    assert c = "x" report "c /= x" severity failure;
    assert inst2.WW = 4 report "inst2.WW /= 4" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
