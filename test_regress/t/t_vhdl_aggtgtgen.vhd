-- DESCRIPTION: Verilator: VHDL aggregate target whose element a generic sizes
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

-- A delay line of vectors as long as a generic says, shifted by an
-- aggregate target: the last stage goes to o, the rest move along
entity t_vhdl_aggtgtgen_sub is
  generic (delay : positive);
  port (
    clk : in std_logic;
    i : in std_logic_vector;
    o : out std_logic_vector
  );
end entity;

architecture rtl of t_vhdl_aggtgtgen_sub is
  type delay_t is array (natural range <>) of std_logic_vector;
  signal d : delay_t(delay - 1 downto 0)(i'range) := (others => (others => '0'));
begin
  (o, d) <= d & i when rising_edge(clk);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_aggtgtgen is
end entity;

architecture a of t_vhdl_aggtgtgen is
  signal clk : std_logic := '0';
  signal i, o1, o3 : std_logic_vector(7 downto 0);
begin
  u1 : entity work.t_vhdl_aggtgtgen_sub generic map (delay => 1)
    port map (clk => clk, i => i, o => o1);
  u3 : entity work.t_vhdl_aggtgtgen_sub generic map (delay => 3)
    port map (clk => clk, i => i, o => o3);

  process
  begin
    for j in 1 to 8 loop
      i <= std_logic_vector(to_unsigned(j, 8));
      clk <= '0';
      wait for 5 ns;
      clk <= '1';
      wait for 5 ns;
      -- One stage of d, then o: j - 1 out; three stages: j - 3 out
      if j >= 2 then
        assert o1 = std_logic_vector(to_unsigned(j - 1, 8)) severity failure;
      end if;
      if j >= 4 then
        assert o3 = std_logic_vector(to_unsigned(j - 3, 8)) severity failure;
      end if;
    end loop;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
