-- DESCRIPTION: Verilator: VHDL bound checks on what built-in procedures write
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.math_real.all;
use std.textio.all;

entity t_vhdl_boundcheck_outarg_bad is
  generic (OUTDIR : string := ".");
end entity;

architecture a of t_vhdl_boundcheck_outarg_bad is
begin
  process is
    subtype word is std_logic_vector(0 to 7);
    type word_file is file of word;
    type word_arr is array (0 to 3) of word;
    type int_arr is array (0 to 3) of integer;
    type real_arr is array (0 to 3) of real;
    type str_arr is array (0 to 3) of string(1 to 3);
    type line_arr is array (0 to 3) of line;
    variable wa : word_arr;
    variable ia : int_arr := (others => 0);
    variable ra : real_arr := (others => 0.0);
    variable ta : str_arr := (others => "...");
    variable la : line_arr;
    variable s1, s2 : positive := 7;
    variable l : line;
    variable ok : boolean;
    variable idx : natural := 0;
    variable n : natural;
    file wf : word_file;
    file tf : text;
  begin
    -- An element at a run-time index filled by an implicit file READ
    file_open(wf, OUTDIR & "/words.dat", write_mode);
    write(wf, word'(X"0F"));
    write(wf, word'(X"A5"));
    file_close(wf);
    file_open(wf, OUTDIR & "/words.dat", read_mode);
    while not endfile(wf) loop
      read(wf, wa(idx));
      idx := idx + 1;
    end loop;
    file_close(wf);
    assert idx = 2 and wa(0) = X"0F" and wa(1) = X"A5" severity failure;
    -- ... by READLINE, textio's READ, HREAD and SREAD, and UNIFORM
    idx := 2;
    file_open(tf, OUTDIR & "/lines.txt", write_mode);
    write(l, string'("hello"));
    writeline(tf, l);
    file_close(tf);
    file_open(tf, OUTDIR & "/lines.txt", read_mode);
    readline(tf, la(idx));
    file_close(tf);
    assert la(2).all = "hello" severity failure;
    l := new string'("42 A5 abc");
    read(l, ia(idx), ok);
    hread(l, wa(idx));
    sread(l, ta(idx), n);
    assert ia(2) = 42 and ok severity failure;
    assert wa(2) = X"A5" severity failure;
    assert ta(2) = "abc" and n = 3 severity failure;
    uniform(s1, s2, ra(idx));
    assert ra(2) > 0.0 and ra(2) < 1.0 severity failure;
    report "before";
    idx := 9;
    l := new string'("1");
    read(l, ia(idx));
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
