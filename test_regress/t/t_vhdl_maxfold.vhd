-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A constant worked out with MAXIMUM of a pure function of a generic, given
-- to an instance as a generic: UVVM's "constant C_EMPTY_WIDTH : natural :=
-- maximum(log2(GC_DATA_WIDTH / C_SYMBOL_WIDTH), 1)" (bitvis_vip_avalon_st/
-- tb/maintenance_tb/avalon_st_bfm_tb.vhd). MAXIMUM read its operands into
-- objects of their own first, so the constant was no longer one elaboration
-- could fold, and the generic map had no value to give.
package t_vhdl_maxfold_pkg is
  function log2(constant num : positive) return natural;
end package;

package body t_vhdl_maxfold_pkg is
  function log2(constant num : positive) return natural is
    variable v_return_val : natural := 0;
  begin
    while (2 ** v_return_val < num) and (v_return_val < 31) loop
      v_return_val := v_return_val + 1;
    end loop;
    return v_return_val;
  end function;
end package body;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_maxfold_sub is
  generic (GC_EMPTY_WIDTH : natural);
  port (e : out std_logic_vector(GC_EMPTY_WIDTH - 1 downto 0));
end entity;

architecture a of t_vhdl_maxfold_sub is
begin
  e <= (others => '1');
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_maxfold_pkg.all;

entity t_vhdl_maxfold is
  generic (GC_DATA_WIDTH : natural := 32);
end entity;

architecture a of t_vhdl_maxfold is
  constant C_SYMBOL_WIDTH : natural := 8;
  constant C_EMPTY_WIDTH : natural := maximum(log2(GC_DATA_WIDTH / C_SYMBOL_WIDTH), 1);
  constant C_NARROW : natural := minimum(log2(GC_DATA_WIDTH), 3);
  signal e2 : std_logic_vector(C_EMPTY_WIDTH - 1 downto 0);
  signal e3 : std_logic_vector(C_NARROW - 1 downto 0);
begin
  u2 : entity work.t_vhdl_maxfold_sub generic map (GC_EMPTY_WIDTH => C_EMPTY_WIDTH)
    port map (e => e2);
  u3 : entity work.t_vhdl_maxfold_sub generic map (GC_EMPTY_WIDTH => C_NARROW)
    port map (e => e3);

  process
  begin
    wait for 1 ns;
    assert C_EMPTY_WIDTH = 2 and C_NARROW = 3 severity failure;
    assert e2 = "11" and e3 = "111" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
