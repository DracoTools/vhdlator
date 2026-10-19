-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An element of a constant array of one element whose value folds, written as
-- a bit string or as an aggregate (GHDL gna issue1387).

entity t_vhdl_onebitconst is
end entity;

architecture behav of t_vhdl_onebitconst is
  constant width : integer := 1;
  constant zeros : bit_vector(0 to width - 1) := (0 to width - 1 => '0');
  constant one : bit_vector(0 to 0) := "1";
  signal s, t : bit;
begin
  s <= zeros(0);
  t <= one(0);
  process begin
    wait for 1 ns;
    assert s = '0' and t = '1' severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
