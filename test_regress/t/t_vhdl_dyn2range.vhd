-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An object declared with the ranges of a parameter held two levels deep:
-- UVVM's "variable v_rx_data_array : t_slv_array(exp_data_array'range)
-- (exp_data_array(exp_data_array'low)'range)" (bitvis_vip_axistream/src/
-- axistream_bfm_pkg.vhd). Which way the array's range runs is the
-- parameter's, known only while running; it was taken to be "downto", so an
-- ascending parameter made an array of a negative length, and the model
-- allocated until it was killed. The elements' range is read from the
-- parameter's element too: the subtype analysis gave it could be the one of
-- the function that made the actual, written in that function's own
-- parameters.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_dyn2range_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  procedure chk(signal clk : in std_logic; constant exp : in t_slv_array;
                constant len, left, w : natural; constant asc, easc : boolean);
end package;

package body t_vhdl_dyn2range_pkg is
  procedure chk(signal clk : in std_logic; constant exp : in t_slv_array;
                constant len, left, w : natural; constant asc, easc : boolean) is
    variable v : t_slv_array(exp'range)(exp(exp'low)'range);
  begin
    for i in v'range loop
      v(i) := exp(i);
    end loop;
    assert v'length = len and v'left = left and v'ascending = asc
      report "outer " & integer'image(v'length) & " " & integer'image(v'left) severity failure;
    assert v(v'low)'length = w and v(v'low)'ascending = easc severity failure;
    assert v(v'right) = exp(exp'right) severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_dyn2range_pkg.all;

entity t_vhdl_dyn2range is
end entity;

architecture a of t_vhdl_dyn2range is
  signal clk : std_logic := '0';
begin
  process
    variable a : t_slv_array(0 to 3)(15 downto 0) := (x"0001", x"0002", x"0003", x"0004");
    variable d : t_slv_array(5 downto 3)(0 to 7) := (x"a1", x"a2", x"a3");
    variable n : natural := 2;
    variable wv : natural := 12;
    -- Words of a width only running settles: the parameter is held two
    -- levels deep
    impure function get(lo, hi : integer; w : natural) return t_slv_array is
      variable r : t_slv_array(lo to hi)(w - 1 downto 0);
    begin
      for k in r'range loop
        r(k) := (others => '0');
        r(k)(3 downto 0) := x"5";
      end loop;
      return r;
    end function;
    impure function getd(hi, lo : integer; w : natural) return t_slv_array is
      variable r : t_slv_array(hi downto lo)(w - 1 downto 0);
    begin
      for k in r'range loop
        r(k) := (others => '1');
      end loop;
      return r;
    end function;
  begin
    chk(clk, get(0, 3, wv), 4, 0, 12, true, false);
    chk(clk, getd(6, 4, wv), 3, 6, 12, false, false);
    chk(clk, a, 4, 0, 16, true, false);
    chk(clk, d, 3, 5, 8, false, true);
    chk(clk, a(1 to n), 2, 1, 16, true, false);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
