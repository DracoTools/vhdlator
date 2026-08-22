-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Exercises IEEE 1076-2008 clause 15 (lexical elements).
/* A delimited comment, VHDL-2008 clause 15.9.
   It spans lines, does not nest, and -- does not start a line comment. */

entity t is
end entity;

architecture rtl of t is
  -- 15.4.2 basic identifiers are case insensitive
  signal MixedCase : bit;
  signal mixedcase_2 : bit;
  -- 15.4.3 extended identifiers are case sensitive and distinct from basic ones
  signal \Bus\ : bit;
  signal \bus\ : bit;
  signal \a\\b\ : bit;

  -- 15.5.2 decimal literals
  constant i1 : integer := 12;
  constant i2 : integer := 123_456;
  constant i3 : integer := 1e6;
  constant r1 : real := 12.0;
  constant r2 : real := 3.14159_26;
  constant r3 : real := 1.34e-12;
  constant r4 : real := 6.023e+24;

  -- 15.5.3 based literals
  constant b1 : integer := 2#1111_1111#;
  constant b2 : integer := 16#FF#;
  constant b3 : integer := 016#0FF#;
  constant b4 : integer := 16#E#E1;
  constant b5 : real := 16#F.FF#e+2;

  -- 15.6 character literals, distinguished from the attribute tick below
  constant c1 : character := 'A';
  constant c2 : character := ''';
  constant c3 : character := ' ';

  -- 15.7 string literals, with a doubled quote
  constant s1 : string := "Setup time is too short";
  constant s2 : string := "";
  constant s3 : string := """";

  -- 15.8 bit string literals, the worked examples from the LRM
  constant v1 : bit_vector := B"1111_1111_1111";
  constant v2 : bit_vector := X"FFF";
  constant v3 : bit_vector := O"777";
  constant v4 : bit_vector := X"777";
  constant v5 : string := UO"27";
  constant v6 : string := SX"3W";
  constant v7 : bit_vector := D"35";
  constant v8 : string := 12UB"X1";
  constant v9 : string := 12SB"X1";
  constant v10 : string := 12UX"F-";
  constant v11 : string := 12SX"F-";
  constant v12 : bit_vector := 12D"13";
  constant v13 : string := 12SX"FFFC00";
begin
  -- The tick after a name is an attribute, not a character literal
  assert i1'image /= "" severity note;
  assert bit'high = '1' severity note;
  -- Compound delimiters, clause 15.3
  process is
    variable v : integer := 0;
  begin
    v := v + 1;
    if v /= 0 and v >= 1 and v <= 2 then
      v := v ** 2;
    end if;
    wait;
  end process;
end architecture;
