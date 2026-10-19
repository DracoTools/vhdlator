-- DESCRIPTION: Verilator: VHDL run-time check: a qualified expression's static operand outside its subtype (9.3.5; GHDL gna issue1240)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.numeric_std.all;

entity t_vhdl_qualrange_bad is
end entity;

architecture a of t_vhdl_qualrange_bad is
  signal s : signed(3 downto 0);
begin
  -- GHDL stops and Questa refuses; one that does not fold is checked by
  -- Questa alone, and not here
  s <= to_signed(natural'(-1), s'length);
end architecture;
