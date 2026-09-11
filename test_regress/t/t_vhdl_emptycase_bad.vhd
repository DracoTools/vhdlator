-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A case statement has at least one alternative.

entity t_vhdl_emptycase_bad is
end entity;

architecture a of t_vhdl_emptycase_bad is
begin
  process
    variable i : integer := 0;
  begin
    case i is
    end case;
    wait;
  end process;
end architecture;
