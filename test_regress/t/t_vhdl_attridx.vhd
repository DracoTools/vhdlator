-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A user-defined attribute holds an ordinary value, so where that value is an
-- array "obj'attr(i)" is one element of it rather than an argument to
-- anything: the parentheses index, they do not call.

package p is
  type three is range 1 to 3;
  type a1 is array (three) of integer;
  function af1 (g : integer) return a1;
end package;

package body p is
  function af1 (g : integer) return a1 is
  begin
    return (10 + g, 20 + g, 30 + g);
  end function;
end package body;

use work.p.all;

entity t is
  port ( signal pt : in bit );
  attribute at1 : a1;
  attribute at1 of pt : signal is af1(1);
  -- Text is held as text rather than element by element, and the index is
  -- the attribute's own subtype's: a descending one puts the first
  -- character at its left bound
  subtype s10 is string (10 downto 1);
  subtype s1t is string (1 to 10);
  attribute atd : s10;
  attribute ata : s1t;
end entity;

architecture rtl of t is
  function f1 (i : integer) return integer is begin return i + 1; end function;
  function f2 (i : integer) return integer is begin return i + 2; end function;
  attribute atd of f1 : function is "ABCDEFGHIJ";
  attribute ata of f1 : function is "ABCDEFGHIJ";
  -- "of all" gives it to every function this region declares (7.2)
  -- Two types sharing a literal. The specification names the designator,
  -- so both of them are given the attribute; a read has to say which one it
  -- means, and its signature is what says so (4.5.3, 7.2, 8.6).
  type state_t is (idle, active);
  type level_t is (idle, busy);
  attribute code : integer;
  attribute code of idle : literal is 7;
  attribute atall : a1;
  attribute atall of all : function is af1(2);
begin
  process
    variable v : integer;
    variable c : character;
  begin
    v := pt'at1(1);
    assert v = 11 report "first element wrong" severity failure;
    v := pt'at1(3);
    assert v = 31 report "third element wrong" severity failure;
    c := f1'atd(10);
    assert c = 'A' report "descending left" severity failure;
    c := f1'atd(1);
    assert c = 'J' report "descending right" severity failure;
    c := f1'ata(1);
    assert c = 'A' report "ascending left" severity failure;
    c := f1'ata(10);
    assert c = 'J' report "ascending right" severity failure;
    assert f1'atd = "ABCDEFGHIJ" report "whole" severity failure;
    -- Both functions were given the attribute by the one specification
    v := f1'atall(2);
    assert v = 22 report "all: f1" severity failure;
    v := f2'atall(2);
    assert v = 22 report "all: f2" severity failure;
    -- Where the name is overloaded, the signature says which declaration
    -- the attribute is asked of; without it the name is ambiguous
    assert idle [return state_t]'code = 7 report "state_t idle" severity failure;
    assert idle [return level_t]'code = 7 report "level_t idle" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
