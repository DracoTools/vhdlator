-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A slice whose width is a generic, in an instance that gives the generic
-- 0: a null slice (5.2.1), legal, and of no elements. UVVM's AXI VVC copies
-- "v_cmd.user_array(i)(GC_USER_WIDTH - 1 downto 0)" and its testbench
-- instantiates one VVC with no user bits (bitvis_vip_axi/src/axi_vvc.vhd,
-- tb/maintenance_tb/axi_vvc_tb.vhd). The part select made of it had a width
-- of nothing, which the back end refused.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_nullgenslice_pkg is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_cmd is record
    user_array : t_slv_array(0 to 3)(31 downto 0);
    len : natural;
  end record;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_nullgenslice_pkg.all;

entity t_vhdl_nullgenslice_sub is
  generic (GC_USER_WIDTH : natural);
end entity;

architecture a of t_vhdl_nullgenslice_sub is
begin
  process
    type t_ptr is access t_slv_array;
    variable v_cmd : t_cmd := (user_array => (others => x"12345678"), len => 2);
    variable p : t_ptr;
    variable w : std_logic_vector(GC_USER_WIDTH - 1 downto 0);
    variable u : std_logic_vector(0 to GC_USER_WIDTH - 1);
    variable word : std_logic_vector(31 downto 0) := x"a5c3e1f0";
    variable asc : std_logic_vector(0 to 31) := x"12345678";
    variable at : natural := 4;
  begin
    p := new t_slv_array(0 to v_cmd.len)(GC_USER_WIDTH - 1 downto 0);
    for i in 0 to v_cmd.len loop
      p(i) := v_cmd.user_array(i)(GC_USER_WIDTH - 1 downto 0);
    end loop;
    w := v_cmd.user_array(1)(GC_USER_WIDTH - 1 downto 0);
    assert p(0)'length = GC_USER_WIDTH and w'length = GC_USER_WIDTH severity failure;
    -- Ends that move with a variable, a width the generic gives
    w := word(at + GC_USER_WIDTH - 1 downto at);
    u := asc(0 to GC_USER_WIDTH - 1);
    if GC_USER_WIDTH = 8 then
      assert p(2) = x"78" and w = x"1f" and u = x"12" severity failure;
    end if;
    wait;
  end process;
end architecture;

entity t_vhdl_nullgenslice is
end entity;

architecture a of t_vhdl_nullgenslice is
begin
  u8 : entity work.t_vhdl_nullgenslice_sub generic map (GC_USER_WIDTH => 8);
  u0 : entity work.t_vhdl_nullgenslice_sub generic map (GC_USER_WIDTH => 0);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
