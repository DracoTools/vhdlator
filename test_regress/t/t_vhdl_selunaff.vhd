-- DESCRIPTION: Verilator: VHDL concurrent selected assignment with unaffected
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- An alternative that is "unaffected" leaves the signal as it was; the others
-- still drive it (10.5.2.1, 11.6; GHDL gna issue2594). The whole statement
-- was dropped, and the signal never driven.

entity t_vhdl_selunaff is
end entity;

architecture a of t_vhdl_selunaff is
  signal y, a0, a1 : bit_vector(0 to 3) := (others => '0');
  signal sel : bit_vector(0 to 1) := "00";
  signal z : integer := 0;
  signal c : boolean := false;
begin
  with sel select
    y <= a0 when "00",
         a1 when "01",
         unaffected when others;

  z <= 5 when c else unaffected;

  process
  begin
    a0 <= "0001";
    a1 <= "0010";
    wait for 1 ns;
    assert y = "0001" severity failure;
    sel <= "01";
    wait for 1 ns;
    assert y = "0010" severity failure;
    sel <= "11";
    a1 <= "1111";
    wait for 1 ns;
    assert y = "0010" severity failure;
    assert z = 0 severity failure;
    c <= true;
    wait for 1 ns;
    assert z = 5 severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
