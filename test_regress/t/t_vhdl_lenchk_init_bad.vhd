-- DESCRIPTION: Verilator: VHDL initial value of another length than its object
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- The value folds while elaborating, and the error is the declaration's
-- (NVC bounds17)

entity t_vhdl_lenchk_init_bad is
end entity;

architecture a of t_vhdl_lenchk_init_bad is
  function get_bits(n : natural) return bit_vector is
  begin
    return (n - 1 downto 0 => '0');
  end function;
begin
  process is
    variable ok : bit_vector(7 downto 0) := get_bits(8);
    variable x : bit_vector(7 downto 0) := get_bits(12);
  begin
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
