-- DESCRIPTION: Verilator: VHDL constant that takes its bounds from "not" of
-- an aggregate, and case choices written with operators
--
-- "constant b : boolean_vector := not (true, false)" is as long as the
-- aggregate, since the predefined "not" of an array has its operand's
-- bounds (9.2.2, 6.4.2.2); and a case choice may be any locally static
-- expression of the selector's type -- a concatenation, an "or", a
-- conversion of a call of NUMERIC_STD (10.9; nvc simp casearray1).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_notagg is
end entity;

architecture a of t_vhdl_notagg is
  signal x : std_logic_vector(7 downto 0);
  signal y : integer;
begin
  process is
    constant b : boolean_vector := not (true, false);
    constant c : bit_vector := not ('1', '0', '0');
  begin
    assert b'length = 2 severity failure;
    assert not b(0) and b(1) severity failure;
    assert c'length = 3 and c = "011" severity failure;
    for i in 0 to 4 loop
      case i is
        when 0 => x <= X"12";
        when 1 => x <= X"1D";
        when 2 => x <= X"52";
        when 3 => x <= X"2A";
        when others => x <= X"00";
      end case;
      wait for 1 ns;
      case x is
        when X"1" & X"2" => y <= 1;
        when X"1" & not X"2" => y <= 2;
        when X"12" or X"52" => y <= 3;
        when std_logic_vector(to_unsigned(42, 8)) => y <= 4;
        when others => y <= 0;
      end case;
      wait for 1 ns;
      case i is
        when 0 => assert y = 1 severity failure;
        when 1 => assert y = 2 severity failure;
        when 2 => assert y = 3 severity failure;
        when 3 => assert y = 4 severity failure;
        when others => assert y = 0 severity failure;
      end case;
    end loop;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
