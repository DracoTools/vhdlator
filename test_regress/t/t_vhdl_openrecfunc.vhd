-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A function that takes and gives back a record type whose array elements
-- are open, the type of an inout port too, called from a procedure that
-- drives the port: OSVVM's InitWishboneRec and InitAxi4LiteRec. Inside the
-- function the elements are as long as the value it was given (5.3.3). Dat
-- is left at 'Z', which two states hold as '0' in a record, so only its
-- length is checked.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_openrecfunc_pkg is
  type rec_t is record
    Cyc : std_logic;
    Adr : std_logic_vector;
    Dat : std_logic_vector;
  end record;
  function InitRec(r : rec_t; v : std_logic := 'Z') return rec_t;
  procedure InitRec(signal r : inout rec_t; v : std_logic := 'Z');
end package;
package body t_vhdl_openrecfunc_pkg is
  function InitRec(r : rec_t; v : std_logic := 'Z') return rec_t is
  begin
    return (Cyc => v, Adr => (r.Adr'range => v), Dat => (r.Dat'range => v));
  end function;
  procedure InitRec(signal r : inout rec_t; v : std_logic := 'Z') is
  begin
    r <= InitRec(r, v);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_openrecfunc_pkg.all;
entity t_vhdl_openrecfunc_mgr is
  port (b : inout rec_t);
end entity;
architecture a of t_vhdl_openrecfunc_mgr is
begin
  InitRec(r => b);
  process
  begin
    wait for 1 ns;
    b.Cyc <= '1';
    b.Adr <= (b.Adr'range => '1');
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_openrecfunc_pkg.all;
entity t_vhdl_openrecfunc is
end entity;
architecture t of t_vhdl_openrecfunc is
  signal s : rec_t(Adr(3 downto 0), Dat(7 downto 0));
begin
  u : entity work.t_vhdl_openrecfunc_mgr port map (b => s);
  process
  begin
    wait for 2 ns;
    report to_string(s.Cyc) & " " & to_string(s.Adr) & " " & to_string(s.Dat);
    assert s.Cyc = '1' and s.Adr = "1111" and s.Dat'length = 8 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
