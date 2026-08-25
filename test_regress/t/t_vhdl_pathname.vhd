-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'INSTANCE_NAME and 'PATH_NAME name where a declaration is (16.2.7): every
-- region from the design unit down, with the library ahead of a package, the
-- architecture written beside its entity in the instance form, and a
-- subprogram named with its parameter and result type profile.

package t_vhdl_pathname_pkg is
  function f return integer;
  function g (a : bit; b : integer) return boolean;
  procedure h (x : inout integer);
end package;

package body t_vhdl_pathname_pkg is
  function f return integer is
  begin
    assert f'instance_name = ":work:t_vhdl_pathname_pkg:f[ return integer]:"
      report "function instance_name is " & f'instance_name severity failure;
    assert f'path_name = f'instance_name
      report "a package path is the same either way" severity failure;
    return 0;
  end function;

  function g (a : bit; b : integer) return boolean is
  begin
    assert g'instance_name
             = ":work:t_vhdl_pathname_pkg:g[bit,integer return boolean]:"
      report "signature is " & g'instance_name severity failure;
    return a = '1' and b = 2;
  end function;

  procedure h (x : inout integer) is
  begin
    assert h'instance_name = ":work:t_vhdl_pathname_pkg:h[integer]:"
      report "a procedure has no result in its profile: " & h'instance_name
      severity failure;
    x := x + 1;
  end procedure;
end package body;

use std.env.all;
use work.t_vhdl_pathname_pkg.all;

entity t_vhdl_pathname is
end entity;

architecture tb of t_vhdl_pathname is
  signal t : integer := 0;
begin

  lbl : block is
    signal b : integer := 0;
  begin
    -- A block is a region of its own, and its label names it
    process is
    begin
      assert b'instance_name = ":t_vhdl_pathname(tb):lbl:b"
        report "block instance_name is " & b'instance_name severity failure;
      assert b'path_name = ":t_vhdl_pathname:lbl:b"
        report "block path_name is " & b'path_name severity failure;
      wait;
    end process;
  end block;

  named : process is
    variable v : integer := 0;
  begin
    -- A labelled process is named; an object is written without a trailing
    -- colon, unlike a subprogram
    assert v'instance_name = ":t_vhdl_pathname(tb):named:v"
      report "process instance_name is " & v'instance_name severity failure;
    wait;
  end process;

  process is
    variable v : integer := 0;
    variable ok : boolean;
  begin
    -- An unlabelled process contributes an empty region name
    assert v'path_name = ":t_vhdl_pathname::v"
      report "unlabelled process path_name is " & v'path_name severity failure;
    assert t'instance_name = ":t_vhdl_pathname(tb):t"
      report "signal instance_name is " & t'instance_name severity failure;
    assert t'simple_name = "t" report "simple_name" severity failure;

    assert f = 0 report "f" severity failure;
    ok := g('1', 2);
    assert ok report "g" severity failure;
    h(v);
    assert v = 1 report "h" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
