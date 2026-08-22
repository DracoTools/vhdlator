-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Expressions whose meaning depends on overload resolution running in both
-- directions (IEEE 1076-2008 clause 12.5). None of this should report an
-- error: every name and operator here has exactly one interpretation once
-- the context is taken into account.

package p is
  type state_t is (idle, run, done);
  type nibble_t is array (3 downto 0) of bit;
  type pair_t is record
    a : integer;
    b : bit;
  end record;
  -- Overloaded on the argument type, and on the result type
  function f(x : integer) return integer;
  function f(x : boolean) return boolean;
  function g return integer;
  function g return boolean;
end package;

package body p is
  function f(x : integer) return integer is begin return x; end function;
  function f(x : boolean) return boolean is begin return x; end function;
  function g return integer is begin return 0; end function;
  function g return boolean is begin return false; end function;
end package body;

use work.p.all;
entity t is
end entity;

architecture rtl of t is
  signal s   : state_t := idle;
  signal i   : integer := 3;
  signal b   : bit := '1';
  signal n   : nibble_t := "1010";
  signal pr  : pair_t;
  signal ok  : boolean;
  constant D : time := 10 ns;
begin
  main : process is
    variable v : integer;
    variable c : character;
  begin
    -- Arithmetic, with an integer literal taking its type from the context
    v := i + 1;
    v := (i * 2) mod 7;
    -- A character literal decided by the other operand: '1' is BIT here,
    -- because b is, and CHARACTER elsewhere
    ok <= b = '1';
    c  := '1';
    -- Enumeration literals, resolved against the signal's type
    ok <= s = run;
    -- Relational and logical operators over different types
    ok <= (i > 2) and (s /= done);
    -- Shifts and concatenation over an array type
    n <= n sll 1;
    n <= '0' & n(3 downto 1);
    -- Record element selection
    v := pr.a;
    -- Overload chosen by the argument type
    v := f(3);
    ok <= f(true);
    -- Overload chosen by the result type alone
    v := g;
    ok <= g;
    -- Attributes
    v := nibble_t'length;
    ok <= s'left = idle;
    -- Aggregates, taking their type from the target
    n  <= (others => '0');
    pr <= (a => 1, b => '0');
    -- A physical literal and its unit
    wait for D;
    wait for 5 ns;
  end process;
end architecture;
