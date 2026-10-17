-- DESCRIPTION: Verilator: VHDL run-time check: an element of a REAL aggregate that is no REAL (NVC real7)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_realagg_bad is
end entity;

architecture a of t_vhdl_realagg_bad is
begin
  process is
    variable r1 : real := 2.0;
    variable a1 : real_vector(0 to 1);
  begin
    a1 := (r1 * r1, 0.5);
    assert a1(0) = 4.0 severity failure;
    report "four";
    r1 := real'left;
    a1 := (r1 * r1, 0.0);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
