-- DESCRIPTION: Verilator: VHDL run-time check: tO_INTEGER of a value INTEGER cannot hold (NVC ieee20)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_toint_range_bad is
end entity;

architecture a of t_vhdl_toint_range_bad is
begin
  process is
    variable x : signed(32 downto 0);
  begin
    x := to_signed(-5, 33);
    wait for 1 ns;
    assert to_integer(x) = -5 severity failure;
    report "minus five fits";
    x := (32 => '1', others => '0');
    wait for 1 ns;
    assert to_integer(x) = 0;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
