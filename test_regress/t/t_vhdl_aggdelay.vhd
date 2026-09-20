-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- One waveform written to an aggregate target reaches every name between
-- the parentheses at the same time, so the delay on it is the delay on all
-- of them (10.5.2.2). A block port associated with a slice of a signal is
-- another name for that part of it (6.5.7.1), with no delta in between.

use std.env.all;

entity t_vhdl_aggdelay is
end entity;

architecture tb of t_vhdl_aggdelay is
  subtype bv2 is bit_vector(0 to 1);
  subtype ch2 is string(1 to 2);
  signal s, t   : bit;
  signal c1, c2 : character;
  signal wide   : bit_vector(0 to 7) := B"1010_1100";
begin

  sliced: block
    port (v : out bit_vector(0 to 3) := "1010");
    port map (wide(0 to 3));
  begin
    v <= B"0101" after 10 ns;
  end block;

  main: process is
    variable bits : bv2 := B"11";
    variable text : ch2 := "ab";
    variable due  : time;
  begin
    -- The aggregate target keeps the waveform's delay
    (s, t) <= bits after 10 ns;
    due := now + 10 ns;
    wait on s, t;
    assert now = due report "the aggregate target lost its delay" severity failure;
    assert s = bits(0) and t = bits(1) report "wrong bits" severity failure;

    -- ... whatever the element type is
    (c1, c2) <= text after 10 ns;
    due := now + 10 ns;
    wait on c1, c2;
    assert now = due report "the character aggregate lost its delay" severity failure;
    assert c1 = text(1) and c2 = text(2) report "wrong characters" severity failure;

    -- The block drove a slice of "wide", and did so with no delta of its own
    assert wide = B"0101_1100" report "the block port did not reach the slice"
      severity failure;
    stop(0);
  end process;

  watch: process is
  begin
    wait for 10 ns;
    assert wide = B"0101_1100"
      report "the slice arrived a delta late" severity failure;
    wait;
  end process;

end architecture;
