-- DESCRIPTION: Verilator: VHDL run-time check: T'VALUE of text that is no literal of T (NVC bounds12)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_value_text_bad is
end entity;

architecture a of t_vhdl_value_text_bad is
  type color is (red, green, blue);
  signal t : string(1 to 5) := "hello";
begin
  process is
  begin
    assert integer'value(" 42 ") = 42 severity failure;
    assert color'value("Green") = green severity failure;
    report "literals read";
    assert integer'value(t) = 5;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
