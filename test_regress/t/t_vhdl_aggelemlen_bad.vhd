-- DESCRIPTION: Verilator: VHDL elaboration check: aggregate elements of different lengths for an open element subtype (9.3.3.3; GHDL gna issue2131)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_aggelemlen_bad is
end entity;

architecture a of t_vhdl_aggelemlen_bad is
  type strings is array (natural range <>) of string;
  constant same : strings(0 to 1) := ("one", "two");
  constant ragged : strings(0 to 3) := ("one", "two", "three", "four");
begin
  process is
  begin
    assert same(1) = "two" severity failure;
    report "the model should have stopped when it started" severity failure;
    wait;
  end process;
end architecture;
