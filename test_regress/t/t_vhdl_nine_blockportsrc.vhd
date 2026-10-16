-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block's port of mode out is a signal of its own: it reads its own
-- driving value (14.7.3.3), not what the actual's sources add up to, and it
-- is one of those sources (14.7.3.2) -- driving its default where nothing in
-- the block drives it (GHDL gna issue328, NVC driver23)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_blockportsrc is
end entity;

architecture a of t_vhdl_nine_blockportsrc is
  type rec_t is record
    x : std_logic;
    y : std_logic_vector(7 downto 0);
  end record;
  signal r : rec_t;
  signal v : std_logic_vector(7 downto 0);
  signal u : std_logic;
  signal done : boolean_vector(0 to 1) := (others => false);
begin
  r <= ('1', X"Z0");
  v <= X"Z0";
  u <= 'Z';

  b1 : block is
    port (p : out rec_t);
    port map (r);
  begin
    process
    begin
      p <= ('0', X"5Z");
      wait for 0 ns;
      assert p = ('0', X"5Z") report "p=" & to_string(p.x) & to_string(p.y) severity failure;
      assert p'driving_value = ('0', X"5Z") severity failure;
      done(0) <= true;
      wait;
    end process;
  end block;

  b2 : block is
    port (q : out std_logic_vector(7 downto 0));
    port map (q => v);
  begin
    process
    begin
      q <= X"5Z";
      wait for 0 ns;
      assert q = X"5Z" report "q=" & to_string(q) severity failure;
      done(1) <= true;
      wait;
    end process;
  end block;

  -- Nothing inside drives the port: it drives its default
  b3 : block is
    port (d : out std_logic := '0');
    port map (d => u);
  begin
  end block;

  process
  begin
    wait for 1 ns;
    assert r = ('X', X"50") report "r=" & to_string(r.x) & to_string(r.y) severity failure;
    assert v = X"50" report "v=" & to_string(v) severity failure;
    assert u = '0' report "u=" & to_string(u) severity failure;
    assert done = (true, true) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
