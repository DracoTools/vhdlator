-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant declared inside a subprogram from what the call supplies has
-- a value only running knows, however constant it is once it has one
-- (4.3.1.1) -- so an object as long as it says is one whose length only
-- running decides.  VUnit's id_pkg builds its lineage exactly this way.

package lenlocal_pkg is
  type it_t is record
    a : integer;
  end record;
  type iv_t is array (integer range <>) of it_t;
  constant fixed_len : positive := 3;   -- and this one is not one of those

  function make(n : natural) return iv_t;
  function total(n : natural) return integer;
end package;

package body lenlocal_pkg is
  function make(n : natural) return iv_t is
    constant len : positive := n + 1;
    variable r : iv_t(1 to len);
  begin
    for i in 1 to len loop
      r(i).a := i;
    end loop;
    return r;
  end;

  function total(n : natural) return integer is
    constant got : iv_t := make(n);
    variable s : integer := 0;
  begin
    -- A constant of the package says a length at analysis, and must go on
    -- doing so: this array is an ordinary one
    for i in 1 to fixed_len loop
      s := s + i;
    end loop;
    for i in got'range loop
      s := s + got(i).a;
    end loop;
    return s;
  end;
end package body;

use work.lenlocal_pkg.all;

entity t_vhdl_lenlocal is
end entity;

architecture a of t_vhdl_lenlocal is
begin
  process
    variable n : natural := 4;
  begin
    assert total(n) = 21 report "wrong total" severity failure;
    n := 0;
    assert total(n) = 7 report "wrong total for one element" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
