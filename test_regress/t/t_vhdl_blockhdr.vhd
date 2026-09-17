-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A block header's maps name the block's own generics and ports as formals
-- (11.2), and it is the formal's subtype that says what an actual written as
-- a literal means: "b => '0'" gives the BIT '0' where b is a BIT, not the
-- CHARACTER '0' nor the STD_ULOGIC '0' (6.5.7.1).

library ieee;
use ieee.std_logic_1164.all;
use std.env.all;

entity t_vhdl_blockhdr is
end entity;

architecture tb of t_vhdl_blockhdr is
  signal outer : std_logic := '1';
  signal seen  : bit := '0';
begin

  b1: block
    generic (B : bit; C : character; S : std_logic; N : integer := 4);
    generic map (B => '0', C => '0', S => '0');
    port (pin : in std_logic);
    port map (pin => outer);
  begin
    check: process is
    begin
      assert B = '0' report "B" severity failure;
      assert C = '0' report "C" severity failure;
      assert character'pos(C) = 48 report "C is the character" severity failure;
      assert S = '0' report "S" severity failure;
      assert N = 4 report "N" severity failure;
      assert pin = '1' report "pin" severity failure;
      -- The block's label reaches what its header declares
      assert b1.B = '0' report "b1.B" severity failure;
      assert b1.pin = '1' report "b1.pin" severity failure;
      seen <= '1';
      wait;
    end process;
  end block;

  main: process is
  begin
    wait for 1 ns;
    assert seen = '1' report "block did not run" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
