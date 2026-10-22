-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An unconstrained generic is given the result of a function whose length
-- one of its parameters sets, and that parameter is integer arithmetic over
-- a generic of the instantiating unit: "slvAll(DELAY_G-1, POL_G)". The copy
-- of the function made for the call reads the expression where it read the
-- parameter, and the generic takes its bounds from the copy's result. The
-- generic is then picked with a selector, "ite(INIT_G = "0", slvZero(N),
-- INIT_G)", whose arm not taken may be of another length. Shaped after
-- surf's RstSync and Synchronizer.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_copygenbounds_pkg is
  function slvAll (size : positive; value : std_logic) return std_logic_vector;
  function slvZero (size : positive) return std_logic_vector;
  function ite (c : boolean; t : std_logic_vector; e : std_logic_vector) return std_logic_vector;
end package;

package body t_vhdl_copygenbounds_pkg is
  function slvAll (size : positive; value : std_logic) return std_logic_vector is
    variable retVar : std_logic_vector(size - 1 downto 0) := (others => value);
  begin
    return retVar;
  end function;
  function slvZero (size : positive) return std_logic_vector is
  begin
    return slvAll(size, '0');
  end function;
  function ite (c : boolean; t : std_logic_vector; e : std_logic_vector) return std_logic_vector is
  begin
    if c then
      return t;
    else
      return e;
    end if;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use work.t_vhdl_copygenbounds_pkg.all;

entity t_vhdl_copygenbounds_sync is
  generic (STAGES_G : positive := 2; INIT_G : std_logic_vector := "0");
  port (o : out std_logic_vector(STAGES_G - 1 downto 0); len : out natural);
end entity;

architecture rtl of t_vhdl_copygenbounds_sync is
  constant INIT_C : std_logic_vector(STAGES_G - 1 downto 0)
    := ite(INIT_G = "0", slvZero(STAGES_G), INIT_G);
  signal r : std_logic_vector(STAGES_G - 1 downto 0) := INIT_C;
begin
  o <= r;
  len <= INIT_G'length;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_copygenbounds_pkg.all;

entity t_vhdl_copygenbounds is
  generic (DELAY_G : integer range 3 to 16 := 4; WIDTH_G : integer := 5; POL_G : std_logic := '1');
end entity;

architecture rtl of t_vhdl_copygenbounds is
  signal o1 : std_logic_vector(DELAY_G - 2 downto 0);
  signal o2 : std_logic_vector(1 downto 0);
  signal o3 : std_logic_vector(WIDTH_G - 1 downto 0);
  signal o4 : std_logic_vector(DELAY_G - 2 downto 0);
  signal l1, l2, l3, l4 : natural;
begin
  -- Arithmetic over a generic
  u1 : entity work.t_vhdl_copygenbounds_sync
    generic map (STAGES_G => DELAY_G - 1, INIT_G => slvAll(DELAY_G - 1, POL_G))
    port map (o => o1, len => l1);
  -- The default, one element, and the arm not taken is three
  u2 : entity work.t_vhdl_copygenbounds_sync port map (o => o2, len => l2);
  -- A generic as it stands
  u3 : entity work.t_vhdl_copygenbounds_sync
    generic map (STAGES_G => WIDTH_G, INIT_G => slvAll(WIDTH_G, '1'))
    port map (o => o3, len => l3);
  -- Zeros of the right length: the selector takes its own
  u4 : entity work.t_vhdl_copygenbounds_sync
    generic map (STAGES_G => DELAY_G - 1, INIT_G => slvAll(2 * DELAY_G - 5, '0'))
    port map (o => o4, len => l4);
  process
  begin
    wait for 1 ns;
    report "o1=" & to_string(o1) & " o2=" & to_string(o2) & " o3=" & to_string(o3)
      & " o4=" & to_string(o4);
    assert o1 = "111" report "o1 " & to_string(o1) severity failure;
    assert o2 = "00" report "o2 " & to_string(o2) severity failure;
    assert o3 = "11111" report "o3 " & to_string(o3) severity failure;
    assert o4 = "000" report "o4 " & to_string(o4) severity failure;
    assert l1 = 3 and l2 = 1 and l3 = 5 and l4 = 3
      report "lengths " & integer'image(l1) & integer'image(l2) & integer'image(l3)
        & integer'image(l4) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
