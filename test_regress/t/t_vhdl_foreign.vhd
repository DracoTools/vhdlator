-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Subprograms implemented in C, bound with the FOREIGN attribute (16.2).
-- Both accepted spellings of the string appear: GHDL's VHPIDIRECT, with and
-- without the library name it wants, and the DPI-C form that shares one C
-- model with a SystemVerilog testbench.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

package foreign_pkg is

  -- The GHDL spelling, which source written for that simulator already uses
  function c_popcount (v : integer) return integer;
  attribute foreign of c_popcount : function is "VHPIDIRECT c_popcount";

  -- GHDL also allows a library name, which has nothing to name here because
  -- Verilator links statically, and is ignored
  function c_triple (v : integer) return integer;
  attribute foreign of c_triple : function is "VHPIDIRECT libfake.so c_triple";

  -- The same C function a SystemVerilog testbench would import
  function c_add (a : integer; b : integer) return integer;
  attribute foreign of c_add : function is "DPI-C c_add";

  -- A procedure, which becomes a void C function; its out parameter comes
  -- back through a pointer, exactly as it would for DPI
  procedure c_divmod (num : in integer; den : in integer;
                      q : out integer; r : out integer);
  attribute foreign of c_divmod : procedure is "VHPIDIRECT c_divmod";

end package;

entity t_vhdl_foreign is
end entity;

use work.foreign_pkg.all;
use std.env.all;

architecture tb of t_vhdl_foreign is
begin

  process is
    variable q, r : integer;
  begin
    assert c_popcount(0) = 0 report "popcount(0)" severity failure;
    assert c_popcount(255) = 8 report "popcount(255)" severity failure;
    assert c_popcount(1024) = 1 report "popcount(1024)" severity failure;

    assert c_triple(7) = 21 report "triple(7)" severity failure;

    assert c_add(20, 22) = 42 report "add(20,22)" severity failure;
    assert c_add(-5, 5) = 0 report "add(-5,5)" severity failure;

    c_divmod(17, 5, q, r);
    assert q = 3 report "divmod quotient" severity failure;
    assert r = 2 report "divmod remainder" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
