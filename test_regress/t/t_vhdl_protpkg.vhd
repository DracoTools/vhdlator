-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- The shape a verification library is written in: a protected type holding
-- the state, one shared variable of it in a package, and package
-- subprograms that pass everything through to that one object. The object
-- is made when the package is elaborated, which is when a constant declared
-- in the body has its value worked out, so the structure is built before
-- anything calls in (5.6, 14.4).

use std.env.all;
use std.textio.all;

package t_vhdl_protpkg_pkg is

  type store is protected
    procedure setname (n : string);
    impure function getname (id : integer := 0) return string;
    procedure add (id : integer; v : integer);
    impure function total (id : integer := 0) return integer;
    impure function count return integer;
  end protected;

  procedure setname (n : string);
  impure function getname (id : integer := 0) return string;
  procedure add (id : integer; v : integer);
  impure function total (id : integer := 0) return integer;
  impure function count return integer;

end package;

package body t_vhdl_protpkg_pkg is

  -- Declared outside the protected type but called from inside it
  function twice (v : integer) return integer is
  begin
    return v * 2;
  end function;

  type store is protected body

    type rec is record
      name : line;
      sum  : integer;
    end record;
    type recs is array (natural range <>) of rec;
    type recsp is access recs;

    variable data : recsp;

    procedure init is
    begin
      data := new recs(0 to 3);
      for i in data'range loop
        data(i).name := new string'("id" & integer'image(i));
        data(i).sum := 0;
      end loop;
    end procedure;

    -- A constant's value is worked out when the object is created, so this
    -- is what builds the structure at elaboration
    impure function initialize return boolean is
    begin
      init;
      return true;
    end function;
    constant construct : boolean := initialize;

    procedure setname (n : string) is
    begin
      deallocate(data(0).name);
      data(0).name := new string'(n);
    end procedure;

    impure function getname (id : integer := 0) return string is
    begin
      return data(id).name.all;
    end function;

    procedure add (id : integer; v : integer) is
    begin
      data(id).sum := data(id).sum + twice(v);
    end procedure;

    impure function total (id : integer := 0) return integer is
    begin
      return data(id).sum;
    end function;

    impure function count return integer is
    begin
      return data'length;
    end function;

  end protected body;

  shared variable st : store;

  procedure setname (n : string) is
  begin
    st.setname(n);
  end procedure;

  impure function getname (id : integer := 0) return string is
  begin
    return st.getname(id);
  end function;

  procedure add (id : integer; v : integer) is
  begin
    st.add(id, v);
  end procedure;

  impure function total (id : integer := 0) return integer is
  begin
    return st.total(id);
  end function;

  impure function count return integer is
  begin
    return st.count;
  end function;

end package body;

use work.t_vhdl_protpkg_pkg.all;
use std.env.all;

entity t_vhdl_protpkg is
end entity;

architecture tb of t_vhdl_protpkg is
begin

  main : process is
  begin
    -- Everything below reaches the one object, so what one call leaves
    -- behind the next one sees

    -- Made and filled before anything called in
    assert count = 4 report "count " & integer'image(count) severity failure;
    assert getname = "id0" report "getname '" & getname & "'" severity failure;
    assert getname(2) = "id2" report "getname 2" severity failure;

    setname("top");
    assert getname = "top" report "after setname '" & getname & "'" severity failure;
    assert getname(1) = "id1" report "another element changed" severity failure;

    add(1, 5);
    add(1, 6);
    assert total(1) = 22 report "total " & integer'image(total(1)) severity failure;
    assert total = 0 report "total of another element" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
