-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An allocator whose value is a constant of an unconstrained array given its
-- bounds by a call, so that only running knows them. VUnit's com package
-- copies what a protected method returns into a new object this way, and
-- the allocator was refused (vunit/vhdl/com/src/com.vhd).
entity t_vhdl_alloccarr is
end entity;

architecture sim of t_vhdl_alloccarr is
  type int_vec is array (natural range <>) of integer;
  type int_vec_ptr is access int_vec;
  type str_ptr is access string;

  -- Descending, from n + 2 down to 3
  function make(n : natural; base : integer) return int_vec is
    variable r : int_vec(n + 2 downto 3);
  begin
    for i in r'range loop
      r(i) := base + i;
    end loop;
    return r;
  end function;

  function text(n : natural) return string is
    variable r : string(1 to n);
  begin
    for i in r'range loop
      r(i) := character'val(character'pos('a') + i - 1);
    end loop;
    return r;
  end function;

  impure function wrap(n : natural) return int_vec_ptr is
    constant v : int_vec := make(n, 100);
  begin
    if v'length = 0 then
      return null;
    else
      return new int_vec'(v);
    end if;
  end function;

  impure function twice(n : natural) return int_vec_ptr is
    constant v : int_vec := make(n, 0);
  begin
    return new int_vec'(v & v);
  end function;

  impure function letters(n : natural) return str_ptr is
    constant t : string := text(n);
  begin
    return new string'(t);
  end function;

  type store_t is protected
    procedure add(x : integer);
    impure function get return int_vec;
  end protected;

  type store_t is protected body
    variable data : int_vec(0 to 15);
    variable n : natural := 0;
    procedure add(x : integer) is
    begin
      data(n) := x;
      n := n + 1;
    end procedure;
    impure function get return int_vec is
    begin
      return data(0 to n - 1);
    end function;
  end protected body;

  shared variable store : store_t;

  impure function snapshot return int_vec_ptr is
    constant v : int_vec := store.get;
  begin
    return new int_vec'(v);
  end function;
begin
  process
    variable p : int_vec_ptr;
    variable q : int_vec_ptr;
    variable s : str_ptr;
  begin
    p := wrap(3);
    assert p /= null severity failure;
    assert p.all'length = 3 severity failure;
    assert p.all'left = 5 and p.all'right = 3 severity failure;
    assert not p.all'ascending severity failure;
    assert p(5) = 105 and p(4) = 104 and p(3) = 103 severity failure;
    p := wrap(0);
    assert p = null severity failure;

    store.add(7);
    store.add(8);
    q := snapshot;
    assert q.all'length = 2 and q.all'left = 0 and q.all'ascending severity failure;
    assert q(0) = 7 and q(1) = 8 severity failure;
    -- The object is a copy, so what the store holds next leaves it be
    store.add(9);
    assert q.all'length = 2 severity failure;
    q := snapshot;
    assert q.all'length = 3 and q(2) = 9 severity failure;

    -- A value the constant goes into an expression with. The result of
    -- "&" runs as the index subtype does, from its left (9.2.5).
    p := twice(2);
    assert p.all'length = 4 and p.all'left = 0 and p.all'right = 3 severity failure;
    assert p(0) = 4 and p(1) = 3 and p(2) = 4 and p(3) = 3 severity failure;

    s := letters(4);
    assert s.all = "abcd" and s.all'left = 1 and s.all'right = 4 severity failure;

    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
