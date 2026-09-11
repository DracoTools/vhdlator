-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Both bounds of a range are of one type, and of the type the context gives
-- where it gives one; and an array's index range is discrete.

entity t_vhdl_rangebnd_bad is
end entity;

architecture a of t_vhdl_rangebnd_bad is
  type mvl1 is ('X', '0', '1');
  type mvl2 is ('X', '0', '1', 'Z');
  type bit_indexed is array (bit range <>) of bit;
  -- Legal
  type ok1 is array (mvl1'low to mvl1'high) of integer;
  subtype ok2 is bit_indexed('0' to '1');
  -- Illegal
  type bad1 is array (mvl1'low to mvl2'high) of integer;
  type bad2 is array (0 to 'B') of integer;
  type bad3 is array (0.0 to 7.0) of integer;
  subtype bad4 is bit_indexed(0 to 1);
  subtype bad5 is character range 1 to 8;
begin
end architecture;
