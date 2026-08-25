-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A user-defined attribute is a value an attribute specification gives to a
-- named entity (6.7, 7.2). It is a static expression, so reading it back is
-- reading that expression, wherever a static value is allowed.

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_userattr is
end entity;

architecture tb of t_vhdl_userattr is

  attribute width  : integer;
  attribute note   : string;
  attribute enable : boolean;

  signal bus_a : std_logic_vector(7 downto 0) := (others => '0');
  attribute width of bus_a : signal is 8;
  attribute note  of bus_a : signal is "the data bus";

  type state_t is (idle, run);
  attribute width  of state_t : type is 2;
  attribute enable of idle    : literal is true;

  constant k : integer := 5;
  attribute width of k : constant is k * 11;

  function f return integer is
  begin
    return 3;
  end function;
  attribute width of f : function is 33;

  -- A static value, so it may size an object
  constant c : integer := bus_a'width;
  signal copy : std_logic_vector(bus_a'width - 1 downto 0) := (others => '1');

begin

  process is
  begin
    assert bus_a'width = 8 report "attribute of a signal" severity failure;
    assert bus_a'note = "the data bus" report "a string attribute" severity failure;
    assert state_t'width = 2 report "attribute of a type" severity failure;
    assert idle'enable report "attribute of a literal" severity failure;
    assert k'width = 55 report "attribute of a constant" severity failure;
    assert f'width = 33 report "attribute of a function" severity failure;
    assert c = 8 report "attribute used statically" severity failure;
    assert copy'length = 8 report "attribute used as a bound" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
