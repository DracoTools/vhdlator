-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- std.textio: building a line and writing it out. The expected output is
-- byte for byte what GHDL prints for the same source.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use std.env.all;

entity t_vhdl_textio is
end entity;

architecture sim of t_vhdl_textio is
begin
  main : process is
    variable l : line;
    variable n : integer := 42;
    variable r : real := 2.5;
    variable v : std_logic_vector(7 downto 0) := "10110001";
    variable b : bit_vector(3 downto 0) := "1010";
  begin
    write(l, string'("count="));
    write(l, n);
    write(l, string'(" bits="));
    write(l, v);
    writeline(output, l);

    write(l, string'("hex="));
    hwrite(l, v);
    write(l, string'(" oct="));
    owrite(l, b);
    writeline(output, l);

    write(l, string'("real="));
    write(l, r);
    writeline(output, l);

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;
end architecture;
