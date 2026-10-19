-- DESCRIPTION: Verilator: VHDL run-time check: an index outside its array in a concurrent assignment (8.4; GHDL gna ticket9)
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

entity t_vhdl_concindex_bad is
end entity;

architecture a of t_vhdl_concindex_bad is
  signal v : bit_vector(15 downto 0) := x"8001";
  signal i : integer := 15;
  signal b : bit;
begin
  b <= v(i);
  process is
  begin
    wait for 1 ns;
    assert b = '1' severity failure;
    i <= 16;
    wait for 1 ns;
    report "v(16) should have stopped the run" severity failure;
    wait;
  end process;
end architecture;
