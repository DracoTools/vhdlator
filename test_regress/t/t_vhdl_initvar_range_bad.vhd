-- DESCRIPTION: Verilator: VHDL run-time check: a subprogram's variable given an initial value outside its subtype (NVC bounds16)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_initvar_range_bad is
end entity;

architecture a of t_vhdl_initvar_range_bad is
  function add_nat(x : in integer) return natural is
    variable r : natural := x;
  begin
    return r + 1;
  end function;
begin
  process is
    variable x : integer;
  begin
    x := 5;
    report integer'image(add_nat(x));
    x := 0;
    report integer'image(add_nat(x));
    x := -1;
    report integer'image(add_nat(x));
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
