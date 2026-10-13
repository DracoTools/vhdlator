-- DESCRIPTION: Verilator: VHDL function returning a value outside its return subtype
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_rangechk_ret_bad is
end entity;

architecture a of t_vhdl_rangechk_ret_bad is
  function pos(x : integer) return natural is
  begin
    return x;
  end function;
begin
  process is
    variable n : natural := 0;
  begin
    n := pos(3);
    report "three fits";
    n := pos(-3);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
