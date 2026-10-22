-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- MAXIMUM and MINIMUM over a generic of an open array type, whose bounds
-- each instance gives (5.3.2.4): the constant they make sizes a vector.
-- Shaped after surf's Scrambler, "constant SCRAMBLER_WIDTH_C : integer :=
-- maximum(TAPS_G)".

entity t_vhdl_maxopengen_child is
  generic (TAPS_G : integer_vector := (0 => 39, 1 => 58));
  port (width, least : out integer);
end entity;

architecture rtl of t_vhdl_maxopengen_child is
  constant WIDTH_C : integer := maximum(TAPS_G);
  constant LEAST_C : integer := minimum(TAPS_G);
  signal s : bit_vector(WIDTH_C - 1 downto 0) := (others => '0');
begin
  width <= s'length;
  least <= LEAST_C;
end architecture;

entity t_vhdl_maxopengen is
end entity;

architecture rtl of t_vhdl_maxopengen is
  signal w1, l1, w2, l2, w3, l3 : integer;
begin
  u1 : entity work.t_vhdl_maxopengen_child port map (width => w1, least => l1);
  u2 : entity work.t_vhdl_maxopengen_child generic map (TAPS_G => (3, 7, 5))
    port map (width => w2, least => l2);
  u3 : entity work.t_vhdl_maxopengen_child generic map (TAPS_G => (5 => 12, 4 => -2, 3 => 6))
    port map (width => w3, least => l3);
  process
  begin
    wait for 1 ns;
    report integer'image(w1) & " " & integer'image(l1) & " " & integer'image(w2) & " "
      & integer'image(l2) & " " & integer'image(w3) & " " & integer'image(l3);
    assert w1 = 58 and l1 = 39 report "u1" severity failure;
    assert w2 = 7 and l2 = 3 report "u2" severity failure;
    assert w3 = 12 and l3 = -2 report "u3" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
