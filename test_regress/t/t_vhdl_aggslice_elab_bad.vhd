-- DESCRIPTION: Verilator: VHDL elaboration check: a slice of an aggregate given a value of another length (9.3.3.3; GHDL gna issue2066)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_aggslice_elab_bad is
end entity;

architecture a of t_vhdl_aggslice_elab_bad is
  signal vec : bit_vector(7 downto 0);
begin
  vec <= (3 downto 0 => "111", others => '0');
  process is
  begin
    wait for 1 ns;
    report "the model should have stopped when it started" severity failure;
    wait;
  end process;
end architecture;
