-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A process that drives an element of a resolved record port through an alias
-- of the element: OSVVM's Axi4LiteManager writes "alias AW : ... is
-- AxiBus.WriteAddress", and a procedure called concurrently drives the whole
-- port too. Each process is a driver of the elements it names (14.7.2), the
-- alias counting as the element it denotes; the alias of an element was not
-- followed, and the assignment through it was refused as one to a resolved
-- signal with no driver here.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_aliaselemdrv_pkg is
  type wb_t is record
    Cyc : std_logic;
    Stb : std_logic;
    Ack : std_logic;
    Adr : std_logic_vector;
  end record;
  procedure init(signal r : inout wb_t; v : std_logic := 'Z');
end package;
package body t_vhdl_aliaselemdrv_pkg is
  procedure init(signal r : inout wb_t; v : std_logic := 'Z') is
  begin
    r <= (Cyc => v, Stb => v, Ack => v, Adr => (r.Adr'range => v));
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliaselemdrv_pkg.all;
entity t_vhdl_aliaselemdrv_mgr is
  generic (tpd : time := 1 ns);
  port (bus_i : inout wb_t);
end;
architecture a of t_vhdl_aliaselemdrv_mgr is
  signal n : integer := 0;
  alias WB is bus_i;
  alias AW : bus_i.Adr'subtype is bus_i.Adr;
begin
  init(r => bus_i);
  CycProc : process
  begin
    bus_i.Cyc <= '0';
    loop
      wait on n;
      bus_i.Cyc <= '1' after tpd;
    end loop;
  end process;
  StbProc : process
  begin
    WB.Stb <= '0';
    WB.Adr <= (WB.Adr'range => '0');
    wait for 5 ns;
    WB.Stb <= '1';
    AW <= (AW'range => '1');
    n <= 1;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliaselemdrv_pkg.all;
entity t_vhdl_aliaselemdrv is end;
architecture t of t_vhdl_aliaselemdrv is
  signal b : wb_t(Adr(3 downto 0));
begin
  u : entity work.t_vhdl_aliaselemdrv_mgr port map (bus_i => b);
  b.Ack <= '1' when b.Stb = '1' else '0';
  process
  begin
    wait for 10 ns;
    report to_string(b.Cyc) & to_string(b.Stb) & to_string(b.Ack) & " " & to_string(b.Adr);
    assert b.Cyc = '1' and b.Stb = '1' and b.Ack = '1' and b.Adr = "1111"
      report "wrong bus value" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
