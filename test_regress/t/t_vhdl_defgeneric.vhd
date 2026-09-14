-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A deferred constant is declared in the package and given its value in the
-- package body, and it is that second declaration which says what the value
-- is (6.4.2.2).  A generic handed one, and a type in the architecture
-- written in terms of that generic, both need the value to be found.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package pk is
  constant POLY : natural := 121;
  function no_bits(value_in : natural) return natural;
  -- Deferred: nothing here says what they are
  constant DEPTH  : natural;
  constant STATES : natural;
end package;

package body pk is
  -- Written so that only the back end can fold it: it builds a vector and
  -- reads it a bit at a time, which the frontend's own evaluator does not do
  function no_bits(value_in : natural) return natural is
    variable v : unsigned(31 downto 0);
  begin
    if value_in = 0 then return 0; end if;
    v := to_unsigned(value_in, 32);
    for i in 31 downto 0 loop
      if v(i) = '1' then return i + 1; end if;
    end loop;
    return 1;
  end function;

  constant DEPTH  : natural := no_bits(POLY) - 1;   -- 6
  constant STATES : natural := 2 ** DEPTH;          -- 64
end package body;

-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;

entity ram is
  generic ( WORDS : integer := 8; BITWIDTH : integer := 8 );
  port ( clk : in std_logic;
         d   : in  std_logic_vector(BITWIDTH - 1 downto 0);
         q   : out std_logic_vector(BITWIDTH - 1 downto 0) );
end entity;

architecture rtl of ram is
  -- A type of the architecture's own, sized by the generic
  type t_ram is array (WORDS - 1 downto 0) of std_logic_vector(BITWIDTH - 1 downto 0);
  signal mem : t_ram := (others => (others => '0'));
begin
  process (clk) is
  begin
    if rising_edge(clk) then
      mem(0) <= d;
    end if;
  end process;
  q <= mem(0);
end architecture;

-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use work.pk.all;
use std.env.all;

entity t_vhdl_defgeneric is
end entity;

architecture tb of t_vhdl_defgeneric is
  signal clk : std_logic := '0';
  signal d, q : std_logic_vector(STATES - 1 downto 0) := (others => '0');
begin

  u : entity work.ram
    generic map ( WORDS => 16, BITWIDTH => STATES )
    port map ( clk => clk, d => d, q => q );

  main : process is
  begin
    assert DEPTH = 6 report "the deferred constant did not take its value" severity failure;
    assert STATES = 64 report "a constant naming a deferred one is wrong" severity failure;
    assert d'length = 64 report "a signal sized by it is the wrong width" severity failure;
    d <= (others => '1');
    wait for 1 ns;
    clk <= '1';
    wait for 1 ns;
    assert q = d report "the instance did not carry the value through" severity failure;
    report "*-* All Finished *-*";
    finish;
  end process;

end architecture;
