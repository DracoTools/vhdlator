-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, numeric_std over vectors a generic sizes, whose
-- bodies are compiled for lengths elaboration settles: the comparison with
-- an integer renumbers its operand through an alias whose bounds are a
-- constant of the subprogram (gna bug010), and division hands back RESIZE
-- of a value carrying its own bounds (gna sr2676)

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_genericnum_sub is
  generic (depth : integer := 3);
  port (free : in unsigned(depth downto 0);
        full : out std_logic;
        less : out std_logic;
        quot : out unsigned(depth downto 0));
end entity;

architecture a of t_vhdl_nine_genericnum_sub is
  constant size : integer := 2 ** depth;
begin
  full <= '1' when free = size else '0';
  less <= '1' when free < 3 else '0';
  quot <= free / to_unsigned(3, depth + 1);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_genericnum is
end entity;

architecture a of t_vhdl_nine_genericnum is
  signal free : unsigned(3 downto 0) := "1000";
  signal full, less : std_logic;
  signal quot : unsigned(3 downto 0);
begin
  s : entity work.t_vhdl_nine_genericnum_sub port map (free, full, less, quot);

  process
  begin
    wait for 1 ns;
    report std_logic'image(full) & " " & std_logic'image(less) & " " & to_string(quot);
    assert full = '1' and less = '0' and quot = "0010" severity failure;
    free <= "0010";
    wait for 1 ns;
    report std_logic'image(full) & " " & std_logic'image(less) & " " & to_string(quot);
    assert full = '0' and less = '1' and quot = "0000" severity failure;
    free <= "1X00";
    wait for 1 ns;
    report std_logic'image(full) & " " & std_logic'image(less) & " " & to_string(quot);
    assert full = '0' and less = '0' and to_string(quot) = "XXXX" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
