-- DESCRIPTION: Verilator: VHDL run-time check: an unresolved signal with two sources (NVC driver9)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_unres_srcs_bad is
end entity;

architecture a of t_vhdl_unres_srcs_bad is
  signal x : integer;
begin
  x <= 1;
  x <= 2;

  process is
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
