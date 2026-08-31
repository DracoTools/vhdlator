-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A result as long as the range of a constant the body declares, where
-- that constant's own length only running decides.  What the subprogram
-- gives back is settled before its declarations are reached, so the
-- length has to be read off the type rather than off the object.  This
-- is VUnit's codec, which decodes a value whose bounds it has just
-- read out of the text.  Answer from nvc 1.22.1.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynrange2 is
end entity;

architecture rtl of t_vhdl_dynrange2 is

  type range_t is array (integer range <>) of bit;

  signal k : integer := 5;

  function get_range(constant code : string) return range_t is
    variable r : range_t(code'length - 1 downto 0) := (others => '0');
  begin
    return r;
  end function;

  function decode(constant code : string) return std_ulogic_vector is
    constant ret_range : range_t := get_range(code);
    variable ret_val : std_ulogic_vector(ret_range'range) := (others => '0');
  begin
    ret_val(ret_val'left) := '1';
    return ret_val;
  end function;

begin
  process
    variable v : std_ulogic_vector(4 downto 0);
  begin
    v := decode(integer'image(k) & "xxxx");
    assert to_integer(unsigned(v)) = 16 report "wrong value" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
