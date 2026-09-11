-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Before VHDL-2008 a conditional or selected assignment is a concurrent
-- statement only, never a sequential one (VHDL-93 8.4, 9.5).
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

entity t_vhdl_seq93_bad is
end entity;

architecture a of t_vhdl_seq93_bad is
  signal s, t : bit;
  signal c : boolean;
begin
  -- Legal
  s <= '1' when c else '0';
  process
    variable v : bit;
  begin
    -- Legal
    t <= '1';
    -- Illegal
    t <= '1' when c else '0';
    v := '1' when c else '0';
    with c select t <= '1' when true, '0' when false;
    wait;
  end process;
end architecture;
