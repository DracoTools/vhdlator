-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'PATH_NAME and 'INSTANCE_NAME under VHDL-93, the LRM's own example (NVC attr13): a
-- subprogram's place is its name alone, with no signature (that came with VHDL-2002); a
-- package of library lib named "lib.p" is that package; and "b1.s" is the s block b1
-- declares, however many inner blocks declare an s of their own.

package t_vhdl_pathname93_p is
  procedure proc (f : inout integer);
end package;

package body t_vhdl_pathname93_p is
  procedure proc (f : inout integer) is
    variable x : integer;
  begin
    assert proc'path_name = ":lib:t_vhdl_pathname93_p:proc:" report proc'path_name
      severity failure;
    assert x'path_name = ":lib:t_vhdl_pathname93_p:proc:x" report x'path_name
      severity failure;
    f := 1;
  end procedure;
end package body;

library lib;
use lib.t_vhdl_pathname93_p.all;

entity t_vhdl_pathname93 is
end entity;

architecture a of t_vhdl_pathname93 is
begin
  b1 : block
    signal s : integer;
  begin
    b2 : block
      signal s : integer;
    begin
      process is
        variable t : integer := 0;
      begin
        assert lib.t_vhdl_pathname93_p'path_name = ":lib:t_vhdl_pathname93_p:"
          report lib.t_vhdl_pathname93_p'path_name severity failure;
        assert s'path_name = ":t_vhdl_pathname93:b1:b2:s" report s'path_name severity failure;
        assert b1.s'path_name = ":t_vhdl_pathname93:b1:s" report b1.s'path_name
          severity failure;
        proc(t);
        report "*-* All Finished *-*";
        wait;
      end process;
    end block;
  end block;
end architecture;
