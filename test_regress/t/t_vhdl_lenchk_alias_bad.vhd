-- DESCRIPTION: Verilator: VHDL alias of another length than its object, in a copy
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- The alias is three long; in the copy of get made for a two-element actual
-- the object is two (NVC alias12)

entity t_vhdl_lenchk_alias_bad is
end entity;

architecture a of t_vhdl_lenchk_alias_bad is
  function get(x : in bit_vector; n : in natural range 1 to 3) return bit is
    alias a : bit_vector(1 to 3) is x;
  begin
    return a(n);
  end function;
begin
  process is
    variable v1 : bit_vector(4 to 6) := "101";
    variable v3 : bit_vector(2 to 3) := "01";
  begin
    assert get(v1, 2) = '0' severity failure;
    report "three fits";
    assert get(v3, 1) = '0' severity failure;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
