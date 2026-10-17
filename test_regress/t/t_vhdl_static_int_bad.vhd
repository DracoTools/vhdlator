-- DESCRIPTION: Verilator: VHDL run-time check: a static value INTEGER cannot hold, standing for an INTEGER, (NVC arith6)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_static_int_bad is
end entity;

architecture a of t_vhdl_static_int_bad is
begin
  process is
    variable i : integer := 7;
  begin
    assert i < 2 ** 30 severity failure;
    report "fits";
    wait for 1 ns;
    assert i < 2 ** 32;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
