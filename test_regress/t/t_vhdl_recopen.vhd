-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A record element may have no length of its own (VHDL-2008, 5.3.3), and
-- then each value of the record says how long that element is.  It carries
-- its own bounds beside it, as any other such value does.  VUnit's
-- verification components pass their transactions around this way.

library ieee;
use ieee.std_logic_1164.all;

package recopen_pkg is
  type txn_t is record
    data : std_logic_vector;
    sop  : boolean;
  end record;

  function make(n : natural) return txn_t;
  function width(t : txn_t) return natural;
  function top(t : txn_t) return std_logic;
  function same(a, b : txn_t) return boolean;
end package;

package body recopen_pkg is
  function make(n : natural) return txn_t is
    variable r : txn_t(data(n - 1 downto 0));
  begin
    r.data := (others => '1');
    r.data(0) := '0';
    r.sop := n > 2;
    return r;
  end;

  function width(t : txn_t) return natural is
  begin
    return t.data'length;
  end;

  function top(t : txn_t) return std_logic is
  begin
    return t.data(t.data'left);
  end;

  function same(a, b : txn_t) return boolean is
  begin
    return a.data = b.data and a.sop = b.sop;
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.recopen_pkg.all;

entity t_vhdl_recopen is
end entity;

architecture a of t_vhdl_recopen is
begin
  process
    variable n : natural := 6;
    variable t : txn_t(data(5 downto 0));
  begin
    assert width(make(n)) = 6 report "wrong width" severity failure;
    assert top(make(n)) = '1' report "wrong top bit" severity failure;
    assert make(n).sop report "sop not set" severity failure;

    -- Copied into an object whose own subtype fixes the length
    t := make(n);
    assert t.data = "111110" report "wrong data" severity failure;
    assert same(t, make(n)) report "not the same" severity failure;

    n := 2;
    assert width(make(n)) = 2 report "wrong width for two" severity failure;
    assert not make(n).sop report "sop set for two" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
