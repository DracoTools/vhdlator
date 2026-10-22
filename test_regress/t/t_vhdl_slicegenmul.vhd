-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of a record element of an array element, written in a loop with
-- ends that multiply a generic, "masters(i).tData(32 * N - 1 downto 0) :=
-- sData(32 * N * (i + 1) - 1 downto 32 * N * i)": the widths of the two
-- sides are compared term by term, and one held a constant the back end had
-- typed beside one it had not, which stopped the build. Shaped after surf's
-- CoaXPressRxLaneMuxWrapper.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicegenmul is
  generic (NUM_LANES_G : positive := 2);
end entity;

architecture rtl of t_vhdl_slicegenmul is
  type MType is record
    tValid : std_logic;
    tData : std_logic_vector(127 downto 0);
  end record;
  type MArray is array (natural range <>) of MType;
  constant M_INIT_C : MType := (tValid => '0', tData => (others => '0'));
  signal sData : std_logic_vector(32 * NUM_LANES_G * NUM_LANES_G - 1 downto 0) := (others => '1');
  signal rx : MArray(NUM_LANES_G - 1 downto 0) := (others => M_INIT_C);
begin
  process (sData)
    variable masters : MArray(NUM_LANES_G - 1 downto 0);
  begin
    masters := (others => M_INIT_C);
    for i in 0 to NUM_LANES_G - 1 loop
      masters(i).tData(32 * NUM_LANES_G - 1 downto 0) :=
        sData(32 * NUM_LANES_G * (i + 1) - 1 downto 32 * NUM_LANES_G * i);
    end loop;
    rx <= masters;
  end process;
  process
  begin
    wait for 1 ns;
    assert rx(1).tData(63 downto 0) = x"FFFFFFFFFFFFFFFF" severity failure;
    assert rx(1).tData(127 downto 64) = x"0000000000000000" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
