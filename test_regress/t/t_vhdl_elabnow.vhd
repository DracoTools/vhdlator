-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- NOW is 0 ns while the design elaborates (14.4.2): a constant an architecture
-- declares with it is static, and sizes what follows (GHDL gna issue648)

entity t_vhdl_elabnow is
end entity;

architecture a of t_vhdl_elabnow is
  constant L : positive := 9 + now / 1 ns;
  signal s : bit_vector(31 downto 0);
  alias part : bit_vector(L - 1 downto 2) is s(L - 1 downto 2);
  signal b : bit_vector(L - 1 downto 2) := (others => '1');
begin
  part <= b;
  process is
  begin
    wait for 5 ns;
    assert now = 5 ns severity failure;
    assert L = 9 and part'length = 7 severity failure;
    assert s(8 downto 2) = "1111111" and s(9) = '0' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
