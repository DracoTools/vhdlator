-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice that moves along an alias renumbering an inout variable parameter,
-- "aData(Index + 7 downto Index)" with "alias aData : std_logic_vector(DataLeft
-- downto 0) is Data", in the copy made for an actual a generic sizes: OSVVM's
-- FifoFillPkg PushWord and the AXI4 and Wishbone PushReadBurstByteData. The
-- subprogram only reads through the alias (6.6.2, 8.5).

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_aliasmoveslice is
  generic (W : integer := 32);
end entity;
architecture t of t_vhdl_aliasmoveslice is
  procedure push(v : std_logic_vector) is
  begin
    report "push " & to_hstring(v) & " len=" & integer'image(v'length);
    assert v'length = 8 severity failure;
    assert to_hstring(v) = "78" or to_hstring(v) = "56" or to_hstring(v) = "34"
      or to_hstring(v) = "12" or to_hstring(v) = "AB" severity failure;
  end procedure;
  procedure pushword(variable Data : inout std_logic_vector; ByteAddress : natural := 0) is
    variable Index : integer := ByteAddress * 8;
    constant DataLeft : integer := Data'length - 1;
    alias aData : std_logic_vector(DataLeft downto 0) is Data;
  begin
    while Index <= DataLeft loop
      push(aData(Index + 7 downto Index));
      Index := Index + 8;
    end loop;
  end procedure;
begin
  process
    variable buf : std_logic_vector(W - 1 downto 0) := x"12345678";
    variable h : std_logic_vector(15 downto 0) := x"ABCD";
  begin
    pushword(buf);
    pushword(h, 1);
    report "*-* All Finished *-*";
    wait;
  end process;
end;
