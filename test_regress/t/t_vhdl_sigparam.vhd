-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal-class parameter names the caller's own signal, not a copy of it
-- (4.2.2.3), so an assignment inside the subprogram drives that signal.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_sigparam is
end entity;

architecture tb of t_vhdl_sigparam is

  signal a, b : integer := 0;
  signal v    : std_logic_vector(3 downto 0) := "0000";

  procedure drive (signal s : out integer; value : in integer) is
  begin
    s <= value;
  end procedure;

  procedure bump (signal s : inout integer) is
  begin
    s <= s + 1;
  end procedure;

  procedure setbits (signal s : out std_logic_vector(3 downto 0)) is
  begin
    s <= "1010";
  end procedure;

begin

  process
  begin
    -- Two calls of one procedure, each driving a different signal
    drive(a, 7);
    drive(b, 9);
    setbits(v);
    wait for 1 ns;
    assert a = 7 report "a = " & integer'image(a) severity failure;
    assert b = 9 report "b = " & integer'image(b) severity failure;
    assert v = "1010" report "v" severity failure;

    bump(a);
    wait for 1 ns;
    assert a = 8 report "a after bump = " & integer'image(a) severity failure;
    assert b = 9 report "b changed" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
