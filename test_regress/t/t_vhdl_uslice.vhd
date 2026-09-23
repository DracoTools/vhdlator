-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice whose ends move but keep a fixed distance apart is an ordinary
-- part select where the elements are bits. Where they are not -- an array
-- of arrays, say -- there is no bit range to cut, so the slice is taken
-- element by element instead.
--
-- What matters is the order: the elements come out in the slice's own order,
-- leftmost first, whichever way round the object and the target are
-- numbered (8.5).

entity t_vhdl_uslice is
end entity;

architecture rtl of t_vhdl_uslice is
  type pair is array (1 to 2) of integer;
  type pairs is array (integer range <>) of pair;
  constant arr : pairs (1 to 8) := ((11, 12), (21, 22), (31, 32), (41, 42),
                                    (51, 52), (61, 62), (71, 72), (81, 82));
  -- The same values, numbered the other way round
  constant dn : pairs (8 downto 1) := ((11, 12), (21, 22), (31, 32), (41, 42),
                                       (51, 52), (61, 62), (71, 72), (81, 82));
begin
  check : process
    variable a : pairs (3 downto 0);
    variable b : pairs (1 to 4);
  begin
    for i in 1 to 2 loop
      -- An ascending slice into a descending target: leftmost to leftmost
      a := arr((i - 1) * 4 + 1 to i * 4);
      assert a(3) = arr((i - 1) * 4 + 1) report "bad leftmost of an ascending slice"
        severity failure;
      assert a(0) = arr(i * 4) report "bad rightmost of an ascending slice" severity failure;
      -- And into an ascending target
      b := arr((i - 1) * 4 + 1 to i * 4);
      assert b(1) = arr((i - 1) * 4 + 1) report "bad leftmost into an ascending target"
        severity failure;
      assert b(4) = arr(i * 4) report "bad rightmost into an ascending target" severity failure;
      -- A descending slice of a descending object
      a := dn(i * 4 downto (i - 1) * 4 + 1);
      assert a(3) = dn(i * 4) report "bad leftmost of a descending slice" severity failure;
      assert a(0) = dn((i - 1) * 4 + 1) report "bad rightmost of a descending slice"
        severity failure;
    end loop;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
