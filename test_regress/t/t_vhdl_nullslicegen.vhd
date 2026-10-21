-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A pipeline shifted by "p(2 to L) <= p(1 to L - 1)": in the instance where
-- the generic L is 1 both slices are null and the assignment does nothing
-- (5.2.1, 10.5.2.1), so p(1) keeps what the statement before gave it.
-- open-logic's RAMs pipe their read valid that way, latency 1 by default.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nullslicegen_m is
  generic (L : positive := 1);
  port (clk, en : in std_logic; v : out std_logic; vv : out std_logic);
end entity;
architecture a of t_vhdl_nullslicegen_m is
  signal p : std_logic_vector(1 to L) := (others => '0');
begin
  process (clk)
    variable q : std_logic_vector(L downto 1) := (others => '0');
  begin
    if rising_edge(clk) then
      p(1) <= en;
      p(2 to L) <= p(1 to L - 1);
      q(L downto 2) := q(L - 1 downto 1);
      q(1) := en;
      vv <= q(L);
    end if;
  end process;
  v <= p(L);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nullslicegen is
end entity;
architecture a of t_vhdl_nullslicegen is
  signal clk, en : std_logic := '0';
  signal v1, v3, vv1, vv3 : std_logic;
begin
  clk <= not clk after 5 ns;
  u1 : entity work.t_vhdl_nullslicegen_m generic map (L => 1) port map (clk, en, v1, vv1);
  u3 : entity work.t_vhdl_nullslicegen_m generic map (L => 3) port map (clk, en, v3, vv3);
  process
  begin
    en <= '1';
    wait until rising_edge(clk);
    wait for 1 ns;
    assert v1 = '1' and v3 = '0' report "after 1: v1 v3 " & std_logic'image(v1)
      & std_logic'image(v3) severity failure;
    assert vv1 = '1' and vv3 = '0' report "after 1: vv1 vv3 " & std_logic'image(vv1)
      & std_logic'image(vv3) severity failure;
    wait until rising_edge(clk);
    wait until rising_edge(clk);
    wait for 1 ns;
    assert v3 = '1' and vv3 = '1' report "after 3: v3 vv3 " & std_logic'image(v3)
      & std_logic'image(vv3) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
