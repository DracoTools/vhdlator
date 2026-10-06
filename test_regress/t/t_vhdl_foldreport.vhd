-- DESCRIPTION: Verilator: VHDL call that reports, in a statement that runs
--
-- "assert work.pkg.f >= 2" calls a pure function of constants that reports
-- before it returns: the report is output the run produces, so the call is
-- made when the statement runs rather than worked out as the design is
-- built (GHDL gna issue321, bug17127). Where such a call sizes a type it is
-- still worked out, its value being needed there.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_foldreport_pkg is
  function f return natural;
  function g(x : natural) return natural;
end package;

package body t_vhdl_foldreport_pkg is
  constant c : natural := 5;
  function f return natural is
  begin
    report "f reads " & integer'image(c);
    return c;
  end function;
  function g(x : natural) return natural is
  begin
    assert x < 10 report "g given too much" severity note;
    return x + 1;
  end function;
end package body;

use work.t_vhdl_foldreport_pkg.all;

entity t_vhdl_foldreport is
end entity;

architecture behav of t_vhdl_foldreport is
  -- Worked out as the design is built: the type needs its size
  type t is array (1 to g(3)) of bit;
begin
  process is
    variable v : t;
  begin
    assert v'length = 4 severity failure;
    assert f >= 2 severity failure;
    assert g(12) = 13 severity failure;
    -- One that holds and says nothing
    assert g(2) = 3 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
