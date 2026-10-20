-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A record of records with open elements, on an inout port, initialized by a
-- function of the record type through a concurrent procedure call and driven
-- in part by a process: OSVVM's "InitAxi4Rec(AxiBusRec => AxiBus)". The
-- function's result, of the record type itself, holds the inner records with
-- their elements carried, and is converted record by record into the port's
-- sized shape; the two drivers are resolved element by element (6.3, 14.7.2).
-- The ReadData element stays 'Z', which two states hold as '0' in a record,
-- so only its length is checked.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nestrecinit_pkg is
  type wa_t is record
    Valid : std_logic;
    Addr : std_logic_vector;
  end record;
  type rd_t is record
    Ready : std_logic;
    Data : std_logic_vector;
  end record;
  type bus_t is record
    WriteAddress : wa_t;
    ReadData : rd_t;
  end record;
  function InitWa(r : wa_t; v : std_logic := 'Z') return wa_t;
  function InitRd(r : rd_t; v : std_logic := 'Z') return rd_t;
  function InitBus(r : bus_t; v : std_logic := 'Z') return bus_t;
  procedure InitBus(signal r : inout bus_t);
end package;
package body t_vhdl_nestrecinit_pkg is
  function InitWa(r : wa_t; v : std_logic := 'Z') return wa_t is
  begin
    return (Valid => v, Addr => (r.Addr'range => v));
  end function;
  function InitRd(r : rd_t; v : std_logic := 'Z') return rd_t is
  begin
    return (Ready => v, Data => (r.Data'range => v));
  end function;
  function InitBus(r : bus_t; v : std_logic := 'Z') return bus_t is
  begin
    return (WriteAddress => InitWa(r.WriteAddress, v), ReadData => InitRd(r.ReadData, v));
  end function;
  procedure InitBus(signal r : inout bus_t) is
  begin
    r <= InitBus(r, 'Z');
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nestrecinit_pkg.all;
entity t_vhdl_nestrecinit_mgr is
  port (b : inout bus_t);
end entity;
architecture a of t_vhdl_nestrecinit_mgr is
begin
  InitBus(r => b);
  process
  begin
    wait for 1 ns;
    b.WriteAddress.Valid <= '1';
    b.WriteAddress.Addr <= (b.WriteAddress.Addr'range => '1');
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nestrecinit_pkg.all;
entity t_vhdl_nestrecinit is
end entity;
architecture t of t_vhdl_nestrecinit is
  signal s : bus_t(WriteAddress(Addr(3 downto 0)), ReadData(Data(7 downto 0)));
begin
  u : entity work.t_vhdl_nestrecinit_mgr port map (b => s);
  process
  begin
    wait for 2 ns;
    report to_string(s.WriteAddress.Valid) & " " & to_string(s.WriteAddress.Addr) & " " & to_string(s.ReadData.Data) & " len=" & integer'image(s.ReadData.Data'length);
    assert s.WriteAddress.Valid = '1' and s.WriteAddress.Addr = "1111" severity failure;
    assert s.ReadData.Data'length = 8 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
