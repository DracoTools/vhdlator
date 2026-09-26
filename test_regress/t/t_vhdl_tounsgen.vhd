-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- TO_UNSIGNED and TO_SIGNED to a width a generic gives, "to_unsigned(n,
-- G)": the result is G elements long (numeric_std), and it was the 32-bit
-- integer, narrowed only by an assignment's own width -- which Verilator
-- reports as a truncation, and which a comparison does not do at all.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_tounsgen_sub is
  generic (G : natural);
end entity;

architecture a of t_vhdl_tounsgen_sub is
  signal s : std_logic_vector(G - 1 downto 0);
begin
  process
    variable v : std_logic_vector(G - 1 downto 0);
    variable sv : signed(G - 1 downto 0);
    variable n : natural := 3;
    variable m : integer := -2;
  begin
    v := std_logic_vector(to_unsigned(n, G));
    s <= std_logic_vector(to_unsigned(n + 1, G));
    sv := to_signed(m, G);
    wait for 1 ns;
    assert v = std_logic_vector(to_unsigned(3, G)) severity failure;
    assert unsigned(s) = to_unsigned(4, G) severity failure;
    assert to_integer(sv) = -2 and sv(G - 1) = '1' severity failure;
    assert to_signed(m, G) = sv severity failure;
    wait;
  end process;
end architecture;

entity t_vhdl_tounsgen is
end entity;

architecture a of t_vhdl_tounsgen is
begin
  u12 : entity work.t_vhdl_tounsgen_sub generic map (G => 12);
  u40 : entity work.t_vhdl_tounsgen_sub generic map (G => 40);

  process
  begin
    wait for 2 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
