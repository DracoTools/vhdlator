-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block statement with generics and ports of its own. A block is not a
-- module: it is elaborated where it stands, so each generic is a constant
-- and each port a signal wired to its actual. A guard expression declares a
-- signal GUARD, and a guarded assignment drives only while it is true.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_block is
end entity;

architecture sim of t_vhdl_block is
  type rec_t is record
    f : integer_vector;
  end record;
  signal x, y, z : integer;
  signal en : std_logic := '0';
  signal d : std_logic_vector(3 downto 0) := "0001";
  signal q : std_logic_vector(3 downto 0) := "0000";
  signal g : boolean := false;
  signal r : rec_t(f(1 to 3)) := (f => (others => -1));
  signal bv : bit_vector(3 downto 0) := "0000";
  signal nones : integer := 0;

  function to_bit (v : boolean) return bit is
  begin
    if v then return '1'; else return '0'; end if;
  end function;

  function ones (v : bit_vector) return integer is
    variable n : integer := 0;
  begin
    for i in v'range loop
      if v(i) = '1' then n := n + 1; end if;
    end loop;
    return n;
  end function;
begin

  latch : block (en = '1') is
  begin
    q <= guarded d;
    g <= GUARD;                     -- the guard is a signal of the block
  end block;

  -- A conversion written on the actual converts what reaches the port, and
  -- one written on the formal converts what the port drives on its way out
  -- (6.5.7.1)
  converted : block is
    port (bp : in bit; bo : out bit_vector(3 downto 0));
    port map (bp => to_bit(x > 0), bo => bv);
  begin
    bo <= "1111" when bp = '1' else "0000";
  end block;

  counted : block is
    port (cp : out bit_vector(3 downto 0));
    port map (ones(cp) => nones);
  begin
    cp <= bv;
  end block;

  scaled : block is
    generic (factor : integer);
    generic map (factor => 5);
    port (i : in integer; o : out integer);
    port map (i => x, o => y);
  begin
    o <= i * factor;
  end block;

  -- A port of mode "inout" is another name for the signal it is mapped to,
  -- and a record port takes the constraint of that signal (11.2, 5.3.3)
  paired : block is
    port (p : inout rec_t);
    port map (p => r);
  begin
    p <= (f => (1, 2, 3));
  end block;

  -- A plain block is just a declarative region
  plain : block is
    signal t : integer;
  begin
    t <= y + 1;
    z <= t;
  end block;

  stim : process is
  begin
    x <= 1;
    wait for 1 ns;
    assert y = 5 report "block port or generic is wrong" severity failure;
    assert z = 6 report "plain block is wrong" severity failure;
    assert r.f = (1, 2, 3) report "inout block port did not drive" severity failure;
    x <= 10;
    wait for 1 ns;
    assert y = 50 report "block did not follow its input" severity failure;
    assert z = 51 report "plain block did not follow" severity failure;
    -- The latch holds while its guard is false and follows while it is true
    assert q = "0000" report "guarded assignment drove while closed" severity failure;
    assert not g report "GUARD is not readable" severity failure;
    en <= '1';
    wait for 1 ns;
    assert q = "0001" and g report "guarded assignment did not open" severity failure;
    d <= "1010";
    wait for 1 ns;
    assert q = "1010" report "guarded assignment did not follow" severity failure;
    en <= '0';
    wait for 1 ns;
    d <= "0110";
    wait for 1 ns;
    assert q = "1010" report "guarded assignment did not hold" severity failure;

    -- x was set above, so the conversion on the actual made bp '1', and the
    -- conversion on the formal counted the bits the block drove
    assert bv = "1111" report "conversion on the actual" severity failure;
    assert nones = 4 report "conversion on the formal, got " & integer'image(nones)
      severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
