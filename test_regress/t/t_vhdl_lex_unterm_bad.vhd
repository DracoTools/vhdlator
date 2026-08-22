-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Lexical elements that run off the end of their line. Clause 15.3 requires
-- each lexical element to fit on one line, so all of these are errors.

architecture bad of t is
  signal \unterminated : bit;
  signal \\ : bit;
  constant s1 : string := "unterminated
  constant c1 : character := 'ab';
  constant c2 : character := '
  constant b1 : string := X"FF
begin
end architecture;
