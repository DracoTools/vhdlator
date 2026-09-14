-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An aggregate given to a port whose length is the instance's own generic.
-- The aggregate has no length of its own, so it takes the port's, and the
-- port's names a generic that has no object in the instantiating module --
-- the generic map is what says its value there (6.5.7.1).

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity child is
  generic (SIZE : natural := 8);
  port (a : in  std_logic_vector(SIZE - 1 downto 0);
        b : in  std_logic_vector(SIZE - 1 downto 0);
        y : out std_logic_vector(SIZE - 1 downto 0));
end entity;

architecture rtl of child is
begin
  y <= a or b;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_portagg is
end entity;

architecture rtl of t_vhdl_portagg is
  constant W : natural := 5;
  signal q   : std_logic_vector(W - 1 downto 0);
  signal ones : std_logic_vector(W - 1 downto 0) := (others => '1');
begin
  u : entity work.child
    generic map (SIZE => W)
    port map (a => (others => '0'), b => ones, y => q);

  process is
  begin
    wait for 1 ns;
    assert q'length = 5 report "len " & integer'image(q'length) severity failure;
    assert q = "11111" report "q wrong" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;
end architecture;
