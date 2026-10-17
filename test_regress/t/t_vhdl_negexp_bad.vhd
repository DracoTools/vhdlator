-- DESCRIPTION: Verilator: VHDL run-time check: an INTEGER raised to a negative power (NVC bounds30)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_negexp_bad is
end entity;

architecture a of t_vhdl_negexp_bad is
  signal s : integer := 3;
begin
  process is
  begin
    assert 2 ** s = 8 severity failure;
    report "eight";
    s <= -1;
    wait for 1 ns;
    assert 2 ** s = 0;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
