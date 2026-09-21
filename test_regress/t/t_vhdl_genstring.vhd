-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A STRING is held as text rather than as a vector of characters, so one
-- declared with a constraint has to start out that many characters long or
-- everything it says about itself is the empty string's answer (6.4.2.2).
-- That was done where the bounds were numbers by the time the declaration
-- was read, and not where they name a generic -- which elaboration settles
-- just as it settles the range itself.

use std.env.all;

entity t_vhdl_genstring is
  generic (lo : integer := 1; hi : integer := 7);
end entity;

architecture tb of t_vhdl_genstring is
  constant clo : integer := 1;
  constant chi : integer := 7;

  type rec_gen is record
                    s : string(lo to hi);
                    v : bit_vector(0 to 3);
                  end record;
  type rec_con is record
                    s : string(clo to chi);
                  end record;

  signal g : rec_gen;
  signal c : rec_con;
  signal plain : string(lo to hi);
begin

  main: process is
  begin
    -- A record element whose bounds name a generic
    assert g.s'left = 1 report "left is " & integer'image(g.s'left) severity failure;
    assert g.s'right = 7 report "right is " & integer'image(g.s'right) severity failure;
    assert g.s'length = 7 report "length is " & integer'image(g.s'length) severity failure;
    -- ... and one written with constants, which always worked
    assert c.s'right = 7 report "the constant-bounded element is " & integer'image(c.s'right)
      severity failure;
    -- ... and an object of the same type, which also always worked
    assert plain'length = 7 report "the plain signal is " & integer'image(plain'length)
      severity failure;
    -- The element beside it is unaffected
    assert g.v'length = 4 report "the vector beside it is " & integer'image(g.v'length)
      severity failure;

    -- It is that long because every character of it is there to be written
    g.s <= "abcdefg";
    wait for 1 ns;
    assert g.s = "abcdefg" report "the string did not keep what was written to it"
      severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;
