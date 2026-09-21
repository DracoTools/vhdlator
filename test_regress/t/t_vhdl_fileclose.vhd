-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A file object is closed when its scope is left (5.5.2), and a
-- subprogram's scope is left when it returns. Without that, what one
-- procedure wrote is still held when the next opens the same name to read
-- it, and the reader finds an empty file.

use std.textio.all;
use std.env.all;

entity t_vhdl_fileclose is
end entity;

architecture tb of t_vhdl_fileclose is
begin

  main: process is

    procedure writes is
      file f : text open write_mode is "t_vhdl_fileclose.tmp";
      variable l : line;
    begin
      for k in 1 to 5 loop
        write(l, k);
        writeline(f, l);
      end loop;
    end procedure;

    -- A function leaves by a return statement rather than by running off the
    -- end, and its file is closed there too
    impure function counts return integer is
      file f : text open read_mode is "t_vhdl_fileclose.tmp";
      variable l : line;
      variable n : integer := 0;
    begin
      while not endfile(f) loop
        readline(f, l);
        n := n + 1;
      end loop;
      return n;
    end function;

  begin
    writes;
    assert counts = 5
      report "read back " & integer'image(counts) & " lines of five" severity failure;
    -- Called again: the function's own file was closed by its return, so
    -- opening it a second time starts at the beginning again
    assert counts = 5
      report "the second read gave " & integer'image(counts) severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
