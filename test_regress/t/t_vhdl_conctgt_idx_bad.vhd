-- DESCRIPTION: Verilator: VHDL run-time check: a concurrent assignment to an element whose index is outside the target (NVC bounds3)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_conctgt_idx_bad is
end entity;

architecture a of t_vhdl_conctgt_idx_bad is
  type int_vec is array (natural range <>) of integer;
  signal s : int_vec(5 to 7);
  signal k : integer := 6;
begin
  s(k) <= 61;

  process is
  begin
    wait for 1 ns;
    assert s(6) = 61 severity failure;
    report "in range";
    k <= 8;
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
