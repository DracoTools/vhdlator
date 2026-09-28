-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- FILE_OPEN given a mode that is a parameter, known only when the call runs
-- (16.3). It was opened for reading whatever the mode said: UVVM's
-- functional coverage writes its reports with the mode its caller passes,
-- and stopped with "cannot open file ... for reading".

use std.textio.all;

entity t_vhdl_fileopenmode is
end entity;

architecture a of t_vhdl_fileopenmode is
  procedure put(constant name : in string; constant m : in file_open_kind;
                constant txt : in string) is
    file f : text;
    variable l : line;
  begin
    file_open(f, name, m);
    write(l, txt);
    writeline(f, l);
    file_close(f);
  end procedure;
begin
  process
    file f : text;
    variable l : line;
    variable n : natural := 0;
  begin
    put("t_vhdl_fileopenmode.tmp", write_mode, "one");
    put("t_vhdl_fileopenmode.tmp", append_mode, "two");
    put("t_vhdl_fileopenmode.tmp", append_mode, "three");
    file_open(f, "t_vhdl_fileopenmode.tmp", read_mode);
    while not endfile(f) loop
      readline(f, l);
      n := n + 1;
      if n = 1 then
        assert l.all = "one" report "line 1: " & l.all severity failure;
      elsif n = 3 then
        assert l.all = "three" report "line 3: " & l.all severity failure;
      end if;
    end loop;
    file_close(f);
    assert n = 3 report "lines: " & integer'image(n) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
