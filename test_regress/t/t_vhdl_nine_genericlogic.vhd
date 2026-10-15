-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, std_logic_1164's operators over vectors whose
-- lengths a generic gives and whose bounds are written differently: the
-- lengths are the same once elaborated, and the operators are emitted
-- natively at that length, as neorv32's PMP compares
-- "(i_addr(31 downto lsb) and mask(r)) = (pmpaddr(r)(29 downto lsb - 2)
-- and mask(r))"

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_genericlogic_sub is
  generic (lsb : natural := 4);
  port (addr : in std_ulogic_vector(31 downto 0);
        cfg : in std_ulogic_vector(29 downto 0);
        hit : out std_ulogic;
        mixed : out std_ulogic_vector(31 downto lsb));
end entity;

architecture a of t_vhdl_nine_genericlogic_sub is
  type mask_t is array (0 to 1) of std_ulogic_vector(31 downto lsb);
  signal mask : mask_t := (others => (others => '1'));
begin
  hit <= '1' when (addr(31 downto lsb) and mask(0)) = (cfg(29 downto lsb - 2) and mask(0))
         else '0';
  mixed <= addr(31 downto lsb) xor cfg(29 downto lsb - 2);
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_genericlogic is
end entity;

architecture a of t_vhdl_nine_genericlogic is
  signal addr : std_ulogic_vector(31 downto 0) := x"12345678";
  signal cfg : std_ulogic_vector(29 downto 0) := x"1234567" & "00";
  signal hit : std_ulogic;
  signal mixed : std_ulogic_vector(31 downto 4);
begin
  s : entity work.t_vhdl_nine_genericlogic_sub port map (addr, cfg, hit, mixed);

  process
  begin
    wait for 1 ns;
    report std_ulogic'image(hit) & " " & to_hstring(mixed);
    assert hit = '1' and mixed = x"0000000" severity failure;
    addr(9) <= 'X';
    addr(5) <= 'H';
    wait for 1 ns;
    report std_ulogic'image(hit) & " " & to_string(mixed);
    assert hit = '0' severity failure;
    assert to_string(mixed) = "0000000000000000000000X00000" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
