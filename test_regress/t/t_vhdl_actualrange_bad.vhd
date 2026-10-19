-- DESCRIPTION: Verilator: VHDL run-time check: an actual outside the subtype of its formal (4.2.2.1; GHDL gna deb585748)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_actualrange_bad is
end entity;

architecture a of t_vhdl_actualrange_bad is
  signal i_s : integer := 3;
  signal v_s : unsigned(7 downto 0);
  function twice (n : natural) return integer is
  begin
    return 2 * n;
  end function;
begin
  v_s <= to_unsigned(i_s, 8);
  process is
    variable r : integer;
  begin
    r := twice(i_s);
    assert r = 6 severity failure;
    i_s <= -1;
    wait for 1 ns;
    report "TO_UNSIGNED of -1 should have stopped the run" severity failure;
    wait;
  end process;
end architecture;
