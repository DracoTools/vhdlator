-- DESCRIPTION: Verilator: VHDL numeric_std "*" over lengths a generic gives
--
-- Emitted natively, by a function of the instance as wide as the generic
-- makes it, also past 64 elements; all 'X' where an operand holds a metavalue.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_mulgeneric_m is
  generic (W : natural);
  port (clk : in std_ulogic;
        a, b : in std_ulogic_vector(W-1 downto 0);
        ps : out std_ulogic_vector(2*W+1 downto 0);
        pu : out std_ulogic_vector(2*W-1 downto 0));
end entity;
architecture rtl of t_vhdl_nine_mulgeneric_m is
  signal sa, sb : signed(W downto 0);
begin
  sa <= signed(a(W-1) & a);
  sb <= signed(b(W-1) & b);
  process (clk)
  begin
    if rising_edge(clk) then
      ps <= std_ulogic_vector(sa * sb);
      pu <= std_ulogic_vector(unsigned(a) * unsigned(b));
    end if;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_nine_mulgeneric is end entity;
architecture a of t_vhdl_nine_mulgeneric is
  signal clk : std_ulogic := '0';
  signal a8, b8 : std_ulogic_vector(7 downto 0);
  signal ps8 : std_ulogic_vector(17 downto 0);
  signal pu8 : std_ulogic_vector(15 downto 0);
  signal a40, b40 : std_ulogic_vector(39 downto 0);
  signal ps40 : std_ulogic_vector(81 downto 0);
  signal pu40 : std_ulogic_vector(79 downto 0);
  signal a70, b70 : std_ulogic_vector(69 downto 0);
  signal ps70 : std_ulogic_vector(141 downto 0);
  signal pu70 : std_ulogic_vector(139 downto 0);
begin
  m8 : entity work.t_vhdl_nine_mulgeneric_m generic map (W => 8) port map (clk, a8, b8, ps8, pu8);
  m40 : entity work.t_vhdl_nine_mulgeneric_m generic map (W => 40)
    port map (clk, a40, b40, ps40, pu40);
  m70 : entity work.t_vhdl_nine_mulgeneric_m generic map (W => 70)
    port map (clk, a70, b70, ps70, pu70);
  process
    procedure tick is
    begin
      clk <= '1';
      wait for 1 ns;
      clk <= '0';
      wait for 1 ns;
    end procedure;
  begin
    a8 <= x"F3";
    b8 <= x"15";
    a40 <= x"80000000FF";
    b40 <= x"0000012345";
    a70 <= (69 => '1', 3 => '1', others => '0');
    b70 <= (68 => '1', 0 => '1', others => '0');
    wait for 1 ns;
    tick;
    assert to_hstring(ps8) = "3FEEF" and to_hstring(pu8) = "13EF"
      report "8: " & to_hstring(ps8) & " " & to_hstring(pu8) severity failure;
    assert to_hstring(ps40) = "3FFFFFF6E5D80012221BB"
      and to_hstring(pu40) = "00000091A280012221BB"
      report "40: " & to_hstring(ps40) & " " & to_hstring(pu40) severity failure;
    assert to_hstring(ps70) = "3E0000000000000000600000000000000008"
      and to_hstring(pu70) = "20000000000000000A00000000000000008"
      report "70: " & to_hstring(ps70) & " " & to_hstring(pu70) severity failure;
    a8 <= "1111X011";
    a40 <= (others => '1');
    b40 <= (others => '1');
    a70 <= (others => '1');
    b70 <= (others => '1');
    wait for 1 ns;
    tick;
    assert to_hstring(ps8) = "XXXXX" and to_hstring(pu8) = "XXXX"
      report "8x: " & to_hstring(ps8) & " " & to_hstring(pu8) severity failure;
    assert to_hstring(ps40) = "000000000000000000001"
      and to_hstring(pu40) = "FFFFFFFFFE0000000001"
      report "40b: " & to_hstring(ps40) & " " & to_hstring(pu40) severity failure;
    assert to_hstring(ps70) = "000000000000000000000000000000000001"
      and to_hstring(pu70) = "FFFFFFFFFFFFFFFFF800000000000000001"
      report "70b: " & to_hstring(ps70) & " " & to_hstring(pu70) severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
