-- DESCRIPTION: Verilator: VHDL access value read whole through a call
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- What an access to an unconstrained array points at, read whole where the
-- access value is itself chosen by a call with an effect of its own --
-- OSVVM's CoveragePkg returns "CovBinPtr(GetRandIndex(ID)).BinVal.all".
-- The value carries its bounds, and the call is made once.

package t_vhdl_accallonce_pkg is
  type range_t is record
    lo : integer;
    hi : integer;
  end record;
  type range_array_t is array (integer range <>) of range_t;
  type store_t is protected
    procedure add(v : range_array_t);
    impure function get(i : natural) return range_array_t;
    impure function picks return natural;
  end protected;
end package;

package body t_vhdl_accallonce_pkg is
  type store_t is protected body
    type range_array_ptr_t is access range_array_t;
    type bin_t is record
      binval : range_array_ptr_t;
      count  : integer;
    end record;
    type bin_array_t is array (natural range <>) of bin_t;
    type bin_ptr_t is access bin_array_t;
    type item_t is record
      binptr : bin_ptr_t;
      nbins  : integer;
    end record;
    type item_array_t is array (integer range <>) of item_t;
    type item_ptr_t is access item_array_t;
    variable items : item_ptr_t := new item_array_t(1 to 2);
    variable n : natural := 0;
    variable calls : natural := 0;

    impure function pick(i : natural) return natural is
    begin
      calls := calls + 1;
      report "pick " & integer'image(i);
      return i;
    end;

    procedure add(v : range_array_t) is
    begin
      if items(1).binptr = null then
        items(1).binptr := new bin_array_t(1 to 4);
      end if;
      n := n + 1;
      items(1).binptr(n).binval := new range_array_t'(v);
    end;

    impure function get(i : natural) return range_array_t is
    begin
      return items(1).binptr(pick(i)).binval.all;
    end;

    impure function picks return natural is
    begin
      return calls;
    end;
  end protected body;
end package body;

use work.t_vhdl_accallonce_pkg.all;

entity t_vhdl_accallonce is
end entity;

architecture a of t_vhdl_accallonce is
  shared variable s : store_t;
begin
  process
    variable r : range_array_t(1 to 2);
  begin
    s.add(((1, 2), (3, 4)));
    s.add((0 => (5, 6)));
    r := s.get(1);
    assert r(1).lo = 1 and r(2).hi = 4 report "first bin" severity failure;
    assert s.get(2)'left = 0 and s.get(2)'length = 1 report "second bounds" severity failure;
    assert s.get(2)(0).lo = 5 report "second bin" severity failure;
    assert s.picks = 4 report "calls " & integer'image(s.picks) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
