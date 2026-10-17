-- DESCRIPTION: Verilator: VHDL run-time check: a pulse rejection limit only running gives, greater than the delay (NVC delay3)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_reject_dyn_bad is
end entity;

architecture a of t_vhdl_reject_dyn_bad is
  signal x : bit;
begin
  process is
    variable v : time := 1 ns;
  begin
    x <= reject v inertial '1' after 2 ns;
    wait for 3 ns;
    assert x = '1' severity failure;
    report "limit fits";
    v := 5 ns;
    x <= reject v inertial '0' after 2 ns;
    wait for 3 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
