-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal named by an external name (8.7), handed through three procedures
-- to a "wait until (target = exp) for timeout": UVVM's clock_generator_tb
-- checks each clock with check_clock(clk_1, ...), which calls await_value,
-- which calls await_value again with a match strictness
-- (uvvm_util/src/methods_pkg.vhd). A wait is sensitive to what its
-- condition reads, and the external name -- a reference into another
-- instance -- was not counted: in the innermost copy the condition also
-- read the constant parameter exp, so the wait was sensitive to exp alone
-- and woke only at its timeout.
library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_extwaitdeep_pkg is
  type t_match is (MATCH_EXACT, MATCH_STD);
  procedure await_value(signal target : in std_logic; constant exp : in std_logic;
                        constant strict : in t_match;
                        constant timeout : in time; constant msg : in string);
  procedure await_value(signal target : in std_logic; constant exp : in std_logic;
                        constant timeout : in time; constant msg : in string);
end package;
package body t_vhdl_extwaitdeep_pkg is
  procedure await_value(signal target : in std_logic; constant exp : in std_logic;
                        constant timeout : in time; constant msg : in string) is
  begin
    await_value(target, exp, MATCH_EXACT, timeout, msg);
  end procedure;
  procedure await_value(signal target : in std_logic; constant exp : in std_logic;
                        constant strict : in t_match;
                        constant timeout : in time; constant msg : in string) is
  begin
    if target /= exp then
      wait until target = exp for timeout;
    end if;
    report msg & " " & std_logic'image(target) & " at " & time'image(now);
    assert target = exp severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_extwaitdeep_gen is
  generic (H : time := 5 ns; START : time := 30 ns);
  port (clk : out std_logic);
end entity;
architecture a of t_vhdl_extwaitdeep_gen is
  signal ena : boolean := false;
begin
  process
  begin
    wait for START;
    ena <= true;
    wait;
  end process;
  process
  begin
    wait for 0 ns;
    loop
      if not ena then
        clk <= '0';
        wait until ena;
      end if;
      clk <= '1';
      wait for H;
      clk <= '0';
      wait for H;
    end loop;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_extwaitdeep_th is
  generic (GC_P1 : time := 10 ns; GC_P2 : time := 20 ns);
end entity;
architecture a of t_vhdl_extwaitdeep_th is
  signal clk_1 : std_logic;
  signal clk_2 : std_logic;
  signal clk_3 : std_logic;
begin
  i1_gen : entity work.t_vhdl_extwaitdeep_gen generic map (H => GC_P1 / 2, START => 30 ns) port map (clk => clk_1);
  i2_gen : entity work.t_vhdl_extwaitdeep_gen generic map (H => GC_P2 / 2, START => 1 us) port map (clk => clk_2);
  i3_gen : entity work.t_vhdl_extwaitdeep_gen generic map (H => 20 ns, START => 1 us) port map (clk => clk_3);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_extwaitdeep_pkg.all;
entity t_vhdl_extwaitdeep is
end entity;
architecture a of t_vhdl_extwaitdeep is
begin
  i_th : entity work.t_vhdl_extwaitdeep_th;
  process
    alias clk_1 is << signal i_th.clk_1 : std_logic >>;
    alias clk_2 is << signal i_th.clk_2 : std_logic >>;
    alias clk_3 is << signal i_th.clk_3 : std_logic >>;
    procedure check_clock(signal clock : std_logic) is
    begin
      await_value(clock, '0', 200 ns, "fall");
      await_value(clock, '1', 200 ns, "rise");
      await_value(clock, '0', 200 ns, "fall2");
    end procedure;
  begin
    wait for 1 ns;
    assert clk_2 = '0' and clk_3 = '0' severity failure;
    check_clock(clk_1);
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
