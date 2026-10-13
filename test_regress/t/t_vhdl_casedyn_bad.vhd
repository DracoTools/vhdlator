-- DESCRIPTION: Verilator: VHDL case expression of another length than its choices
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- The expression's length is only known while running; one other than the
-- choices' is an error then (10.9)

entity t_vhdl_casedyn_bad is
end entity;

architecture a of t_vhdl_casedyn_bad is
  function get_bits(n : natural; b : bit) return bit_vector is
    variable v : bit_vector(1 to n);
  begin
    v := (others => b);
    return v;
  end function;
begin
  process is
    variable n : natural := 5;
  begin
    report "before";
    case get_bits(n, '1') is
      when "111" => report "ones";
      when others => report "others";
    end case;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
