-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Under --vhdl-metavalues (see t_vhdl_nine_recarrayport): a procedure given one element of an open inout port of an array of
-- records, as OSVVM's interrupt test calls "Send(InterruptRecArray(0), ...)":
-- the call did not count as a driver of the port, which was lowered as an
-- input the procedure then could not write.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nine_procarrelem_pkg is
  function rmaxi(s : integer_vector) return integer;
  subtype rint is rmaxi integer range 0 to integer'high;
  subtype aint is rmaxi integer range -1 to integer'high;
  function rmax(s : std_ulogic_vector) return std_ulogic;
  subtype slv_max is (rmax) std_ulogic_vector;
  type rec_t is record
    Rdy : rint;
    Ack : aint;
    DataToModel : slv_max;
    ParamToModel : slv_max;
  end record;
  type rec_arr is array (natural range <>) of rec_t;
  procedure Send(signal TransRec : inout rec_t; constant Data : in std_logic_vector);
end package;
package body t_vhdl_nine_procarrelem_pkg is
  function rmaxi(s : integer_vector) return integer is
    variable r : integer := integer'low;
  begin
    for i in s'range loop if s(i) > r then r := s(i); end if; end loop;
    return r;
  end function;
  function rmax(s : std_ulogic_vector) return std_ulogic is
    variable r : std_ulogic := 'U';
  begin
    for i in s'range loop if s(i) > r then r := s(i); end if; end loop;
    return r;
  end function;
  procedure Send(signal TransRec : inout rec_t; constant Data : in std_logic_vector) is
  begin
    TransRec.DataToModel <= Data;
    TransRec.Rdy <= TransRec.Rdy + 1;
    wait for 0 ns;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_procarrelem_pkg.all;
entity t_vhdl_nine_procarrelem_ctl is
  port (Arr : inout rec_arr);
end;
architecture a of t_vhdl_nine_procarrelem_ctl is
begin
  process
  begin
    wait for 1 ns;
    Send(Arr(0), "1");
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_procarrelem_pkg.all;
entity t_vhdl_nine_procarrelem_gen is
  port (TransRec : inout rec_t);
end;
architecture a of t_vhdl_nine_procarrelem_gen is
begin
  process
  begin
    assert TransRec.Rdy = 0 and TransRec.Ack = -1 severity failure;
    TransRec.Ack <= TransRec.Ack + 1;
    wait until TransRec.Ack /= TransRec.Rdy;
    report "gen woke rdy=" & integer'image(TransRec.Rdy) & " ack=" & integer'image(TransRec.Ack);
    assert now = 1 ns and TransRec.Rdy = 1 severity failure;
    report "gen data=" & to_string(TransRec.DataToModel);
    assert TransRec.DataToModel = "1" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_procarrelem_pkg.all;
entity t_vhdl_nine_procarrelem is end;
architecture t of t_vhdl_nine_procarrelem is
  signal Arr : rec_arr(0 downto 0)(DataToModel(0 downto 0), ParamToModel(1 to 0));
begin
  g : for i in Arr'range generate
    gi : entity work.t_vhdl_nine_procarrelem_gen port map (TransRec => Arr(i));
  end generate;
  c : entity work.t_vhdl_nine_procarrelem_ctl port map (Arr => Arr);
end;
