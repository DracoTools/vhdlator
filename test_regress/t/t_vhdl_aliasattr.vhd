-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A signal attribute of an alias of one element of a signal declared in a
-- package: UVVM's "alias sbi_vvc_transaction_info_trigger : std_logic is
-- global_sbi_vvc_transaction_trigger(C_SBI_VVC)" and its 'event
-- (bitvis_vip_uart/tb/uvvm_demo_th.vhd). The alias has the history of the
-- element it denotes (6.6.2), and 'event of it was refused.
library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_aliasattr_pkg is
  type t_dir is (RX, TX);
  type t_trig_2d is array (t_dir range <>, natural range <>) of std_logic;
  signal global_sbi_trigger : std_logic_vector(0 to 3) := (others => '0');
  signal global_uart_trigger : t_trig_2d(t_dir'left to t_dir'right, 0 to 3)
    := (others => (others => '0'));
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_aliasattr_pkg.all;

entity t_vhdl_aliasattr is
end entity;

architecture a of t_vhdl_aliasattr is
begin
  global_sbi_trigger(1) <= '1' after 2 ns, '0' after 3 ns;
  global_uart_trigger(TX, 2) <= '1' after 5 ns;

  process
    alias sbi_trig : std_logic is global_sbi_trigger(1);
    alias tx_trig : std_logic is global_uart_trigger(TX, 2);
    variable events : natural := 0;
  begin
    wait until sbi_trig = '1';
    assert sbi_trig'event and not tx_trig'event severity failure;
    assert sbi_trig'last_value = '0' severity failure;
    wait until sbi_trig = '0';
    assert sbi_trig'event and sbi_trig'last_value = '1' severity failure;
    assert now = 3 ns severity failure;
    wait until tx_trig = '1';
    assert tx_trig'event and not sbi_trig'event severity failure;
    assert sbi_trig'last_event = 2 ns report time'image(sbi_trig'last_event) severity failure;
    assert sbi_trig'stable(1 ns) and not tx_trig'stable severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
