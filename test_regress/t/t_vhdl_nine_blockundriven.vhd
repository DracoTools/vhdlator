-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, a block's out port that nothing in the block
-- drives is still the one source of the signal associated with it: the
-- signal is the port's default from the start, or 'U' where the port has
-- none, whatever the signal's own declaration says (gna issue328)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_blockundriven is
end entity;

architecture a of t_vhdl_nine_blockundriven is
  signal s0 : std_logic;
  signal su : std_logic := '0';
  signal sv : std_logic_vector(3 downto 0) := "0000";
  signal sd : std_logic := '1';
begin
  b0 : block
    port (p : out std_logic := '0');
    port map (p => s0);
  begin
  end block;

  bu : block
    port (p : out std_logic);
    port map (p => su);
  begin
  end block;

  bv : block
    port (p : out std_logic_vector(3 downto 0) := "HL1Z");
    port map (p => sv);
  begin
  end block;

  bd : block
    port (p : out std_logic := 'L');
    port map (p => sd);
  begin
    p <= '1' after 2 ns;
  end block;

  assert s0 = '0' severity failure;

  process
  begin
    wait for 1 ns;
    report std_logic'image(s0) & " " & std_logic'image(su) & " " & to_string(sv) & " "
      & std_logic'image(sd);
    assert s0 = '0' severity failure;
    assert su = 'U' severity failure;
    assert sv = "HL1Z" severity failure;
    assert sd = 'L' severity failure;
    wait for 2 ns;
    report std_logic'image(sd);
    assert sd = '1' severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
