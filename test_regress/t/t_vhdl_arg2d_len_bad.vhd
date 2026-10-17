-- DESCRIPTION: Verilator: VHDL run-time check: an actual of another length in a dimension of a two-dimensional formal (NVC bounds19)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_arg2d_len_bad is
end entity;

architecture a of t_vhdl_arg2d_len_bad is
  type bv2d is array (positive range <>, positive range <>) of bit;
  function func(x : bv2d(1 to 5, 1 to 3)) return bit is
  begin
    return x(1, 2) and x(5, 1);
  end function;
  procedure proc(x, y : positive) is
    variable v : bv2d(1 to x, 1 to y);
  begin
    assert func(v) = '0' severity failure;
  end procedure;
begin
  process is
  begin
    proc(5, 3);
    report "five by three fits";
    proc(5, 2);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
