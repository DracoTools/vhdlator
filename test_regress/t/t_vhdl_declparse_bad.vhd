-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An attribute specification names at least one declaration, each by its
-- simple name. A physical type's primary unit is declared by its name
-- alone, and a secondary unit as a whole number of another. A file type
-- names a type mark alone. A configuration specification says what it binds
-- to. An operator function has a parameter for each operand. A block has a label, and its maps follow the clauses they give values
-- to.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package t_vhdl_declparse_pkg is
  constant c : integer := 1;
end package;

entity t_vhdl_declparse_bad is
end entity;

architecture a of t_vhdl_declparse_bad is
  attribute att : integer;
  signal s : bit;
  component comp
  end component;
  -- Legal
  type len is range 0 to 9 units nm; pm = 1 nm; end units;
  attribute att of s : signal is 1;
  -- Illegal
  attribute att of work.t_vhdl_declparse_pkg.c : constant is 2;
  attribute att of : signal is 3;
  type len2 is range 0 to 9 units xa = 1 xa; end units;
  type len3 is range 0 to 9 units xb; xc = 1.5 xb; end units;
  type len4 is range 0 to 9 units xd; xe = 2 * xd; end units;
  type ft is file of bit_vector(0 to 3);
  for u : comp;
  function "not" (a : bit; b : bit := '0') return bit is begin return a; end function;
  function "*" (a : bit) return bit is begin return a; end function;
begin
  -- Legal
  blk : block port (p : bit); port map (p => s); begin end block;
  -- Illegal
  block begin end block;
  blk2 : block port map (p => s); begin end block;
  blk3 : block generic map (g => 1); begin end block;
  -- Legal
  u : comp;
end architecture;
