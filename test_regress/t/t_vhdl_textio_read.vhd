-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- std.textio: reading items back off a line. Successive READs walk along one
-- line, each taking the leftmost item and leaving the rest.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use std.env.all;

entity t_vhdl_textio_read is
end entity;

architecture sim of t_vhdl_textio_read is
begin
  main : process is
    file     fh   : text;
    variable l    : line;
    variable i    : integer;
    variable j    : integer;
    variable k    : integer;
    variable r    : real;
    variable v    : std_logic_vector(7 downto 0);
    variable h    : std_logic_vector(7 downto 0);
    variable good : boolean;
  begin
    -- Write a file, then read it back, so the test needs no input of its own
    file_open(fh, "obj_vlt/t_vhdl_textio_read/data.dat", write_mode);
    write(l, string'("42 -7 100"));
    writeline(fh, l);
    write(l, string'("10100101 b1 2.5"));
    writeline(fh, l);
    file_close(fh);

    file_open(fh, "obj_vlt/t_vhdl_textio_read/data.dat", read_mode);

    readline(fh, l);
    read(l, i);
    read(l, j);
    read(l, k);
    assert i = 42 report "first item" severity failure;
    assert j = -7 report "second item" severity failure;
    assert k = 100 report "third item" severity failure;

    readline(fh, l);
    read(l, v);
    hread(l, h);
    read(l, r);
    assert v = "10100101" report "vector item" severity failure;
    assert h = x"b1" report "hex item" severity failure;
    assert r > 2.4 and r < 2.6 report "real item" severity failure;

    -- Nothing is left, and the GOOD form says so rather than failing
    read(l, i, good);
    assert not good report "read past the end of a line reported good" severity failure;

    file_close(fh);

    -- 'IMAGE of an enumeration is the literal's own name, not its position
    assert boolean'image(true) = "true" report "boolean image" severity failure;
    assert boolean'image(false) = "false" report "boolean image" severity failure;
    assert severity_level'image(warning) = "warning" report "enum image" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;
end architecture;
