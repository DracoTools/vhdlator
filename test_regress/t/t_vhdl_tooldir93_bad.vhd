-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Tool directives arrived with VHDL-2008; before it a grave accent is a
-- character only a comment or a string may hold. References: nvc 1.22.1,
-- GHDL 7.0.0-dev, Questa 2025.2.

entity t is
end entity;

`warning "hello"

architecture a of t is
  constant s : string := "a`b";
begin
end architecture;
