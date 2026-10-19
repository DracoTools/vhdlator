-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A slice running 'downto' of a formal whose actual runs 'to' -- an instance's generic given
-- a string literal, which runs from NATURAL'LEFT upward (9.3.2) -- is an error (8.5; GHDL
-- gna issue559).

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicedir_gen_bad_sub is
  generic (g : std_logic_vector);
end entity;

architecture a of t_vhdl_slicedir_gen_bad_sub is
  function low_two (arg : std_logic_vector) return std_logic_vector is
    variable v : std_logic_vector(1 downto 0);
  begin
    report "arg is " & integer'image(arg'left) & " to " & integer'image(arg'right);
    v := arg(1 downto 0);
    return v;
  end function;
  constant c : std_logic_vector(1 downto 0) := low_two(g);
begin
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicedir_gen_bad is
end entity;

architecture a of t_vhdl_slicedir_gen_bad is
begin
  u : entity work.t_vhdl_slicedir_gen_bad_sub generic map (g => "00");
  process is
  begin
    wait for 1 ns;
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
