-- DESCRIPTION: Verilator: VHDL user-defined attribute holding an array of
-- records, indexed
--
-- An attribute of an unconstrained array of records, given a positional
-- aggregate (7.2), whose value is indexed and an element of the record
-- read: "s'attr(1).b". The value is held as an array of the records, as
-- long as the aggregate, rather than as a vector of bits -- GHDL gna
-- issue10's "t_tuple'attr(i).a". (That test's "t_tuple'attr'range" is
-- refused by nvc and by GHDL in 2008 mode, a type not being an object, and
-- is left so.)
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_userattrrec is
end entity;

architecture rtl of t_vhdl_userattrrec is
  type t_tuple is record
    a : natural;
    b : natural;
  end record;
  type t_vector is array (natural range <>) of t_tuple;
  attribute attr : t_vector;
  signal s : bit;
  attribute attr of s : signal is ((8, 32), (8, 20), (8, 36));
begin
  process is
    variable i : natural := 2;
  begin
    assert s'attr(1).a = 8 and s'attr(1).b = 20 severity failure;
    assert s'attr(i).b = 36 report "indexed by a variable" severity failure;
    assert s'attr(0) = (8, 32) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
