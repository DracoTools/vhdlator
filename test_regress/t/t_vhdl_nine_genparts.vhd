-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A std_logic_vector driven in parts whose bounds a generic gives, PoC's
-- sort_lru_cache: "MovesUpCond(ELEMENTS downto 1) <= UnEqual; MovesUpCond(0)
-- <= Free". Analysis cannot tell the parts apart, so it counts two drivers of
-- the whole, which were refused ("resolved by a function of its own"). Each
-- element has one driver, and the value is that driver's, weak and high
-- impedance values included.

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nine_genparts_sub is
  generic (ELEMENTS : positive := 4);
  port (ue : in std_logic_vector(ELEMENTS - 1 downto 0); free : in std_logic;
        o : out std_logic_vector(ELEMENTS + 1 downto 0));
end;
architecture rtl of t_vhdl_nine_genparts_sub is
  signal MovesUpCond : std_logic_vector(ELEMENTS + 1 downto 0);
begin
  MovesUpCond(ELEMENTS downto 1) <= ue;
  MovesUpCond(0) <= free;
  MovesUpCond(ELEMENTS + 1) <= not free;
  o <= MovesUpCond;
end;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_nine_genparts is end;
architecture t of t_vhdl_nine_genparts is
  signal ue : std_logic_vector(3 downto 0);
  signal free : std_logic;
  signal o : std_logic_vector(5 downto 0);
begin
  u : entity work.t_vhdl_nine_genparts_sub generic map (ELEMENTS => 4)
    port map (ue => ue, free => free, o => o);
  process begin
    wait for 0 ns;
    report "o0=" & to_string(o);
    assert o = "UUUUUU" severity failure;
    ue <= "1010"; free <= '1';
    wait for 1 ns;
    report "o1=" & to_string(o);
    assert o = "010101" severity failure;
    ue <= "ZLH-"; free <= 'Z';
    wait for 1 ns;
    report "o2=" & to_string(o);
    assert o = "XZLH-Z" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
