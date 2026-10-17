-- DESCRIPTION: Verilator: VHDL run-time check: T'VALUE of a value outside the subtype T (NVC bounds13)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_value_range_bad is
end entity;

architecture a of t_vhdl_value_range_bad is
  signal t : string(1 to 1) := "4";
begin
  process is
    type myint is range 1 to 3;
  begin
    assert myint'value("  3  ") = 3 severity failure;
    report "three fits";
    assert myint'value(t) = 1;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
