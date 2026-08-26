-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal-class parameter names the signal the call passed, not a value of
-- it (4.2.2.3), so inside the subprogram its history attributes answer about
-- that signal: 'EVENT is true only where the signal has just changed, and
-- 'LAST_VALUE is what it held before. That is only knowable at a call site,
-- so the body is compiled once per call rather than once for the design --
-- which is also what lets rising_edge be written out rather than recognised.

use std.env.all;

entity t_vhdl_sigparamattr is
end entity;

architecture tb of t_vhdl_sigparamattr is

  function had_event (signal x : bit_vector) return boolean is
  begin
    return x'event;
  end function;

  function was (signal x : bit_vector) return bit_vector is
  begin
    return x'last_value;
  end function;

  function rose (signal c : bit) return boolean is
  begin
    return c'event and c = '1';
  end function;

  procedure note (signal x : bit_vector; result : out boolean) is
  begin
    result := x'event;
  end procedure;

  signal s : bit_vector(1 to 3) := "000";
  signal t : bit_vector(1 to 3) := "111";
  signal c : bit := '0';

begin

  s <= "101" after 1 ns, "011" after 2 ns;
  c <= '1' after 1 ns, '0' after 3 ns;

  main : process is
    variable b : boolean;
  begin
    wait for 1 ns;
    -- Both signals changed at this instant, but only s and c have a history
    -- of their own; t was never assigned
    assert had_event(s) report "s should have an event" severity failure;
    assert not had_event(t) report "t should not" severity failure;
    assert was(s) = "000" report "s was " & to_string(was(s)) severity failure;
    assert rose(c) report "c should have risen" severity failure;
    note(s, b);
    assert b report "procedure form" severity failure;

    wait for 1 ns;
    assert had_event(s) report "s should have an event at 2 ns" severity failure;
    assert was(s) = "101" report "s was " & to_string(was(s)) severity failure;
    assert not rose(c) report "c did not rise at 2 ns" severity failure;

    wait for 2 ns;
    assert not had_event(s) report "s is quiet at 4 ns" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
