-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- What a function elaboration runs to give a constant its value reports, it
-- reports once, as the design elaborates (14.4.2); a generic package as
-- written is not elaborated, an instance of it is. A concurrent assertion
-- reports what the calls in it report each time it runs (GHDL gna issue2353).

package t_vhdl_elabreport_gpkg is
  generic (type t);
  function get return natural;
end package;

package body t_vhdl_elabreport_gpkg is
  function get1 return natural is
  begin
    report "get1 called";
    return 123;
  end function;
  constant c : natural := get1;
  function get return natural is
  begin
    report "get called, return: " & natural'image(c);
    return c;
  end function;
end package body;

package t_vhdl_elabreport_pkg is
  function checked (n : natural) return natural;
  constant k : natural;
end package;

package body t_vhdl_elabreport_pkg is
  function checked (n : natural) return natural is
  begin
    assert n < 10 report "n is " & natural'image(n) severity warning;
    return n;
  end function;
  constant k : natural := checked(42);
end package body;

use work.t_vhdl_elabreport_pkg.all;

entity t_vhdl_elabreport is
end entity;

architecture a of t_vhdl_elabreport is
  package inst is new work.t_vhdl_elabreport_gpkg generic map (t => bit);
  constant quiet : natural := checked(3);
begin
  assert inst.get > 120 and inst.get < 130 severity failure;
  process is
  begin
    assert k = 42 and quiet = 3 severity failure;
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
