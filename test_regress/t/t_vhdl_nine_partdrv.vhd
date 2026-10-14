-- DESCRIPTION: Verilator: drivers of the nine values that drive only part of a signal
--
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

-- A process has a driver only for the elements it assigns (14.7.2): one
-- that assigns a field of a record of resolved fields drives that field
-- and no other, and a signal a generate declares has one driver per
-- iteration, its own

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_nine_partdrv is
end entity;

architecture a of t_vhdl_nine_partdrv is
  type t_if is record
    tvalid : std_logic;
    tready : std_logic;
    tdata : std_logic_vector(3 downto 0);
  end record;
  signal small : t_if;
  signal clk : std_logic := '0';
  signal d : std_logic_vector(1 downto 0) := "00";
  signal q : std_logic_vector(1 downto 0);
begin
  small.tready <= '1';
  small.tdata(0) <= 'H';

  g : for i in 1 downto 0 generate
    signal s : std_logic;
  begin
    s <= d(i) when rising_edge(clk);
    q(i) <= s;
  end generate;

  process
  begin
    small <= (tvalid => '0', tready => 'Z', tdata => "ZZ10");
    d <= "10";
    wait for 1 ns;
    clk <= '1';
    wait for 1 ns;
    report "tvalid=" & std_logic'image(small.tvalid) & " tready=" & std_logic'image(small.tready)
      & " tdata=" & to_string(small.tdata) & " q=" & to_string(q);
    assert small.tvalid = '0' and small.tready = '1' severity failure;
    assert small.tdata = "ZZ10" severity failure;
    assert q = "10" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
