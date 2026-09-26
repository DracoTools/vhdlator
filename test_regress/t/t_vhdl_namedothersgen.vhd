-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- "(3 => '1', others => '0')" for a vector whose length a generic gives:
-- UVVM's "constant C_DUMMY_DATA_3 : std_logic_vector(GC_DATA_WIDTH - 1
-- downto 0) := (3 => '1', others => '0')" (uvvm_assertions/tb/
-- demo_entities.vhd). Only an aggregate of one value for every element was
-- lowered where the length is not a number here. nvc 1.22.1, GHDL 7.0.0-dev
-- and Questa 25.1std run this; Questa warns (vcom-1074) that OTHERS beside
-- other choices wants a locally static subtype.
library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_namedothersgen_sub is
  generic (W : natural);
end entity;

architecture a of t_vhdl_namedothersgen_sub is
  function pick(c : boolean; a, b : std_logic) return std_logic is
  begin
    if c then
      return a;
    end if;
    return b;
  end function;
  constant C_DOWN : std_logic_vector(W - 1 downto 0) := (0 => '1', others => '0');
  constant C_UP : std_logic_vector(0 to W - 1) := (0 => '1', others => '0');
  signal s : std_logic_vector(W + 1 downto 2) := (2 => '0', others => '1');
begin
  process
  begin
    for i in 0 to W - 1 loop
      assert C_DOWN(i) = pick(i = 0, '1', '0') severity failure;
      assert C_UP(i) = pick(i = 0, '1', '0') severity failure;
      assert s(i + 2) = pick(i = 0, '0', '1') severity failure;
    end loop;
    wait;
  end process;
end architecture;

entity t_vhdl_namedothersgen is
end entity;

architecture a of t_vhdl_namedothersgen is
begin
  u8 : entity work.t_vhdl_namedothersgen_sub generic map (W => 8);
  u4 : entity work.t_vhdl_namedothersgen_sub generic map (W => 4);
  u1 : entity work.t_vhdl_namedothersgen_sub generic map (W => 1);

  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
