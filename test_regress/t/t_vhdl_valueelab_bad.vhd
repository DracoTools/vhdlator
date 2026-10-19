-- DESCRIPTION: Verilator: VHDL elaboration check: 'VALUE of text that is no value of the type (16.2.3; GHDL gna bug0143)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_valueelab_bad is
end entity;

architecture a of t_vhdl_valueelab_bad is
  constant s : string := "";
  constant v : real := real'value(s);
begin
  process is
  begin
    report "the model should have stopped when it started" severity failure;
    wait;
  end process;
end architecture;
