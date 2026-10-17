-- DESCRIPTION: Verilator: VHDL run-time check: a value of another length than the parameter a call gave (NVC bounds25)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_copy_len_bad is
end entity;

architecture a of t_vhdl_copy_len_bad is
  procedure fill(x : out bit_vector; b : bit) is
  begin
    x := (b, b, b, b);
  end procedure;
begin
  process is
    variable v4 : bit_vector(1 to 4);
    variable v2 : bit_vector(1 to 2);
  begin
    fill(v4, '1');
    assert v4 = "1111" severity failure;
    report "four fits";
    fill(v2, '1');
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
