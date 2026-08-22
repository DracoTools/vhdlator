-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Declarations that analysis must turn into symbols and types, using only
-- the predefined environment of IEEE 1076-2008 clause 16.3.

package p is
  type state_t is (idle, running, done);
  type opcode_t is ('a', 'b', 'c');
  type byte_t is range 0 to 255;
  type level_t is range -1.0 to 1.0;
  type freq_t is range 0 to 1000000
    units
      hz;
      khz = 1000 hz;
      mhz = 1000 khz;
    end units;
  type word_t is array (natural range <>) of bit;
  type matrix_t is array (0 to 3, 0 to 3) of integer;
  type point_t is record
    x : integer;
    y : integer;
  end record;
  type ptr_t is access point_t;
  type text_t is file of character;

  subtype nibble_t is bit_vector(3 downto 0);
  subtype small_t is integer range 0 to 15;

  constant K : integer := 42;
  constant NAME : string := "hello";

  function add1(x : integer) return integer;
  function add1(x : real) return real;
  procedure swap(a : inout integer; b : inout integer);

  attribute mark : boolean;
end package;

package body p is
  function add1(x : integer) return integer is
    variable v : integer := x;
  begin
    v := v + 1;
    return v;
  end function;

  function add1(x : real) return real is
  begin
    return x;
  end function;

  procedure swap(a : inout integer; b : inout integer) is
    variable t : integer;
  begin
    t := a;
    a := b;
    b := t;
  end procedure;
end package body;

entity t is
  generic (WIDTH : positive := 8);
  port (clk : in bit;
        q   : out bit_vector(7 downto 0));
end entity;

architecture rtl of t is
  signal count : natural := 0;
  signal flag  : boolean;
  shared variable total : integer;
begin
  main : process (clk) is
    variable local : integer := 0;
  begin
    local := local + 1;
  end process;

  gen : for i in 0 to 3 generate
    signal inner : bit;
  begin
  end generate;
end architecture;
