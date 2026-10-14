-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- Under --vhdl-metavalues, INOUT ports of the top-level entity that the
-- design drives itself, with nothing outside to join them: each reads what
-- the design drives on it -- one process, two resolved together, or none
-- at all -- and not a value the testbench never set (gna bug0129)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_topinout is
  port (tready : inout std_logic;
        bus2 : inout std_logic_vector(3 downto 0);
        idle : inout std_logic := 'H');
end entity;

architecture a of t_vhdl_nine_topinout is
  signal cnt : natural;
  signal cnt_next : natural;
begin
  process (all)
  begin
    if tready = '1' then
      cnt_next <= cnt + 1;
    else
      cnt_next <= cnt;
    end if;
  end process;

  bus2 <= "10ZZ";
  bus2 <= "ZZ1L";

  process
  begin
    tready <= '1';
    cnt <= 1;
    wait for 1 ns;
    report std_logic'image(tready) & " " & to_string(bus2) & " " & std_logic'image(idle)
      & " " & natural'image(cnt_next);
    assert cnt_next = 2 severity failure;
    assert bus2 = "101L" severity failure;
    assert idle = 'H' severity failure;
    tready <= 'W';
    wait for 1 ns;
    report std_logic'image(tready) & " " & natural'image(cnt_next);
    assert tready = 'W' severity failure;
    assert cnt_next = 1 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
