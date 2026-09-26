-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- The history of a signal named by an external name (8.7): UVVM's clock
-- generator testbench passes "alias clk_1 is << signal
-- i_test_harness.clk_1 : std_logic >>" through a procedure of its own to
-- check_stable, which asks 'LAST_VALUE and 'LAST_EVENT of its signal
-- formal (bitvis_vip_clock_generator/tb/maintenance_tb/
-- clock_generator_tb.vhd, uvvm_util/src/methods_pkg.vhd). The signal is
-- another module's, reached by a hierarchical reference, and the call fell
-- back to passing its value, which has no history.
-- nvc 1.22.1 and Questa 25.1std run this; GHDL 7.0.0-dev stops with "NULL
-- access dereferenced" as soon as the process declares the alias.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_extattr_pkg is
  procedure check_stable(signal target : std_logic; constant stable_req : time;
                         constant expect_ok : boolean; constant expect_last : std_logic);
end package;

package body t_vhdl_extattr_pkg is
  procedure check_stable(signal target : std_logic; constant stable_req : time;
                         constant expect_ok : boolean; constant expect_last : std_logic) is
    constant C_LAST_VALUE : std_logic := target'last_value;
    constant C_LAST_CHANGE : time := target'last_event;
  begin
    assert (C_LAST_CHANGE >= stable_req) = expect_ok
      report "last change " & time'image(C_LAST_CHANGE) severity failure;
    assert C_LAST_VALUE = expect_last
      report "last value " & std_logic'image(C_LAST_VALUE) severity failure;
  end procedure;
end package body;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extattr_th is
end entity;

architecture a of t_vhdl_extattr_th is
  signal clk_1 : std_logic := '0';
begin
  clk_1 <= '1' after 10 ns, '0' after 20 ns;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_extattr_pkg.all;

entity t_vhdl_extattr is
end entity;

architecture a of t_vhdl_extattr is
begin
  i_test_harness : entity work.t_vhdl_extattr_th;

  process
    alias clk_1 is << signal i_test_harness.clk_1 : std_logic >>;
    procedure check_clock(signal clock : std_logic; constant req : time;
                          constant expect_ok : boolean; constant expect_last : std_logic) is
    begin
      check_stable(clock, req, expect_ok, expect_last);
    end procedure;
  begin
    wait for 15 ns;
    -- Directly, and through a procedure of the process's own
    assert clk_1'last_event = 5 ns and clk_1'last_value = '0' severity failure;
    check_stable(clk_1, 3 ns, true, '0');
    check_clock(clk_1, 8 ns, false, '0');
    wait until clk_1'event;
    assert now = 20 ns and clk_1 = '0' severity failure;
    check_clock(clk_1, 0 ns, true, '1');
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
