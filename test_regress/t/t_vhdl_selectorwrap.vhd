-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A function of the architecture whose body only returns a selector over
-- its parameter, "return ite(INPUT = "1", allOnes(WIDTH_G), ite(INPUT = "0",
-- allZeros(WIDTH_G), INPUT))", gives a constant that elaboration settles:
-- one element of it goes to each instance's generic. Shaped after surf's
-- SynchronizerOneShotCntVector, "IN_POLARITY_C(i)".

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_selectorwrap_pkg is
  function ite (c : boolean; t : std_logic_vector; e : std_logic_vector) return std_logic_vector;
  function allOf (size : positive; value : std_logic) return std_logic_vector;
end package;

package body t_vhdl_selectorwrap_pkg is
  function ite (c : boolean; t : std_logic_vector; e : std_logic_vector) return std_logic_vector is
  begin
    if c then
      return t;
    else
      return e;
    end if;
  end function;
  function allOf (size : positive; value : std_logic) return std_logic_vector is
    variable ret : std_logic_vector(size - 1 downto 0) := (others => value);
  begin
    return ret;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_selectorwrap_bit is
  generic (POL_G : std_logic := '1');
  port (o : out std_logic);
end entity;

architecture rtl of t_vhdl_selectorwrap_bit is
begin
  o <= POL_G;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use work.t_vhdl_selectorwrap_pkg.all;

entity t_vhdl_selectorwrap_vec is
  generic (WIDTH_G : positive := 4; POL_G : std_logic_vector := "1");
  port (o : out std_logic_vector(WIDTH_G - 1 downto 0));
end entity;

architecture rtl of t_vhdl_selectorwrap_vec is
  function fillVec (INPUT : std_logic_vector) return std_logic_vector is
  begin
    return ite(INPUT = "1", allOf(WIDTH_G, '1'), ite(INPUT = "0", allOf(WIDTH_G, '0'), INPUT));
  end function;
  constant POL_C : std_logic_vector(WIDTH_G - 1 downto 0) := fillVec(POL_G);
begin
  G : for i in WIDTH_G - 1 downto 0 generate
    u : entity work.t_vhdl_selectorwrap_bit generic map (POL_G => POL_C(i)) port map (o => o(i));
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_selectorwrap is
end entity;

architecture rtl of t_vhdl_selectorwrap is
  signal o1 : std_logic_vector(3 downto 0);
  signal o0 : std_logic_vector(2 downto 0);
  signal ov : std_logic_vector(4 downto 0);
begin
  u1 : entity work.t_vhdl_selectorwrap_vec port map (o => o1);
  u0 : entity work.t_vhdl_selectorwrap_vec generic map (WIDTH_G => 3, POL_G => "0")
    port map (o => o0);
  uv : entity work.t_vhdl_selectorwrap_vec generic map (WIDTH_G => 5, POL_G => "10110")
    port map (o => ov);
  process
  begin
    wait for 1 ns;
    report "o1=" & to_string(o1) & " o0=" & to_string(o0) & " ov=" & to_string(ov);
    assert o1 = "1111" report "o1" severity failure;
    assert o0 = "000" report "o0" severity failure;
    assert ov = "10110" report "ov" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
