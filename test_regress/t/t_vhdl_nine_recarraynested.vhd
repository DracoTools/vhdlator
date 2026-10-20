-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Under --vhdl-metavalues, OSVVM's StreamTransactionArrayPkg: Send hands its array of transaction
-- records on to LocalSend, given data whose length a constant of the entity
-- works out from a port ("DATA_WIDTH := StreamTxRec(1).DataToModel'length").
-- No copy of Send was made for the call, as the data's length could not be
-- settled, so LocalSend's copy saw the records' elements open and refused
-- "TransactionRec(Index).ParamToModel'length". And in the copy made for an
-- empty Param, "LocalParam(Param'length - 1 downto 0) := Param" wrote two
-- elements into a null slice.

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_nine_recarraynested_pkg is
  function rmax(s : integer_vector) return integer;
  subtype int_max is rmax integer;
  function rmax(s : std_ulogic_vector) return std_ulogic;
  subtype slv_max is (rmax) std_ulogic_vector;
  type rec_t is record
    Op           : int_max;
    DataToModel  : slv_max;
    ParamToModel : slv_max;
  end record;
  type rec_array_t is array (integer range <>) of rec_t;
  procedure LocalSend(signal TransactionRec : inout rec_array_t; constant Index : in integer;
                      constant Data : in std_logic_vector; constant Param : in std_logic_vector);
  procedure Send(signal TransactionRec : inout rec_array_t; constant Index : in integer;
                 constant Data : in std_logic_vector; constant Param : in std_logic_vector);
end package;
package body t_vhdl_nine_recarraynested_pkg is
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
  procedure LocalSend(signal TransactionRec : inout rec_array_t; constant Index : in integer;
                      constant Data : in std_logic_vector; constant Param : in std_logic_vector) is
    variable LocalParam : std_logic_vector(TransactionRec(Index).ParamToModel'length - 1 downto 0)
      := (others => '-');
  begin
    LocalParam(Param'length - 1 downto 0) := Param;
    TransactionRec(Index).Op <= Data'length;
    TransactionRec(Index).DataToModel <= Data;
    TransactionRec(Index).ParamToModel <= LocalParam;
  end procedure;
  procedure Send(signal TransactionRec : inout rec_array_t; constant Index : in integer;
                 constant Data : in std_logic_vector; constant Param : in std_logic_vector) is
  begin
    LocalSend(TransactionRec, Index, Data, Param);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recarraynested_pkg.all;
entity t_vhdl_nine_recarraynested_ctl is
  port (TxRec : inout rec_array_t);
  constant DW : integer := TxRec(1).DataToModel'length;
end;
architecture a of t_vhdl_nine_recarraynested_ctl is
begin
  process
    variable Data : std_logic_vector(DW - 1 downto 0);
  begin
    Data := x"BEEF";
    Send(TxRec, 1, Data, "");
    wait for 1 ns;
    assert TxRec(1).ParamToModel = "--------" and TxRec(1).DataToModel = x"BEEF"
      report "empty param: " & to_string(TxRec(1).ParamToModel) severity failure;
    Send(TxRec, 0, Data, "101");
    wait for 1 ns;
    report "op=" & integer'image(TxRec(1).Op) & " data=" & to_hstring(TxRec(1).DataToModel)
      & " param=" & to_string(TxRec(1).ParamToModel) & " len=" & integer'image(TxRec(0).ParamToModel'length);
    assert TxRec(0).ParamToModel = "-----101" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nine_recarraynested_pkg.all;
entity t_vhdl_nine_recarraynested is end;
architecture t of t_vhdl_nine_recarraynested is
  signal arr : rec_array_t(0 to 1)(DataToModel(15 downto 0), ParamToModel(7 downto 0));
begin
  c : entity work.t_vhdl_nine_recarraynested_ctl port map (TxRec => arr);
end;
