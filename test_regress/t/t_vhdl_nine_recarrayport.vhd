-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Under --vhdl-metavalues, an array of records whose elements functions of the design resolve, on an
-- inout port that leaves the array's range open: OSVVM's StreamRecArrayType of
-- transaction records, whose vector elements are resolved one element at a
-- time in turn. Only a record was resolved one subelement at a time, so the
-- array had a single driver on each side and neither saw the other's writes.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nine_recarrayport_pkg is
  function rmax(s : integer_vector) return integer;
  subtype int_max is rmax integer;
  function rmax(s : std_ulogic_vector) return std_ulogic;
  subtype slv_max is (rmax) std_ulogic_vector;
  type rec_t is record
    Rdy          : int_max;
    ParamToModel : slv_max;
    DataToModel  : slv_max;
    Ack          : int_max;
  end record;
  type rec_array_t is array (natural range <>) of rec_t;
  subtype PARAM_RANGE_TYPE is integer range 3 downto 0;
end package;
package body t_vhdl_nine_recarrayport_pkg is
  function rmax(s : integer_vector) return integer is
    variable r : integer := integer'low;
  begin
    for i in s'range loop
      if s(i) > r then r := s(i); end if;
    end loop;
    return r;
  end function;
  function rmax(s : std_ulogic_vector) return std_ulogic is
    variable r : std_ulogic := 'U';
  begin
    for i in s'range loop
      if s(i) > r then r := s(i); end if;
    end loop;
    return r;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recarrayport_pkg.all;
entity t_vhdl_nine_recarrayport_sub is
  generic (PW : integer := 4);
  port (Rec : inout rec_t; Arr : inout rec_array_t);
end;
architecture a of t_vhdl_nine_recarrayport_sub is
begin
  process
  begin
    Rec.Ack <= 0;
    Arr(1).Ack <= 0;
    wait for 1 ns;
    report "rec Rdy=" & integer'image(Rec.Rdy) & " P'len=" & integer'image(Rec.ParamToModel'length)
      & " D=" & to_hstring(Rec.DataToModel) & " arr Rdy=" & integer'image(Arr(1).Rdy)
      & " D=" & to_hstring(Arr(1).DataToModel);
    assert Rec.Rdy = 3 and Arr(1).Rdy = 4 and Arr(1).DataToModel = x"5A" severity failure;
    Rec.Ack <= 5;
    Arr(1).Ack <= 6;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recarrayport_pkg.all;
entity t_vhdl_nine_recarrayport is
  generic (PW : integer := 4);
end;
architecture t of t_vhdl_nine_recarrayport is
  signal r : rec_t(ParamToModel(PW - 1 downto 0), DataToModel(7 downto 0));
  signal a : rec_array_t(0 to 1)(ParamToModel(PARAM_RANGE_TYPE), DataToModel(7 downto 0));
begin
  s : entity work.t_vhdl_nine_recarrayport_sub port map (Rec => r, Arr => a);
  process
  begin
    r.Rdy <= 3; r.DataToModel <= x"A5";
    a(1).Rdy <= 4; a(1).DataToModel <= x"5A";
    wait for 2 ns;
    report "top Ack=" & integer'image(r.Ack) & " arr Ack=" & integer'image(a(1).Ack);
    assert r.Ack = 5 and a(1).Ack = 6 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
