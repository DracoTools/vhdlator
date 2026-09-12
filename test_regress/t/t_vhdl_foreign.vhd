-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Subprograms implemented in C, bound with the FOREIGN attribute (16.2).
-- Every spelling of the string that a simulator writes appears: GHDL's
-- VHPIDIRECT, with and without the library name it wants, NVC's VHPI, and the
-- DPI-C form that shares one C model with a SystemVerilog testbench. A
-- subprogram written in VHDL as well as bound to C uses the VHDL it was
-- given.

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

  -- The spelling NVC writes, whose library name is ignored the same way
  function c_quad (v : integer) return integer;
  attribute foreign of c_quad : function is "VHPI work c_quad";

  -- The same C function a SystemVerilog testbench would import
  function c_add (a : integer; b : integer) return integer;
  attribute foreign of c_add : function is "DPI-C c_add";

  -- A procedure, which becomes a void C function; its out parameter comes
  -- back through a pointer, exactly as it would for DPI
  procedure c_divmod (num : in integer; den : in integer;
                      q : out integer; r : out integer);
  attribute foreign of c_divmod : procedure is "VHPIDIRECT c_divmod";

  -- Written in VHDL as well, which is what the other simulators run when
  -- they cannot load the C model; the VHDL wins here too
  function c_negate (v : integer) return integer;
  attribute foreign of c_negate : function is "VHPIDIRECT c_negate";

end package;

package body foreign_pkg is

  function c_negate (v : integer) return integer is
  begin
    return -v;
  end function;

end package body;

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

    assert c_quad(6) = 24 report "quad(6)" severity failure;

    assert c_negate(9) = -9 report "negate(9)" severity failure;

    assert c_add(20, 22) = 42 report "add(20,22)" severity failure;
    assert c_add(-5, 5) = 0 report "add(-5,5)" severity failure;

    c_divmod(17, 5, q, r);
    assert q = 3 report "divmod quotient" severity failure;
    assert r = 2 report "divmod remainder" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
