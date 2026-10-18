-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- numeric_std's TO_UNSIGNED and TO_SIGNED warn "vector truncated" where the
-- integer does not fit the length (NVC ieee11), and not where it does: a
-- negative value that fits a SIGNED was taken for one that does not

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_tounsigned_trunc is
end entity;

architecture t of t_vhdl_tounsigned_trunc is
begin
  process
    variable five : integer := 5;
    variable one : integer := 1;
    variable u : unsigned(7 downto 0);
  begin
    assert to_unsigned(five, 8) = X"05" severity failure;      -- fits
    assert to_unsigned(five, 2) = "01" severity failure;       -- truncated (1)
    assert to_signed(five, 8) = X"05" severity failure;        -- fits
    assert to_signed(-five, 8) = "11111011" severity failure;  -- fits
    assert to_signed(-five, 2) = "11" severity failure;        -- truncated (2)
    assert to_signed(one, 1) = "1" severity failure;           -- truncated (3)
    assert to_signed(-one, 1) = "1" severity failure;          -- fits
    u := X"10";
    assert u + five = X"15" severity failure;                  -- fits
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
