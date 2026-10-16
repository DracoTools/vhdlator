-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic of an array type indexed by an enumeration, whose element is
-- open too -- UVVM's Ethernet VVC, "GC_DUT_IF_FIELD_CONFIG :
-- t_dut_if_field_config_direction_array := C_..._DEFAULT", indexed by
-- t_direction. The bounds an instance gives it are values of the
-- enumeration, and the element's are the element's own, from a constant
-- or an aggregate as much as from the default.

package t_vhdl_genericenumidx_pkg is
  type t_direction is (TRANSMIT, RECEIVE);
  type t_arr is array (natural range <>) of integer;
  type t_dir_arr is array (t_direction range <>) of t_arr;
  constant C_DEF : t_dir_arr(t_direction'low to t_direction'high)(0 to 0) := (others => (others => 5));
  constant C_TWO : t_dir_arr(TRANSMIT to RECEIVE)(3 to 4) := (TRANSMIT => (3 => 1, 4 => 2), RECEIVE => (3 => 7, 4 => 8));
end package;

use work.t_vhdl_genericenumidx_pkg.all;
entity t_vhdl_genericenumidx_sub is
  generic (G : t_dir_arr := C_DEF;
           FIRST : integer := 5;
           LAST : integer := 5;
           ELEM_LEFT : integer := 0);
end entity;
architecture a of t_vhdl_genericenumidx_sub is
begin
  process
  begin
    assert G'left = TRANSMIT and G'right = RECEIVE and G'length = 2 severity failure;
    assert G'element'left = ELEM_LEFT severity failure;
    assert G(TRANSMIT)(G'element'left) = FIRST
      report "first=" & integer'image(G(TRANSMIT)(G'element'left)) severity failure;
    assert G(RECEIVE)(G'element'right) = LAST
      report "last=" & integer'image(G(RECEIVE)(G'element'right)) severity failure;
    wait;
  end process;
end architecture;

use work.t_vhdl_genericenumidx_pkg.all;
entity t_vhdl_genericenumidx is
end entity;
architecture a of t_vhdl_genericenumidx is
begin
  u1 : entity work.t_vhdl_genericenumidx_sub;
  u2 : entity work.t_vhdl_genericenumidx_sub
    generic map (G => C_TWO, FIRST => 1, LAST => 8, ELEM_LEFT => 3);
  u3 : entity work.t_vhdl_genericenumidx_sub
    generic map (G => (TRANSMIT => (0 => 1), RECEIVE => (0 => 2)), FIRST => 1, LAST => 2);
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
