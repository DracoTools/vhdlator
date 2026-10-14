-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, an unbound instance of a component whose out
-- port is sized by the component's own generics: the actual takes the
-- port's default value (12.4.3), 'U' in each element, which needs the
-- port's length as this instance's generic map gives it (gna issue44).
-- nvc 1.22.1 and GHDL 7.0.0-dev answer this way; Questa 2025.2 leaves each
-- actual at its own initial value

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_unbound is
end entity;

architecture a of t_vhdl_nine_unbound is
  component no_such_multiplier is
    generic (a_size, b_size : integer);
    port (a : in signed(a_size - 1 downto 0);
          b : in signed(b_size - 1 downto 0);
          p : out signed(a_size + b_size - 2 downto 0);
          q : out std_logic_vector(a_size - 1 downto 0) := (others => 'H'));
  end component;
  constant ca : signed(3 downto 0) := "0101";
  signal sb : signed(5 downto 0) := "001100";
  signal sp : signed(8 downto 0) := (others => '0');
  signal sq : std_logic_vector(3 downto 0) := (others => '0');
begin
  m0 : no_such_multiplier
    generic map (a_size => ca'length, b_size => sb'length)
    port map (ca, sb, sp, sq);

  process
  begin
    wait for 1 ns;
    report to_string(sp) & " " & to_string(sq);
    assert to_string(sp) = "UUUUUUUUU" severity failure;
    assert sq = "HHHH" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
