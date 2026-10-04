-- DESCRIPTION: Verilator: VHDL unconstrained port given a concatenation
--
-- An input port whose subtype leaves the index open takes its bounds from
-- the actual (6.5.6.3), and an actual that is a concatenation has the
-- bounds 9.2.5 gives it in VHDL-2008: as long as its operands together,
-- from the index subtype's left in its direction, whatever the left operand
-- is -- GHDL's gna issue2190, "vec => a(0) & b(1)".
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portcatact_sub is
  generic (exp_left, exp_right : integer; exp_value : std_logic_vector);
  port (vec : in std_logic_vector);
end entity;

architecture rtl of t_vhdl_portcatact_sub is
begin
  process is
  begin
    wait for 1 ns;
    assert vec'left = exp_left and vec'right = exp_right and vec'ascending
      report "bounds " & integer'image(vec'left) & " " & integer'image(vec'right)
      severity failure;
    assert vec = exp_value report "value" severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portcatact is
end entity;

architecture test of t_vhdl_portcatact is
  constant a : std_logic_vector(2 downto 0) := "010";
  constant b : std_logic_vector(2 downto 0) := "101";
  constant c : std_logic_vector(1 to 2) := "10";
begin
  -- Two elements: from the index subtype's left, ascending
  u1 : entity work.t_vhdl_portcatact_sub
    generic map (exp_left => 0, exp_right => 1, exp_value => "00")
    port map (vec => a(0) & b(1));
  -- An array on the left: still from the index subtype's left
  u2 : entity work.t_vhdl_portcatact_sub
    generic map (exp_left => 0, exp_right => 2, exp_value => "101")
    port map (vec => c & b(0));
  -- An element, then text
  u3 : entity work.t_vhdl_portcatact_sub
    generic map (exp_left => 0, exp_right => 2, exp_value => "111")
    port map (vec => b(2) & "11");

  process is
  begin
    wait for 2 ns;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
