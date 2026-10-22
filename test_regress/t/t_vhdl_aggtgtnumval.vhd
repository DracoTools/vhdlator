-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An aggregate target whose names are a port and a signal as long as the
-- top's generics make them, given a signal sized by a constant that folds
-- to a number: "(bid_extension, master_bid) <= master_bid_extended"
-- (10.6.2). The value's length is a number and the names' are expressions;
-- each name takes its run of the value from its left. From GHDL gna
-- issue2516, with a single element among the names and the values checked.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_aggtgtnumval is
  generic (MASTER_ID_WIDTH : positive := 4; ID_EXTENSION_WIDTH : positive := 3);
  port (bid_extension : out std_logic_vector(ID_EXTENSION_WIDTH - 1 downto 0));
end entity;

architecture rtl of t_vhdl_aggtgtnumval is
  constant SLAVE_ID_WIDTH : positive := MASTER_ID_WIDTH + ID_EXTENSION_WIDTH + 1;
  signal mid : std_logic;
  signal master_bid : std_logic_vector(MASTER_ID_WIDTH - 1 downto 0);
  signal master_bid_extended : std_logic_vector(SLAVE_ID_WIDTH - 1 downto 0) := "10100110";
begin
  (bid_extension, mid, master_bid) <= master_bid_extended;
  process
  begin
    wait for 1 ns;
    report to_string(bid_extension) & " " & std_logic'image(mid) & " " & to_string(master_bid);
    assert bid_extension = "101" and mid = '0' and master_bid = "0110" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
