-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice as long as a generic of an alias that renumbers a parameter:
-- OSVVM's CalculateByteAddress takes "aAddr(AddrBitsPerWord downto 1)" of
-- "alias aAddr : std_logic_vector(Address'length downto 1) is Address",
-- given a generic. Made a value that carries its bounds for to_string, the
-- slice was as long as the alias, its other elements 'U'.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_aliasgenwidth_sub is
  generic (G : integer := 2; EXPECT : string := "");
end;

architecture a of t_vhdl_aliasgenwidth_sub is
  function low(constant Address : in std_logic_vector; constant n : in integer) return string is
    alias aAddr : std_logic_vector(Address'length downto 1) is Address;
  begin
    return to_string(aAddr(n downto 1));
  end function;
  function lowint(constant Address : in std_logic_vector; constant n : in integer) return integer is
    alias aAddr : std_logic_vector(Address'length downto 1) is Address;
  begin
    return to_integer(unsigned(aAddr(n downto 1)));
  end function;
begin
  process
    variable v : std_logic_vector(31 downto 0) := x"AAAAAAA6";
  begin
    report "low " & low(v, G) & " " & integer'image(lowint(v, G));
    assert low(v, G) = EXPECT severity failure;
    assert lowint(v, G) = to_integer(unsigned(v(G - 1 downto 0))) severity failure;
    wait;
  end process;
end;

entity t_vhdl_aliasgenwidth is end;

architecture t of t_vhdl_aliasgenwidth is
begin
  u2 : entity work.t_vhdl_aliasgenwidth_sub generic map (EXPECT => "10");
  u3 : entity work.t_vhdl_aliasgenwidth_sub generic map (G => 3, EXPECT => "110");
  u5 : entity work.t_vhdl_aliasgenwidth_sub generic map (G => 5, EXPECT => "00110");
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
