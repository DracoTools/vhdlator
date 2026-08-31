-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A local as long as a parameter that carries its own bounds, given the
-- result of a call that is as long as the same parameter.  Nothing on
-- the way says a number, and the length has to be read off the type
-- rather than off the object.  This is IEEE fixed_generic_pkg's Is_X.
-- Answer from nvc 1.22.1.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynparam is
end entity;

architecture rtl of t_vhdl_dynparam is

  signal k : integer := 6;

  function mkv(constant kk : integer) return std_ulogic_vector is
    variable z : std_ulogic_vector(kk - 1 downto 0) := (others => '0');
  begin
    z(1) := '1';
    return z;
  end function;

  function reversed(constant arg : std_ulogic_vector) return std_ulogic_vector is
    variable r : std_ulogic_vector(arg'length - 1 downto 0);
  begin
    for i in r'range loop
      r(i) := arg(arg'low + i);
    end loop;
    return r;
  end function;

  function probe(constant arg : std_ulogic_vector) return integer is
    variable v : std_ulogic_vector(arg'length - 1 downto 0);
  begin
    v := reversed(arg);
    return to_integer(unsigned(v)) + 1000 * v'length;
  end function;

begin
  process
  begin
    assert probe(mkv(k)) = 6002 report "wrong value" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
