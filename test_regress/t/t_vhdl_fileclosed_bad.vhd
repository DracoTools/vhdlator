-- DESCRIPTION: Verilator: VHDL read from a file after FILE_CLOSE
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Reading a file that is not open is an error (5.5.2): nvc reports "read
-- from closed file", GHDL and Questa stop as well

use std.textio.all;

entity t_vhdl_fileclosed_bad is
  generic (OUTDIR : string := ".");
end entity;

architecture a of t_vhdl_fileclosed_bad is
begin
  process is
    file f : text;
    variable l : line;
  begin
    file_open(f, OUTDIR & "/closed.txt", write_mode);
    write(l, string'("hello"));
    writeline(f, l);
    file_close(f);
    file_open(f, OUTDIR & "/closed.txt", read_mode);
    readline(f, l);
    assert l.all = "hello" severity failure;
    file_close(f);
    report "closed";
    readline(f, l);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
