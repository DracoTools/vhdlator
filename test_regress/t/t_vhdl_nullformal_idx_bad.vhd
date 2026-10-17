-- DESCRIPTION: Verilator: VHDL run-time check: a constant index into a parameter whose actual has a null range (NVC bounds27)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_nullformal_idx_bad is
end entity;

architecture a of t_vhdl_nullformal_idx_bad is
  procedure proc (x : out bit_vector) is
    variable a : bit_vector(x'range);
  begin
    x(1) := '1';
  end procedure;
begin
  process is
    variable v : bit_vector(1 to 3);
    variable n : bit_vector(1 to 0);
  begin
    proc(v);
    assert v = "100" severity failure;
    report "one fits";
    proc(n);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
