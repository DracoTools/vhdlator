-- DESCRIPTION: Verilator: VHDL actual of another length than its formal, in a copy
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Each call of proc gets a copy in which v's length is a number; in the one
-- for proc(3) it is not func's formal's (NVC bounds18)

entity t_vhdl_lenchk_call_bad is
end entity;

architecture a of t_vhdl_lenchk_call_bad is
  function func(x : bit_vector(1 to 5)) return bit is
  begin
    return x(1) and x(5);
  end function;
  procedure proc(n : positive) is
    variable v : bit_vector(1 to n);
  begin
    assert func(v) = '0' severity failure;
  end procedure;
begin
  process is
  begin
    proc(5);
    report "five fits";
    proc(3);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
