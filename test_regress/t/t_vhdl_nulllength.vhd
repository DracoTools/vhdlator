-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- 'LENGTH of a null range is 0 (16.2.3), also where its ends are expressions
-- that only the instance settles: a port "(n - 1 downto 0)" with the generic
-- n given 0 (GHDL gna bug081 has it of a package constant).

entity t_vhdl_nulllength_sub is
  generic (n : natural);
  port (o : out bit_vector(n - 1 downto 0); len : out integer);
end entity;

architecture behav of t_vhdl_nulllength_sub is
begin
  len <= o'length;
end architecture;

entity t_vhdl_nulllength is
end entity;

architecture behav of t_vhdl_nulllength is
  signal o : bit_vector(-1 downto 0);
  signal len : integer := -5;
begin
  sub : entity work.t_vhdl_nulllength_sub generic map (n => 0) port map (o => o, len => len);
  process begin
    wait for 1 ns;
    assert len = 0 report "length " & integer'image(len) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
