-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- An aggregate of an array type whose element subtype is left open takes
-- the one its elements have (9.3.3.3), and a string or bit string literal
-- is as long as what it stands for (9.3.2). UVVM's random testbench hands
-- "(0 => (v_min, v_max))" and "(0 => (x"0", x"3"))" to a parameter of an
-- array of arrays of UNSIGNED, three levels open (uvvm_util/tb/
-- maintenance_tb/rand_tb_pkg.vhd, t_range_uns_vec). The aggregate's
-- subtype said only its own range, and the call had no length to give the
-- copy of the procedure made for it.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package t_vhdl_agg3level_pkg is
  type t_unsigned_vector is array (natural range <>) of unsigned;
  type t_range_uns_vec is array (natural range <>) of t_unsigned_vector;
  function in_range(constant value : unsigned; constant range_vec : t_range_uns_vec)
    return boolean;
  procedure check(constant value : in unsigned; constant range_vec : in t_range_uns_vec;
                  constant expect : in boolean);
  procedure check(constant value : in std_logic_vector; constant range_vec : in t_range_uns_vec;
                  constant expect : in boolean);
end package;

package body t_vhdl_agg3level_pkg is
  function in_range(constant value : unsigned; constant range_vec : t_range_uns_vec)
    return boolean is
  begin
    for i in range_vec'range loop
      if value >= range_vec(i)(0) and value <= range_vec(i)(1) then
        return true;
      end if;
    end loop;
    return false;
  end function;

  procedure check(constant value : in unsigned; constant range_vec : in t_range_uns_vec;
                  constant expect : in boolean) is
  begin
    assert in_range(value, range_vec) = expect
      report "in_range " & to_hstring(value) & " not " & boolean'image(expect)
      severity failure;
  end procedure;

  procedure check(constant value : in std_logic_vector; constant range_vec : in t_range_uns_vec;
                  constant expect : in boolean) is
  begin
    check(unsigned(value), range_vec, expect);
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.t_vhdl_agg3level_pkg.all;

entity t_vhdl_agg3level is
end entity;

architecture a of t_vhdl_agg3level is
begin
  process
    variable v_uns_long : unsigned(127 downto 0) := (others => '0');
    variable v_min : unsigned(127 downto 0) := (others => '0');
    variable v_max : unsigned(127 downto 0) := (others => '1');
    variable v_uns : unsigned(3 downto 0) := x"2";
    variable v_slv : std_logic_vector(15 downto 0) := x"0042";
    variable v_range : t_range_uns_vec(0 to 0)(0 to 1)(15 downto 0);
  begin
    v_uns_long(70) := '1';
    check(v_uns_long, (0 => (v_min, v_max)), true);
    v_max := (others => '0');
    v_max(69) := '1';
    check(v_uns_long, (0 => (v_min, v_max)), false);
    -- Two ranges, of literals
    check(v_uns, (0 => (x"0", x"1")), false);
    check(v_uns, ((x"0", x"1"), (x"2", x"3")), true);
    check(v_uns, (0 => (x"3", x"f")), false);
    -- A variable fully constrained already
    v_range(0) := (x"0040", x"0050");
    check(v_slv, v_range, true);
    v_range(0)(1) := x"0041";
    check(v_slv, v_range, false);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
