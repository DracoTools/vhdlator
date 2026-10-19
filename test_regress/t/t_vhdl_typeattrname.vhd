-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'SIMPLE_NAME of a prefix that is 'SUBTYPE or 'BASE is the name of the type it denotes:
-- "bv'subtype'base" is BIT_VECTOR, "v'subtype" the type mark v is declared with (16.2.7;
-- GHDL gna issue261, where GHDL and Questa agree).

entity t_vhdl_typeattrname is
end entity;

architecture a of t_vhdl_typeattrname is
  type at_t is access integer;
  type ft_t is file of integer;
begin
  process is
    variable bv : bit_vector(0 to 1);
    variable v : at_t;
    file f : ft_t;
  begin
    report bv'simple_name;
    report bv'subtype'base'simple_name;
    report v'subtype'simple_name;
    report v'subtype'base'simple_name;
    report f'subtype'simple_name;
    report f'subtype'base'simple_name;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
