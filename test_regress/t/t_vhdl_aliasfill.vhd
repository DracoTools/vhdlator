-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "(others => '1')" and "(aData'range => '0')" assigned to an alias that
-- renumbers a whole variable parameter, "alias aData : std_logic_vector(DataLeft
-- downto 0) is Data" with DataLeft a constant of the subprogram, in the copy
-- made for an actual a generic sizes: as long as the object (6.6.2), which
-- elaboration settles. OSVVM's Axi4ModelPkg, FifoFillPkg and WishboneModelPkg.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aliasfill is
  generic (W : integer := 16);
end entity;

architecture t of t_vhdl_aliasfill is
  procedure fill(variable Data : inout std_logic_vector; ByteAddress : natural := 0) is
    constant DataLeft : integer := Data'length - 1;
    alias aData : std_logic_vector(DataLeft downto 0) is Data;
    variable Index : integer := ByteAddress * 8;
  begin
    aData := (others => '1');
    while Index <= DataLeft loop
      aData(Index + 7 downto Index) := x"5A";
      Index := Index + 8;
    end loop;
  end procedure;
  procedure clear(variable Data : out std_logic_vector) is
    constant DataLeft : integer := Data'length - 1;
    alias aData : std_logic_vector(DataLeft downto 0) is Data;
  begin
    aData := (aData'range => '0');
  end procedure;
begin
  process
    variable buf : std_logic_vector(W - 1 downto 0);
    variable h : std_logic_vector(15 downto 0);
  begin
    fill(buf, 1);
    report to_string(buf);
    assert buf = "0101101011111111" severity failure;
    fill(h);
    assert h = x"5A5A" severity failure;
    clear(buf);
    assert buf = x"0000" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
