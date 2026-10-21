-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Generics given elements of a constant that an impure function builds
-- when the design is elaborated: it allocates, logs, and returns what it
-- allocated. The constant is an object of the design (its value calls an
-- impure function), so each generic takes the value elaboration gave it.
-- Shaped after open-logic's olo_base_pkg_crc_tb, "getCrcSettings(CrcName_g)".

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_impuregenval_pkg is
  type settings_r is record
    poly : std_logic_vector;
    init : std_logic_vector;
    order : string;
  end record;
  constant a_c : settings_r := (poly => x"2F", init => x"FF", order => "MSB_FIRST");
  constant b_c : settings_r := (poly => x"1021", init => x"FFFF", order => "LSB_FIRST");
  procedure note(msg : string);
end package;
package body t_vhdl_impuregenval_pkg is
  procedure note(msg : string) is
  begin
    report "note: " & msg;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_impuregenval_dut is
  generic (poly_g : std_logic_vector; init_g : std_logic_vector; order_g : string);
  port (o : out std_logic_vector(poly_g'length - 1 downto 0));
end entity;
architecture a of t_vhdl_impuregenval_dut is
begin
  o <= poly_g xor init_g when order_g = "MSB_FIRST" else poly_g;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_impuregenval_pkg.all;
entity t_vhdl_impuregenval is
end entity;
architecture a of t_vhdl_impuregenval is
  type ptr_t is access settings_r;
  impure function get(name : string) return settings_r is
    variable r : ptr_t;
  begin
    if name = "a_c" then
      r := new settings_r'(a_c);
    else
      r := new settings_r'(b_c);
    end if;
    note("poly " & to_string(r.all.poly));
    return r.all;
  end function;
  constant s1_c : settings_r := get("a_c");
  constant s2_c : settings_r := get("b_c");
  signal o1 : std_logic_vector(s1_c.poly'length - 1 downto 0);
  signal o2 : std_logic_vector(s2_c.poly'length - 1 downto 0);
begin
  u1 : entity work.t_vhdl_impuregenval_dut
    generic map (poly_g => s1_c.poly, init_g => s1_c.init, order_g => s1_c.order)
    port map (o => o1);
  u2 : entity work.t_vhdl_impuregenval_dut
    generic map (poly_g => s2_c.poly, init_g => s2_c.init, order_g => s2_c.order)
    port map (o => o2);
  process
  begin
    wait for 1 ns;
    assert o1 = x"D0" report "o1 " & to_hstring(o1) severity failure;
    assert o2 = x"1021" report "o2 " & to_hstring(o2) severity failure;
    -- An element of the constant handed to an open parameter
    assert to_hstring(s1_c.init) = "FF" and to_hstring(s2_c.poly) = "1021"
      report "elements " & to_hstring(s1_c.init) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
