-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A record constant holding arrays of sixteen thousand bytes and words given
-- "(others => (others => '0'))", with an element nothing folds, so the
-- aggregate is lowered rather than worked out: UVVM's C_VVC_CMD_DEFAULT
-- (bitvis_vip_axistream/src/vvc_cmd_pkg.vhd). Each element was written as
-- one concatenation, sixteen thousand deep, and the back end's recursion
-- over it ran out of stack.
package t_vhdl_bigrecagg_idx is
  impure function next_idx return natural;
end package;

package body t_vhdl_bigrecagg_idx is
  -- Nothing here folds: what "now" is, is only known when this is called
  impure function next_idx return natural is
  begin
    return natural(now / 1 ns) + 7;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_bigrecagg_idx.all;

package t_vhdl_bigrecagg_pkg is
  constant C_MAX_BYTES : natural := 16 * 1024;
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_user_array is array (natural range <>) of std_logic_vector(31 downto 0);
  type t_cmd is record
    data_array : t_slv_array(0 to C_MAX_BYTES - 1)(7 downto 0);
    data_array_length : integer range -10 to C_MAX_BYTES;
    user_array : t_user_array(0 to C_MAX_BYTES - 1);
    msg : string(1 to 20);
    cmd_idx : natural;
  end record;
  constant C_CMD_DEFAULT : t_cmd := (
    data_array => (others => (others => '0')),
    data_array_length => 1,
    user_array => (0 => x"00000001", others => (others => '0')),
    msg => (others => NUL),
    cmd_idx => next_idx);
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_bigrecagg_pkg.all;

entity t_vhdl_bigrecagg is
end entity;

architecture a of t_vhdl_bigrecagg is
begin
  process
    variable v : t_cmd := C_CMD_DEFAULT;
  begin
    v.data_array(5) := x"a5";
    assert v.data_array(5) = x"a5" and v.data_array(6) = x"00"
      and v.data_array(C_MAX_BYTES - 1) = x"00" severity failure;
    assert v.user_array(0) = x"00000001" and v.user_array(1) = x"00000000" severity failure;
    assert v.cmd_idx = 7 and v.data_array_length = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
