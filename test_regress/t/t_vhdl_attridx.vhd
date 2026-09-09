-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A user-defined attribute holds an ordinary value, so where that value is an
-- array "obj'attr(i)" is one element of it rather than an argument to
-- anything: the parentheses index, they do not call.

package p is
  type three is range 1 to 3;
  type a1 is array (three) of integer;
  function af1 (g : integer) return a1;
end package;

package body p is
  function af1 (g : integer) return a1 is
  begin
    return (10 + g, 20 + g, 30 + g);
  end function;
end package body;

use work.p.all;

entity t is
  port ( signal pt : in bit );
  attribute at1 : a1;
  attribute at1 of pt : signal is af1(1);
end entity;

architecture rtl of t is
begin
  process
    variable v : integer;
  begin
    v := pt'at1(1);
    assert v = 11 report "first element wrong" severity failure;
    v := pt'at1(3);
    assert v = 31 report "third element wrong" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
