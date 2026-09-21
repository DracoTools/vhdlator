-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A concurrent selected signal assignment schedules the waveform its
-- selector picks, and that waveform carries its own delay (11.6, 10.5.2.2).
-- The simple and conditional forms already did; the selected one folded its
-- alternatives into a single value and dropped every "after" with them, so
-- each one arrived at once.

use std.env.all;

entity t_vhdl_seldelay is
end entity;

architecture tb of t_vhdl_seldelay is
  subtype word is string(1 to 4);
  constant text : string(1 to 8) := "12345678";

  signal sel : bit := '1';
  signal one : bit := '0';
  signal many : integer := 0;
  -- A selector that is a qualified expression over a slice, which is what
  -- makes the choice a string rather than the slice's own subtype
  signal qual : bit := '0';
begin

  with sel select
    one <= '1' after 10 ns when '1',
           '0' after 10 ns when others;

  -- Several elements in the chosen waveform, each at its own time
  with sel select
    many <= 1 after 10 ns, 2 after 20 ns, 3 after 30 ns when '1',
            0 after 10 ns when others;

  with word'(text(1 to 4)) select
    qual <= '1' after 10 ns when "1234",
            '0' after 10 ns when others;

  main: process is
  begin
    wait for 1 ns;
    assert one = '0' report "the chosen waveform arrived at once" severity failure;
    assert qual = '0' report "the qualified selector arrived at once" severity failure;
    assert many = 0 report "the first element arrived at once" severity failure;

    wait for 10 ns;  -- 11 ns
    assert one = '1' report "the chosen waveform never arrived" severity failure;
    assert qual = '1' report "the qualified selector chose OTHERS" severity failure;
    assert many = 1 report "the first element is " & integer'image(many) severity failure;

    wait for 10 ns;  -- 21 ns
    assert many = 2 report "the second element is " & integer'image(many) severity failure;
    wait for 10 ns;  -- 31 ns
    assert many = 3 report "the third element is " & integer'image(many) severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
