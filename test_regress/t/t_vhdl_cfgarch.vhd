-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "use entity work.sub(rca)" chooses the architecture as well as the entity
-- (7.3.2.1), in a configuration declaration and in a configuration
-- specification. It was read as the entity alone, and the default -- the
-- architecture analyzed last -- was built. Also an entity whose architecture
-- instantiates the entity itself, which its configuration binds to another
-- architecture (GHDL gna issue123).

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_cfgarch_sub is
  port (a : in bit; f : out bit);
end entity;
architecture rca of t_vhdl_cfgarch_sub is
begin
  f <= a;
end architecture;
architecture csa of t_vhdl_cfgarch_sub is
begin
  f <= not a;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_cfgarch_adder is
  generic (WIDTH : positive := 8);
  port (cin : in std_logic;
        a, b : in std_logic_vector(WIDTH - 1 downto 0);
        f : out std_logic_vector(WIDTH - 1 downto 0);
        cout : out std_logic);
end entity;
architecture rca of t_vhdl_cfgarch_adder is
  signal cin0 : unsigned(0 downto 0);
  signal fin : unsigned(WIDTH downto 0);
begin
  cin0(0) <= cin;
  fin <= resize(unsigned(a), WIDTH + 1) + resize(unsigned(b), WIDTH + 1) + cin0;
  f <= std_logic_vector(fin(WIDTH - 1 downto 0));
  cout <= fin(WIDTH);
end architecture;
architecture csa of t_vhdl_cfgarch_adder is
  component t_vhdl_cfgarch_adder is
    generic (WIDTH : positive);
    port (cin : in std_logic;
          a, b : in std_logic_vector(WIDTH - 1 downto 0);
          f : out std_logic_vector(WIDTH - 1 downto 0);
          cout : out std_logic);
  end component;
  signal f0, f1 : std_logic_vector(WIDTH - 1 downto 0);
  signal cout0, cout1 : std_logic;
begin
  add0 : t_vhdl_cfgarch_adder generic map (WIDTH => WIDTH)
    port map (cin => '0', a => a, b => b, f => f0, cout => cout0);
  add1 : t_vhdl_cfgarch_adder generic map (WIDTH => WIDTH)
    port map (cin => '1', a => a, b => b, f => f1, cout => cout1);
  cout <= cout1 when cin = '1' else cout0;
  f <= f1 when cin = '1' else f0;
end architecture;

configuration t_vhdl_cfgarch_csac of t_vhdl_cfgarch_adder is
  for csa
    for all : t_vhdl_cfgarch_adder
      use entity work.t_vhdl_cfgarch_adder(rca);
    end for;
  end for;
end configuration;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_cfgarch is
end entity;
architecture tb of t_vhdl_cfgarch is
  component t_vhdl_cfgarch_sub is
    port (a : in bit; f : out bit);
  end component;
  -- Configuration specifications, by label and for all the rest
  for u1 : t_vhdl_cfgarch_sub use entity work.t_vhdl_cfgarch_sub(rca);
  signal a : bit := '0';
  signal f1, f2, f3 : bit;
  signal cin : std_logic := '0';
  signal x, y, s : std_logic_vector(7 downto 0) := (others => '0');
  signal co : std_logic;
begin
  u1 : t_vhdl_cfgarch_sub port map (a, f1);
  u2 : t_vhdl_cfgarch_sub port map (a, f2);  -- The configuration says
  u3 : t_vhdl_cfgarch_sub port map (a, f3);  -- Unbound: the default, csa
  add : configuration work.t_vhdl_cfgarch_csac
    port map (cin => cin, a => x, b => y, f => s, cout => co);
  process
  begin
    wait for 1 ns;
    assert f1 = '0' report "u1 is not rca" severity failure;
    assert f2 = '0' report "u2 is not rca" severity failure;
    assert f3 = '1' report "u3 is not csa" severity failure;
    x <= x"5a";
    y <= x"c3";
    cin <= '1';
    wait for 1 ns;
    assert s = x"1e" and co = '1' report "5a + c3 + 1 wrong" severity failure;
    cin <= '0';
    wait for 1 ns;
    assert s = x"1d" and co = '1' report "5a + c3 wrong" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;

configuration t_vhdl_cfgarch_top of t_vhdl_cfgarch is
  for tb
    for u2 : t_vhdl_cfgarch_sub
      use entity work.t_vhdl_cfgarch_sub(rca);
    end for;
  end for;
end configuration;
