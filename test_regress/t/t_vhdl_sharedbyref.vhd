-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A composite variable parameter may be passed by reference (4.2.2.2), and
-- a shared variable handed to a procedure that never returns is only ever
-- seen that way. UVVM's UART monitor (bitvis_vip_uart/src/uart_monitor.vhd)
-- runs forever from a concurrent procedure call, writes what it saw into a
-- shared transaction record through an alias, pulses a trigger the
-- testbench waits on, and reads a configuration record the testbench
-- changes while it runs.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_sharedbyref_pkg is
  type t_ch is (TX, RX);
  type t_info is record
    count : natural;
    data  : std_logic_vector(7 downto 0);
  end record;
  type t_config is record
    step : natural;
    gap  : time;
  end record;
  type t_info_array is array (t_ch range <>, natural range <>) of t_info;
  type t_config_array is array (t_ch range <>, natural range <>) of t_config;
  type t_trig_array is array (t_ch range <>, natural range <>) of std_logic;
  shared variable info : t_info_array(TX to RX, 0 to 3) := (others => (others => (0, x"00")));
  shared variable config : t_config_array(TX to RX, 0 to 3) := (others => (others => (1, 10 ns)));
  signal trig : t_trig_array(TX to RX, 0 to 3) := (others => (others => '0'));
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sharedbyref_pkg.all;

entity t_vhdl_sharedbyref_mon is
  generic (IDX : natural := 1);
end entity;

architecture a of t_vhdl_sharedbyref_mon is
  alias tx_info : t_info is info(TX, IDX);
  alias tx_trig : std_logic is trig(TX, IDX);

  procedure watch(signal t : inout std_logic;
                  variable i : inout t_info;
                  variable c : in t_config) is
    variable n : natural := 0;
  begin
    loop
      wait for c.gap;
      n := n + c.step;
      i.count := n;
      i.data := x"a5";
      t <= '1';
      wait for 0 ns;
      t <= '0';
    end loop;
  end procedure;
begin
  p_watch : watch(tx_trig, tx_info, config(TX, IDX));
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_sharedbyref_pkg.all;

entity t_vhdl_sharedbyref is
end entity;

architecture a of t_vhdl_sharedbyref is
begin
  i_mon : entity work.t_vhdl_sharedbyref_mon generic map (IDX => 1);

  process
    variable seen : natural := 0;
  begin
    for k in 1 to 3 loop
      wait until trig(TX, 1) = '1';
      seen := seen + 1;
      assert info(TX, 1).count = k
        report "pulse " & integer'image(k) & " saw count " & integer'image(info(TX, 1).count)
        severity failure;
      assert info(TX, 1).data = x"a5" report "data not seen" severity failure;
    end loop;
    -- The monitor reads the configuration as it is now, not as it was
    -- when the call was made
    config(TX, 1).step := 10;
    wait until trig(TX, 1) = '1';
    assert info(TX, 1).count = 13
      report "after the change saw count " & integer'image(info(TX, 1).count)
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
