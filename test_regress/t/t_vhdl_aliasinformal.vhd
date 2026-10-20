-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An alias that renumbers a formal of mode in written with no class, which
-- is a constant (4.2.2.1), given a slice whose bounds the instance settles:
-- OSVVM's MemRead takes "Addr : in std_logic_vector", renumbers it "alias
-- aAddr : std_logic_vector(Addr'length - 1 downto 0) is Addr" and takes
-- "aAddr(BlockWidth - 1 downto 0)" as the word. The alias was given no
-- carrier of its own, as one that might be written, so its slices counted
-- in the actual's numbering, and every read of the Wishbone subordinate's
-- registers found another word.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_aliasinformal_sub is
  port (Adr : in std_logic_vector);
  constant ADDR_WIDTH : integer := Adr'length;
  constant BYTE_ADDR_WIDTH : integer := 2;
  constant WORD_ADDR_WIDTH : integer := ADDR_WIDTH - BYTE_ADDR_WIDTH;
  subtype WordAddrRange is natural range WORD_ADDR_WIDTH - 1 downto BYTE_ADDR_WIDTH;
end;

architecture a of t_vhdl_aliasinformal_sub is
  procedure word_of(Addr : in std_logic_vector; Word : out integer; Low : out std_logic_vector) is
    variable BlockWidth : integer;
    alias aAddr : std_logic_vector(Addr'length - 1 downto 0) is Addr;
  begin
    BlockWidth := 10;
    Word := to_integer(unsigned(aAddr(BlockWidth - 1 downto 0)));
    Low := aAddr(7 downto 0);
  end procedure;
begin
  process
    variable w : integer;
    variable l : std_logic_vector(7 downto 0);
  begin
    wait on Adr;
    word_of(Adr(WordAddrRange), w, l);
    report "word " & integer'image(w) & " low " & to_hstring(l);
    assert w = 16#0F# and l = x"0F" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasinformal is end;

architecture t of t_vhdl_aliasinformal is
  signal adr : std_logic_vector(31 downto 0) := x"00000000";
begin
  s : entity work.t_vhdl_aliasinformal_sub port map (Adr => adr);
  adr <= x"8000003C" after 5 ns;
end;
