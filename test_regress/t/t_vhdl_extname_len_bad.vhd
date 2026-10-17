-- DESCRIPTION: Verilator: VHDL run-time check: an external name whose subtype is longer than the object (NVC ename5)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_extname_len_bad is
end entity;

architecture a of t_vhdl_extname_len_bad is
begin
  b1 : block is
    signal s1 : bit_vector(1 to 3);
    constant c1 : string := "hello";
  begin
    s1 <= "101";
  end block;

  process is
  begin
    wait for 1 ns;
    assert << signal b1.s1 : bit_vector >> = "101" severity failure;
    assert << constant b1.c1 : string(1 to 6) >> = "hello";
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
