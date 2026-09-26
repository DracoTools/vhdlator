-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- Two defects UVVM's random generator met (uvvm_util/src/rand_pkg.vhd,
-- "v_ret := resize(min_value + rand(C_LEFTMOST_BIT, ...), length)").
-- A function whose body calls another that is emitted on the way took that
-- one's result length for its own, and its "return" cut the value to it:
-- the result of f below came back three elements long. And RESIZE to a
-- width that is a number, of a sum of two values that carry their own
-- bounds, was a cast of a structure, which the back end refused.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_nestedret_pkg is
  type t_rand is protected
    impure function rand(constant length : positive; constant min_value : unsigned;
                         constant max_value : unsigned) return unsigned;
  end protected;
end package;

package body t_vhdl_nestedret_pkg is
  type t_rand is protected body
    variable seed : natural := 3;

    impure function rand(constant length : natural; constant cyc : boolean) return unsigned is
      variable r : unsigned(length - 1 downto 0);
    begin
      r := to_unsigned(seed, length);
      return r;
    end function;

    impure function find_leftmost(v : unsigned) return natural is
    begin
      for i in v'left downto v'right loop
        if v(i) = '1' then
          return i;
        end if;
      end loop;
      return 0;
    end function;

    impure function rand(constant length : positive; constant min_value : unsigned;
                         constant max_value : unsigned) return unsigned is
      constant C_LEFTMOST_BIT : natural := find_leftmost(max_value - min_value) + 1;
      variable v_ret : unsigned(length - 1 downto 0);
      variable v_valid : boolean := false;
    begin
      while not v_valid loop
        v_ret := resize(min_value + rand(C_LEFTMOST_BIT, false), length);
        v_valid := v_ret >= min_value and v_ret <= max_value;
      end loop;
      return v_ret;
    end function;
  end protected body;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_nestedret_pkg.all;

entity t_vhdl_nestedret is
end entity;

architecture a of t_vhdl_nestedret is
  shared variable gen : t_rand;

  impure function g(n : natural) return unsigned is
    variable r : unsigned(n - 1 downto 0) := (others => '1');
  begin
    return r;
  end function;

  -- g is emitted while f's body is, for a width that folds to three
  impure function f(len : positive; min_value : unsigned) return unsigned is
    constant C_BITS : natural := min_value'length - 1;
    variable v_ret : unsigned(len - 1 downto 0);
  begin
    v_ret := resize(min_value + g(C_BITS), len);
    return v_ret;
  end function;
begin
  process
    variable m : unsigned(3 downto 0) := x"2";
    variable n : positive := 6;
    variable x : unsigned(5 downto 0);
    variable y : unsigned(7 downto 0);
  begin
    x := f(n, m);
    assert x = "001001" report to_string(x) severity failure;
    y := gen.rand(8, x"02", x"10");
    assert y = x"05" report to_string(y) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
