-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A call may cover one formal with several associations, each naming a part
-- of it: "p(v.a => x, v.b => y)" and "p(v(1) => a, v(2) => b)" (6.5.7.1).
-- The parameter is one object however the call was written, so one is made
-- for it: the parts that are read are filled before the call, and the parts
-- the body writes are copied back to their actuals after. Written out as
-- one argument per association, a call covering three parameters with four
-- associations named the same argument twice.

use std.env.all;

entity t_vhdl_elemassoc is
end entity;

architecture tb of t_vhdl_elemassoc is
  type pair is record
                 a : integer;
                 b : integer;
               end record;
  type duo is array (1 to 2) of integer;
begin

  main: process is

    -- A record formal, read and written
    procedure swap_rec (p : inout pair; was : in pair) is
    begin
      assert p = was report "the record arrived as ("
        & integer'image(p.a) & ", " & integer'image(p.b) & ")" severity failure;
      p := (p.b, p.a);
    end procedure;

    -- An array formal, likewise
    procedure swap_arr (p : inout duo; was : in duo) is
    begin
      assert p = was report "the array arrived as ("
        & integer'image(p(1)) & ", " & integer'image(p(2)) & ")" severity failure;
      p := (p(2), p(1));
    end procedure;

    variable x : integer := 1;
    variable y : integer := 2;
  begin
    -- The elements of the formal are crossed over, so what the body sees is
    -- (y, x) and what comes back goes to the other one of the two
    swap_rec(p.a => y, p.b => x, was => (2, 1));
    assert y = 1 report "y is " & integer'image(y) severity failure;
    assert x = 2 report "x is " & integer'image(x) severity failure;

    x := 1;
    y := 2;
    swap_arr(p(1) => y, p(2) => x, was => (2, 1));
    assert y = 1 report "array: y is " & integer'image(y) severity failure;
    assert x = 2 report "array: x is " & integer'image(x) severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
