-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, what the NVC regression and GHDL's testsuite
-- found: a range choice of a matching case, a slice of what TO_UNSIGNED
-- gives, a block's port default as its driver's start, drivers in if
-- generates, an inout port an inner instance drives in part, and an
-- UNSIGNED plus a STD_ULOGIC beside a joining a generic sizes

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_lib2_part is
  generic (N : natural := 1);
  port (o : out std_logic);
end entity;
architecture a of t_vhdl_nine_lib2_part is
begin
  o <= 'H';
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_lib2_bus is
  port (b : inout std_logic_vector(1 to 2) := (others => 'Z'));
end entity;
architecture a of t_vhdl_nine_lib2_bus is
begin
  u : entity work.t_vhdl_nine_lib2_part port map (o => b(2));
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_lib2_gen is
  generic (W : natural := 3; SEL : boolean := true);
  port (a : in unsigned(W - 1 downto 0); c : in std_logic;
        y : out unsigned(W downto 0); x : out std_logic);
end entity;
architecture a of t_vhdl_nine_lib2_gen is
begin
  y <= ('0' & a) + c;
  g1 : if SEL generate
    x <= '1';
  end generate;
  g2 : if not SEL generate
    x <= '0';
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_nine_lib2 is
end entity;

architecture a of t_vhdl_nine_lib2 is
  signal p      : std_logic := '0';
  signal q      : integer := 0;
  signal s      : std_logic := 'Z';
  signal bus2   : std_logic_vector(1 to 2);
  signal a3     : unsigned(2 downto 0) := "111";
  signal y4     : unsigned(3 downto 0);
  signal x      : std_logic;
begin
  process (p) is
  begin
    case? p is
      when '0' to '1' => q <= 99;
      when others => q <= 3;
    end case?;
  end process;

  b : block
    port (s1 : out std_logic := '0');
    port map (s1 => s);
  begin
    process
    begin
      wait for 2 ns;
      s1 <= 'Z';
      wait;
    end process;
  end block;

  ub : entity work.t_vhdl_nine_lib2_bus port map (b => bus2);
  ug : entity work.t_vhdl_nine_lib2_gen generic map (W => 3, SEL => true)
    port map (a => a3, c => '1', y => y4, x => x);

  process
    variable v : std_logic_vector(15 downto 0);
  begin
    wait for 1 ns;
    report "q " & integer'image(q);
    v := std_logic_vector(to_unsigned(250000, 32)(31 downto 16)) or x"0040";
    report "v " & to_hstring(v);
    report "s " & std_logic'image(s);
    report "bus " & to_string(bus2);
    report "y " & to_string(y4) & " x " & std_logic'image(x);
    wait for 2 ns;
    report "s " & std_logic'image(s);
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
