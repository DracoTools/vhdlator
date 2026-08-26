-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A file declaration may name the file to open (6.4.2.5), and then the file
-- is opened when the declaration is elaborated rather than by a call. Where
-- that happens matters: a file of an architecture or a process is opened once
-- before the design runs, a file of a subprogram on every call. Without an
-- open kind the file is opened for reading.

use std.textio.all;
use std.env.all;

entity t_vhdl_fileopen is
  -- Where to put the scratch file, given on the command line
  generic (OUTDIR : string := ".");
end entity;

architecture tb of t_vhdl_fileopen is

  -- Opened where it is declared, for writing
  constant SCRATCH : string := OUTDIR & "/t_vhdl_fileopen.tmp";

  file outf : text open write_mode is SCRATCH;

  -- Opened on every call, for reading, which is the default
  procedure check_it (expect_s : in string; expect_n : in integer) is
    file inf : text is SCRATCH;
    variable l : line;
    variable s : string(1 to 5);
    variable n : integer;
  begin
    readline(inf, l);
    read(l, s);
    assert s = expect_s report "line 1 is " & s severity failure;
    readline(inf, l);
    read(l, n);
    assert n = expect_n report "line 2 is " & integer'image(n) severity failure;
    assert endfile(inf) report "more than two lines" severity failure;
  end procedure;

begin

  main : process is
    variable l : line;
  begin
    write(l, string'("hello"));
    writeline(outf, l);
    write(l, 42);
    writeline(outf, l);
    file_close(outf);

    -- Called twice: each call opens the file again from the start, so the
    -- second call must read the same two lines as the first
    check_it("hello", 42);
    check_it("hello", 42);

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
