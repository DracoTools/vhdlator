-- DESCRIPTION: Verilator: The nine values of STD_ULOGIC: defaults, the logic tables, resolution, edges
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_logic is
end entity;

architecture a of t_vhdl_nine_logic is
  signal u : std_logic;                       -- starts 'U'
  signal v : std_logic_vector(3 downto 0);    -- starts "UUUU"
  signal r : std_logic;                       -- two drivers
  signal clk : std_logic := '0';
  signal edges : natural := 0;
begin
  r <= '1';
  r <= 'Z', '0' after 2 ns, 'L' after 4 ns;

  process (clk) is
  begin
    if rising_edge(clk) then
      edges <= edges + 1;
    end if;
  end process;

  process is
    variable x : std_logic_vector(3 downto 0);
    variable n : unsigned(3 downto 0);
  begin
    report "u=" & std_logic'image(u) & " v=" & to_string(v);
    assert u = 'U' severity failure;
    assert v = "UUUU" severity failure;
    assert is_x(v) severity failure;
    x := "01XZ";
    assert (x and "1111") = "01XX" severity failure;
    assert (x or "0000") = "01XX" severity failure;
    assert (not x) = "10XX" severity failure;
    assert to_x01(std_logic_vector'("LHWZ")) = "01XX" severity failure;
    assert std_match(std_logic_vector'("1-0"), std_logic_vector'("110")) severity failure;
    assert not std_match(std_logic_vector'("1X0"), std_logic_vector'("110")) severity failure;
    x := "1H0L";
    assert x /= "1100" severity failure;
    assert to_x01(x) = "1100" severity failure;
    n := unsigned(std_logic_vector'("01H0"));
    assert n + 1 = "0111" severity failure;
    n := "0U01";
    report "n+1=" & to_string(n + 1);
    wait for 1 ns;
    report "r=" & std_logic'image(r);
    assert r = '1' severity failure;
    wait for 2 ns;
    report "r=" & std_logic'image(r);
    assert r = 'X' severity failure;
    wait for 2 ns;
    assert r = '1' severity failure;
    case x is
      when "1H0L" => report "case exact";
      when others => assert false severity failure;
    end case;
    clk <= 'H'; wait for 1 ns;
    clk <= 'L'; wait for 1 ns;
    clk <= '1'; wait for 1 ns;
    clk <= 'X'; wait for 1 ns;
    clk <= '1'; wait for 1 ns;
    report "edges=" & integer'image(edges);
    assert edges = 2 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
