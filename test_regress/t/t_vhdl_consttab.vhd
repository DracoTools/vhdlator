-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An array aggregate whose elements are static is a static value in its
-- turn, and an indexed name over one is static as well (9.4.2).  That is
-- how a package writes its parameters as a table and reads them back, and
-- until the table can be held there is nothing for the index to read.

package pk is
  constant NPAR : natural := 4;
  type nat_vec is array (natural range <>) of natural;
  type fixed_vec is array (NPAR - 1 downto 0) of natural;

  -- Positional, named, a range as a choice, and "others"
  constant FIB    : nat_vec(0 to 6) := (1, 1, 2, 3, 5, 8, 13);
  constant NAMED  : nat_vec(0 to 3) := (0 => 10, 2 => 30, others => 99);
  constant RANGED : nat_vec(0 to 5) := (0 to 2 => 7, others => 4);
  constant POLYS  : fixed_vec := (121, 91, 55, 31);

  -- An alias of one element, which is what a generic is then given
  alias FIB5 : natural is FIB(5);

  function width_of(v : natural) return natural;
  function calc_widest return natural;
  constant WIDEST : natural;
end package;

package body pk is
  function width_of(v : natural) return natural is
    variable n : natural := 0;
    variable r : natural := v;
  begin
    while r > 0 loop
      n := n + 1;
      r := r / 2;
    end loop;
    return n;
  end function;

  -- Reads the table from inside a function body, which is where it failed
  function calc_widest return natural is
    variable m : natural := 0;
  begin
    for i in NPAR - 1 downto 0 loop
      if width_of(POLYS(i)) > m then m := width_of(POLYS(i)); end if;
    end loop;
    return m;
  end function;

  constant WIDEST : natural := calc_widest;
end package body;

-------------------------------------------------------------------------------

use work.pk.all;

entity sub is
  generic ( v : natural );
end entity;

architecture rtl of sub is
begin
  -- Fails at analysis if the generic did not arrive as the number 8
  assert v = 8 report "the generic did not take the table's element" severity failure;
end architecture;

-------------------------------------------------------------------------------

use work.pk.all;
use std.env.all;

entity t_vhdl_consttab is
end entity;

architecture tb of t_vhdl_consttab is
  -- A user-defined attribute whose value is an aggregate, read by index
  attribute att : nat_vec;
  constant anchor : integer := 1;
  attribute att of anchor : constant is (2, 3);

  -- Bounds worked out from the table: WIDEST is 7, since 121 needs 7 bits
  signal wide : bit_vector(WIDEST - 1 downto 0) := (others => '0');
begin

  dut : entity work.sub generic map ( v => FIB5 );

  main : process is
  begin
    assert FIB(5) = 8 report "positional table read wrong" severity failure;
    assert FIB5 = 8 report "alias of an element read wrong" severity failure;
    assert NAMED(0) = 10 and NAMED(2) = 30 and NAMED(1) = 99 and NAMED(3) = 99
      report "named choices and others read wrong" severity failure;
    assert RANGED(0) = 7 and RANGED(2) = 7 and RANGED(3) = 4
      report "a range as a choice read wrong" severity failure;
    assert POLYS(3) = 121 and POLYS(0) = 31
      report "a descending table read wrong" severity failure;
    assert WIDEST = 7 report "a bound worked out from the table is wrong"
      severity failure;
    assert wide'length = 7 report "the signal took the wrong width" severity failure;
    assert anchor'att(0) = 2 and anchor'att(1) = 3
      report "an attribute whose value is an aggregate read wrong" severity failure;
    report "*-* All Finished *-*";
    finish;
  end process;

end architecture;
