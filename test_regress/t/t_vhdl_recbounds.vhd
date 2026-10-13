-- DESCRIPTION: Verilator: VHDL record subtypes giving an element different bounds
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- Two subtypes of a record constrain an open element to different bounds of
-- the same length; a value of one is assigned to the other position by
-- position (GHDL issue1038). And an access value to an array whose elements
-- have bounds of their own, given one aggregate value for a range of them (GHDL issue2166)

entity t_vhdl_recbounds is
end entity;

architecture a of t_vhdl_recbounds is
  type bv_array is array (natural range <>) of bit_vector;
  subtype byte_array is bv_array(open)(7 downto 0);
  type mrec is record
    b : boolean;
    data : byte_array;
  end record;
  type outer is record
    n : integer;
    r : mrec;
  end record;
  signal s : mrec(data(0 to 3));
  signal so : outer(r(data(10 to 13)));

  function fill(size : integer) return integer_vector is
  begin
    return (1 to size => 7);
  end function;
  subtype mem_word is integer_vector;
  type mem_block is array (integer range <>) of mem_word;
  type mem_ptr is access mem_block;
begin
  process is
    variable v : mrec(data(1 to 4));
    variable w : mrec(data(5 to 8));
    variable o : outer(r(data(20 to 23)));
    variable mem : mem_ptr;
  begin
    v := (b => true, data => (x"11", x"22", x"33", x"44"));
    s <= v;
    w := v;
    o := (n => 9, r => v);
    so <= o;
    wait for 1 ns;
    assert s.b and s.data(0) = x"11" and s.data(3) = x"44" severity failure;
    assert w.data(5) = x"11" and w.data(8) = x"44" severity failure;
    assert o.r.data(20) = x"11" and o.r.data(23) = x"44" severity failure;
    assert so.n = 9 and so.r.data(10) = x"11" and so.r.data(13) = x"44" severity failure;
    assert s.data'left = 0 and w.data'left = 5 severity failure;

    mem := new mem_block(0 to 3)(0 to 31);
    mem.all := (0 to 3 => fill(32));
    assert mem(2)(31) = 7 and mem(0)'length = 32 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
