-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A binding indication associates the *entity's* generics, and its actuals
-- may name the *component's* own -- which hold what the instance gave them,
-- or the defaults the component declared (3.4.2, 7.3.2.2). Written
-- positionally, the Nth association is the Nth generic of the entity, whose
-- names need not be the component's.

entity t_vhdl_bindgen_leaf is
  generic (ew1 : integer := 1; ew2 : integer := 1);
  port (o : out integer);
end entity;
architecture rtl of t_vhdl_bindgen_leaf is
begin
  o <= 100 * ew1 + ew2;
end architecture;

use std.env.all;

entity t_vhdl_bindgen is
end entity;

architecture tb of t_vhdl_bindgen is
  component sock is
    generic (cg1 : integer := 2; cg2 : integer := 3);
    port (o : out integer);
  end component;

  -- Named, reading the component's generics
  for i1 : sock use entity work.t_vhdl_bindgen_leaf(rtl)
    generic map (ew1 => cg1 + cg1, ew2 => cg2);
  -- Positional: ew1 then ew2, not cg1 then cg2
  for i2 : sock use entity work.t_vhdl_bindgen_leaf(rtl)
    generic map (cg1, cg2 + 4);

  signal s1, s2 : integer;
begin

  i1 : sock port map (o => s1);
  i2 : sock generic map (cg1 => 5) port map (o => s2);

  main : process is
  begin
    wait for 1 ns;
    -- cg1 defaults to 2, so ew1 is 4 and ew2 is cg2's default 3
    assert s1 = 403 report "s1 is " & integer'image(s1) severity failure;
    -- the instance gives cg1 => 5, so ew1 is 5 and ew2 is 3 + 4
    assert s2 = 507 report "s2 is " & integer'image(s2) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
