-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An underline in a bit string literal only ever separates two characters
-- of its value, so it cannot lead, trail, or follow another underline.

entity t_vhdl_bitstr_bad is
end entity;

architecture a of t_vhdl_bitstr_bad is
  constant ok : bit_vector := b"0_1_0_1";
  constant bad1 : bit_vector := b"_0101";
  constant bad2 : bit_vector := b"01__01";
  constant bad3 : bit_vector := b"0101_";
begin
end architecture;
