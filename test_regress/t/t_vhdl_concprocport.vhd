-- DESCRIPTION: Verilator: VHDL concurrent procedure call over ports
--
-- This file is part of Verilator.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
--
-- A concurrent procedure call is a process that calls the procedure and
-- then waits on the signals its actuals name (11.4), and a port is a
-- signal (6.5.2). VUnit's check_stable returns when a stability window
-- closes and relies on being called again for the next one; called in an
-- entity over its ports, it was called once and never again.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_concprocport_pkg is
  type fsm_t is (idle, active);
  procedure stable(signal clock : in std_logic; signal start_event : in std_logic;
                   signal end_event : in std_logic; signal expr : in std_logic_vector;
                   signal fails : inout natural);
end package;

package body t_vhdl_concprocport_pkg is
  procedure stable(signal clock : in std_logic; signal start_event : in std_logic;
                   signal end_event : in std_logic; signal expr : in std_logic_vector;
                   signal fails : inout natural) is
    variable state : fsm_t := idle;
    variable ref   : std_logic_vector(expr'range);
  begin
    loop
      wait until rising_edge(clock);
      case state is
        when idle =>
          if start_event = '1' then
            ref := expr;
            exit when end_event = '1';
            state := active;
          end if;
        when active =>
          if ref /= expr then
            report "unstable at " & time'image(now);
            fails <= fails + 1;
          end if;
          exit when end_event = '1';
      end case;
    end loop;
  end;
end package body;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_concprocport_pkg.all;

entity t_vhdl_concprocport_sub is
  generic (w : natural);
  port (
    clk    : in std_logic;
    tvalid : in std_logic;
    tready : in std_logic;
    tdata  : in std_logic_vector(w - 1 downto 0);
    fails  : inout natural := 0);
end entity;

architecture a of t_vhdl_concprocport_sub is
begin
  stable(clk, tvalid, tready, tdata, fails);
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_concprocport is
end entity;

architecture a of t_vhdl_concprocport is
  signal clk            : std_logic := '0';
  signal tvalid, tready : std_logic := '0';
  signal tdata          : std_logic_vector(7 downto 0) := (others => '0');
  signal fails          : natural;
begin
  clk <= not clk after 5 ns;

  sub : entity work.t_vhdl_concprocport_sub
    generic map (w => 8)
    port map (clk => clk, tvalid => tvalid, tready => tready, tdata => tdata, fails => fails);

  process
    constant zeros : std_logic_vector(7 downto 0) := (others => '0');
    constant ones  : std_logic_vector(7 downto 0) := (others => '1');
  begin
    -- Two windows, tdata changing inside each
    for phase in 1 to 2 loop
      wait until rising_edge(clk);
      tvalid <= '1';
      wait until rising_edge(clk);
      tdata <= ones;
      wait until rising_edge(clk);
      tdata <= zeros;
      tready <= '1';
      wait until rising_edge(clk);
      tready <= '0';
      tvalid <= '0';
      wait until rising_edge(clk);
    end loop;
    assert fails = 2 report "instabilities seen " & integer'image(fails) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
