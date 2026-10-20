-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Under --vhdl-metavalues (see t_vhdl_nine_recarrayport): a record with a null element connected to a port that leaves the element
-- open, as OSVVM's harnesses connect "ParamToModel(NULL_RANGE_TYPE)". A null
-- vector whose ends are numbers was one place wide, one whose ends the
-- instance's bounds give as many places as its range was written, so the two
-- sides laid the record out differently: every element before the null one
-- was read shifted, and an array of such records was refused at the port map.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nine_nullrecport_pkg is
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
  subtype NULL_RANGE_TYPE is integer range 0 downto 1;
  subtype WIDE_NULL_TYPE is integer range 3 to 0;
end package;
package body t_vhdl_nine_nullrecport_pkg is
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
use work.t_vhdl_nine_nullrecport_pkg.all;
entity t_vhdl_nine_nullrecport_sub is
  generic (ID : integer);
  port (Rec : inout rec_t);
end;
architecture a of t_vhdl_nine_nullrecport_sub is
begin
  process
  begin
    Rec.Ack <= 0;
    wait for 1 ns;
    report "sub" & integer'image(ID) & " Rdy=" & integer'image(Rec.Rdy)
      & " P'len=" & integer'image(Rec.ParamToModel'length) & " D=" & to_hstring(Rec.DataToModel);
    assert Rec.Rdy = 3 + ID and Rec.ParamToModel'length = 0 and to_hstring(Rec.DataToModel) = "A5"
      severity failure;
    Rec.Ack <= 5 + ID;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_nullrecport_pkg.all;
entity t_vhdl_nine_nullrecport_arr is
  port (RecArr : inout rec_array_t);
end;
architecture a of t_vhdl_nine_nullrecport_arr is
begin
  process
  begin
    RecArr(1).Ack <= 0;
    wait for 1 ns;
    report "arr Rdy=" & integer'image(RecArr(1).Rdy) & " D=" & to_hstring(RecArr(1).DataToModel);
    assert RecArr(1).Rdy = 4 and to_hstring(RecArr(1).DataToModel) = "5A"
      and RecArr(1).ParamToModel'length = 0 severity failure;
    RecArr(1).Ack <= 9;
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_nullrecport_pkg.all;
entity t_vhdl_nine_nullrecport is
  generic (PW : integer := 0);
end;
architecture t of t_vhdl_nine_nullrecport is
  -- Ends a generic gives, ends that are numbers, and a null range of four places
  signal r : rec_t(ParamToModel(PW - 1 downto 0), DataToModel(7 downto 0));
  signal n : rec_t(ParamToModel(NULL_RANGE_TYPE), DataToModel(7 downto 0));
  signal w : rec_t(ParamToModel(WIDE_NULL_TYPE), DataToModel(7 downto 0));
  signal a : rec_array_t(0 to 1)(ParamToModel(NULL_RANGE_TYPE), DataToModel(7 downto 0));
  constant C : rec_t(ParamToModel(NULL_RANGE_TYPE), DataToModel(7 downto 0))
    := (Rdy => 6, ParamToModel => "", DataToModel => x"C3", Ack => 2);
begin
  s0 : entity work.t_vhdl_nine_nullrecport_sub generic map (0) port map (Rec => r);
  s1 : entity work.t_vhdl_nine_nullrecport_sub generic map (1) port map (Rec => n);
  s2 : entity work.t_vhdl_nine_nullrecport_sub generic map (2) port map (Rec => w);
  sa : entity work.t_vhdl_nine_nullrecport_arr port map (RecArr => a);
  process
    variable v : rec_t(ParamToModel(NULL_RANGE_TYPE), DataToModel(7 downto 0));
  begin
    r.Rdy <= 3; r.DataToModel <= x"A5";
    n.Rdy <= 4; n.DataToModel <= x"A5";
    w.Rdy <= 5; w.DataToModel <= x"A5";
    a(1).Rdy <= 4; a(1).DataToModel <= x"5A";
    v := C;
    wait for 2 ns;
    report "top Ack=" & integer'image(r.Ack) & " " & integer'image(n.Ack) & " "
      & integer'image(w.Ack) & " " & integer'image(a(1).Ack);
    assert r.Ack = 5 and n.Ack = 6 and w.Ack = 7 and a(1).Ack = 9 severity failure;
    report "v Rdy=" & integer'image(v.Rdy) & " D=" & to_hstring(v.DataToModel)
      & " Ack=" & integer'image(v.Ack);
    assert v.Rdy = 6 and v.DataToModel = x"C3" and v.Ack = 2 severity failure;
    v := (Rdy => 8, ParamToModel => (others => '0'), DataToModel => x"3C", Ack => 1);
    assert v.Rdy = 8 and v.DataToModel = x"3C" and v.Ack = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
