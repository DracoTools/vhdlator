-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A port is a signal whatever class is written for it, and an interface
-- list declares at least one element.

entity t_vhdl_portdecl_bad is
  port (
    signal ok : in bit;
    constant c : in integer;
    variable v : in boolean
  );
end entity;

architecture a of t_vhdl_portdecl_bad is
  component empty is
    port ( );
  end component;
begin
end architecture;
