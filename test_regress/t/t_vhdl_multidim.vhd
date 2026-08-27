-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity sub is
  generic (n : integer := 3);
end entity;

architecture rtl of sub is
  -- An index written as the name of a subtype whose bounds name a generic:
  -- the range is fixed once the instance is elaborated, not before
  subtype t_row is integer range 1 to n;
  type t_enum is (red, green);
  type t_grid is array (t_row, t_enum) of integer;
  signal grid : t_grid := (others => (others => 7));
begin
  check : process is
    variable v : t_grid;
  begin
    assert grid(1, red) = 7 report "grid init" severity failure;
    assert grid(n, green) = 7 report "grid init n" severity failure;
    v := (others => (11, 22));
    assert v(2, red) = 11 report "row value" severity failure;
    assert v(2, green) = 22 report "row value g" severity failure;
    report "sub ok";
    wait;
  end process;
end architecture;

-- ---------------------------------------------------------------------------

entity t_vhdl_multidim is
end entity;

architecture rtl of t_vhdl_multidim is
  type matrix_t is array (integer range <>, integer range <>) of integer;
  constant c : matrix_t(0 to 1, 0 to 1) := ((1, 2), (3, 4));
  type pair_t is array (1 to 2) of integer;
  type table_t is array (1 to 2) of pair_t;
begin
  u_sub : entity work.sub generic map (n => 4);

  main : process is
    variable m : matrix_t(1 to 2, 1 to 3);
  begin
    assert c(1, 0) = 3 report "c(1,0)" severity failure;
    -- A multidimensional aggregate standing as an operand takes its index
    -- range from its own elements. Checked against nvc 1.22.1, which agrees;
    -- ghdl 7.0.0-dev stops with an internal error on this line.
    assert c = ((1, 2), (3, 4)) report "compare equal" severity failure;
    assert c /= ((1, 2), (3, 5)) report "compare unequal" severity failure;
    m := ((1, 2, 3), (4, 5, 6));
    assert m(2, 3) = 6 report "m(2,3)" severity failure;
    assert m = ((1, 2, 3), (4, 5, 6)) report "compare m" severity failure;
    -- Named choices in the outer dimension, positional within a row
    m := (2 => (7, 8, 9), 1 => (4, 5, 6));
    assert m(1, 1) = 4 report "named outer 1" severity failure;
    assert m(2, 1) = 7 report "named outer 2" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
