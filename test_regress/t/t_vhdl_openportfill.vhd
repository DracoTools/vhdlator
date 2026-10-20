-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- "(TID'range => '0')" with TID a port of an unconstrained type, handed to a
-- function of an unconstrained parameter: the aggregate has the range of the
-- port's actual, whichever way that runs (9.3.3.3, 6.5.6.3) -- OSVVM's AXI
-- stream components initialize "ParamID := ifelse(INIT_ID'length > 0,
-- INIT_ID, (TID'range => '0'))".

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_openportfill_pkg is
  function IfElse(Expr : boolean; A, B : std_logic_vector) return std_logic_vector;
end package;

package body t_vhdl_openportfill_pkg is
  function IfElse(Expr : boolean; A, B : std_logic_vector) return std_logic_vector is
  begin
    if Expr then return A; else return B; end if;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_openportfill_pkg.all;

entity t_vhdl_openportfill_vc is
  generic (INIT_ID : std_logic_vector := "");
  port (TID : out std_logic_vector);
end entity;

architecture a of t_vhdl_openportfill_vc is
  signal ParamID : std_logic_vector(TID'range)
    := IfElse(INIT_ID'length > 0, INIT_ID, (TID'range => '0'));
  signal Reversed : std_logic_vector(TID'range)
    := IfElse(false, TID, (TID'reverse_range => '1'));
begin
  TID <= ParamID;
  process
  begin
    assert Reversed = (Reversed'range => '1') severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_openportfill is
end entity;

architecture t of t_vhdl_openportfill is
  signal a : std_logic_vector(3 downto 0);
  signal b : std_logic_vector(7 downto 0);
  signal c : std_logic_vector(0 to 5);
begin
  u1 : entity work.t_vhdl_openportfill_vc port map (TID => a);
  u2 : entity work.t_vhdl_openportfill_vc generic map (INIT_ID => x"A5") port map (TID => b);
  u3 : entity work.t_vhdl_openportfill_vc port map (TID => c);
  process
  begin
    wait for 1 ns;
    report to_string(a) & " " & to_string(b) & " " & to_string(c);
    assert a = "0000" severity failure;
    assert b = x"A5" severity failure;
    assert c = "000000" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
