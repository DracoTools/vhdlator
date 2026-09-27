-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- TEXTIO's WRITE of a BOOLEAN writes its literal (16.4), "true", not its
-- position. UVVM's functional coverage writes its
-- database this way and reads it back with READ of a BOOLEAN, which wants
-- the word: written "1", the flag read back false, and every bin loaded
-- from the file was taken for a new one (uvvm_util/src/func_cov_pkg.vhd,
-- write_coverage_db and load_coverage_db).
use std.textio.all;

entity t_vhdl_writebool is
end entity;

architecture a of t_vhdl_writebool is
begin
  process
    variable l : line;
    variable flag : boolean := true;
    variable b : bit := '1';
    variable back : boolean;
    variable ok : boolean;
  begin
    write(l, flag);
    write(l, ' ');
    write(l, not flag);
    write(l, ' ');
    write(l, b);
    assert l.all = "TRUE FALSE 1" or l.all = "true false 1"
      report "wrote '" & l.all & "'" severity failure;
    -- And what was written reads back
    read(l, back, ok);
    assert ok and back severity failure;
    read(l, back, ok);
    assert ok and not back severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
