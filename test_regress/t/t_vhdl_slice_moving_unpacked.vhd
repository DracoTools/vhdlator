-- DESCRIPTION: Verilator: VHDL test
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of an array whose elements do not pack, with ends that move but a
-- length that is a number, as the target of a variable and a signal
-- assignment: "v_packet(22 + n to 22 + n + 3) := ..." in UVVM's Ethernet VVC.
-- The value may be another slice, a function's result, or a slice of the same
-- object overlapping the target.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slice_moving_unpacked is
end entity;

architecture a of t_vhdl_slice_moving_unpacked is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  subtype t_byte_array is t_slv_array(open)(7 downto 0);
  signal s : t_byte_array(0 to 15) := (others => x"00");

  function inverted(constant value : t_slv_array) return t_slv_array is
    variable r : t_slv_array(value'range)(value(value'low)'range);
  begin
    for i in value'range loop
      r(i) := not value(i);
    end loop;
    return r;
  end function;
begin
  process
    variable v : t_byte_array(0 to 15) := (others => x"00");
    variable w : t_byte_array(0 to 3) := (x"11", x"22", x"33", x"44");
    variable n : natural := 3;
  begin
    v(2 + n to 2 + n + 3) := w;
    assert v(4) = x"00" and v(5) = x"11" and v(8) = x"44" and v(9) = x"00" severity failure;
    v(n to n + 3) := inverted(w);
    assert v(3) = x"EE" and v(6) = x"BB" and v(7) = x"33" severity failure;
    -- Overlapping: the value is read before any of it is written
    v(n + 1 to n + 4) := v(n to n + 3);
    assert v(3) = x"EE" and v(4) = x"EE" and v(5) = x"DD" and v(7) = x"BB" and v(8) = x"44"
      severity failure;
    s(2 + n to 2 + n + 3) <= w;
    wait for 1 ns;
    assert s(4) = x"00" and s(5) = x"11" and s(8) = x"44" and s(9) = x"00" severity failure;
    s(n + 1 to n + 4) <= s(n + 2 to n + 5);
    wait for 1 ns;
    assert s(3) = x"00" and s(4) = x"11" and s(5) = x"22" and s(7) = x"44" and s(8) = x"44"
      severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
