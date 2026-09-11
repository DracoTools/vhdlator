-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- VHDL-2008 gave physical types "mod" and "rem"; before it they had neither.

entity t_vhdl_physmod93_bad is
end entity;

architecture a of t_vhdl_physmod93_bad is
  constant ok : time := 10 ns / 2;
  constant bad1 : time := 10 ns mod 3 ns;
  constant bad2 : time := 10 ns rem 3 ns;
begin
end architecture;
