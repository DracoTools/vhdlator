-- DESCRIPTION: Verilator: VHDL table of widths a generic sizes, read by a
-- generate's slices
--
-- Two constant arrays of a type the generic constrains: the widths, from a
-- function, and where each field starts, from another function over the
-- widths. A generate makes one field per element and drives its slice of
-- a vector, "dout(C_L(i) + C_W(i) - 1 downto C_L(i))". The slice's bounds
-- are settled when the design elaborates, so the constants they name are
-- too -- the shape of NVC issue820, with the table's type constrained.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_widthtable is
  generic (G_NUM : integer := 4);
end entity;
architecture rtl of t_vhdl_widthtable is
  type t_arr is array (0 to G_NUM - 1) of natural;
  function num_arr return t_arr is
    variable r : t_arr := (others => 8);
  begin
    r(1) := 4;
    return r;
  end function;
  function low_of (w : t_arr) return t_arr is
    variable r : t_arr := (others => 0);
  begin
    for i in 1 to G_NUM - 1 loop
      r(i) := r(i - 1) + w(i - 1);
    end loop;
    return r;
  end function;
  constant C_W : t_arr := num_arr;
  constant C_L : t_arr := low_of(C_W);
  signal dout : std_logic_vector(27 downto 0);
begin
  g : for i in 0 to G_NUM - 1 generate
    signal d : unsigned(C_W(i) - 1 downto 0) := to_unsigned(i + 1, C_W(i));
  begin
    dout(C_L(i) + C_W(i) - 1 downto C_L(i)) <= std_logic_vector(d);
  end generate;
  process is
  begin
    wait for 1 ns;
    assert dout = x"0403201" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
