-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A function returning a slice of its parameter whose end another
-- parameter gives, called with a generic for it: "v(29 downto n - 2)" in
-- the copy made for the call is as long as its ends say, not as long as v.
-- Given to std_logic_1164's "xor" beside a slice a generic bounds, the two
-- lengths were taken to differ (bugs.md 388mg).

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicegenarg_sub is
  generic (lsb : natural := 4);
  port (addr : in std_ulogic_vector(31 downto 0);
        cfg : in std_ulogic_vector(29 downto 0);
        hit : out std_ulogic;
        mixed : out std_ulogic_vector(31 downto lsb));
end entity;

architecture a of t_vhdl_slicegenarg_sub is
  function slc (v : std_ulogic_vector; n : natural) return std_ulogic_vector is
  begin
    return v(29 downto n - 2);
  end function;
  type mask_t is array (0 to 1) of std_ulogic_vector(31 downto lsb);
  signal mask : mask_t := (others => (others => '1'));
begin
  hit <= '1' when (addr(31 downto lsb) and mask(0)) = (slc(cfg, lsb) and mask(0)) else '0';
  mixed <= addr(31 downto lsb) xor slc(cfg, lsb);
  process
    variable m : natural := lsb;
    constant r : std_ulogic_vector := slc(cfg, lsb);
    constant rm : std_ulogic_vector := slc(cfg, m);
  begin
    assert r'length = 32 - lsb severity failure;
    assert r'right = lsb - 2 severity failure;
    assert rm'length = 32 - lsb severity failure;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_slicegenarg is
end entity;

architecture a of t_vhdl_slicegenarg is
  signal addr : std_ulogic_vector(31 downto 0) := x"12345678";
  signal cfg : std_ulogic_vector(29 downto 0) := x"1234567" & "00";
  signal hit : std_ulogic;
  signal mixed : std_ulogic_vector(31 downto 4);
begin
  s : entity work.t_vhdl_slicegenarg_sub port map (addr, cfg, hit, mixed);
  process
  begin
    wait for 1 ns;
    assert hit = '1' and mixed = x"0000000" report to_hstring(mixed) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
