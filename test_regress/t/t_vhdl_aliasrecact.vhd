-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Aliases that renumber a parameter, in the copy made for an actual that is
-- an element of a record whose subtype the port gives -- "variable LocalWD :
-- AxiBus.WriteData'subtype" in OSVVM's AXI4 subordinates, handed to
-- Axi4ModelPkg's AlignDataBusToBytes and CalculateByteAddress. The copy's
-- bounds are the element's, which the instance settles, direction included:
-- - a slice of an alias given a carrier of its own was put back onto the
--   object's numbering as well ("Array lengths do not match");
-- - an index of an alias whose object runs the way the instance says was
--   put onto it the other way round;
-- - a slice of an alias as long as a generic was as long as the alias.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_aliasrecact_pkg is
  type wa_t is record
    Valid : std_logic;
    Addr  : std_logic_vector;
  end record;
  type wd_t is record
    Valid : std_logic;
    Data  : std_logic_vector;
  end record;
  type bus_t is record
    WriteAddress : wa_t;
    WriteData    : wd_t;
  end record;
  function AlignDataBusToBytes(constant Data : in std_logic_vector; constant DataWidth : in integer;
                               constant ByteAddr : in integer) return std_logic_vector;
  function CalculateByteAddress(constant Address : in std_logic_vector;
                                constant AddrBitsPerWord : in integer) return integer;
  function LowBits(constant Address : in std_logic_vector; constant n : in integer) return string;
end package;

package body t_vhdl_aliasrecact_pkg is
  function AlignDataBusToBytes(constant Data : in std_logic_vector; constant DataWidth : in integer;
                               constant ByteAddr : in integer) return std_logic_vector is
    constant DATA_LEFT : integer := Data'length - 1;
    alias aData : std_logic_vector(DATA_LEFT downto 0) is Data;
    variable Result : std_logic_vector(DATA_LEFT downto 0) := (others => '0');
  begin
    if DataWidth < Data'length then
      Result(DataWidth - 1 downto 0) := aData(DataWidth + ByteAddr * 8 - 1 downto ByteAddr * 8);
    else
      Result(DATA_LEFT downto ByteAddr * 8) := aData(DATA_LEFT downto ByteAddr * 8);
    end if;
    return Result;
  end function;
  function CalculateByteAddress(constant Address : in std_logic_vector;
                                constant AddrBitsPerWord : in integer) return integer is
    alias aAddr : std_logic_vector(Address'length downto 1) is Address;
  begin
    if Address'length = 0 then
      return 0;
    else
      return to_integer(unsigned(aAddr(AddrBitsPerWord downto 1)));
    end if;
  end function;
  function LowBits(constant Address : in std_logic_vector; constant n : in integer) return string is
    alias aAddr : std_logic_vector(Address'length downto 1) is Address;
  begin
    return to_string(aAddr(n downto 1)) & to_string(aAddr(Address'length)) & to_string(aAddr(1));
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliasrecact_pkg.all;

entity t_vhdl_aliasrecact_sub is
  generic (AXI_BYTE_ADDR_WIDTH : integer := 2);
  port (AxiBus : inout bus_t);
end;

architecture a of t_vhdl_aliasrecact_sub is
begin
  process
    variable LocalAW : AxiBus.WriteAddress'subtype;
    variable LocalWD : AxiBus.WriteData'subtype;
    variable ba : integer;
    variable k : integer := 3;
  begin
    LocalAW.Addr := x"AAAAAAA6";
    LocalWD.Data := x"5555AAAA";
    ba := CalculateByteAddress(LocalAW.Addr, AXI_BYTE_ADDR_WIDTH);
    report "ba=" & integer'image(ba) & " low " & LowBits(LocalAW.Addr, k)
      & " " & LowBits(LocalAW.Addr, AXI_BYTE_ADDR_WIDTH);
    assert ba = 2 severity failure;
    assert LowBits(LocalAW.Addr, k) = "11010" severity failure;
    assert LowBits(LocalAW.Addr, AXI_BYTE_ADDR_WIDTH) = "1010" severity failure;
    for i in 0 to 3 loop
      report "align " & integer'image(i) & " " & to_hstring(AlignDataBusToBytes(LocalWD.Data, 32, i));
    end loop;
    assert AlignDataBusToBytes(LocalWD.Data, 32, 1) = x"5555AA00" severity failure;
    assert AlignDataBusToBytes(LocalWD.Data, 16, 1) = x"000055AA" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliasrecact_pkg.all;

entity t_vhdl_aliasrecact is end;

architecture t of t_vhdl_aliasrecact is
  signal AxiBus : bus_t(WriteAddress(Addr(31 downto 0)), WriteData(Data(31 downto 0)));
begin
  s : entity work.t_vhdl_aliasrecact_sub port map (AxiBus => AxiBus);
end;
