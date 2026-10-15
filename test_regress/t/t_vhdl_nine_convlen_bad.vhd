-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues numeric_std's conversions are compiled from their
-- bodies, and their results are still as long as the standard says: a
-- TO_SIGNED of seventeen elements for a record element of sixteen is the
-- same error it is without the option (gna issue2350)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_convlen_bad is
end entity;

architecture a of t_vhdl_nine_convlen_bad is
  type cx_t is record
    re : signed;
    im : signed;
  end record;
  subtype c16_t is cx_t(re(15 downto 0), im(15 downto 0));
  constant x : c16_t := (re => to_signed(0, 16), im => to_signed(0, 17));
  signal v : unsigned(7 downto 0) := resize(unsigned'(x"ff"), 9);
begin
end architecture;
