-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Records whose elements do not all fit in one machine word, so the record
-- is a structure rather than a vector, and its aggregate is written member
-- by member (5.3.3, 9.3.3.2).

use std.env.all;

entity t_vhdl_record is
end entity;

architecture tb of t_vhdl_record is

  -- Packs: every element is a vector or an integer
  type flags is record
    a : bit;
    b : bit_vector(3 downto 0);
  end record;

  -- Does not pack: a real and an array of integers
  type sample is record
    n : integer;
    r : real;
    v : integer_vector(1 to 3);
  end record;

  signal f : flags := (a => '1', b => "1010");
  signal s : sample := (n => 1, r => 2.5, v => (4, 5, 6));
  constant c : sample := (7, 8.5, (1, 2, 3));

begin

  process
    variable t : sample;
  begin
    assert f.a = '1' report "f.a" severity failure;
    assert f.b = "1010" report "f.b" severity failure;

    assert s.n = 1 report "s.n" severity failure;
    assert s.r = 2.5 report "s.r" severity failure;
    assert s.v(2) = 5 report "s.v" severity failure;

    assert c.n = 7 report "c.n" severity failure;
    assert c.r = 8.5 report "c.r" severity failure;
    assert c.v(3) = 3 report "c.v" severity failure;

    -- A whole record copies, and its parts are then its own
    t := s;
    t.n := 9;
    assert t.n = 9 report "t.n" severity failure;
    assert t.v(1) = 4 report "t.v" severity failure;
    assert s.n = 1 report "s.n after copy" severity failure;

    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
