-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- RESIZE to the length of an array reached through an access value and a
-- record element, "ParamPtr(i).SlvParam'length": a length only running
-- decides, so the result carries its bounds and is copied into what the
-- access value designates -- OSVVM's ModelParametersSingletonPkg.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std_unsigned.all;
package t_vhdl_accresize_pkg is
  type mp_t is protected
    procedure init(n : natural);
    procedure set(i : natural; d : std_logic_vector);
    impure function get(i : natural) return std_logic_vector;
  end protected;
end package;
package body t_vhdl_accresize_pkg is
  type mp_t is protected body
    type SlvPtrType is access std_logic_vector;
    type ParamRec is record
      SlvParam : SlvPtrType;
      IntParam : integer;
    end record;
    type ParamArr is array (natural range <>) of ParamRec;
    type ParamPtrType is access ParamArr;
    variable ParamPtr : ParamPtrType;
    procedure init(n : natural) is
    begin
      ParamPtr := new ParamArr(0 to n - 1);
      for i in 0 to n - 1 loop
        ParamPtr(i).SlvParam := new std_logic_vector'(x"00");
      end loop;
    end procedure;
    procedure set(i : natural; d : std_logic_vector) is
      alias ad : std_logic_vector(d'length - 1 downto 0) is d;
    begin
      ParamPtr(i).SlvParam.all := resize(ad, ParamPtr(i).SlvParam'length);
    end procedure;
    impure function get(i : natural) return std_logic_vector is
    begin
      return ParamPtr(i).SlvParam.all;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_accresize_pkg.all;
entity t_vhdl_accresize is
end entity;
architecture t of t_vhdl_accresize is
  shared variable mp : mp_t;
begin
  process
  begin
    mp.init(2);
    mp.set(1, x"3");
    mp.set(0, x"1F5");
    report to_string(mp.get(0)) & " " & to_string(mp.get(1));
    assert mp.get(0) = x"F5" severity failure;
    assert mp.get(1) = x"03" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end;
