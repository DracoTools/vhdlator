-- DESCRIPTION: Verilator: VHDL run-time check: a type conversion's result outside its type mark (9.3.6; GHDL gna issue618, ticket31)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_convrange_bad is
end entity;

architecture a of t_vhdl_convrange_bad is
  type t is range 0 to 1;
begin
  process is
    variable m : integer := 1;
    variable r : real := 0.25;
    variable n : integer;
  begin
    n := integer(t(m));
    n := natural(r - 0.5);
    assert n = 0 severity failure;
    r := -1.5;
    n := natural(r);
    report "natural(-1.5) should have stopped the run" severity failure;
    wait;
  end process;
end architecture;
