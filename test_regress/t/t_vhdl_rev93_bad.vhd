-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 a package has no generics and is declared only as a
-- design unit of its own, and a string between percent signs, standing for
-- one between quotation marks, holds none of them (VHDL-93 13.10). A
-- logical operator has two operands; VHDL-2008 added the reductions.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package t_vhdl_rev93_pkg is
  -- Illegal
  generic (n : natural := 2);
end package;

entity t_vhdl_rev93_bad is
end entity;

architecture a of t_vhdl_rev93_bad is
  -- Legal
  constant s1 : string := %abc%;
  -- Illegal
  package inner is end package;
  constant s2 : string := %a"b%;
  function "and" (a : bit) return bit is begin return a; end function;
begin
end architecture;
