-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

package meta_pkg is
  generic (item_w : natural);
  constant field_w : natural := item_w / 8;
  subtype word_t is bit_vector(item_w - 1 downto 0);
  constant all_ones : word_t := (others => '1');
end package;

-- ---------------------------------------------------------------------------

entity sub is
  generic (item_w : natural := 16);
end entity;

architecture rtl of sub is
  -- The generic's value is a generic of this entity: not a number during
  -- analysis, but fixed once the instance is elaborated (4.9)
  package pkg_i is new work.meta_pkg generic map (item_w => item_w);
  use pkg_i.all;

  signal w : word_t := (others => '0');
begin
  check : process is
  begin
    assert field_w = item_w / 8 report "field_w" severity failure;
    assert w'length = item_w report "word length" severity failure;
    w <= all_ones;
    wait for 1 ns;
    assert w(item_w - 1) = '1' report "all ones high" severity failure;
    assert w(0) = '1' report "all ones low" severity failure;
    report "sub ok";
    wait;
  end process;
end architecture;

-- ---------------------------------------------------------------------------

entity t_vhdl_pkggen is
end entity;

architecture rtl of t_vhdl_pkggen is
begin
  u16 : entity work.sub generic map (item_w => 16);
  u32 : entity work.sub generic map (item_w => 32);

  main : process is
  begin
    wait for 2 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
