-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A logical expression repeats one operator, and nand and nor not even that.
-- A wait statement's 'on' names a signal. A generate statement has a label.
-- An access type names what it designates, and an instance what it
-- instantiates.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_exprparse_bad is
end entity;

architecture a of t_vhdl_exprparse_bad is
  signal a, b, c : boolean;
  -- Legal
  type p is access integer;
  -- Illegal
  type q is access ;
begin
  process
    variable x : boolean;
  begin
    -- Legal
    x := a and b and c;
    x := (a and b) or c;
    x := a nor (b nor c);
    wait on a, b;
    -- Illegal
    x := a and b or c;
    x := a nor b nor c;
    wait on ;
    wait;
  end process;
  -- Legal
  g : for i in 0 to 1 generate end generate;
  -- Illegal
  for i in 0 to 1 generate end generate;
  ins : entity ;
end architecture;
