-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An index into a name that denotes a slice of another object: an alias of
-- one, or a signal parameter a call bound to one. A slice keeps the
-- numbering of the object it is taken from (8.5), and the index was put
-- onto the slice's value counted from zero instead, so it named the wrong
-- element or none. VUnit's event package is the shape: notify is handed
-- runner(15 to 19) and writes event(event'left), and the process waiting
-- for it never woke (vunit/vhdl/data_types/src/event_common_pkg.vhd).
-- A copy that hands part of its own formal on to another call lost the
-- part, and two copies given two signals shared the one they called.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sigslice_pkg is
  subtype any_event_t is std_logic_vector;
  constant p_inactive_event : std_logic_vector := "ZZ";
  impure function is_active(signal event : any_event_t) return boolean;
  procedure notify(signal event : inout any_event_t);
end package;

package body t_vhdl_sigslice_pkg is
  impure function is_active(signal event : any_event_t) return boolean is
  begin
    return event(event'left to event'left + 1) /= p_inactive_event;
  end;

  procedure activate(signal event : inout any_event_t) is
  begin
    if event(event'left) = '1' then
      event(event'left + 1) <= '1';
    else
      event(event'left) <= '1';
    end if;
  end;

  procedure inactivate(signal event : inout any_event_t) is
  begin
    event(event'left to event'left + 1) <= p_inactive_event;
  end;

  procedure notify(signal event : inout any_event_t) is
  begin
    activate(event);
    wait until is_active(event);
    inactivate(event);
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sigslice_pkg.all;

entity t_vhdl_sigslice is
end entity;

architecture sim of t_vhdl_sigslice is
  signal runner : std_logic_vector(0 to 20) := (others => 'Z');
  signal seen0, seen15 : integer := 0;

  signal r : std_logic_vector(0 to 20) := (others => '0');
  signal d : std_logic_vector(20 downto 0) := (others => '0');
  signal w : std_logic_vector(7 downto 3) := (others => '0');
  signal c : std_logic_vector(0 to 20) := (others => '0');
  signal e : std_logic_vector(20 downto 0) := (others => '0');
  signal iv : integer_vector(1 to 10) := (others => 0);
  signal lft, rgt, len : integer := -1;
  signal cr, cq : std_logic_vector(10 to 19) := (others => '0');
  signal cp : std_logic_vector(0 to 20) := (others => '0');
  signal cd : std_logic_vector(19 downto 10) := (others => '0');
  signal clen : integer := -1;

  alias a is r(5 to 9);
  alias b : std_logic_vector(4 downto 0) is r(10 to 14);

  -- Takes the bounds of whatever it is given
  procedure setl(signal s : inout std_logic_vector) is
  begin
    s(s'left) <= '1';
    s(s'right) <= s(s'left);
  end;

  impure function ends(signal s : std_logic_vector) return std_logic_vector is
  begin
    return s(s'left) & s(s'right);
  end;

  -- Numbers its elements its own way, whatever the actual's bounds
  procedure setc(signal s : inout std_logic_vector(0 to 4);
                 signal l, rt, n : out integer) is
  begin
    s(s'left) <= '1';
    s(3) <= '1';
    l <= s'left;
    rt <= s'right;
    n <= s'length;
  end;

  procedure seti(signal s : inout integer_vector; v : integer) is
  begin
    s(s'left) <= v;
    s(s'right) <= v + 1;
  end;

  -- Given an actual numbered the other way round, and sliced at ends only
  -- running knows
  procedure rev(signal s : inout std_logic_vector(0 to 4); k : integer) is
  begin
    s(k to k + 1) <= "10";
    s(4) <= '1';
  end;

  procedure count(signal s : in std_logic_vector; signal n : out integer) is
  begin
    n <= s'length;
  end;

  -- Hands part of what it was given on
  procedure outer(signal s : inout std_logic_vector; signal n : out integer) is
  begin
    setl(s(12 to 13));
    count(s(11 to 13), n);
  end;

  procedure outerc(signal s : inout std_logic_vector(0 to 9)) is
  begin
    setl(s(2 to 3));
  end;
begin
  waiter15 : process
  begin
    wait until is_active(runner(15 to 19));
    seen15 <= seen15 + 1;
  end process;

  waiter0 : process
  begin
    wait until is_active(runner(0 to 4));
    seen0 <= seen0 + 1;
  end process;

  process
    variable x : string(1 to 8) := "abcdefgh";
    alias s2 : string(1 to 3) is x(3 to 5);
    alias s3 is x(3 to 5);
    variable ivv : integer_vector(0 to 7) := (others => 0);
    alias ia is ivv(2 to 4);
    variable v : std_logic_vector(0 to 20) := (others => '0');
    alias va is v(3 to 5);
  begin
    notify(runner(15 to 19));
    wait for 1 ns;
    notify(runner(0 to 4));
    wait for 1 ns;
    notify(runner(15 to 19));
    wait for 1 ns;
    assert seen0 = 1 and seen15 = 2 severity failure;

    -- A formal that takes the actual's bounds. The second element is
    -- given what the first held before the call, so it takes two.
    setl(r(15 to 19));
    setl(d(19 downto 15));
    setl(w);
    wait for 1 ns;
    assert r(15) = '1' and r(16 to 19) = "0000" severity failure;
    assert d(19) = '1' and d(18 downto 15) = "0000" severity failure;
    assert ends(r(15 to 19)) = "10" and ends(r(14 to 15)) = "01" severity failure;
    assert ends(d(20 downto 19)) = "01" severity failure;
    setl(r(15 to 19));
    setl(d(19 downto 15));
    setl(w);
    wait for 1 ns;
    assert r(15) = '1' and r(19) = '1' and r(16 to 18) = "000" severity failure;
    assert d(19) = '1' and d(15) = '1' and d(18 downto 16) = "000" severity failure;
    assert w = "10001" severity failure;
    assert ends(r(15 to 19)) = "11" severity failure;

    -- A formal with bounds of its own
    setc(c(10 to 14), lft, rgt, len);
    wait for 1 ns;
    assert c(10) = '1' and c(13) = '1' and c(11 to 12) = "00" and c(14) = '0'
      severity failure;
    assert lft = 0 and rgt = 4 and len = 5 severity failure;
    w <= (others => '0');
    wait for 1 ns;
    setc(w, lft, rgt, len);
    wait for 1 ns;
    assert w = "10010" severity failure;

    seti(iv(4 to 6), 7);
    rev(e(19 downto 15), 1);
    wait for 1 ns;
    assert iv(4) = 7 and iv(5) = 0 and iv(6) = 8 severity failure;
    assert e(18) = '1' and e(17) = '0' and e(15) = '1' severity failure;
    assert e(19) = '0' and e(16) = '0' severity failure;

    -- Aliases of a slice, with and without a subtype of their own
    a(6) <= '1';
    b(0) <= '1';
    wait for 1 ns;
    assert r(6) = '1' and r(14) = '1' and r(9) = '0' and r(10) = '0' severity failure;
    assert a(6) = '1' and a(6 to 7) = "10" severity failure;
    va(4) := '1';
    assert v(4) = '1' and v(5) = '0' severity failure;
    ia(3) := 5;
    assert ivv(3) = 5 and ivv(2) = 0 severity failure;
    assert s2(1) = 'c' and s2(3) = 'e' and s3(4) = 'd' and s3(3 to 4) = "cd"
      severity failure;

    -- Part of a formal handed on
    outer(cr, clen);
    outer(cq, clen);
    outer(cp(10 to 19), clen);
    outerc(cd);
    wait for 1 ns;
    assert cr = "0010000000" and cq = "0010000000" severity failure;
    assert cp(12) = '1' and cp(10) = '0' and cp(13) = '0' severity failure;
    assert clen = 3 severity failure;
    assert cd(17) = '1' and cd(19) = '0' and cd(12) = '0' severity failure;

    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
