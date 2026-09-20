-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A file declaration names no status, so a file that will not open is an
-- error and the run ends there (5.5.2). Read as an empty file instead, a
-- testbench reads nothing and reports whatever it started with.

use std.textio.all;
use std.env.all;

entity t_vhdl_fileopen_bad is
end entity;

architecture tb of t_vhdl_fileopen_bad is
begin
  main: process is
    file inf : text open read_mode is "t_vhdl_fileopen_bad_no_such_file.txt";
    variable l : line;
  begin
    readline(inf, l);
    report "the missing file was read" severity failure;
    stop(0);
  end process;
end architecture;
