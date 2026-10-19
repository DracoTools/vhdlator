-- DESCRIPTION: Verilator: VHDL run-time check: READLINE past the end of a file (5.5.2; GHDL gna bug096)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

use std.textio.all;

entity t_vhdl_readline_eof_bad is
end entity;

architecture a of t_vhdl_readline_eof_bad is
begin
  process is
    file f : text;
    variable l : line;
    variable n : integer := 0;
  begin
    file_open(f, "t_vhdl_readline_eof_bad.tmp", write_mode);
    write(l, string'("one"));
    writeline(f, l);
    write(l, string'("two"));
    writeline(f, l);
    file_close(f);
    file_open(f, "t_vhdl_readline_eof_bad.tmp", read_mode);
    while not endfile(f) loop
      readline(f, l);
      n := n + 1;
    end loop;
    assert n = 2 severity failure;
    report "read " & integer'image(n) & " lines";
    readline(f, l);
    report "READLINE past the end should have stopped the run" severity failure;
    wait;
  end process;
end architecture;
