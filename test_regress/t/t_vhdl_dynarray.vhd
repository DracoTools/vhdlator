-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-License-Identifier: CC0-1.0

-- An access value pointing at an array whose bounds only the allocation
-- fixes: the object carries them, so the attributes and the indexing that
-- read them are answered from it rather than from the type.

entity t is
end entity;

architecture rtl of t is

  type int_array is array (integer range <>) of integer;
  type int_array_ptr is access int_array;

  type nat_array is array (natural range <>) of integer;
  type nat_array_ptr is access nat_array;
  subtype int_array5 is int_array(11 to 15);

  type rec is record
    id : integer;
    tag : integer;
  end record;
  type rec_array is array (natural range <>) of rec;
  type rec_array_ptr is access rec_array;

begin

  process
    variable p : int_array_ptr;
    variable q : int_array_ptr;
    variable d : int_array_ptr;
    variable r : rec_array_ptr;
    variable e : int_array_ptr;
    variable n : nat_array_ptr;
    variable c : int_array(1 to 3) := (7, 8, 9);
    variable cn : nat_array(1 to 3) := (7, 8, 9);
    variable sum : integer := 0;
  begin
    -- An ascending range
    p := new int_array(1 to 5);
    assert p'left = 1 report "p'left " & integer'image(p'left) severity failure;
    assert p'right = 5 report "p'right " & integer'image(p'right) severity failure;
    assert p'low = 1 report "p'low" severity failure;
    assert p'high = 5 report "p'high" severity failure;
    assert p'length = 5 report "p'length " & integer'image(p'length) severity failure;
    assert p'ascending report "p'ascending" severity failure;
    for i in p'range loop
      p(i) := i * 10;
    end loop;
    sum := 0;
    for i in p'range loop
      sum := sum + p(i);
    end loop;
    assert sum = 150 report "p sum " & integer'image(sum) severity failure;

    -- A descending range, and one that does not start at one
    q := new int_array(8 downto 3);
    assert q'left = 8 report "q'left " & integer'image(q'left) severity failure;
    assert q'right = 3 report "q'right " & integer'image(q'right) severity failure;
    assert q'low = 3 report "q'low " & integer'image(q'low) severity failure;
    assert q'high = 8 report "q'high " & integer'image(q'high) severity failure;
    assert q'length = 6 report "q'length " & integer'image(q'length) severity failure;
    assert not q'ascending report "q'ascending" severity failure;
    for i in q'range loop
      q(i) := i;
    end loop;
    sum := 0;
    for i in q'reverse_range loop
      sum := sum * 10 + q(i);
    end loop;
    assert sum = 345678 report "q sum " & integer'image(sum) severity failure;

    -- A slice copied from another object. What an allocation leaves in the
    -- elements is the element type's own default, which is not written here.
    d := new int_array(1 to 5);
    for i in d'range loop
      d(i) := 0;
    end loop;
    d(2 to 4) := p(1 to 3);
    assert d(2) = 10 and d(3) = 20 and d(4) = 30
      report "d slice " & integer'image(d(2)) & " " & integer'image(d(3)) severity failure;
    assert d(1) = 0 and d(5) = 0 report "d untouched" severity failure;

    -- A slice copied from a value whose bounds its subtype fixes
    d(3 to 5) := c;
    assert d(3) = 7 and d(4) = 8 and d(5) = 9 report "d from c" severity failure;

    -- A slice of a value whose bounds its subtype fixes keeps the index
    -- values it had there
    d(1 to 2) := c(2 to 3);
    assert d(1) = 8 and d(2) = 9 report "d from c slice" severity failure;

    -- Between two objects that run opposite ways, element by element
    q(8 downto 6) := p(3 to 5);
    assert q(8) = 30 and q(7) = 40 and q(6) = 50
      report "q from p " & integer'image(q(8)) & " " & integer'image(q(7)) severity failure;
    assert q(5) = 5 report "q untouched" severity failure;

    -- The whole of one object copied into another
    d.all := p.all;
    sum := 0;
    for i in d'range loop
      sum := sum + d(i);
    end loop;
    assert sum = 150 report "d sum " & integer'image(sum) severity failure;

    -- An array of records
    r := new rec_array(0 to 2);
    for i in r'range loop
      r(i).id := i;
      r(i).tag := i * i;
    end loop;
    assert r(2).id = 2 and r(2).tag = 4 report "r" severity failure;
    assert r'length = 3 report "r'length" severity failure;

    -- A subtype that constrains the array itself
    e := new int_array5;
    assert e'left = 11 and e'right = 15 and e'length = 5
      report "e " & integer'image(e'left) & " " & integer'image(e'length) severity failure;

    -- A value the allocation takes its bounds from
    n := new nat_array'(5, 6, 7);
    assert n'left = 0 and n'length = 3
      report "n " & integer'image(n'left) & " " & integer'image(n'length) severity failure;
    assert n(0) = 5 and n(1) = 6 and n(2) = 7 report "n values" severity failure;
    n := new nat_array'(cn);
    assert n'left = 1 and n'length = 3 and n(1) = 7 and n(3) = 9
      report "n from cn" severity failure;

    -- What is deallocated points at nothing
    deallocate(p);
    assert p = null report "p not null" severity failure;

    -- A null range gives an object with no elements
    d := new int_array(1 to 0);
    assert d'length = 0 report "empty length" severity failure;

    report "*-* All Finished *-*";
    wait;
  end process;

end architecture;
