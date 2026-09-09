-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of what a call gave back, where how long that is only running
-- decides. The slice is another such value, and carries its own bounds.

library ieee;
use ieee.std_logic_1164.all;

entity t is
end entity;

architecture rtl of t is
  signal width : integer := 8;

  impure function grab (k : integer) return std_ulogic_vector is
    variable r : std_ulogic_vector(width - 1 downto 0);
  begin
    for i in r'range loop
      r(i) := '1' when ((i + k) mod 2) = 0 else '0';
    end loop;
    return r;
  end function;

  function img (v : std_logic_vector) return string is
    variable r : string(1 to v'length);
    variable k : integer := 1;
  begin
    for i in v'range loop
      r(k) := '1' when v(i) = '1' else '0';
      k := k + 1;
    end loop;
    return r;
  end function;

  procedure take (variable data : inout std_logic_vector) is
  begin
    data := grab(1)(data'range);
  end procedure;

begin
  process
    variable d : std_logic_vector(3 downto 0);
    variable e : std_logic_vector(5 downto 0);
  begin
    take(d);
    assert img(d) = "1010" report "d=" & img(d) severity failure;
    take(e);
    assert img(e) = "101010" report "e=" & img(e) severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;
