-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Under --vhdl-metavalues, a record of resolved elements shared by two
-- instances' inout ports, each process a driver of the elements it assigns
-- (14.7.2), as OSVVM's AXI4 models drive AxiBus: the manager through "alias
-- AW : ... is AxiBus.WriteAddress", the subordinate through a procedure given
-- AxiBus.WriteAddress.Ready. Both were taken to drive the whole record, the
-- elements they never assign at 'U', and the manager never saw Ready.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nine_recpartdrv_pkg is
  type wa_t is record
    Valid : std_logic;
    Ready : std_logic;
    Addr  : std_logic_vector;
    Prot  : std_logic_vector(2 downto 0);
  end record;
  type wd_t is record
    Valid : std_logic;
    Ready : std_logic;
    Data  : std_logic_vector;
  end record;
  type bus_t is record
    WriteAddress : wa_t;
    WriteData    : wd_t;
  end record;
  function initwa(AW : wa_t; v : std_logic := 'Z') return wa_t;
  function initwd(WD : wd_t; v : std_logic := 'Z') return wd_t;
  function initbus(b : bus_t; v : std_logic := 'Z') return bus_t;
  procedure InitRec(signal b : inout bus_t);
  procedure Handshake(signal Clk : in std_logic; signal Valid : in std_logic;
                      signal Ready : out std_logic);
end package;
package body t_vhdl_nine_recpartdrv_pkg is
  function initwa(AW : wa_t; v : std_logic := 'Z') return wa_t is
  begin
    return (Valid => v, Ready => v, Addr => (AW.Addr'range => v), Prot => (others => v));
  end function;
  function initwd(WD : wd_t; v : std_logic := 'Z') return wd_t is
  begin
    return (Valid => v, Ready => v, Data => (WD.Data'range => v));
  end function;
  function initbus(b : bus_t; v : std_logic := 'Z') return bus_t is
  begin
    return (WriteAddress => initwa(b.WriteAddress, v), WriteData => initwd(b.WriteData, v));
  end function;
  procedure InitRec(signal b : inout bus_t) is
  begin
    b <= initbus(b, 'Z');
  end procedure;
  procedure Handshake(signal Clk : in std_logic; signal Valid : in std_logic;
                      signal Ready : out std_logic) is
  begin
    Ready <= '1' after 1 ns;
    wait until rising_edge(Clk) and Valid = '1';
    Ready <= '0';
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recpartdrv_pkg.all;
entity t_vhdl_nine_recpartdrv_sub is
  port (Clk : in std_logic; AxiBus : inout bus_t);
end;
architecture a of t_vhdl_nine_recpartdrv_sub is
begin
  InitRec(AxiBus);
  WAH : process
    alias AW : AxiBus.WriteAddress'subtype is AxiBus.WriteAddress;
  begin
    AW.Ready <= '0';
    wait for 0 ns;
    wait until rising_edge(Clk);
    wait until rising_edge(Clk);
    loop
      Handshake(Clk => Clk, Valid => AxiBus.WriteAddress.Valid, Ready => AxiBus.WriteAddress.Ready);
      report "sub got addr " & to_hstring(AW.Addr);
      assert AW.Addr = x"AA" severity failure;
    end loop;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recpartdrv_pkg.all;
entity t_vhdl_nine_recpartdrv_mgr is
  port (Clk : in std_logic; AxiBus : inout bus_t);
end;
architecture a of t_vhdl_nine_recpartdrv_mgr is
begin
  InitRec(AxiBus);
  WAP : process
    alias AW : AxiBus.WriteAddress'subtype is AxiBus.WriteAddress;
  begin
    AW.Valid <= '0';
    AW.Addr <= (AW.Addr'range => '0');
    wait until rising_edge(Clk);
    wait until rising_edge(Clk);
    wait until rising_edge(Clk);
    AW.Addr <= x"AA";
    AW.Valid <= '1';
    loop
      wait until rising_edge(Clk);
      exit when AW.Ready = '1';
      assert now < 100 ns report "mgr: ready is " & to_string(AW.Ready) severity failure;
    end loop;
    AW.Valid <= '0';
    report "mgr: handshake done";
    wait for 20 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recpartdrv_pkg.all;
entity t_vhdl_nine_recpartdrv is end;
architecture t of t_vhdl_nine_recpartdrv is
  signal Clk : std_logic := '0';
  signal AxiBus : bus_t(WriteAddress(Addr(7 downto 0)), WriteData(Data(15 downto 0)));
begin
  Clk <= not Clk after 5 ns when now < 200 ns;
  s : entity work.t_vhdl_nine_recpartdrv_sub port map (Clk => Clk, AxiBus => AxiBus);
  m : entity work.t_vhdl_nine_recpartdrv_mgr port map (Clk => Clk, AxiBus => AxiBus);
end;
