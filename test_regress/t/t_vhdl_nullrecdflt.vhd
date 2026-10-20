-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The default of a record with a null element, OSVVM's StreamRecType given
-- "ParamToModel(NULL_RANGE_TYPE)": the null element had no default, so the
-- record had none, and Ack started at zero rather than at AckType'LEFT. The
-- interrupt generator then took a transaction no one had requested.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nullrecdflt_pkg is
  function rmaxi(s : integer_vector) return integer;
  subtype integer_max is rmaxi integer;
  subtype RdyType is rmaxi integer range 0 to integer'high;
  subtype AckType is rmaxi integer range -1 to integer'high;
  function rmaxt(s : time_vector) return time;
  subtype time_max is rmaxt time;
  function rmaxb(s : boolean_vector) return boolean;
  subtype boolean_max is rmaxb boolean;
  function rmaxl(s : std_ulogic_vector) return std_ulogic;
  subtype std_logic_max is rmaxl std_ulogic;
  type std_logic_vector_max_c is array (natural range <>) of std_logic_max;
  type op_t is (NOT_DRIVEN, SEND, GET);
  type op_vec is array (natural range <>) of op_t;
  function rmaxo(s : op_vec) return op_t;
  subtype op_max is rmaxo op_t;
  type id_t is record
    Id : integer_max;
  end record;
  type rec_t is record
    Rdy : RdyType;
    Ack : AckType;
    Operation : op_max;
    DataToModel : std_logic_vector_max_c;
    ParamToModel : std_logic_vector_max_c;
    BurstFifo : id_t;
    IntToModel : integer_max;
    BoolToModel : boolean_max;
    TimeToModel : time_max;
  end record;
  type rec_arr is array (integer range <>) of rec_t;
end package;
package body t_vhdl_nullrecdflt_pkg is
  function rmaxi(s : integer_vector) return integer is
    variable r : integer := integer'low;
  begin
    for i in s'range loop if s(i) > r then r := s(i); end if; end loop;
    return r;
  end function;
  function rmaxt(s : time_vector) return time is
    variable r : time := time'low;
  begin
    for i in s'range loop if s(i) > r then r := s(i); end if; end loop;
    return r;
  end function;
  function rmaxb(s : boolean_vector) return boolean is
  begin
    for i in s'range loop if s(i) then return true; end if; end loop;
    return false;
  end function;
  function rmaxl(s : std_ulogic_vector) return std_ulogic is
    variable r : std_ulogic := 'U';
  begin
    for i in s'range loop if s(i) > r then r := s(i); end if; end loop;
    return r;
  end function;
  function rmaxo(s : op_vec) return op_t is
    variable r : op_t := NOT_DRIVEN;
  begin
    for i in s'range loop if s(i) > r then r := s(i); end if; end loop;
    return r;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nullrecdflt_pkg.all;
entity t_vhdl_nullrecdflt is end;
architecture t of t_vhdl_nullrecdflt is
  signal Arr : rec_arr(0 downto 0)(DataToModel(0 downto 0), ParamToModel(1 to 0));
  signal One : rec_t(DataToModel(0 downto 0), ParamToModel(1 to 0));
begin
  process begin
    report "arr ack=" & integer'image(Arr(0).Ack) & " one ack=" & integer'image(One.Ack);
    assert Arr(0).Ack = -1 and One.Ack = -1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
