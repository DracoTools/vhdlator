-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An inout port of a record of the nine values, with an open element, driven
-- element by element by several processes, one of them through an alias of
-- the port, and as a whole by a concurrent procedure call -- OSVVM's
-- Wishbone manager, "alias WB is WishboneBus". A process writing through the
-- alias is one of the port's drivers (6.6.2, 14.7.2).

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_aliasrecdrv_pkg is
  type wb_t is record
    Cyc : std_logic;
    Stb : std_logic;
    Ack : std_logic;
    Adr : std_logic_vector;
  end record;
  procedure init(signal r : inout wb_t; v : std_logic := 'Z');
end package;
package body t_vhdl_aliasrecdrv_pkg is
  procedure init(signal r : inout wb_t; v : std_logic := 'Z') is
  begin
    r <= (Cyc => v, Stb => v, Ack => v, Adr => (r.Adr'range => v));
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliasrecdrv_pkg.all;
entity t_vhdl_aliasrecdrv_mgr is
  generic (tpd : time := 1 ns);
  port (bus_i : inout wb_t);
end;
architecture a of t_vhdl_aliasrecdrv_mgr is
  signal n : integer := 0;
  alias WB is bus_i;
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
    WB.Adr <= (WB.Adr'range => '1');
    n <= 1;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliasrecdrv_pkg.all;
entity t_vhdl_aliasrecdrv is
end entity;
architecture t of t_vhdl_aliasrecdrv is
  signal b : wb_t(Adr(3 downto 0));
begin
  u : entity work.t_vhdl_aliasrecdrv_mgr port map (bus_i => b);
  b.Ack <= '1' when b.Stb = '1' else '0';
  process
  begin
    wait for 10 ns;
    report to_string(b.Cyc) & to_string(b.Stb) & to_string(b.Ack) & " " & to_string(b.Adr);
    assert b.Cyc = '1' and b.Stb = '1' and b.Ack = '1' and b.Adr = "1111" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
