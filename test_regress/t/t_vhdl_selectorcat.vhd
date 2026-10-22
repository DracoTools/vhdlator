-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Selectors nested in a joining handed to an open parameter: "ite(c,
-- slv'(("0000" & ite(c2, slv'("01"), slv'("00")))), d(9 downto 4))", where
-- an arm is a qualified joining of a literal and another selector's result.
-- Each actual is made a value carrying its bounds, and the selector's result
-- is whichever it chooses. Shaped after surf's generated RoCEv2 mkQP.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_selectorcat_pkg is
  subtype slv is std_logic_vector;
  function ite (c : boolean; t : slv; e : slv) return slv;
  function ite (c : boolean; t : std_logic; e : std_logic) return std_logic;
  function resize (vec : slv; newSize : integer) return slv;
end package;

package body t_vhdl_selectorcat_pkg is
  function ite (c : boolean; t : slv; e : slv) return slv is
  begin
    if c then
      return t;
    else
      return e;
    end if;
  end function;
  function ite (c : boolean; t : std_logic; e : std_logic) return std_logic is
  begin
    if c then
      return t;
    else
      return e;
    end if;
  end function;
  function resize (vec : slv; newSize : integer) return slv is
    variable ret : slv(newSize - 1 downto 0) := (others => '0');
    variable tmp : slv(vec'length - 1 downto 0) := vec;
  begin
    for i in 0 to newSize - 1 loop
      if i < vec'length then
        ret(i) := tmp(i);
      end if;
    end loop;
    return ret;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_selectorcat_pkg.all;

entity t_vhdl_selectorcat is
end entity;

architecture rtl of t_vhdl_selectorcat is
  signal d : slv(19 downto 0) := x"A5B33";
  signal f : std_logic := '1';
  signal o : slv(15 downto 0);
begin
  o <= resize((d(19 downto 13) & ite(d(9 downto 6) = "1100", (f or d(12)), d(12)) & d(11 downto 10)
               & ite(d(9 downto 6) = "1100", ite((f = '1'), slv'(("0000" & ite(d(3 downto 0) = "0011",
               slv'("01"), slv'("00")))), d(9 downto 4)), d(9 downto 4))), 16);
  process
  begin
    wait for 1 ns;
    assert o = "1010010110000001" report "step 1 " & to_string(o) severity failure;
    d <= x"A5B30";
    wait for 1 ns;
    assert o = "1010010110000000" report "step 2 " & to_string(o) severity failure;
    f <= '0';
    wait for 1 ns;
    assert o = "1010010110110011" report "step 3 " & to_string(o) severity failure;
    d <= x"A5A33";
    wait for 1 ns;
    assert o = "1010010110100011" report "step 4 " & to_string(o) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
