-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An out formal of a fixed length given a slice, whose ends only running
-- settles, of a variable whose length is its caller's: what the call writes
-- reaches the variable. std_logic_1164's HREAD writes each digit that way
-- ("Char2QuadBits(c, sv(4*i to 4*i+3), ok, true)").

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_outslicecarried is
end entity;

architecture a of t_vhdl_outslicecarried is
  procedure quad(constant c : in character; result : out std_logic_vector(3 downto 0)) is
  begin
    if c = 'A' then result := "1010"; else result := "0101"; end if;
  end procedure;
  procedure fill(v : out std_logic_vector) is
    constant ne : integer := (v'length + 3) / 4;
    constant pad : integer := ne * 4 - v'length;
    variable sv : std_logic_vector(0 to ne * 4 - 1);
  begin
    for i in 0 to ne - 1 loop
      quad('A', sv(4 * i to 4 * i + 3));
    end loop;
    v := sv(pad to sv'high);
  end procedure;
begin
  process
    variable sl : std_logic_vector(1 to 8) := (others => '0');
    variable s2 : std_logic_vector(1 to 8) := (others => '0');
    variable n : natural := 3;
  begin
    fill(sl(n - 2 to n + 5));
    assert sl = "10101010" report "slice " & to_string(sl) severity failure;
    fill(s2);
    assert s2 = "10101010" report "whole " & to_string(s2) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
