-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Two arrays are compared element by element in index order, and where every
-- element they share is equal the shorter of the two is the lesser (5.2.6).
-- That holds when the expression is folded as well as when it runs: folding
-- read a composite value as an integer, which is zero whatever it holds, so
-- any two constant arrays came out equal and neither was less than the other.

use std.env.all;

entity t_vhdl_constcmp is
end entity;

architecture tb of t_vhdl_constcmp is
  type arr is array (1 to 4) of integer;
  type rec is record
                a : integer;
                b : integer;
              end record;
  type chr is array (1 to 4) of character;

  constant klo : arr := (1, 1, 1, 1);
  constant khi : arr := (8, 1, 1, 8);
  constant ksame : arr := (1, 1, 1, 1);
  constant r1 : rec := (a => 1, b => 2);
  constant r2 : rec := (a => 1, b => 3);
  constant clo : chr := ('a', 'a', 'a', 'a');
  constant chi : chr := ('z', 'a', 'a', 'z');

  signal slo : arr := (1, 1, 1, 1);
  signal shi : arr := (8, 1, 1, 8);
begin

  main: process is
    variable vlo : arr := (1, 1, 1, 1);
    variable vhi : arr := (8, 1, 1, 8);
  begin
    -- Folded
    assert klo < khi report "a folded array was not the lesser" severity failure;
    assert khi > klo report "a folded array was not the greater" severity failure;
    assert klo /= khi report "two different folded arrays compared equal" severity failure;
    assert klo = ksame report "two equal folded arrays compared unequal" severity failure;
    assert klo <= ksame report "an equal folded array was not <=" severity failure;
    assert r1 /= r2 report "two different folded records compared equal" severity failure;
    assert clo < chi report "a folded character array was not the lesser" severity failure;

    -- ... and the same questions of things that only running answers
    assert vlo < vhi report "a variable array was not the lesser" severity failure;
    assert vlo /= vhi report "two different variable arrays compared equal" severity failure;
    wait for 0 ns;
    assert slo < shi report "a signal array was not the lesser" severity failure;
    assert slo /= shi report "two different signal arrays compared equal" severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
