-- DESCRIPTION: Verilator: VHDL declaration in a subprogram given a value of
-- another length, only in the copy made for one call
--
-- The lengths are only known in the copy made for the call, so it is an
-- error once the declaration is elaborated (5.3.2.2, 14.6), not when the
-- design is built (NVC bounds45).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_declen_bad is
end entity;

architecture a of t_vhdl_declen_bad is
  type arr_t is array (natural range <>) of bit_vector(7 downto 0);
  function f(x : arr_t) return bit_vector is
    constant y : arr_t(0 to x'length * 2) := x;
  begin
    return y(0);
  end function;
begin
  process
    variable v : arr_t(0 to 3) := (others => x"00");
  begin
    report "before";
    report "got " & to_hstring(f(v(1 to 3)));
    wait;
  end process;
end architecture;
