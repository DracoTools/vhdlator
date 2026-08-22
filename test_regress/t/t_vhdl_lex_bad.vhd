-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Malformed lexical elements, IEEE 1076-2008 clause 15. Every construct here
-- is illegal; the scanner must report it and keep going.

architecture bad of t is
  -- 15.5.3 based literals
  constant c1 : integer := 20#11#;
  constant c2 : integer := 16##;
  constant c3 : integer := 2#12#;
  constant c4 : integer := 16#FF;
  -- 15.5.2 exponents
  constant c5 : integer := 1e;
  constant c6 : integer := 1e-5;
  constant c7 : integer := 99999999999999999999999;
  -- 15.8 bit string literals
  constant c8 : string := D"1X";
  constant c9 : string := 8SX"";
  constant c10 : string := 8UO"477";
  constant c11 : string := 32222222222sb"1";
  -- 15.4.2 identifiers
  signal a__b : bit;
  signal abc_ : bit;
  -- 15.3 not a delimiter in any revision
  signal bad : bit := $;
begin
end architecture;
