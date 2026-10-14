-- DESCRIPTION: Verilator: numeric_std over the nine values: arithmetic, comparisons and conversions
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_numeric is
  generic (G : positive := 6);
end entity;

architecture a of t_vhdl_nine_numeric is
  signal cnt : unsigned(G - 1 downto 0);
  signal clk : std_logic := '0';
  signal rst : std_logic := '0';
begin
  process (clk) is
  begin
    if rising_edge(clk) then
      if rst = '1' then
        cnt <= (others => '0');
      else
        cnt <= cnt + 1;
      end if;
    end if;
  end process;

  process is
    variable u : unsigned(7 downto 0);
    variable s : signed(7 downto 0);
    variable n : integer;
    variable w : unsigned(11 downto 0);
  begin
    u := x"F0";
    assert u + 1 = x"F1" severity failure;
    assert u + 16 = x"00" severity failure;
    assert u - 1 = x"EF" severity failure;
    assert 1 - u = x"11" severity failure;
    assert u + u = x"E0" severity failure;
    assert u > 200 and u < 241 and u >= 240 and u <= 240 and u = 240 and u /= 241 severity failure;
    assert not (u = 1000) severity failure;
    assert u < 1000 severity failure;
    s := to_signed(-5, 8);
    assert to_integer(s) = -5 severity failure;
    assert s + 7 = 2 severity failure;
    assert s - 7 = -12 severity failure;
    assert s < 0 and s > -6 and s >= -5 severity failure;
    assert -s = 5 and abs(s) = 5 severity failure;
    assert to_integer(u) = 240 severity failure;
    n := 77;
    u := to_unsigned(n, 8);
    assert u = x"4D" severity failure;
    assert to_integer(u) = 77 severity failure;
    w := resize(u, 12);
    assert w = x"04D" severity failure;
    assert resize(s, 4) = "1011" severity failure;
    assert resize(s, 12) = x"FFB" severity failure;
    u := "0000HL1L";
    assert u = 10 severity failure;
    assert u + 1 = 11 severity failure;
    u := "000X0000";
    assert to_integer(u) = 0 severity failure;
    report "u+1=" & to_string(u + 1);
    assert not (u = 16) severity failure;
    assert u /= 16 severity failure;
    assert not (u < 100) severity failure;
    report "cnt=" & to_string(cnt);
    clk <= '1'; wait for 1 ns;
    report "cnt=" & to_string(cnt);
    rst <= '1'; clk <= '0'; wait for 1 ns; clk <= '1'; wait for 1 ns; rst <= '0';
    for i in 1 to 3 loop
      clk <= '0'; wait for 1 ns; clk <= '1'; wait for 1 ns;
    end loop;
    report "cnt=" & to_string(cnt);
    assert cnt = 3 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
