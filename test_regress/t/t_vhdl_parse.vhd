-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A cross-section of the VHDL-2008 grammar, so that a change in how any
-- construct is parsed shows up as a diff in the golden parse tree.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package p is
  type state_t is (idle, running, done);
  type byte_array_t is array (natural range <>) of std_logic_vector(7 downto 0);
  type rec_t is record
    a : std_logic;
    b : integer range 0 to 15;
  end record;
  subtype nibble_t is std_logic_vector(3 downto 0);
  constant K : integer := 16#FF#;
  function add1(x : integer) return integer;
  procedure bump(signal s : inout integer; constant n : in integer);
  attribute mark : boolean;
end package;

package body p is
  function add1(x : integer) return integer is
    variable v : integer := x;
  begin
    v := v + 1;
    return v;
  end function add1;

  procedure bump(signal s : inout integer; constant n : in integer) is
  begin
    s <= s + n;
  end procedure;
end package body;

entity t is
  generic (WIDTH : positive := 8;
           NAME  : string := "top");
  port (clk  : in  std_logic;
        rst  : in  std_logic;
        d    : in  std_logic_vector(WIDTH-1 downto 0);
        q    : out std_logic_vector(WIDTH-1 downto 0);
        busy : out std_logic);
end entity t;

architecture rtl of t is
  signal cnt   : unsigned(WIDTH-1 downto 0) := (others => '0');
  signal state : work.p.state_t := idle;
  signal sel   : std_logic_vector(1 downto 0);
  component sub is
    generic (N : integer);
    port (a : in std_logic; y : out std_logic);
  end component;
  for all : sub use entity work.sub(rtl);
begin
  -- A clocked process with an asynchronous reset
  seq : process (clk, rst) is
  begin
    if rst = '1' then
      cnt   <= (others => '0');
      state <= idle;
    elsif rising_edge(clk) then
      case state is
        when idle =>
          if d /= (d'range => '0') then state <= running; end if;
        when running =>
          cnt <= cnt + 1;
          for i in 0 to 3 loop
            exit when cnt(i) = '1';
          end loop;
          state <= done;
        when others =>
          state <= idle;
      end case;
    end if;
  end process seq;

  -- Concurrent forms
  q <= std_logic_vector(cnt);
  busy <= '1' when state = running else '0';
  with sel select
    q(0) <= d(0) when "00",
            d(1) when "01",
            '0'  when others;

  gen : for i in 0 to WIDTH-1 generate
    inst : component sub generic map (N => i) port map (a => d(i), y => open);
  end generate gen;

  cond : if WIDTH > 4 generate
    assert WIDTH mod 2 = 0 report "even only" severity warning;
  else generate
    assert false severity note;
  end generate cond;

  -- A process with waits rather than a sensitivity list
  stim : process is
    variable l : integer := 0;
  begin
    wait until rising_edge(clk);
    wait for 10 ns;
    wait on d, rst until rst = '0';
    l := l + 1;
    report "tick " & integer'image(l) severity note;
    wait;
  end process stim;
end architecture rtl;
