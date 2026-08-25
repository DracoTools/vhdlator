-- DESCRIPTION: Verilator Example: VHDL calling C, and C calling VHDL
--
-- This file ONLY is placed under the Creative Commons Public Domain
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

package foreign_pkg is

  -- VHDL calling C. The string is the one GHDL uses, so source written for
  -- that simulator needs no change. "DPI-C c_add" is also accepted, and says
  -- that this C function is shared with a SystemVerilog testbench.
  function c_popcount (v : integer) return integer;
  attribute foreign of c_popcount : function is "VHPIDIRECT c_popcount";

  function c_add (a : integer; b : integer) return integer;
  attribute foreign of c_add : function is "DPI-C c_add";

  -- A procedure becomes a void C function, its out parameter a pointer
  procedure c_divmod (num : in integer; den : in integer;
                      q : out integer; r : out integer);
  attribute foreign of c_divmod : procedure is "VHPIDIRECT c_divmod";

end package;

use work.foreign_pkg.all;
use std.env.all;

entity top is
end entity;

architecture rtl of top is
begin

  -- C calling VHDL. Nothing in the source says so: the control file names
  -- this subprogram, which keeps the VHDL portable to other simulators.
  process is
    variable q, r : integer;
  begin
    report "popcount(255) = " & integer'image(c_popcount(255));
    assert c_popcount(255) = 8 severity failure;

    report "add(20, 22) = " & integer'image(c_add(20, 22));
    assert c_add(20, 22) = 42 severity failure;

    c_divmod(17, 5, q, r);
    report "divmod(17, 5) = " & integer'image(q) & " remainder "
      & integer'image(r);
    assert q = 3 and r = 2 severity failure;

    report "-- VHDL and C are talking. --";
    stop;
    wait;
  end process;

end architecture;
