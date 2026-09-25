-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "if x then" over a STD_ULOGIC goes through the condition operator, which
-- is true for '1' and 'H' (9.2.9). A pure function called with a literal is
-- worked out during analysis, and there the condition was taken as a
-- BOOLEAN: '1' is not where TRUE is, so check_case('1') answered FALSE.
-- The compliance suite's tb_implicit_condition is this exactly.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_condfold is
end entity;

architecture a of t_vhdl_condfold is
  function check_case(x : std_logic) return boolean is
  begin
    if x then
      return true;
    else
      return false;
    end if;
  end function;

  function count_ones(v : std_logic_vector) return natural is
    variable n : natural := 0;
  begin
    for i in v'range loop
      if v(i) then
        n := n + 1;
      end if;
    end loop;
    return n;
  end function;

  function bit_case(b : bit) return integer is
  begin
    if b then
      return 1;
    end if;
    return 0;
  end function;

  constant ones : natural := count_ones("110100");
  signal sized : std_logic_vector(count_ones("0111") - 1 downto 0);
begin
  process
    variable s : std_logic := '1';
  begin
    assert check_case('1') and check_case('H') severity failure;
    assert not check_case('0') and not check_case('L') and not check_case('Z') severity failure;
    assert check_case(s) severity failure;
    assert ones = 3 and sized'length = 3 severity failure;
    assert bit_case('1') = 1 and bit_case('0') = 0 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
