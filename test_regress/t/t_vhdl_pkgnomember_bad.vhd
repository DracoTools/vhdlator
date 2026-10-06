-- DESCRIPTION: Verilator: VHDL name selected from a package that does not declare it
--
-- A package declares what it declares, and nothing else is reached through
-- its name (4.7, 8.3). "math_real.uniform" of a math_real with no uniform
-- (VESTS ashenden ch_19_random-b) was let through to the lowering, which
-- could only say that it could not call it.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_pkgnomember_pkg is
  procedure q (x : out integer);
  constant c : integer := 3;
end package;

package body t_vhdl_pkgnomember_pkg is
  procedure q (x : out integer) is
  begin
    x := c;
  end procedure;
end package body;

use work.t_vhdl_pkgnomember_pkg;

entity t_vhdl_pkgnomember_bad is
end entity;

architecture a of t_vhdl_pkgnomember_bad is
begin
  process
    variable v : integer;
  begin
    t_vhdl_pkgnomember_pkg.q(v);
    v := t_vhdl_pkgnomember_pkg.c;
    t_vhdl_pkgnomember_pkg.nosuch(v);
    v := t_vhdl_pkgnomember_pkg.nosuchf(1);
    work.t_vhdl_pkgnomember_pkg.nosuch2(v);
    wait;
  end process;
end architecture;
