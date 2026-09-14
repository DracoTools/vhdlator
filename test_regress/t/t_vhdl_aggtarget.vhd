-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An aggregate as an assignment target, where the run each name stands for
-- is as long as a generic says rather than a number written here (10.6.2).
-- The value itself often says nothing about its own length -- below it is a
-- conversion of a sum -- so the length is what the names add up to.

use std.env.all;

entity t_vhdl_aggtarget is
  generic (W : positive := 4);
end entity;

architecture tb of t_vhdl_aggtarget is
begin

  main : process is
    variable hi, lo : bit_vector(W - 1 downto 0);
    variable both   : bit_vector(2 * W - 1 downto 0) := "10110001";
    variable one    : bit;
    variable rest   : bit_vector(2 * W - 2 downto 0);
  begin
    -- Two halves, each as long as the generic says
    (hi, lo) := both;
    assert hi = "1011" report "hi " & to_string(hi) severity failure;
    assert lo = "0001" report "lo " & to_string(lo) severity failure;

    -- A single element beside a run: the first name takes one bit
    (one, rest) := both;
    assert one = '1' report "one" severity failure;
    assert rest = "0110001" report "rest " & to_string(rest) severity failure;

    -- The other way round, so the run comes first
    (rest, one) := both;
    assert rest = "1011000" report "rest2 " & to_string(rest) severity failure;
    assert one = '1' report "one2" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
