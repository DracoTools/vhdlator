-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A shift whose count is TO_INTEGER of a vector, in a process: the count's
-- operand is put into a temporary inside the expression, and the bits are
-- gathered out of it through a value wider than 64 bits, which the back end
-- puts into a temporary of its own. That one has to be taken after the
-- first, not before the statement (V3Premit), or it reads the vector as it
-- was the time before -- neorv32's trace unit shifts its byte enables so

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_shiftcount is
end entity;

architecture a of t_vhdl_nine_shiftcount is
  signal ben : std_ulogic_vector(3 downto 0) := "1111";
  signal sel : std_ulogic_vector(1 downto 0) := "10";
begin
  process
    variable r : std_ulogic_vector(3 downto 0);
    type exp_t is array (0 to 3) of std_ulogic_vector(3 downto 0);
    constant exp_c : exp_t := ("1111", "0111", "0011", "0001");
  begin
    -- Never assigned before it is read
    wait for 1 ns;
    r := std_ulogic_vector(shift_right(unsigned(ben), to_integer(unsigned(sel))));
    assert r = "0011" report "initial r=" & to_string(r) severity failure;
    for k in 0 to 3 loop
      sel <= std_ulogic_vector(to_unsigned(k, 2));
      wait for 1 ns;
      r := std_ulogic_vector(shift_right(unsigned(ben), to_integer(unsigned(sel))));
      assert r = exp_c(k) report "k=" & integer'image(k) & " r=" & to_string(r) severity failure;
    end loop;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
